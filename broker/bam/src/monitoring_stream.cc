/*
** Copyright 2014-2017 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <spdlog/fmt/ostr.h>

#include "com/centreon/broker/bam/monitoring_stream.hh"

#include "bbdo/bam/ba_status.hh"
#include "bbdo/bam/kpi_status.hh"
#include "bbdo/bam/rebuild.hh"
#include "bbdo/events.hh"
#include "com/centreon/broker/bam/configuration/reader_v2.hh"
#include "com/centreon/broker/bam/configuration/state.hh"
#include "com/centreon/broker/bam/event_cache_visitor.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/fifo_client.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/broker/neb/service_status.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/timestamp.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;
using namespace com::centreon::broker::database;

/**
 *  Constructor.
 *
 *  @param[in] ext_cmd_file    The command file to write into.
 *  @param[in] db_cfg          Main (centreon) database configuration.
 *  @param[in] storage_db_cfg  Storage (centreon_storage) database
 *                             configuration.
 *  @param[in] cache           The persistent cache.
 */
monitoring_stream::monitoring_stream(const std::string& ext_cmd_file,
                                     const database_config& db_cfg,
                                     const database_config& storage_db_cfg,
                                     std::shared_ptr<persistent_cache> cache)
    : io::stream("BAM"),
      _ext_cmd_file(ext_cmd_file),
      _mysql(db_cfg),
      _pending_events(0),
      _pending_request(0),
      _storage_db_cfg(storage_db_cfg),
      _cache(std::move(cache)),
      _forced_svc_checks_timer{pool::io_context()} {
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring_stream constructor");
  // Prepare queries.
  _prepare();

  // Let's update BAs then we will be able to load the cache with inherited
  // downtimes.
  update();
  // Read cache.
  _read_cache();
}

/**
 *  Destructor.
 */
monitoring_stream::~monitoring_stream() {
  // save cache
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring_stream destructor");
  try {
    _write_cache();
  } catch (std::exception const& e) {
    log_v2::bam()->error("BAM: can't save cache: '{}'", e.what());
  }
  SPDLOG_LOGGER_DEBUG(log_v2::bam(), "BAM: monitoring_stream destruction done");
}

/**
 *  Flush data.
 *
 *  @return Number of acknowledged events.
 */
