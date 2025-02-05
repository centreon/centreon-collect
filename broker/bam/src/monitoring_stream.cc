/**
 * Copyright 2014-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <spdlog/fmt/ostr.h>

#include "com/centreon/broker/bam/monitoring_stream.hh"

#include "bbdo/bam/ba_status.hh"
#include "bbdo/bam/kpi_status.hh"
#include "bbdo/bam/rebuild.hh"
#include "com/centreon/broker/bam/configuration/reader_v2.hh"
#include "com/centreon/broker/bam/event_cache_visitor.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/misc/fifo_client.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;
using namespace com::centreon::broker::database;

using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] ext_cmd_file    The command file to write into.
 *  @param[in] db_cfg          Main (centreon) database configuration.
 *  @param[in] storage_db_cfg  Storage (centreon_storage) database
 *                             configuration.
 *  @param[in] cache           The persistent cache.
 */
monitoring_stream::monitoring_stream(
    const std::string& ext_cmd_file,
    const database_config& db_cfg,
    const database_config& storage_db_cfg,
    std::shared_ptr<persistent_cache> cache,
    const std::shared_ptr<spdlog::logger>& logger)
    : io::stream("BAM"),
      _ext_cmd_file(ext_cmd_file),
      _logger{logger},
      _applier(_logger),
      _mysql(db_cfg.auto_commit_conf(), logger),
      _conf_queries_per_transaction(db_cfg.get_queries_per_transaction()),
      _pending_events(0),
      _pending_request(0),
      _storage_db_cfg(storage_db_cfg),
      _cache(std::move(cache)),
      _forced_svc_checks_timer{com::centreon::common::pool::io_context()} {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring_stream constructor");
  if (!_conf_queries_per_transaction) {
    _conf_queries_per_transaction = 1;
  }
  // Prepare queries.
  _prepare();

  // Let's update BAs then we will be able to load the cache with inherited
  // downtimes.
  update();
}

/**
 *  Destructor.
 */
monitoring_stream::~monitoring_stream() {
  // save cache
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring_stream destructor");
  try {
    _write_cache();
  } catch (std::exception const& e) {
    _logger->error("BAM: can't save cache: '{}'", e.what());
  }
  SPDLOG_LOGGER_DEBUG(_logger, "BAM: monitoring_stream destruction done");
}

/**
 *  Flush data.
 *
 *  @return Number of acknowledged events.
 */
int32_t monitoring_stream::flush() {
  _execute();
  _pending_request = 0;
  int retval = _pending_events;
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring_stream flush: {} events",
                      retval);
  _pending_events = 0;
  return retval;
}

/**
 * @brief Flush data and stop the stream.
 *
 * @return Number of acknowledged events.
 */
int32_t monitoring_stream::stop() {
  int32_t retval = flush();
  _logger->info("monitoring stream: stopped with {} events acknowledged",
                retval);
  /* I want to be sure the timer is really stopped. */
  std::promise<void> p;
  {
    _logger->info(
        "bam: monitoring_stream - waiting for forced service checks to be "
        "done");
    std::lock_guard<std::mutex> lck(_forced_svc_checks_m);
    _forced_svc_checks_timer.expires_after(std::chrono::seconds(0));
    _forced_svc_checks_timer.async_wait(
        [this, &p](const boost::system::error_code& ec) {
          _explicitly_send_forced_svc_checks(ec);
          {
            std::lock_guard<std::mutex> lck(_forced_svc_checks_m);
            _forced_svc_checks_timer.cancel();
          }
          p.set_value();
        });
  }
  p.get_future().wait();
  /* Now, it is really cancelled. */
  _logger->info("bam: monitoring_stream - stop finished");

  return retval;
}

/**
 *  Generate default state.
 */
void monitoring_stream::initialize() {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring_stream initialize");
  multiplexing::publisher pblshr;
  event_cache_visitor ev_cache;
  _applier.visit(&ev_cache);
  ev_cache.commit_to(pblshr);
}

/**
 *  Read from the datbase.
 *  Get the next available bam event.
 *
 *  @param[out] d         Cleared.
 *  @param[in]  deadline  Timeout.
 *
 *  @return This method will throw.
 */
bool monitoring_stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  (void)deadline;
  d.reset();
  throw exceptions::shutdown("cannot read from BAM monitoring stream");
  return true;
}