int32_t monitoring_stream::flush() {
  _mysql.commit();
  _pending_request = 0;
  int retval = _pending_events;
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring_stream flush: {} events",
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
  log_v2::core()->info("monitoring stream: stopped with {} events acknowledged",
                       retval);
  /* I want to be sure the timer is really stopped. */
  std::promise<void> p;
  {
    log_v2::bam()->info(
        "bam: monitoring_stream - waiting for forced service checks to be "
        "done");
    std::lock_guard<std::mutex> lck(_forced_svc_checks_m);
    _forced_svc_checks_timer.expires_after(std::chrono::seconds(0));
    _forced_svc_checks_timer.async_wait([this, &p](const asio::error_code& ec) {
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
  log_v2::bam()->info("bam: monitoring_stream - stop finished");

  return retval;
}

/**
 *  Generate default state.
 */
void monitoring_stream::initialize() {
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring_stream initialize");
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
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring_stream update");
  try {
    configuration::state s;
    configuration::reader_v2 r(_mysql, _storage_db_cfg);
    r.read(s);
    _applier.apply(s);
    _ba_mapping = s.get_ba_svc_mapping();
    _rebuild();
    initialize();
  } catch (std::exception const& e) {
    throw msg_fmt("BAM: could not process configuration update: {}", e.what());
  }
}

/**
 *  Write an event.
 *
 *  @param[in] data Event pointer.
 *
 *  @return Number of events acknowledged.
 */
int monitoring_stream::write(std::shared_ptr<io::data> const& data) {
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring_stream write {}", *data);
  // Take this event into account.
  ++_pending_events;

  // this lambda ask mysql to do a commit if we have get_queries_per_transaction
  // waiting requests
  auto commit_if_needed = [this]() {
    if (_mysql.get_config().get_queries_per_transaction() > 0) {
      if (_pending_request >=
          _mysql.get_config().get_queries_per_transaction()) {
        _mysql.commit();
        _pending_request = 0;
      }
    } else {  // auto commit => nothing to do
      _pending_request = 0;
    }
  };

  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: {} pending events", _pending_events);

  // Process service status events.
  switch (data->type()) {
    case neb::service_status::static_type():
    case neb::service::static_type(): {
      std::shared_ptr<neb::service_status> ss(
          std::static_pointer_cast<neb::service_status>(data));
      SPDLOG_LOGGER_TRACE(
          log_v2::bam(),
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
          log_v2::bam(),
          "BAM: processing pb service status (host: {}, service: {}, hard "
          "state {}, current state {})",
          o.host_id(), o.service_id(), o.last_hard_state(), o.state());
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(ss, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case neb::pb_service::static_type(): {
      auto s = std::static_pointer_cast<neb::pb_service>(data);
      auto& o = s->obj();
      SPDLOG_LOGGER_TRACE(
          log_v2::bam(),
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
      SPDLOG_LOGGER_TRACE(log_v2::bam(),
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
      SPDLOG_LOGGER_TRACE(log_v2::bam(),
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
          log_v2::bam(),
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
      SPDLOG_LOGGER_TRACE(
          log_v2::bam(),
          "BAM: processing downtime (pb) ({}) on service ({}, {}) started: {}, "
          "stopped: {}",
          downtime.id(), downtime.host_id(), downtime.service_id(),
          downtime.started(), downtime.cancelled());
      multiplexing::publisher pblshr;
      event_cache_visitor ev_cache;
      _applier.book_service().update(dt, &ev_cache);
      ev_cache.commit_to(pblshr);
    } break;
    case bam::ba_status::static_type(): {
      ba_status* status(static_cast<ba_status*>(data.get()));
      SPDLOG_LOGGER_TRACE(
          log_v2::bam(),
          "BAM: processing BA status (id {}, nominal {}, acknowledgement {}, "
          "downtime {}) - in downtime {}, state {}",
          status->ba_id, status->level_nominal, status->level_acknowledgement,
          status->level_downtime, status->in_downtime, status->state);
      _ba_update.bind_value_as_f64(0, status->level_nominal);
      _ba_update.bind_value_as_f64(1, status->level_acknowledgement);
      _ba_update.bind_value_as_f64(2, status->level_downtime);
      _ba_update.bind_value_as_u32(6, status->ba_id);
      if (status->last_state_change.is_null())
        _ba_update.bind_null_u64(3);
      else
        _ba_update.bind_value_as_u64(3, status->last_state_change.get_time_t());
      _ba_update.bind_value_as_bool(4, status->in_downtime);
      _ba_update.bind_value_as_i32(5, status->state);

      _mysql.run_statement(_ba_update, database::mysql_error::update_ba);
      ++_pending_request;
      commit_if_needed();

      if (status->state_changed) {
        std::pair<std::string, std::string> ba_svc_name(
            _ba_mapping.get_service(status->ba_id));
        if (ba_svc_name.first.empty() || ba_svc_name.second.empty()) {
          log_v2::bam()->error(
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
      SPDLOG_LOGGER_TRACE(log_v2::bam(),
                          "BAM: processing pb BA status (id {}, nominal {}, "
                          "acknowledgement {}, "
                          "downtime {}) - in downtime {}, state {}",
                          status.ba_id(), status.level_nominal(),
                          status.level_acknowledgement(),
                          status.level_downtime(), status.in_downtime(),
                          status.state());
      _ba_update.bind_value_as_f64(0, status.level_nominal());
      _ba_update.bind_value_as_f64(1, status.level_acknowledgement());
      _ba_update.bind_value_as_f64(2, status.level_downtime());
      _ba_update.bind_value_as_u32(6, status.ba_id());
      if (status.last_state_change() == 0)
        _ba_update.bind_null_u64(3);
      else
        _ba_update.bind_value_as_u64(3, status.last_state_change());
      _ba_update.bind_value_as_bool(4, status.in_downtime());
      _ba_update.bind_value_as_i32(5, status.state());

      _mysql.run_statement(_ba_update, database::mysql_error::update_ba);
      ++_pending_request;
      commit_if_needed();

      if (status.state_changed()) {
        std::pair<std::string, std::string> ba_svc_name(
            _ba_mapping.get_service(status.ba_id()));
        if (ba_svc_name.first.empty() || ba_svc_name.second.empty()) {
          log_v2::bam()->error(
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
    case bam::kpi_status::static_type(): {
      kpi_status* status(static_cast<kpi_status*>(data.get()));
      SPDLOG_LOGGER_DEBUG(
          log_v2::bam(),
          "BAM: processing KPI status (id {}, level {}, acknowledgement {}, "
          "downtime {})",
          status->kpi_id, status->level_nominal_hard,
          status->level_acknowledgement_hard, status->level_downtime_hard);

      _kpi_update.bind_value_as_f64(0, status->level_acknowledgement_hard);
      _kpi_update.bind_value_as_i32(1, status->state_hard);
      _kpi_update.bind_value_as_f64(2, status->level_downtime_hard);
      _kpi_update.bind_value_as_f64(3, status->level_nominal_hard);
      _kpi_update.bind_value_as_i32(4, 1 + 1);
      if (status->last_state_change.is_null())
        _kpi_update.bind_null_u64(5);
      else
        _kpi_update.bind_value_as_u64(5,
                                      status->last_state_change.get_time_t());
      _kpi_update.bind_value_as_f64(6, status->last_impact);
      _kpi_update.bind_value_as_bool(7, status->valid);
      _kpi_update.bind_value_as_bool(8, status->in_downtime);
      _kpi_update.bind_value_as_u32(9, status->kpi_id);

      _mysql.run_statement(_kpi_update, database::mysql_error::update_kpi);
      ++_pending_request;
      commit_if_needed();

    } break;
    case bam::pb_kpi_status::static_type(): {
      const KpiStatus& status(
          std::static_pointer_cast<pb_kpi_status>(data)->obj());
      SPDLOG_LOGGER_DEBUG(
          log_v2::bam(),
          "BAM: processing KPI status (id {}, level {}, acknowledgement {}, "
          "downtime {})",
          status.kpi_id(), status.level_nominal_hard(),
          status.level_acknowledgement_hard(), status.level_downtime_hard());

      _kpi_update.bind_value_as_f64(0, status.level_acknowledgement_hard());
      _kpi_update.bind_value_as_i32(1, status.state_hard());
      _kpi_update.bind_value_as_f64(2, status.level_downtime_hard());
      _kpi_update.bind_value_as_f64(3, status.level_nominal_hard());
      _kpi_update.bind_value_as_i32(4, 1 + 1);
      if (status.last_state_change() <= 0)
        _kpi_update.bind_null_u64(5);
      else
        _kpi_update.bind_value_as_u64(5, status.last_state_change());
      _kpi_update.bind_value_as_f64(6, status.last_impact());
      _kpi_update.bind_value_as_bool(7, status.valid());
      _kpi_update.bind_value_as_bool(8, status.in_downtime());
      _kpi_update.bind_value_as_u32(9, status.kpi_id());

      _mysql.run_statement(_kpi_update, database::mysql_error::update_kpi);
      ++_pending_request;
      commit_if_needed();

    } break;
    case inherited_downtime::static_type(): {
      std::string cmd;
      timestamp now = timestamp::now();
      inherited_downtime const& dwn =
          *std::static_pointer_cast<inherited_downtime const>(data);
      SPDLOG_LOGGER_TRACE(
          log_v2::bam(), "BAM: processing inherited downtime (ba id {}, now {}",
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
          log_v2::bam(),
          "BAM: processing pb inherited downtime (ba id {}, now {}",
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
    default:
      break;
  }

  // if uncommited request, we can't yet acknowledge
  if (_pending_request) {
    if (_pending_events >=
        10 * _mysql.get_config().get_queries_per_transaction()) {
      log_v2::bam()->trace(
          "BAM: monitoring_stream write: too many pending events =>flush and "
          "acknowledge {} events",
          _pending_events);
      return flush();
    }
    log_v2::bam()->trace(
        "BAM: monitoring_stream write: 0 events (request pending) {} to "
        "acknowledge",
        _pending_events);
    return 0;
  }
  int retval = _pending_events;
  _pending_events = 0;
  log_v2::bam()->trace("BAM: monitoring_stream write: {} events", retval);
  return retval;
}

/**
 *  Prepare queries.
 */
void monitoring_stream::_prepare() {
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring stream _prepare");
  // BA status.
  {
    std::string query(
        "UPDATE mod_bam SET current_level=?,acknowledged=?,downtime=?,"
        "last_state_change=?,in_downtime=?,current_status=? WHERE ba_id=?");
    _ba_update = _mysql.prepare_query(query);
  }

  // KPI status.
  {
    std::string query(
        "UPDATE mod_bam_kpi SET acknowledged=?,current_status=?,"
        "downtime=?, last_level=?,state_type=?,last_state_change=?,"
        "last_impact=?, valid=?,in_downtime=? WHERE kpi_id=?");
    _kpi_update = _mysql.prepare_query(query);
  }
}

/**
 *  Rebuilds BA durations/availibities from BA events.
 */
void monitoring_stream::_rebuild() {
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring stream _rebuild");
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

  SPDLOG_LOGGER_TRACE(log_v2::bam(),
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
 * @param ec asio::error_code to handle errors due to asio, not the files sent
 * to centengine. Usually, "operation aborted" when cbd stops.
 */
void monitoring_stream::_explicitly_send_forced_svc_checks(
    const asio::error_code& ec) {
  if (!ec) {
    if (_timer_forced_svc_checks.empty()) {
      std::lock_guard<std::mutex> lck(_forced_svc_checks_m);
      _timer_forced_svc_checks.swap(_forced_svc_checks);
    }
    if (!_timer_forced_svc_checks.empty()) {
      std::lock_guard<std::mutex> lock(_ext_cmd_file_m);
      log_v2::bam()->trace("opening {}", _ext_cmd_file);
      misc::fifo_client fc(_ext_cmd_file);
      SPDLOG_LOGGER_DEBUG(log_v2::bam(), "BAM: {} forced checks to schedule",
                          _timer_forced_svc_checks.size());
      for (auto& p : _timer_forced_svc_checks) {
        time_t now = time(nullptr);
        std::string cmd{fmt::format("[{}] SCHEDULE_FORCED_SVC_CHECK;{};{};{}\n",
                                    now, p.first, p.second, now)};
        log_v2::bam()->debug("writing '{}' into {}", cmd, _ext_cmd_file);
        if (fc.write(cmd) < 0) {
          log_v2::bam()->error(
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
  SPDLOG_LOGGER_TRACE(log_v2::bam(),
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
  SPDLOG_LOGGER_TRACE(log_v2::bam(),
                      "BAM: monitoring stream _write_external_command <<{}>>",
                      cmd);
  std::lock_guard<std::mutex> lock(_ext_cmd_file_m);
  misc::fifo_client fc(_ext_cmd_file);
  if (fc.write(cmd) < 0) {
    log_v2::bam()->error(
        "BAM: could not write BA check result to command file '{}'",
        _ext_cmd_file);
  } else
    SPDLOG_LOGGER_DEBUG(log_v2::bam(), "BAM: sent external command '{}'", cmd);
}

/**
 *  Get inherited downtime from the cache.
 */
void monitoring_stream::_read_cache() {
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring stream _read_cache");
  if (_cache == nullptr)
    SPDLOG_LOGGER_DEBUG(log_v2::bam(), "BAM: no cache configured");
  else {
    SPDLOG_LOGGER_DEBUG(log_v2::bam(), "BAM: loading cache");
    _applier.load_from_cache(*_cache);
  }
}

/**
 *  Save inherited downtime to the cache.
 */
void monitoring_stream::_write_cache() {
  SPDLOG_LOGGER_TRACE(log_v2::bam(), "BAM: monitoring stream _write_cache");
  if (_cache == nullptr)
    SPDLOG_LOGGER_DEBUG(log_v2::bam(), "BAM: no cache configured");
  else {
    SPDLOG_LOGGER_DEBUG(log_v2::bam(), "BAM: saving cache");
    _applier.save_to_cache(*_cache);
  }
}