/**
 *  Rebuild index and metrics cache.
 */
void monitoring_stream::update() {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring_stream update");
  try {
    configuration::state s{_logger};
    configuration::reader_v2 r(_mysql, _storage_db_cfg);
    r.read(s);
    _applier.apply(s);
    _ba_mapping = s.get_ba_svc_mapping();
    _rebuild();
    initialize();
    // Read cache.
    _read_cache();
  } catch (std::exception const& e) {
    throw msg_fmt("BAM: could not process configuration update: {}", e.what());
  }
}

// When bulk statements are available.
struct bulk_ba_binder {
  const std::shared_ptr<io::data>& event;
  void operator()(database::mysql_bulk_bind& binder) const {
    if (event->type() == ba_status::static_type()) {
      const ba_status& status = *std::static_pointer_cast<ba_status>(event);
      binder.set_value_as_f32(0, status.level_nominal);
      binder.set_value_as_f32(1, status.level_acknowledgement);
      binder.set_value_as_f32(2, status.level_downtime);
      binder.set_value_as_u32(7, status.ba_id);
      if (status.last_state_change.is_null())
        binder.set_null_u64(3);
      else
        binder.set_value_as_u64(3, status.last_state_change.get_time_t());
      binder.set_value_as_bool(4, status.in_downtime);
      binder.set_value_as_i32(5, status.state);
      binder.set_null_str(6);
    } else {
      const BaStatus& status =
          static_cast<const pb_ba_status*>(event.get())->obj();
      binder.set_value_as_f32(0, status.level_nominal());
      binder.set_value_as_f32(1, status.level_acknowledgement());
      binder.set_value_as_f32(2, status.level_downtime());
      binder.set_value_as_u32(7, status.ba_id());
      if (status.last_state_change() <= 0)
        binder.set_null_u64(3);
      else
        binder.set_value_as_u64(3, status.last_state_change());
      binder.set_value_as_bool(4, status.in_downtime());
      binder.set_value_as_i32(5, status.state());
      binder.set_value_as_str(6, status.output());
    }
    binder.next_row();
  }
};

// When bulk statements are not available.
struct ba_binder {
  const std::shared_ptr<io::data>& event;
  std::string operator()() const {
    if (event->type() == ba_status::static_type()) {
      const ba_status& status = *std::static_pointer_cast<ba_status>(event);
      if (status.last_state_change.is_null())
        return fmt::format("({},{},{},NULL,{},{},NULL,{})",
                           status.level_nominal, status.level_acknowledgement,
                           status.level_downtime, int(status.in_downtime),
                           status.state, status.ba_id);
      else
        return fmt::format("({},{},{},{},{},{},NULL,{})", status.level_nominal,
                           status.level_acknowledgement, status.level_downtime,
                           status.last_state_change.get_time_t(),
                           int(status.in_downtime), status.state, status.ba_id);
    } else {
      const BaStatus& status =
          static_cast<const pb_ba_status*>(event.get())->obj();
      if (status.last_state_change() <= 0)
        return fmt::format(
            "({},{},{},NULL,{},{},'{}',{})", status.level_nominal(),
            status.level_acknowledgement(), status.level_downtime(),
            int(status.in_downtime()), int(status.state()), status.output(),
            status.ba_id());
      else
        return fmt::format(
            "({},{},{},{},{},{},'{}',{})", status.level_nominal(),
            status.level_acknowledgement(), status.level_downtime(),
            status.last_state_change(), int(status.in_downtime()),
            int(status.state()), status.output(), status.ba_id());
    }
  }
};

// When bulk statements are available.
struct bulk_kpi_binder {
  const std::shared_ptr<io::data>& event;
  void operator()(database::mysql_bulk_bind& binder) const {
    if (event->type() == kpi_status::static_type()) {
      const kpi_status& status = *std::static_pointer_cast<kpi_status>(event);
      binder.set_value_as_f32(0, status.level_acknowledgement_hard);
      binder.set_value_as_i32(1, status.state_hard);
      binder.set_value_as_f32(2, status.level_downtime_hard);
      binder.set_value_as_f32(3, status.level_nominal_hard);
      binder.set_value_as_i32(4, 1 + 1);
      if (status.last_state_change.is_null())
        binder.set_null_u64(5);
      else
        binder.set_value_as_u64(5, status.last_state_change.get_time_t());
      binder.set_value_as_f32(6, status.last_impact);
      binder.set_value_as_bool(7, status.valid);
      binder.set_value_as_bool(8, status.in_downtime);
      binder.set_value_as_u32(9, status.kpi_id);
    } else {
      const KpiStatus& status(
          std::static_pointer_cast<pb_kpi_status>(event)->obj());
      binder.set_value_as_f32(0, status.level_acknowledgement_hard());
      binder.set_value_as_i32(1, status.state_hard());
      binder.set_value_as_f32(2, status.level_downtime_hard());
      binder.set_value_as_f32(3, status.level_nominal_hard());
      binder.set_value_as_i32(4, 1 + 1);
      if (status.last_state_change() <= 0)
        binder.set_null_u64(5);
      else
        binder.set_value_as_u64(5, status.last_state_change());
      binder.set_value_as_f32(6, status.last_impact());
      binder.set_value_as_bool(7, status.valid());
      binder.set_value_as_bool(8, status.in_downtime());
      binder.set_value_as_u32(9, status.kpi_id());
    }
    binder.next_row();
  }
};

// When bulk statements are not available.
struct kpi_binder {
  const std::shared_ptr<io::data>& event;
  std::string operator()() const {
    if (event->type() == kpi_status::static_type()) {
      const kpi_status& status = *std::static_pointer_cast<kpi_status>(event);
      if (status.last_state_change.is_null()) {
        return fmt::format("({},{},{},{},1+1,NULL,{},{},{},{})",
                           status.level_acknowledgement_hard, status.state_hard,
                           status.level_downtime_hard,
                           status.level_nominal_hard, status.last_impact,
                           int(status.valid), int(status.in_downtime),
                           status.kpi_id);
      } else {
        return fmt::format(
            "({},{},{},{},1+1,{},{},{},{},{})",
            status.level_acknowledgement_hard, status.state_hard,
            status.level_downtime_hard, status.level_nominal_hard,
            status.last_state_change.get_time_t(), status.last_impact,
            int(status.valid), int(status.in_downtime), status.kpi_id);
      }
    } else {
      const KpiStatus& status(
          std::static_pointer_cast<pb_kpi_status>(event)->obj());
      if (status.last_state_change() <= 0) {
        return fmt::format("({},{},{},{},1+1,NULL,{},{},{},{})",
                           status.level_acknowledgement_hard(),
                           static_cast<uint32_t>(status.state_hard()),
                           status.level_downtime_hard(),
                           status.level_nominal_hard(), status.last_impact(),
                           int(status.valid()), int(status.in_downtime()),
                           status.kpi_id());
      } else {
        return fmt::format(
            "({},{},{},{},1+1,{},{},{},{},{})",
            status.level_acknowledgement_hard(),
            static_cast<uint32_t>(status.state_hard()),
            status.level_downtime_hard(), status.level_nominal_hard(),
            status.last_state_change(), status.last_impact(),
            int(status.valid()), int(status.in_downtime()), status.kpi_id());
      }
    }
  }
};

/**
 *  Write an event.
 *
 *  @param[in] data Event pointer.
 *
 *  @return Number of events acknowledged.
 */
int monitoring_stream::write(const std::shared_ptr<io::data>& data) {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring_stream write {}", *data);
  // Take this event into account.
  ++_pending_events;

  // this lambda asks mysql to do a commit if we have
  // _conf_queries_per_transaction waiting requests
  auto commit_if_needed = [this]() {
    if (_conf_queries_per_transaction > 0) {
      if (_pending_request >= _conf_queries_per_transaction) {
        _execute();
        _pending_request = 0;
      }
    } else {  // auto commit => nothing to do
      _pending_request = 0;
    }
  };

  SPDLOG_LOGGER_TRACE(_logger, "BAM: {} pending events", _pending_events);

  // Process service status events.
  switch (data->type()) {
    case neb::service_status::static_type():
    case neb::service::static_type(): {
      std::shared_ptr<neb::service_status> ss(
          std::static_pointer_cast<neb::service_status>(data));
      SPDLOG_LOGGER_TRACE(
          _logger,
          "BAM: processing service status (host: {}, service: {}, hard state "
          "{}, "
          "current state {})",
          ss->host_id, ss->service_id, ss->last_hard_state, ss->current_state);
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(ss, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case neb::pb_service_status::static_type(): {
      auto ss = std::static_pointer_cast<neb::pb_service_status>(data);
      auto& o = ss->obj();
      SPDLOG_LOGGER_TRACE(
          _logger,
          "BAM: processing pb service status (host: {}, service: {}, hard "
          "state {}, current state {})",
          o.host_id(), o.service_id(), o.last_hard_state(), o.state());
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(ss, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case neb::pb_adaptive_service_status::static_type(): {
      auto ss = std::static_pointer_cast<neb::pb_adaptive_service_status>(data);
      auto& o = ss->obj();
      SPDLOG_LOGGER_TRACE(_logger,
                          "BAM: processing pb adaptive service status (host: "
                          "{}, service: {})",
                          o.host_id(), o.service_id());
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(ss, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case neb::pb_service::static_type(): {
      auto s = std::static_pointer_cast<neb::pb_service>(data);
      auto& o = s->obj();
      SPDLOG_LOGGER_TRACE(
          _logger,
          "BAM: processing pb service (host: {}, service: {}, hard "
          "state {}, current state {})",
          o.host_id(), o.service_id(), o.last_hard_state(), o.state());
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(s, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case neb::pb_acknowledgement::static_type(): {
      std::shared_ptr<neb::pb_acknowledgement> ack(
          std::static_pointer_cast<neb::pb_acknowledgement>(data));
      SPDLOG_LOGGER_TRACE(_logger,
                          "BAM: processing acknowledgement on service ({}, {})",
                          ack->obj().host_id(), ack->obj().service_id());
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(ack, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case neb::acknowledgement::static_type(): {
      std::shared_ptr<neb::acknowledgement> ack(
          std::static_pointer_cast<neb::acknowledgement>(data));
      SPDLOG_LOGGER_TRACE(_logger,
                          "BAM: processing acknowledgement on service ({}, {})",
                          ack->host_id, ack->service_id);
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(ack, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case neb::downtime::static_type(): {
      std::shared_ptr<neb::downtime> dt(
          std::static_pointer_cast<neb::downtime>(data));
      SPDLOG_LOGGER_TRACE(
          _logger,
          "BAM: processing downtime ({}) on service ({}, {}) started: {}, "
          "stopped: {}",
          dt->internal_id, dt->host_id, dt->service_id, dt->was_started,
          dt->was_cancelled);
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(dt, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case neb::pb_downtime::static_type(): {
      std::shared_ptr<neb::pb_downtime> dt(
          std::static_pointer_cast<neb::pb_downtime>(data));
      auto& downtime = dt->obj();
      SPDLOG_LOGGER_TRACE(_logger,
                          "BAM: processing downtime (pb) ({}) on service "
                          "({}, {}) started: {}, "
                          "stopped: {}",
                          downtime.id(), downtime.host_id(),
                          downtime.service_id(), downtime.started(),
                          downtime.cancelled());
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(dt, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case bam::ba_status::static_type(): {
      ba_status* status(static_cast<ba_status*>(data.get()));
      if (_ba_query->is_bulk())
        _ba_query->add_bulk_row(bulk_ba_binder{data});
      else
        _ba_query->add_multi_row(ba_binder{data});

      ++_pending_request;
      commit_if_needed();

      if (status->state_changed) {
        std::pair<std::string, std::string> ba_svc_name(
            _ba_mapping.get_service(status->ba_id));
        if (ba_svc_name.first.empty() || ba_svc_name.second.empty()) {
          _logger->error(
              "BAM: could not trigger check of virtual service of BA {}:"
              "host name and service description were not found",
              status->ba_id);
        } else {
          time_t now = time(nullptr);
          std::string cmd(
              fmt::format("[{}] SCHEDULE_FORCED_SVC_CHECK;{};{};{}\n", now,
                          ba_svc_name.first, ba_svc_name.second, now));
          _write_forced_svc_check(ba_svc_name.first, ba_svc_name.second);
        }
      }
    } break;
    case bam::pb_ba_status::static_type(): {
      const BaStatus& status =
          static_cast<const pb_ba_status*>(data.get())->obj();
      if (_ba_query->is_bulk())
        _ba_query->add_bulk_row(bulk_ba_binder{data});
      else
        _ba_query->add_multi_row(ba_binder{data});
      ++_pending_request;
      commit_if_needed();

      if (status.state_changed()) {
        std::pair<std::string, std::string> ba_svc_name(
            _ba_mapping.get_service(status.ba_id()));
        if (ba_svc_name.first.empty() || ba_svc_name.second.empty()) {
          _logger->error(
              "BAM: could not trigger check of virtual service of BA {}:"
              "host name and service description were not found",
              status.ba_id());
        } else {
          time_t now = time(nullptr);
          std::string cmd(
              fmt::format("[{}] SCHEDULE_FORCED_SVC_CHECK;{};{};{}\n", now,
                          ba_svc_name.first, ba_svc_name.second, now));
          _write_forced_svc_check(ba_svc_name.first, ba_svc_name.second);
        }
      }
    } break;
    case bam::kpi_status::static_type():
    case bam::pb_kpi_status::static_type():
      if (_kpi_query->is_bulk())
        _kpi_query->add_bulk_row(bulk_kpi_binder{data});
      else
        _kpi_query->add_multi_row(kpi_binder{data});
      ++_pending_request;
      commit_if_needed();
      break;
    case inherited_downtime::static_type(): {
      std::string cmd;
      timestamp now = timestamp::now();
      inherited_downtime const& dwn =
          *std::static_pointer_cast<inherited_downtime const>(data);
      SPDLOG_LOGGER_TRACE(
          _logger, "BAM: processing inherited downtime (ba id {}, now {}",
          dwn.ba_id, now);
      if (dwn.in_downtime)
        cmd = fmt::format(
            "[{}] "
            "SCHEDULE_SVC_DOWNTIME;_Module_BAM_{};ba_{};{};{};1;0;0;Centreon "
            "Broker BAM Module;Automatic downtime triggered by BA downtime "
            "inheritance\n",
            now, config::applier::state::instance().poller_id(), dwn.ba_id, now,
            4102444799);
      else
        cmd = fmt::format(
            "[{}] DEL_SVC_DOWNTIME_FULL;_Module_BAM_{};ba_{};;;1;0;;Centreon "
            "Broker BAM Module;Automatic downtime triggered by BA downtime "
            "inheritance\n",
            now, config::applier::state::instance().poller_id(), dwn.ba_id);
      _write_external_command(cmd);
    } break;
    case pb_inherited_downtime::static_type(): {
      std::string cmd;
      timestamp now = timestamp::now();
      pb_inherited_downtime const& dwn =
          *std::static_pointer_cast<pb_inherited_downtime const>(data);
      SPDLOG_LOGGER_TRACE(
          _logger, "BAM: processing pb inherited downtime (ba id {}, now {}",
          dwn.obj().ba_id(), now);
      if (dwn.obj().in_downtime())
        cmd = fmt::format(
            "[{}] "
            "SCHEDULE_SVC_DOWNTIME;_Module_BAM_{};ba_{};{};{};1;0;0;Centreon "
            "Broker BAM Module;Automatic downtime triggered by BA downtime "
            "inheritance\n",
            now, config::applier::state::instance().poller_id(),
            dwn.obj().ba_id(), now, 4102444799);
      else
        cmd = fmt::format(
            "[{}] DEL_SVC_DOWNTIME_FULL;_Module_BAM_{};ba_{};;;1;0;;Centreon "
            "Broker BAM Module;Automatic downtime triggered by BA downtime "
            "inheritance\n",
            now, config::applier::state::instance().poller_id(),
            dwn.obj().ba_id());
      _write_external_command(cmd);
    } break;
    case extcmd::pb_ba_info::static_type(): {
      _logger->info("BAM: dump BA");
      extcmd::pb_ba_info const& e =
          *std::static_pointer_cast<const extcmd::pb_ba_info>(data);
      auto& obj = e.obj();
      auto ba = _applier.find_ba(obj.id());
      if (ba)
        ba->dump(obj.output_file());
      else
        _logger->error(
            "extcmd: Unable to get info about BA {} - it doesn't exist",
            obj.id());
    } break;
    default:
      break;
  }

  // if uncommited request, we can't yet acknowledge
  if (_pending_request) {
    if (_pending_events >= 10 * _conf_queries_per_transaction) {
      _logger->trace(
          "BAM: monitoring_stream write: too many pending events =>flush and "
          "acknowledge {} events",
          _pending_events);
      return flush();
    }
    _logger->trace(
        "BAM: monitoring_stream write: 0 events (request pending) {} to "
        "acknowledge",
        _pending_events);
    return 0;
  }
  int retval = _pending_events;
  _pending_events = 0;
  _logger->trace("BAM: monitoring_stream write: {} events", retval);
  return retval;
}

/**
 *  Prepare bulk queries.
 */
void monitoring_stream::_prepare() {
  if (_mysql.support_bulk_statement()) {
    _logger->trace("BAM: monitoring stream _prepare");
    _ba_query = std::make_unique<database::bulk_or_multi>(
        _mysql,
        "UPDATE mod_bam SET "
        "current_level=?,acknowledged=?,downtime=?,last_state_change=?,"
        "in_downtime=?,current_status=?,comment=? WHERE ba_id=?",
        _conf_queries_per_transaction, _logger);
    _kpi_query = std::make_unique<database::bulk_or_multi>(
        _mysql,
        "UPDATE mod_bam_kpi SET acknowledged=?,current_status=?,downtime=?, "
        "last_level=?,state_type=?,last_state_change=?,last_impact=?, "
        "valid=?,in_downtime=? WHERE kpi_id=?",
        _conf_queries_per_transaction, _logger);

  } else {
    _ba_query = std::make_unique<database::bulk_or_multi>(
        "INSERT INTO mod_bam (current_level, acknowledged, downtime, "
        "last_state_change, in_downtime, current_status, comment, ba_id) "
        "VALUES",
        "ON DUPLICATE KEY UPDATE current_level=VALUES(current_level), "
        "acknowledged=VALUES(acknowledged), downtime=VALUES(downtime), "
        "last_state_change=VALUES(last_state_change), "
        "in_downtime=VALUES(in_downtime), "
        "current_status=VALUES(current_status), "
        "comment=VALUES(comment)");
    _kpi_query = std::make_unique<database::bulk_or_multi>(
        "INSERT INTO mod_bam_kpi (acknowledged, current_status, "
        "downtime, last_level, state_type, last_state_change, last_impact, "
        "valid, in_downtime, kpi_id) VALUES",
        "ON DUPLICATE KEY UPDATE acknowledged=VALUES(acknowledged), "
        "current_status=VALUES(current_status), "
        "downtime=VALUES(downtime), last_level=VALUES(last_level), "
        "state_type=VALUES(state_type), "
        "last_state_change=VALUES(last_state_change), "
        "last_impact=VALUES(last_impact), valid=VALUES(valid), "
        "in_downtime=VALUES(in_downtime)");
  }
}

/**
 *  Rebuilds BA durations/availabilities from BA events.
 */
void monitoring_stream::_rebuild() {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring stream _rebuild");
  // Get the list of the BAs that should be rebuild.
  std::vector<uint32_t> bas_to_rebuild;
  {
    std::string query("SELECT ba_id FROM mod_bam WHERE must_be_rebuild='1'");
    std::promise<mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    _mysql.run_query_and_get_result(query, std::move(promise), 0);
    try {
      mysql_result res(future.get());
      while (_mysql.fetch_row(res))
        bas_to_rebuild.push_back(res.value_as_u32(0));
    } catch (std::exception const& e) {
      throw msg_fmt("BAM: could not select the list of BAs to rebuild: {}",
                    e.what());
    }
  }

  // Nothing to rebuild.
  if (bas_to_rebuild.empty())
    return;

  SPDLOG_LOGGER_TRACE(_logger,
                      "BAM: rebuild asked, sending the rebuild signal");

  auto r{std::make_shared<rebuild>(
      fmt::format("{}", fmt::join(bas_to_rebuild, ", ")))};
  auto out{std::make_unique<multiplexing::publisher>()};
  out->write(r);

  // Set all the BAs to should not be rebuild.
  {
    std::string query("UPDATE mod_bam SET must_be_rebuild='0'");
    _mysql.run_query(query, database::mysql_error::rebuild_ba);
  }
}

/**
 * @brief This internal function is called by the _forced_svc_checks_timer. It
 * empties the set _forced_svc_checks, fills the set _timer_forced_svc_checks
 * and sends all its contents to centengine, using a misc::fifo_client object
 * to avoid getting stuck. If at a moment, it fails to send queries, the
 * function is rescheduled in 5s.
 *
 * @param ec boost::system::error_code to handle errors due to asio, not the
 * files sent to centengine. Usually, "operation aborted" when cbd stops.
 */
void monitoring_stream::_explicitly_send_forced_svc_checks(
    const boost::system::error_code& ec) {
  static int count = 0;
  SPDLOG_LOGGER_DEBUG(_logger, "BAM: time to send forced service checks {}",
                      count++);
  if (!ec) {
    if (_timer_forced_svc_checks.empty()) {
      std::lock_guard<std::mutex> lck(_forced_svc_checks_m);
      _timer_forced_svc_checks.swap(_forced_svc_checks);
    }
    if (!_timer_forced_svc_checks.empty()) {
      std::lock_guard<std::mutex> lock(_ext_cmd_file_m);
      _logger->trace("opening {}", _ext_cmd_file);
      misc::fifo_client fc(_ext_cmd_file);
      SPDLOG_LOGGER_DEBUG(_logger, "BAM: {} forced checks to schedule",
                          _timer_forced_svc_checks.size());
      for (auto& p : _timer_forced_svc_checks) {
        time_t now = time(nullptr);
        std::string cmd{fmt::format("[{}] SCHEDULE_FORCED_SVC_CHECK;{};{};{}\n",
                                    now, p.first, p.second, now)};
        _logger->debug("writing '{}' into {}", cmd, _ext_cmd_file);
        if (fc.write(cmd) < 0) {
          _logger->error(
              "BAM: could not write forced service check to command file '{}'",
              _ext_cmd_file);
          _forced_svc_checks_timer.expires_after(std::chrono::seconds(5));
          _forced_svc_checks_timer.async_wait(
              std::bind(&monitoring_stream::_explicitly_send_forced_svc_checks,
                        this, std::placeholders::_1));
          break;
        }
      }
    }
  }
}

/**
 *  Write an external command to Engine.
 *
 *  @param[in] cmd  Command to write to the external command pipe.
 */
void monitoring_stream::_write_forced_svc_check(
    const std::string& host,
    const std::string& description) {
  SPDLOG_LOGGER_TRACE(_logger,
                      "BAM: monitoring stream _write_forced_svc_check");
  std::lock_guard<std::mutex> lck(_forced_svc_checks_m);
  _forced_svc_checks.emplace(host, description);
  _forced_svc_checks_timer.expires_after(std::chrono::seconds(5));
  _forced_svc_checks_timer.async_wait(
      std::bind(&monitoring_stream::_explicitly_send_forced_svc_checks, this,
                std::placeholders::_1));
}

/**
 *  Write an external command to Engine.
 *
 *  @param[in] cmd  Command to write to the external command pipe.
 */
void monitoring_stream::_write_external_command(const std::string& cmd) {
  SPDLOG_LOGGER_TRACE(
      _logger, "BAM: monitoring stream _write_external_command <<{}>>", cmd);
  std::lock_guard<std::mutex> lock(_ext_cmd_file_m);
  misc::fifo_client fc(_ext_cmd_file);
  if (fc.write(cmd) < 0) {
    _logger->error("BAM: could not write BA check result to command file '{}'",
                   _ext_cmd_file);
  } else
    SPDLOG_LOGGER_DEBUG(_logger, "BAM: sent external command '{}'", cmd);
}

/**
 *  Get inherited downtime from the cache.
 */
void monitoring_stream::_read_cache() {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring stream _read_cache");
  if (_cache == nullptr)
    SPDLOG_LOGGER_DEBUG(_logger, "BAM: no cache configured");
  else {
    SPDLOG_LOGGER_DEBUG(_logger, "BAM: loading cache");
    _applier.load_from_cache(*_cache);
  }
}

/**
 *  Save inherited downtime to the cache.
 */
void monitoring_stream::_write_cache() {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: monitoring stream _write_cache");
  if (_cache == nullptr)
    SPDLOG_LOGGER_DEBUG(_logger, "BAM: no cache configured");
  else {
    SPDLOG_LOGGER_DEBUG(_logger, "BAM: saving cache");
    _applier.save_to_cache(*_cache);
  }
}

/**
 * @brief no commit as we work in autocommit but executes requests (bulk or
 * multi insert)
 *
 */
void monitoring_stream::_execute() {
  _ba_query->execute(_mysql);
  _kpi_query->execute(_mysql);
}
