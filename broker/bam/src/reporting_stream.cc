/**
 * Copyright 2014-2015,2017, 2021 Centreon
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

#include "com/centreon/broker/bam/reporting_stream.hh"

#include "bbdo/bam/ba_duration_event.hh"
#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/bam/dimension_ba_event.hh"
#include "bbdo/bam/dimension_ba_timeperiod_relation.hh"
#include "bbdo/bam/dimension_bv_event.hh"
#include "bbdo/bam/dimension_kpi_event.hh"
#include "bbdo/bam/dimension_timeperiod.hh"
#include "bbdo/bam/dimension_truncate_table_signal.hh"
#include "bbdo/bam/kpi_event.hh"
#include "bbdo/bam/rebuild.hh"
#include "bbdo/events.hh"
#include "com/centreon/broker/bam/ba.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/sql/table_max_size.hh"
#include "com/centreon/broker/time/timezone_manager.hh"
#include "com/centreon/common/utf8.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::bam;
using namespace com::centreon::broker::database;

using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] db_cfg                  BAM DB configuration.
 */
reporting_stream::reporting_stream(
    database_config const& db_cfg,
    const std::shared_ptr<spdlog::logger>& logger)
    : io::stream("BAM-BI"),
      _ack_events(0),
      _pending_events(0),
      _mysql(db_cfg, logger),
      _ba_full_event_insert(logger),
      _ba_event_update(logger),
      _ba_duration_event_insert(logger),
      _ba_duration_event_update(logger),
      _kpi_full_event_insert(logger),
      _kpi_event_link(logger),
      _kpi_event_link_update(logger),
      _dimension_ba_insert(logger),
      _dimension_bv_insert(logger),
      _dimension_ba_bv_relation_insert(logger),
      _dimension_timeperiod_insert(logger),
      _dimension_ba_timeperiod_insert(logger),
      _processing_dimensions(false),
      _logger{logger} {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: reporting stream constructor");
  // Prepare queries.
  _prepare();

  // Load timeperiods.
  _load_timeperiods();

  _load_kpi_ba_events();

  // Close inconsistent events.
  _close_inconsistent_events("BA", "mod_bam_reporting_ba_events", "ba_id");
  _close_inconsistent_events("KPI", "mod_bam_reporting_kpi_events", "kpi_id");

  // Close remaining events.
  _close_all_events();

  // Initialize the availabilities thread.
  _availabilities =
      std::make_unique<availability_thread>(db_cfg, _timeperiods, _logger);
  _availabilities->start_and_wait();
}

/**
 *  Destructor.
 */
reporting_stream::~reporting_stream() {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: reporting stream destructor");
  // Terminate the availabilities thread.
  _availabilities->terminate();
  _availabilities->wait();
}

/**
 *  Read from the database.
 *  Get the next available bam event.
 *
 *  @param[out] d         Cleared.
 *  @param[in]  deadline  Timeout.
 *
 *  @return This method will throw.
 */
bool reporting_stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  (void)deadline;
  d.reset();
  throw exceptions::shutdown("cannot read from BAM reporting stream");
  return true;
}
/**
 *  Get endpoint statistics.
 *
 *  @param[out] tree Output tree.
 */
void reporting_stream::statistics(nlohmann::json& tree) const {
  std::lock_guard<std::mutex> lock(_statusm);
  if (!_status.empty())
    tree["status"] = _status;
}

/**
 *  Flush the stream.
 *
 *  @return Number of acknowledged events.
 */
int32_t reporting_stream::flush() {
  SPDLOG_LOGGER_TRACE(_logger, "BAM: reporting stream flush");
  _commit();
  int retval(_ack_events + _pending_events);
  _ack_events = 0;
  _pending_events = 0;
  return retval;
}

/**
 * @brief Flush the stream and stop it.
 *
 * @return Number of acknowledged events.
 */
int32_t reporting_stream::stop() {
  int32_t retval = flush();
  _logger->info("reporting stream stopped with {} events acknowledged", retval);
  return retval;
}

/**
 *  Write an event.
 *
 *  @param[in] data Event pointer.
 *
 *  @return Number of events acknowledged.
 */
int reporting_stream::write(std::shared_ptr<io::data> const& data) {
  // Take this event into account.
  ++_pending_events;
  assert(data);

  if (_logger->level() == spdlog::level::trace) {
    SPDLOG_LOGGER_TRACE(
        _logger, "BAM: reporting stream write - event of type {}", *data);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger,
                        "BAM: reporting stream write - event of type {:x}",
                        data->type());
  }

  auto commit_if_needed = [this]() {
    if (_pending_events >= _mysql.get_config().get_queries_per_transaction()) {
      _commit();
    }
  };

  switch (data->type()) {
    case io::events::data_type<io::bam, bam::de_kpi_event>::value:
      _process_kpi_event(data);
      commit_if_needed();
      break;
    case bam::pb_kpi_event::static_type():
      _process_pb_kpi_event(data);
      commit_if_needed();
      break;
    case io::events::data_type<io::bam, bam::de_ba_event>::value:
      _process_ba_event(data);
      commit_if_needed();
      break;
    case bam::pb_ba_event::static_type():
      _process_pb_ba_event(data);
      commit_if_needed();
      break;
    case io::events::data_type<io::bam, bam::de_ba_duration_event>::value:
      _process_ba_duration_event(data);
      commit_if_needed();
      break;
    case bam::pb_ba_duration_event::static_type():
      _process_pb_ba_duration_event(data);
      commit_if_needed();
      break;
    case io::events::data_type<io::bam,
                               bam::de_dimension_truncate_table_signal>::value:
      _process_dimension_truncate_signal(data);
      commit_if_needed();
      break;
    case bam::pb_dimension_truncate_table_signal::static_type():
      _process_pb_dimension_truncate_signal(data);
      commit_if_needed();
      break;
    case io::events::data_type<io::bam, bam::de_dimension_ba_event>::value:
    case io::events::data_type<io::bam, bam::de_dimension_bv_event>::value:
    case io::events::data_type<io::bam,
                               bam::de_dimension_ba_bv_relation_event>::value:
    case io::events::data_type<io::bam, bam::de_dimension_kpi_event>::value:
    case io::events::data_type<io::bam, bam::de_dimension_timeperiod>::value:
    case io::events::data_type<io::bam,
                               bam::de_dimension_timeperiod_exception>::value:
    case io::events::data_type<io::bam,
                               bam::de_dimension_timeperiod_exclusion>::value:
    case io::events::data_type<io::bam,
                               bam::de_dimension_ba_timeperiod_relation>::value:
      _process_dimension(data);
      commit_if_needed();
      break;
    case bam::pb_dimension_bv_event::static_type():
    case pb_dimension_ba_bv_relation_event::static_type():
    case bam::pb_dimension_timeperiod::static_type():
    case bam::pb_dimension_ba_event::static_type():
    case bam::pb_dimension_kpi_event::static_type():
    case bam::pb_dimension_ba_timeperiod_relation::static_type():
      _process_pb_dimension(data);
      commit_if_needed();
      break;
    case io::events::data_type<io::bam, bam::de_rebuild>::value:
      _process_rebuild(data);
      commit_if_needed();
      break;
    default:
      SPDLOG_LOGGER_TRACE(_logger, "BAM: nothing to do with event of type {:x}",
                          data->type());
      if (_pending_events ==
          1) {  // no request in transaction => acknowledge right now
        ++_ack_events;
        _pending_events = 0;
      }
      break;
  }

  // Event acknowledgement.
  int retval = _ack_events;
  _ack_events = 0;
  return retval;
}

/**
 *  Apply a timeperiod declaration.
 *
 *  @param[in] tp  Timeperiod declaration.
 */
void reporting_stream::_apply(const DimensionTimeperiod& tp) {
  SPDLOG_LOGGER_TRACE(_logger, "BAM-BI: applying timeperiod {} to cache",
                      tp.id());
  _timeperiods.add_timeperiod(
      tp.id(),
      time::timeperiod::ptr(std::make_shared<time::timeperiod>(
          tp.id(), tp.name(), "", tp.sunday(), tp.monday(), tp.tuesday(),
          tp.wednesday(), tp.thursday(), tp.friday(), tp.saturday())));
}

/**
 *  Delete inconsistent events.
 *
 *  @param[in] event_type  Event type (BA or KPI).
 *  @param[in] table       Table name.
 *  @param[in] id          Table ID name.
 */
void reporting_stream::_close_inconsistent_events(char const* event_type,
                                                  char const* table,
                                                  char const* id) {
  SPDLOG_LOGGER_TRACE(
      _logger,
      "BAM-BI: reporting stream _close_inconsistent events (type {}, table: "
      "{}, id: {})",
      event_type, table, id);
  // Get events to close.
  std::list<std::pair<uint32_t, time_t>> events;
  {
    std::string query(
        fmt::format("SELECT e1.{0}, e1.start_time FROM {1} AS e1 INNER JOIN "
                    "(SELECT {0}, MAX(start_time) AS max_start_time FROM {1} "
                    "GROUP BY {0}) AS e2 ON e1.{0}=e2.{0} WHERE e1.end_time IS "
                    "NULL AND e1.start_time!=e2.max_start_time",
                    id, table));
    std::promise<mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
    _mysql.run_query_and_get_result(query, std::move(promise));
    try {
      mysql_result res(future.get());
      while (_mysql.fetch_row(res))
        events.emplace_back(std::make_pair(
            res.value_as_u32(0), static_cast<time_t>(res.value_as_i32(1))));
    } catch (std::exception const& e) {
      throw msg_fmt("BAM-BI: could not get inconsistent events: {}", e.what());
    }
  }

  // Close each event.
  for (auto& p : events) {
    time_t end_time;
    {
      std::string query_str(
          fmt::format("SELECT start_time FROM {} WHERE {}={} AND start_time>{} "
                      "ORDER BY start_time ASC LIMIT 1",
                      table, id, p.first, p.second));
      SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query_str);
      std::promise<mysql_result> promise;
      std::future<database::mysql_result> future = promise.get_future();
      _mysql.run_query_and_get_result(query_str, std::move(promise));
      try {
        mysql_result res(future.get());
        if (!_mysql.fetch_row(res))
          throw msg_fmt("no event following this one");

        end_time = res.value_as_i32(0);
      } catch (std::exception const& e) {
        throw msg_fmt(
            "BAM-BI: could not get end time of inconsistent event of {} {} "
            "starting"
            " at {} : {}",
            event_type, p.first, p.second, e.what());
      }
    }
    {
      std::string query(
          fmt::format("UPDATE {} SET end_time={} WHERE {}={} AND start_time={}",
                      table, end_time, id, p.first, p.second));
      SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
      _mysql.run_query(query, database::mysql_error::close_event);
    }
  }
}

void reporting_stream::_close_all_events() {
  SPDLOG_LOGGER_TRACE(_logger, "reporting stream _close_all_events");
  time_t now(::time(nullptr));
  std::string query(
      fmt::format("UPDATE mod_bam_reporting_ba_events SET end_time={} WHERE "
                  "end_time IS NULL",
                  now));
  SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
  _mysql.run_query(query, database::mysql_error::close_ba_events);

  query = fmt::format(
      "UPDATE mod_bam_reporting_kpi_events SET end_time={} WHERE end_time IS "
      "NULL",
      now);
  SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
  _mysql.run_query(query, database::mysql_error::close_kpi_events);
}

/**
 *  Load timeperiods from DB.
 */
void reporting_stream::_load_timeperiods() {
  SPDLOG_LOGGER_TRACE(_logger, "reporting stream _load_timeperiods");
  // Clear old timeperiods.
  _timeperiods.clear();

  // Load timeperiods.
  {
    std::string query(
        "SELECT timeperiod_id, name, sunday, monday, tuesday, wednesday, "
        "thursday, friday, saturday FROM mod_bam_reporting_timeperiods");
    std::promise<mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
    _mysql.run_query_and_get_result(query, std::move(promise));
    try {
      mysql_result res(future.get());
      while (_mysql.fetch_row(res)) {
        _timeperiods.add_timeperiod(
            res.value_as_u32(0),
            time::timeperiod::ptr(new time::timeperiod(
                res.value_as_u32(0), res.value_as_str(1), "",
                res.value_as_str(2), res.value_as_str(3), res.value_as_str(4),
                res.value_as_str(5), res.value_as_str(6), res.value_as_str(7),
                res.value_as_str(8))));
      }
    } catch (std::exception const& e) {
      throw msg_fmt("BAM-BI: could not load timeperiods from DB: {}", e.what());
    }
  }

  // Load exceptions.
  {
    std::string query(
        "SELECT timeperiod_id, daterange, timerange FROM "
        "mod_bam_reporting_timeperiods_exceptions");
    std::promise<mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
    _mysql.run_query_and_get_result(query, std::move(promise));
    try {
      mysql_result res(future.get());
      while (_mysql.fetch_row(res)) {
        time::timeperiod::ptr tp =
            _timeperiods.get_timeperiod(res.value_as_u32(0));
        if (!tp)
          SPDLOG_LOGGER_ERROR(
              _logger,
              "BAM-BI: could not apply exception to non-existing timeperiod {}",
              res.value_as_u32(0));
        else
          tp->add_exception(res.value_as_str(1), res.value_as_str(2));
      }
    } catch (std::exception const& e) {
      throw msg_fmt("BAM-BI: could not load timeperiods exceptions from DB: {}",
                    e.what());
    }
  }

  // Load exclusions.
  {
    std::string query(
        "SELECT timeperiod_id, excluded_timeperiod_id"
        "  FROM mod_bam_reporting_timeperiods_exclusions");
    std::promise<mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
    _mysql.run_query_and_get_result(query, std::move(promise));
    try {
      mysql_result res(future.get());
      while (_mysql.fetch_row(res)) {
        time::timeperiod::ptr tp =
            _timeperiods.get_timeperiod(res.value_as_u32(0));
        time::timeperiod::ptr excluded_tp =
            _timeperiods.get_timeperiod(res.value_as_u32(1));
        if (!tp || !excluded_tp)
          SPDLOG_LOGGER_ERROR(
              _logger,
              "BAM-BI: could not apply exclusion of timeperiod {} by "
              "timeperiod {}: at least one timeperiod does not exist",
              res.value_as_u32(1), res.value_as_u32(0));
        else
          tp->add_excluded(excluded_tp);
      }
    } catch (std::exception const& e) {
      throw msg_fmt("BAM-BI: could not load exclusions from DB: {} ", e.what());
    }
  }

  // Load BA/timeperiods relations.
  {
    std::string query(
        "SELECT ba_id, timeperiod_id, is_default"
        "  FROM mod_bam_reporting_relations_ba_timeperiods");
    std::promise<mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
    _mysql.run_query_and_get_result(query, std::move(promise));
    try {
      mysql_result res(future.get());
      while (_mysql.fetch_row(res))
        _timeperiods.add_relation(res.value_as_u32(0), res.value_as_u32(1),
                                  res.value_as_bool(2));
    } catch (std::exception const& e) {
      throw msg_fmt("BAM-BI: could not load BA/timeperiods relations: {}",
                    e.what());
    }
  }
}

/**
 * @brief feed caches with mod_bam_reporting_ba_events and
 * mod_bam_reporting_kpi_events tables
 *
 */
void reporting_stream::_load_kpi_ba_events() {
  SPDLOG_LOGGER_TRACE(_logger, "reporting stream _load_kpi_ba_events");
  _ba_event_cache.clear();
  _kpi_event_cache.clear();

  // Load ba events.
  std::string query(
      "SELECT ba_event_id, ba_id, start_time FROM mod_bam_reporting_ba_events");
  std::promise<mysql_result> promise;
  std::future<database::mysql_result> future = promise.get_future();
  SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
  _mysql.run_query_and_get_result(query, std::move(promise));
  try {
    mysql_result res(future.get());
    while (_mysql.fetch_row(res)) {
      _ba_event_cache.emplace(
          std::make_pair(res.value_as_u32(1), res.value_as_u64(2)),
          res.value_as_u32(0));
    }
  } catch (std::exception const& e) {
    throw msg_fmt("BAM-BI: could not load ba events from DB: {}", e.what());
  }

  // load kpi events
  query =
      "SELECT kpi_event_id, kpi_id, start_time FROM "
      "mod_bam_reporting_kpi_events";
  std::promise<mysql_result> kpi_promise;
  std::future<database::mysql_result> kpi_future = kpi_promise.get_future();
  SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
  _mysql.run_query_and_get_result(query, std::move(kpi_promise));
  try {
    mysql_result res(kpi_future.get());
    while (_mysql.fetch_row(res)) {
      _kpi_event_cache.emplace(
          std::make_pair(res.value_as_u32(1), res.value_as_u64(2)),
          res.value_as_u32(0));
    }
  } catch (std::exception const& e) {
    throw msg_fmt("BAM-BI: could not load kpi events from DB: {}", e.what());
  }
}

// When bulk statements are available.
struct bulk_dimension_kpi_binder {
  std::shared_ptr<spdlog::logger> logger;
  const std::shared_ptr<io::data>& event;
  void operator()(database::mysql_bulk_bind& binder) const {
    if (event->type() == bam::dimension_kpi_event::static_type()) {
      bam::dimension_kpi_event const& dk{
          *std::static_pointer_cast<bam::dimension_kpi_event const>(event)};
      std::string kpi_name;
      if (!dk.service_description.empty())
        kpi_name.append(dk.host_name)
            .append(" ")
            .append(dk.service_description);
      else if (!dk.kpi_ba_name.empty())
        kpi_name = dk.kpi_ba_name;
      else if (!dk.boolean_name.empty())
        kpi_name = dk.boolean_name;
      else if (!dk.meta_service_name.empty())
        kpi_name = dk.meta_service_name;
      SPDLOG_LOGGER_DEBUG(logger,
                          "BAM-BI: processing declaration of KPI {} ('{}')",
                          dk.kpi_id, kpi_name);

      binder.set_value_as_i32(0, dk.kpi_id);
      binder.set_value_as_str(
          1,
          com::centreon::common::truncate_utf8(
              kpi_name, get_centreon_storage_mod_bam_reporting_kpi_col_size(
                            centreon_storage_mod_bam_reporting_kpi_kpi_name)));
      binder.set_value_as_i32(2, dk.ba_id);
      binder.set_value_as_str(
          3,
          com::centreon::common::truncate_utf8(
              dk.ba_name, get_centreon_storage_mod_bam_reporting_kpi_col_size(
                              centreon_storage_mod_bam_reporting_kpi_ba_name)));
      binder.set_value_as_i32(4, dk.host_id);
      binder.set_value_as_str(
          5, com::centreon::common::truncate_utf8(
                 dk.host_name,
                 get_centreon_storage_mod_bam_reporting_kpi_col_size(
                     centreon_storage_mod_bam_reporting_kpi_host_name)));
      binder.set_value_as_i32(6, dk.service_id);
      binder.set_value_as_str(
          7,
          com::centreon::common::truncate_utf8(
              dk.service_description,
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_service_description)));
      if (dk.kpi_ba_id)
        binder.set_value_as_i32(8, dk.kpi_ba_id);
      else
        binder.set_null_i32(8);
      binder.set_value_as_str(
          9, com::centreon::common::truncate_utf8(
                 dk.kpi_ba_name,
                 get_centreon_storage_mod_bam_reporting_kpi_col_size(
                     centreon_storage_mod_bam_reporting_kpi_kpi_ba_name)));
      binder.set_value_as_i32(10, dk.meta_service_id);
      binder.set_value_as_str(
          11,
          com::centreon::common::truncate_utf8(
              dk.meta_service_name,
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_meta_service_name)));
      binder.set_value_as_f32(12, dk.impact_warning);
      binder.set_value_as_f32(13, dk.impact_critical);
      binder.set_value_as_f32(14, dk.impact_unknown);
      binder.set_value_as_i32(15, dk.boolean_id);
      binder.set_value_as_str(
          16, com::centreon::common::truncate_utf8(
                  dk.boolean_name,
                  get_centreon_storage_mod_bam_reporting_kpi_col_size(
                      centreon_storage_mod_bam_reporting_kpi_boolean_name)));
    } else {
      const DimensionKpiEvent& dk =
          std::static_pointer_cast<bam::pb_dimension_kpi_event const>(event)
              ->obj();
      std::string kpi_name;
      if (!dk.service_description().empty())
        kpi_name.append(dk.host_name())
            .append(" ")
            .append(dk.service_description());
      else if (!dk.kpi_ba_name().empty())
        kpi_name = dk.kpi_ba_name();
      else if (!dk.boolean_name().empty())
        kpi_name = dk.boolean_name();
      else if (!dk.meta_service_name().empty())
        kpi_name = dk.meta_service_name();
      SPDLOG_LOGGER_DEBUG(logger,
                          "BAM-BI: processing declaration of KPI {} ('{}')",
                          dk.kpi_id(), kpi_name);

      binder.set_value_as_i32(0, dk.kpi_id());
      binder.set_value_as_str(
          1,
          com::centreon::common::truncate_utf8(
              kpi_name, get_centreon_storage_mod_bam_reporting_kpi_col_size(
                            centreon_storage_mod_bam_reporting_kpi_kpi_name)));
      binder.set_value_as_i32(2, dk.ba_id());
      binder.set_value_as_str(
          3, com::centreon::common::truncate_utf8(
                 dk.ba_name(),
                 get_centreon_storage_mod_bam_reporting_kpi_col_size(
                     centreon_storage_mod_bam_reporting_kpi_ba_name)));
      binder.set_value_as_i32(4, dk.host_id());
      binder.set_value_as_str(
          5, com::centreon::common::truncate_utf8(
                 dk.host_name(),
                 get_centreon_storage_mod_bam_reporting_kpi_col_size(
                     centreon_storage_mod_bam_reporting_kpi_host_name)));
      binder.set_value_as_i32(6, dk.service_id());
      binder.set_value_as_str(
          7,
          com::centreon::common::truncate_utf8(
              dk.service_description(),
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_service_description)));
      if (dk.kpi_ba_id())
        binder.set_value_as_i32(8, dk.kpi_ba_id());
      else
        binder.set_null_i32(8);
      binder.set_value_as_str(
          9, com::centreon::common::truncate_utf8(
                 dk.kpi_ba_name(),
                 get_centreon_storage_mod_bam_reporting_kpi_col_size(
                     centreon_storage_mod_bam_reporting_kpi_kpi_ba_name)));
      binder.set_value_as_i32(10, dk.meta_service_id());
      binder.set_value_as_str(
          11,
          com::centreon::common::truncate_utf8(
              dk.meta_service_name(),
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_meta_service_name)));
      binder.set_value_as_f32(12, dk.impact_warning());
      binder.set_value_as_f32(13, dk.impact_critical());
      binder.set_value_as_f32(14, dk.impact_unknown());
      binder.set_value_as_i32(15, dk.boolean_id());
      binder.set_value_as_str(
          16, com::centreon::common::truncate_utf8(
                  dk.boolean_name(),
                  get_centreon_storage_mod_bam_reporting_kpi_col_size(
                      centreon_storage_mod_bam_reporting_kpi_boolean_name)));
    }
    binder.next_row();
  }
};

// When bulk statements are not available.
struct dimension_kpi_binder {
  std::shared_ptr<spdlog::logger> logger;
  const std::shared_ptr<io::data>& event;
  std::string operator()() const {
    if (event->type() == bam::dimension_kpi_event::static_type()) {
      bam::dimension_kpi_event const& dk{
          *std::static_pointer_cast<bam::dimension_kpi_event const>(event)};
      std::string kpi_name;
      if (!dk.service_description.empty())
        kpi_name.append(dk.host_name)
            .append(" ")
            .append(dk.service_description);
      else if (!dk.kpi_ba_name.empty())
        kpi_name = dk.kpi_ba_name;
      else if (!dk.boolean_name.empty())
        kpi_name = dk.boolean_name;
      else if (!dk.meta_service_name.empty())
        kpi_name = dk.meta_service_name;
      std::string sz_kpi_ba_id =
          dk.kpi_ba_id ? std::to_string(dk.kpi_ba_id) : "NULL";
      SPDLOG_LOGGER_DEBUG(logger,
                          "BAM-BI: processing declaration of KPI {} ('{}')",
                          dk.kpi_id, kpi_name);
      return fmt::format(
          "({},'{}',{},'{}',{},'{}',{},'{}',{},'{}',{},'{}',{},{},{},{},'{}')",
          dk.kpi_id,
          com::centreon::common::truncate_utf8(
              kpi_name, get_centreon_storage_mod_bam_reporting_kpi_col_size(
                            centreon_storage_mod_bam_reporting_kpi_kpi_name)),
          dk.ba_id,
          com::centreon::common::truncate_utf8(
              dk.ba_name, get_centreon_storage_mod_bam_reporting_kpi_col_size(
                              centreon_storage_mod_bam_reporting_kpi_ba_name)),
          dk.host_id,
          com::centreon::common::truncate_utf8(
              dk.host_name,
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_host_name)),
          dk.service_id,
          com::centreon::common::truncate_utf8(
              dk.service_description,
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_service_description)),
          sz_kpi_ba_id,
          com::centreon::common::truncate_utf8(
              dk.kpi_ba_name,
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_kpi_ba_name)),
          dk.meta_service_id,
          com::centreon::common::truncate_utf8(
              dk.meta_service_name,
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_meta_service_name)),
          dk.impact_warning, dk.impact_critical, dk.impact_unknown,
          dk.boolean_id,
          com::centreon::common::truncate_utf8(
              dk.boolean_name,
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_boolean_name)));
    } else {
      const DimensionKpiEvent& dk =
          std::static_pointer_cast<bam::pb_dimension_kpi_event const>(event)
              ->obj();
      std::string kpi_name;
      if (!dk.service_description().empty())
        kpi_name.append(dk.host_name())
            .append(" ")
            .append(dk.service_description());
      else if (!dk.kpi_ba_name().empty())
        kpi_name = dk.kpi_ba_name();
      else if (!dk.boolean_name().empty())
        kpi_name = dk.boolean_name();
      else if (!dk.meta_service_name().empty())
        kpi_name = dk.meta_service_name();
      std::string sz_kpi_ba_id =
          dk.kpi_ba_id() ? std::to_string(dk.kpi_ba_id()) : "NULL";
      SPDLOG_LOGGER_DEBUG(logger,
                          "BAM-BI: processing declaration of KPI {} ('{}')",
                          dk.kpi_id(), kpi_name);
      return fmt::format(
          "({},'{}',{},'{}',{},'{}',{},'{}',{},'{}',{},'{}',{},{},{},{},'{}')",
          dk.kpi_id(),
          com::centreon::common::truncate_utf8(
              kpi_name, get_centreon_storage_mod_bam_reporting_kpi_col_size(
                            centreon_storage_mod_bam_reporting_kpi_kpi_name)),
          dk.ba_id(),
          com::centreon::common::truncate_utf8(
              dk.ba_name(),
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_ba_name)),
          dk.host_id(),
          com::centreon::common::truncate_utf8(
              dk.host_name(),
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_host_name)),
          dk.service_id(),
          com::centreon::common::truncate_utf8(
              dk.service_description(),
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_service_description)),
          sz_kpi_ba_id,
          com::centreon::common::truncate_utf8(
              dk.kpi_ba_name(),
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_kpi_ba_name)),
          dk.meta_service_id(),
          com::centreon::common::truncate_utf8(
              dk.meta_service_name(),
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_meta_service_name)),
          dk.impact_warning(), dk.impact_critical(), dk.impact_unknown(),
          dk.boolean_id(),
          com::centreon::common::truncate_utf8(
              dk.boolean_name(),
              get_centreon_storage_mod_bam_reporting_kpi_col_size(
                  centreon_storage_mod_bam_reporting_kpi_boolean_name)));
    }
  }
};

// When bulk statements are available.
struct bulk_kpi_event_update_binder {
  const std::shared_ptr<io::data>& event;
  void operator()(database::mysql_bulk_bind& binder) const {
    if (event->type() == bam::kpi_event::static_type()) {
      bam::kpi_event const& ke =
          *std::static_pointer_cast<bam::kpi_event const>(event);
      if (ke.end_time.is_null())
        binder.set_null_u64(0);
      else
        binder.set_value_as_u64(
            0, static_cast<uint64_t>(ke.end_time.get_time_t()));
      binder.set_value_as_tiny(1, ke.status);
      binder.set_value_as_i32(2, ke.in_downtime);
      binder.set_value_as_i32(3, ke.impact_level);
      binder.set_value_as_i32(4, ke.kpi_id);
      binder.set_value_as_u64(
          5, static_cast<uint64_t>(ke.start_time.get_time_t()));
    } else {
      const KpiEvent& ke =
          std::static_pointer_cast<bam::pb_kpi_event const>(event)->obj();
      if (ke.end_time() <= 0)
        binder.set_null_u64(0);
      else
        binder.set_value_as_u64(0, ke.end_time());
      binder.set_value_as_tiny(1, ke.status());
      binder.set_value_as_i32(2, ke.in_downtime());
      binder.set_value_as_i32(3, ke.impact_level());
      binder.set_value_as_i32(4, ke.kpi_id());
      binder.set_value_as_u64(5, ke.start_time());
    }
    binder.next_row();
  }
};

// When bulk statements are not available.
struct kpi_event_update_binder {
  const std::shared_ptr<io::data>& event;
  std::string operator()() const {
    if (event->type() == bam::kpi_event::static_type()) {
      bam::kpi_event const& ke =
          *std::static_pointer_cast<bam::kpi_event const>(event);
      std::string sz_ke_time{fmt::format("{}", ke.end_time)};
      return fmt::format("({},{},{},{},{},{})", sz_ke_time, ke.status,
                         int(ke.in_downtime), ke.impact_level, ke.kpi_id,
                         ke.start_time);
    } else {
      const KpiEvent& ke =
          std::static_pointer_cast<bam::pb_kpi_event const>(event)->obj();
      std::string sz_ke_time =
          ke.end_time() <= 0 ? "NULL" : std::to_string(ke.end_time());
      return fmt::format("({},{},{},{},{},{})", sz_ke_time, int(ke.status()),
                         int(ke.in_downtime()), ke.impact_level(), ke.kpi_id(),
                         ke.start_time());
    }
  }
};

/**
 *  Prepare queries.
 */
void reporting_stream::_prepare() {
  SPDLOG_LOGGER_TRACE(_logger, "reporting stream _prepare");
  std::string query{
      "INSERT INTO mod_bam_reporting_ba_events (ba_id,"
      "first_level,start_time,end_time,status,in_downtime)"
      " VALUES(?,?,?,?,?,?)"};
  _ba_full_event_insert = _mysql.prepare_query(query);

  query =
      "UPDATE mod_bam_reporting_ba_events"
      "  SET end_time=?, first_level=?,"
      "      status=?, in_downtime=?"
      "  WHERE ba_id=? AND start_time=?";
  _ba_event_update = _mysql.prepare_query(query);

  query =
      "INSERT INTO mod_bam_reporting_ba_events_durations ("
      "                ba_event_id, start_time, "
      "                end_time, duration, sla_duration, timeperiod_id, "
      "                timeperiod_is_default)"
      "  SELECT b.ba_event_id, ?, ?, ?, "
      "         ?, ?, ?"
      "  FROM mod_bam_reporting_ba_events AS b"
      "  WHERE b.ba_id=? AND b.start_time=?";
  _ba_duration_event_insert = _mysql.prepare_query(query);

  query =
      "UPDATE mod_bam_reporting_ba_events_durations AS d"
      "  INNER JOIN mod_bam_reporting_ba_events AS e"
      "    ON d.ba_event_id=e.ba_event_id"
      "  SET d.start_time=?, d.end_time=?, "
      "      d.duration=?, d.sla_duration=?,"
      "      d.timeperiod_is_default=?"
      "  WHERE e.ba_id=?"
      "    AND e.start_time=?"
      "    AND d.timeperiod_id=?";
  _ba_duration_event_update = _mysql.prepare_query(query);

  query =
      "INSERT INTO mod_bam_reporting_kpi_events (kpi_id,"
      " start_time, end_time, status, in_downtime,"
      " impact_level) VALUES (?, ?, ?, ?, ?, ?)";
  _kpi_full_event_insert = _mysql.prepare_query(query);

  if (_mysql.support_bulk_statement()) {
    _kpi_event_update = std::make_unique<bulk_or_multi>(
        _mysql,
        "UPDATE mod_bam_reporting_kpi_events"
        " SET end_time=?, status=?,"
        " in_downtime=?, impact_level=?"
        " WHERE kpi_id=? AND start_time=?",
        _mysql.get_config().get_queries_per_transaction(), _logger);
  } else {
    _kpi_event_update = std::make_unique<bulk_or_multi>(
        "INSERT INTO mod_bam_reporting_kpi_events (end_time, status, "
        "in_downtime, impact_level, kpi_id, start_time) VALUES",
        "ON DUPLICATE KEY UPDATE end_time=VALUES(end_time), "
        "status=VALUES(status), in_downtime=VALUES(in_downtime), "
        "impact_level=VALUES(impact_level)");
  }

  query =
      "INSERT INTO mod_bam_reporting_relations_ba_kpi_events"
      " (ba_event_id, kpi_event_id)"
      " SELECT be.ba_event_id, ke.kpi_event_id"
      " FROM mod_bam_reporting_kpi_events AS ke"
      " LEFT JOIN mod_bam_reporting_ba_events AS be"
      " ON (ke.start_time >= be.start_time"
      " AND (be.end_time IS NULL OR ke.start_time < be.end_time))"
      " WHERE ke.kpi_id=? AND ke.start_time=? AND be.ba_id=?";
  _kpi_event_link = _mysql.prepare_query(query);

  query =
      "UPDATE mod_bam_reporting_relations_ba_kpi_events"
      " SET ba_event_id=?"
      " WHERE relation_id=?";
  _kpi_event_link_update = _mysql.prepare_query(query);

  query =
      "INSERT INTO mod_bam_reporting_ba (ba_id, ba_name, ba_description, "
      "sla_month_percent_crit, sla_month_percent_warn, "
      "sla_month_duration_crit, sla_month_duration_warn)"
      " VALUES (?,?,?,?,?,?,?)";
  _dimension_ba_insert = _mysql.prepare_query(query);

  query =
      "INSERT INTO mod_bam_reporting_bv (bv_id, bv_name, bv_description)"
      " VALUES (?,?,?)";
  _dimension_bv_insert = _mysql.prepare_query(query);

  query =
      "INSERT INTO mod_bam_reporting_relations_ba_bv (ba_id, bv_id)"
      "  VALUES (?, ?)";
  _dimension_ba_bv_relation_insert = _mysql.prepare_query(query);

  query =
      "INSERT INTO mod_bam_reporting_timeperiods"
      "            (timeperiod_id, name, sunday, monday,"
      "             tuesday, wednesday, thursday, friday,"
      "             saturday)"
      "  VALUES (?, ?, ?, ?,"
      "          ?, ?, ?, ?,"
      "          ?)";
  _dimension_timeperiod_insert = _mysql.prepare_query(query);

  // Dimension BA/timeperiod insertion.
  query =
      "INSERT INTO mod_bam_reporting_relations_ba_timeperiods ("
      "            ba_id, timeperiod_id, is_default)"
      "  VALUES (?, ?, ?)";
  _dimension_ba_timeperiod_insert = _mysql.prepare_query(query);

  _dimension_truncate_tables.clear();
  query = "DELETE FROM mod_bam_reporting_kpi";
  _dimension_truncate_tables.emplace_back(_mysql.prepare_query(query));
  query = "DELETE FROM mod_bam_reporting_relations_ba_bv";
  _dimension_truncate_tables.emplace_back(_mysql.prepare_query(query));
  query = "DELETE FROM mod_bam_reporting_ba";
  _dimension_truncate_tables.emplace_back(_mysql.prepare_query(query));
  query = "DELETE FROM mod_bam_reporting_bv";
  _dimension_truncate_tables.emplace_back(_mysql.prepare_query(query));
  query = "DELETE FROM mod_bam_reporting_timeperiods";
  _dimension_truncate_tables.emplace_back(_mysql.prepare_query(query));

  // Dimension KPI insertion
  if (_mysql.support_bulk_statement()) {
    _dimension_kpi_insert = std::make_unique<bulk_or_multi>(
        _mysql,
        "INSERT INTO mod_bam_reporting_kpi (kpi_id, kpi_name,"
        " ba_id, ba_name, host_id, host_name,"
        " service_id, service_description, kpi_ba_id,"
        " kpi_ba_name, meta_service_id, meta_service_name,"
        " impact_warning, impact_critical, impact_unknown,"
        " boolean_id, boolean_name)"
        " VALUES (?, ?, ?, ?, ?,"
        " ?, ?, ?,"
        " ?, ?, ?,"
        " ?, ?, ?,"
        " ?, ?, ?)",
        _mysql.get_config().get_queries_per_transaction(), _logger);
  } else {
    _dimension_kpi_insert = std::make_unique<bulk_or_multi>(
        "INSERT INTO mod_bam_reporting_kpi (kpi_id, kpi_name,"
        " ba_id, ba_name, host_id, host_name,"
        " service_id, service_description, kpi_ba_id,"
        " kpi_ba_name, meta_service_id, meta_service_name,"
        " impact_warning, impact_critical, impact_unknown,"
        " boolean_id, boolean_name)"
        " VALUES ",
        "");
  }
}

/**
 *  Process a ba event and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_ba_event(std::shared_ptr<io::data> const& e) {
  bam::ba_event const& be = *std::static_pointer_cast<bam::ba_event const>(e);
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM-BI: processing event of BA {} (start time {}, end time {}, status "
      "{}, in downtime {})",
      be.ba_id, be.start_time, be.end_time, be.status, be.in_downtime);

  id_start ba_key = std::make_pair(
      be.ba_id, static_cast<uint64_t>(be.start_time.get_time_t()));
  // event exists?
  if (_ba_event_cache.find(ba_key) != _ba_event_cache.end()) {
    if (be.end_time.is_null())
      _ba_event_update.bind_null_u64(0);
    else
      _ba_event_update.bind_value_as_u64(0, be.end_time.get_time_t());
    _ba_event_update.bind_value_as_i32(1, be.first_level);
    _ba_event_update.bind_value_as_tiny(2, be.status);
    _ba_event_update.bind_value_as_bool(3, be.in_downtime);
    _ba_event_update.bind_value_as_i32(4, be.ba_id);
    _ba_event_update.bind_value_as_u64(
        5, static_cast<uint64_t>(be.start_time.get_time_t()));

    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    _mysql.run_statement_and_get_int<int>(_ba_event_update, std::move(promise),
                                          mysql_task::int_type::AFFECTED_ROWS);

  } else {
    // Event was not found, insert one.
    try {
      _ba_full_event_insert.bind_value_as_i32(0, be.ba_id);
      _ba_full_event_insert.bind_value_as_i32(1, be.first_level);
      _ba_full_event_insert.bind_value_as_u64(
          2, static_cast<uint64_t>(be.start_time.get_time_t()));

      if (be.end_time.is_null())
        _ba_full_event_insert.bind_null_u64(3);
      else
        _ba_full_event_insert.bind_value_as_u64(
            3, static_cast<uint64_t>(be.end_time.get_time_t()));
      _ba_full_event_insert.bind_value_as_tiny(4, be.status);
      _ba_full_event_insert.bind_value_as_bool(5, be.in_downtime);

      std::promise<uint32_t> result;
      std::future<uint32_t> future_r = result.get_future();
      _mysql.run_statement_and_get_int<uint32_t>(
          _ba_full_event_insert, std::move(result), mysql_task::LAST_INSERT_ID,
          -1);
      uint32_t newba = future_r.get();
      _ba_event_cache[ba_key] = newba;
      // check events for BA
      if (_last_inserted_kpi.find(be.ba_id) != _last_inserted_kpi.end()) {
        std::map<std::time_t, uint64_t>& m_events =
            _last_inserted_kpi[be.ba_id];
        if (m_events.find(be.start_time.get_time_t()) != m_events.end()) {
          // Insert kpi event link.
          _kpi_event_link_update.bind_value_as_i32(0, newba);
          _kpi_event_link_update.bind_value_as_u64(
              1, m_events[be.start_time.get_time_t()]);
          _mysql.run_statement(_kpi_event_link_update,
                               database::mysql_error::update_kpi_event);
        }
        // remove older events for BA
        for (auto it = m_events.begin(); it != m_events.end();) {
          if (it->first < be.start_time.get_time_t())
            it = m_events.erase(it);
          else
            break;
        }
      }
    } catch (std::exception const& e) {
      throw msg_fmt(
          "BAM-BI: could not update event of BA {} starting at {} and ending "
          "at {}: {}",
          be.ba_id, be.start_time, be.end_time, e.what());
    }
  }

  // Compute the associated event durations.
  if (!be.end_time.is_null() && be.start_time != be.end_time) {
    com::centreon::broker::BaEvent pb_ba_ev;
    pb_ba_ev.set_ba_id(be.ba_id);
    pb_ba_ev.set_start_time(be.start_time.get_time_t());
    pb_ba_ev.set_end_time(be.end_time.get_time_t());
    _compute_event_durations(pb_ba_ev, this);
  }
}

/**
 *  Process a ba event and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_pb_ba_event(
    std::shared_ptr<io::data> const& e) {
  const BaEvent& be =
      std::static_pointer_cast<bam::pb_ba_event const>(e)->obj();
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM-BI: processing pb_ba_event of BA {} (start time {}, end time {}, "
      "status "
      "{}, in downtime {})",
      be.ba_id(), be.start_time(), be.end_time(),
      static_cast<uint32_t>(be.status()), be.in_downtime());

  id_start ba_key = std::make_pair(be.ba_id(), be.start_time());
  // event exists?
  if (_ba_event_cache.find(ba_key) != _ba_event_cache.end()) {
    if (static_cast<int64_t>(be.end_time()) <= 0)
      _ba_event_update.bind_null_u64(0);
    else
      _ba_event_update.bind_value_as_u64(0, be.end_time());
    _ba_event_update.bind_value_as_i32(1, be.first_level());
    _ba_event_update.bind_value_as_tiny(2, be.status());
    _ba_event_update.bind_value_as_bool(3, be.in_downtime());
    _ba_event_update.bind_value_as_i32(4, be.ba_id());
    _ba_event_update.bind_value_as_u64(5, be.start_time());

    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    _mysql.run_statement_and_get_int<int>(_ba_event_update, std::move(promise),
                                          mysql_task::int_type::AFFECTED_ROWS);

  } else {
    // Event was not found, insert one.
    try {
      _ba_full_event_insert.bind_value_as_i32(0, be.ba_id());
      _ba_full_event_insert.bind_value_as_i32(1, be.first_level());
      _ba_full_event_insert.bind_value_as_u64(2, be.start_time());

      if (static_cast<int64_t>(be.end_time()) <= 0)
        _ba_full_event_insert.bind_null_i64(3);
      else
        _ba_full_event_insert.bind_value_as_i64(3, be.end_time());
      _ba_full_event_insert.bind_value_as_tiny(4, be.status());
      _ba_full_event_insert.bind_value_as_bool(5, be.in_downtime());

      std::promise<uint32_t> result;
      std::future<uint32_t> future_r = result.get_future();
      _mysql.run_statement_and_get_int<uint32_t>(
          _ba_full_event_insert, std::move(result), mysql_task::LAST_INSERT_ID,
          -1);
      uint32_t newba = future_r.get();
      _ba_event_cache[ba_key] = newba;
      // check events for BA
      if (_last_inserted_kpi.find(be.ba_id()) != _last_inserted_kpi.end()) {
        std::map<std::time_t, uint64_t>& m_events =
            _last_inserted_kpi[be.ba_id()];
        if (m_events.find(be.start_time()) != m_events.end()) {
          // Insert kpi event link.
          _kpi_event_link_update.bind_value_as_i32(0, newba);
          _kpi_event_link_update.bind_value_as_u64(1,
                                                   m_events[be.start_time()]);
          _mysql.run_statement(_kpi_event_link_update,
                               database::mysql_error::update_kpi_event);
        }
        // remove older events for BA
        for (auto it = m_events.begin(); it != m_events.end();) {
          if (it->first < be.start_time())
            it = m_events.erase(it);
          else
            break;
        }
      }
    } catch (std::exception const& e) {
      throw msg_fmt(
          "BAM-BI: could not update event of BA {} starting at {} and ending "
          "at {}: {}",
          be.ba_id(), be.start_time(), be.end_time(), e.what());
    }
  }
  // Compute the associated event durations.
  if (be.end_time() > 0 && be.start_time() != be.end_time())
    _compute_event_durations(be, this);
}

/**
 *  Process a ba duration event and write it to the db.
 *
 *  @param[in] e  The event.
 */
void reporting_stream::_process_ba_duration_event(
    std::shared_ptr<io::data> const& e) {
  bam::ba_duration_event const& bde =
      *std::static_pointer_cast<bam::ba_duration_event const>(e);
  SPDLOG_LOGGER_DEBUG(_logger,
                      "BAM-BI: processing BA duration event of BA {} (start "
                      "time {}, end time "
                      "{}, duration {}, sla duration {})",
                      bde.ba_id, bde.start_time, bde.end_time, bde.duration,
                      bde.sla_duration);

  // Try to update first.
  _ba_duration_event_update.bind_value_as_u64(
      1, static_cast<uint64_t>(bde.end_time.get_time_t()));
  _ba_duration_event_update.bind_value_as_u64(
      0, static_cast<uint64_t>(bde.start_time.get_time_t()));
  _ba_duration_event_update.bind_value_as_i32(2, bde.duration);
  _ba_duration_event_update.bind_value_as_i32(3, bde.sla_duration);
  _ba_duration_event_update.bind_value_as_i32(4, bde.timeperiod_is_default);
  _ba_duration_event_update.bind_value_as_i32(5, bde.ba_id);
  _ba_duration_event_update.bind_value_as_u64(
      6, static_cast<uint64_t>(bde.real_start_time.get_time_t()));
  _ba_duration_event_update.bind_value_as_i32(7, bde.timeperiod_id);

  std::promise<int> promise;
  std::future<int> future = promise.get_future();
  int thread_id(_mysql.run_statement_and_get_int<int>(
      _ba_duration_event_update, std::move(promise),
      mysql_task::int_type::AFFECTED_ROWS));
  try {
    // Insert if no rows was updated.
    if (future.get() == 0) {
      _ba_duration_event_insert.bind_value_as_u64(
          0, static_cast<uint64_t>(bde.start_time.get_time_t()));
      _ba_duration_event_insert.bind_value_as_u64(
          1, static_cast<uint64_t>(bde.end_time.get_time_t()));
      _ba_duration_event_insert.bind_value_as_i32(2, bde.duration);
      _ba_duration_event_insert.bind_value_as_i32(3, bde.sla_duration);
      _ba_duration_event_insert.bind_value_as_i32(4, bde.timeperiod_id);
      _ba_duration_event_insert.bind_value_as_f64(5, bde.timeperiod_is_default);
      _ba_duration_event_insert.bind_value_as_i32(6, bde.ba_id);
      _ba_duration_event_insert.bind_value_as_u64(
          7, static_cast<uint64_t>(bde.real_start_time.get_time_t()));

      _mysql.run_statement(_ba_duration_event_insert,
                           database::mysql_error::empty, thread_id);
    }
  } catch (std::exception const& e) {
    throw msg_fmt(
        "BAM-BI: could not insert duration event of BA {}"
        " starting at {} : {}",
        bde.ba_id, bde.start_time, e.what());
  }
}

/**
 *  Process a ba duration event and write it to the db.
 *
 *  @param[in] e  The event.
 */
void reporting_stream::_process_pb_ba_duration_event(
    std::shared_ptr<io::data> const& e) {
  const BaDurationEvent& bde =
      std::static_pointer_cast<bam::pb_ba_duration_event>(e)->obj();
  SPDLOG_LOGGER_DEBUG(_logger,
                      "BAM-BI: processing BA duration event of BA {} (start "
                      "time {}, end time "
                      "{}, duration {}, sla duration {})",
                      bde.ba_id(), bde.start_time(), bde.end_time(),
                      bde.duration(), bde.sla_duration());

  // Try to update first.
  _ba_duration_event_update.bind_value_as_u64(1, bde.end_time());
  _ba_duration_event_update.bind_value_as_u64(0, bde.start_time());
  _ba_duration_event_update.bind_value_as_i32(2, bde.duration());
  _ba_duration_event_update.bind_value_as_i32(3, bde.sla_duration());
  _ba_duration_event_update.bind_value_as_i32(4, bde.timeperiod_is_default());
  _ba_duration_event_update.bind_value_as_i32(5, bde.ba_id());
  _ba_duration_event_update.bind_value_as_u64(
      6, static_cast<uint64_t>(bde.real_start_time()));
  _ba_duration_event_update.bind_value_as_i32(7, bde.timeperiod_id());

  std::promise<int> promise;
  std::future<int> future = promise.get_future();
  int thread_id(_mysql.run_statement_and_get_int<int>(
      _ba_duration_event_update, std::move(promise),
      mysql_task::int_type::AFFECTED_ROWS));
  try {
    // Insert if no rows was updated.
    if (future.get() == 0) {
      _ba_duration_event_insert.bind_value_as_u64(0, bde.start_time());
      _ba_duration_event_insert.bind_value_as_u64(1, bde.end_time());
      _ba_duration_event_insert.bind_value_as_i32(2, bde.duration());
      _ba_duration_event_insert.bind_value_as_i32(3, bde.sla_duration());
      _ba_duration_event_insert.bind_value_as_i32(4, bde.timeperiod_id());
      _ba_duration_event_insert.bind_value_as_f64(5,
                                                  bde.timeperiod_is_default());
      _ba_duration_event_insert.bind_value_as_i32(6, bde.ba_id());
      _ba_duration_event_insert.bind_value_as_u64(7, bde.real_start_time());

      _mysql.run_statement(_ba_duration_event_insert,
                           database::mysql_error::empty, thread_id);
    }
  } catch (std::exception const& e) {
    throw msg_fmt(
        "BAM-BI: could not insert duration event of BA {}"
        " starting at {} : {}",
        bde.ba_id(), bde.start_time(), e.what());
  }
}

/**
 *  Process a kpi event and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_kpi_event(std::shared_ptr<io::data> const& e) {
  bam::kpi_event const& ke = *std::static_pointer_cast<bam::kpi_event const>(e);
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM-BI: processing event of KPI {} (start time {}, end time {}, state "
      "{}, in downtime {})",
      ke.kpi_id, ke.start_time, ke.end_time, ke.status, ke.in_downtime);

  if (ke.start_time.is_null()) {
    SPDLOG_LOGGER_ERROR(_logger, "BAM_BI invalid null start_time ");
    return;
  }
  id_start kpi_key = std::make_pair(
      ke.kpi_id, static_cast<uint64_t>(ke.start_time.get_time_t()));
  // event exists?
  if (_kpi_event_cache.find(kpi_key) != _kpi_event_cache.end()) {
    if (_kpi_event_update->is_bulk())
      _kpi_event_update->add_bulk_row(bulk_kpi_event_update_binder{e});
    else
      _kpi_event_update->add_multi_row(kpi_event_update_binder{e});
  } else {
    // don't exist.
    try {
      _kpi_full_event_insert.bind_value_as_i32(0, ke.kpi_id);
      _kpi_full_event_insert.bind_value_as_u64(
          1, static_cast<uint64_t>(ke.start_time.get_time_t()));
      if (ke.end_time.is_null())
        _kpi_full_event_insert.bind_null_u64(2);
      else
        _kpi_full_event_insert.bind_value_as_u64(
            2, static_cast<uint64_t>(ke.end_time.get_time_t()));
      _kpi_full_event_insert.bind_value_as_tiny(3, ke.status);
      _kpi_full_event_insert.bind_value_as_bool(4, ke.in_downtime);
      _kpi_full_event_insert.bind_value_as_i32(5, ke.impact_level);

      std::promise<uint64_t> result_kpi_insert;
      std::future<uint64_t> future_kpi_insert = result_kpi_insert.get_future();

      int thread_id(_mysql.run_statement_and_get_int<uint64_t>(
          _kpi_full_event_insert, std::move(result_kpi_insert),
          mysql_task::LAST_INSERT_ID));
      _kpi_event_cache[kpi_key] = future_kpi_insert.get();

      // Insert kpi event link.
      _kpi_event_link.bind_value_as_i32(0, ke.kpi_id);
      _kpi_event_link.bind_value_as_u64(
          1, static_cast<uint64_t>(ke.start_time.get_time_t()));
      _kpi_event_link.bind_value_as_u32(2, ke.ba_id);

      std::promise<uint64_t> result;
      std::future<uint64_t> future_r = result.get_future();
      _mysql.run_statement_and_get_int<uint64_t>(
          _kpi_event_link, std::move(result), mysql_task::LAST_INSERT_ID,
          thread_id);

      uint64_t evt_id{
          future_r.get()};  //_kpi_event_link.last_insert_id().toUInt()};
      _last_inserted_kpi[ke.ba_id].insert({ke.start_time.get_time_t(), evt_id});
    } catch (std::exception const& e) {
      throw msg_fmt(
          "BAM-BI: could not update KPI {} starting at {}"
          " and ending at {}: {}",
          ke.kpi_id, ke.start_time, ke.end_time, e.what());
    }
  }
}

/**
 *  Process a kpi event and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_pb_kpi_event(
    std::shared_ptr<io::data> const& e) {
  const KpiEvent& ke =
      std::static_pointer_cast<bam::pb_kpi_event const>(e)->obj();
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM-BI: processing event of KPI {} (start time {}, end time {}, state "
      "{}, in downtime {})",
      ke.kpi_id(), ke.start_time(), ke.end_time(), ke.status(),
      ke.in_downtime());

  if (ke.start_time() > std::numeric_limits<int32_t>::max()) {
    SPDLOG_LOGGER_ERROR(_logger, "BAM_BI invalid start_time {}",
                        ke.start_time());
    return;
  }

  id_start kpi_key = std::make_pair(ke.kpi_id(), ke.start_time());
  // event exists?
  if (_kpi_event_cache.find(kpi_key) != _kpi_event_cache.end()) {
    if (_kpi_event_update->is_bulk())
      _kpi_event_update->add_bulk_row(bulk_kpi_event_update_binder{e});
    else
      _kpi_event_update->add_multi_row(kpi_event_update_binder{e});
  } else {
    // don't exist => insert one.
    try {
      _kpi_full_event_insert.bind_value_as_i32(0, ke.kpi_id());
      _kpi_full_event_insert.bind_value_as_u64(1, ke.start_time());
      if (ke.end_time() <= 0)
        _kpi_full_event_insert.bind_null_u64(2);
      else
        _kpi_full_event_insert.bind_value_as_u64(2, ke.end_time());
      _kpi_full_event_insert.bind_value_as_tiny(3, ke.status());
      _kpi_full_event_insert.bind_value_as_bool(4, ke.in_downtime());
      _kpi_full_event_insert.bind_value_as_i32(5, ke.impact_level());

      std::promise<uint64_t> result_kpi_insert;
      std::future<uint64_t> future_kpi_insert = result_kpi_insert.get_future();

      int thread_id(_mysql.run_statement_and_get_int<uint64_t>(
          _kpi_full_event_insert, std::move(result_kpi_insert),
          mysql_task::LAST_INSERT_ID));
      _kpi_event_cache[kpi_key] = future_kpi_insert.get();

      // Insert kpi event link.
      _kpi_event_link.bind_value_as_i32(0, ke.kpi_id());
      _kpi_event_link.bind_value_as_u64(1, ke.start_time());
      _kpi_event_link.bind_value_as_u32(2, ke.ba_id());

      std::promise<uint64_t> result;
      std::future<uint64_t> future_r = result.get_future();
      _mysql.run_statement_and_get_int<uint64_t>(
          _kpi_event_link, std::move(result), mysql_task::LAST_INSERT_ID,
          thread_id);

      uint64_t evt_id{
          future_r.get()};  //_kpi_event_link.last_insert_id().toUInt()};
      _last_inserted_kpi[ke.ba_id()].insert({ke.start_time(), evt_id});
    } catch (std::exception const& e) {
      throw msg_fmt(
          "BAM-BI: could not update KPI {} starting at {}"
          " and ending at {}: {}",
          ke.kpi_id(), ke.start_time(), ke.end_time(), e.what());
    }
  }
}

/**
 *  Process a dimension ba and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_dimension_ba(
    std::shared_ptr<io::data> const& e) {
  bam::dimension_ba_event const& dba =
      *std::static_pointer_cast<bam::dimension_ba_event const>(e);
  SPDLOG_LOGGER_DEBUG(_logger, "BAM-BI: processing declaration of BA {} ('{}')",
                      dba.ba_id, dba.ba_description);
  _dimension_ba_insert.bind_value_as_i32(0, dba.ba_id);
  _dimension_ba_insert.bind_value_as_str(
      1, com::centreon::common::truncate_utf8(
             dba.ba_name, get_centreon_storage_mod_bam_reporting_ba_col_size(
                              centreon_storage_mod_bam_reporting_ba_ba_name)));
  _dimension_ba_insert.bind_value_as_str(
      2, com::centreon::common::truncate_utf8(
             dba.ba_description,
             get_centreon_storage_mod_bam_reporting_ba_col_size(
                 centreon_storage_mod_bam_reporting_ba_ba_description)));
  _dimension_ba_insert.bind_value_as_f64(3, dba.sla_month_percent_crit);
  _dimension_ba_insert.bind_value_as_f64(4, dba.sla_month_percent_warn);
  _dimension_ba_insert.bind_value_as_f64(5, dba.sla_duration_crit);
  _dimension_ba_insert.bind_value_as_f64(6, dba.sla_duration_warn);
  _mysql.run_statement(_dimension_ba_insert, database::mysql_error::insert_ba);
}

/**
 *  Process a dimension ba and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_pb_dimension_ba(
    std::shared_ptr<io::data> const& e) {
  const DimensionBaEvent& dba =
      std::static_pointer_cast<bam::pb_dimension_ba_event const>(e)->obj();
  SPDLOG_LOGGER_DEBUG(_logger,
                      "BAM-BI: pb processing declaration of BA {} ('{}')",
                      dba.ba_id(), dba.ba_description());
  _dimension_ba_insert.bind_value_as_i32(0, dba.ba_id());
  _dimension_ba_insert.bind_value_as_str(
      1,
      com::centreon::common::truncate_utf8(
          dba.ba_name(), get_centreon_storage_mod_bam_reporting_ba_col_size(
                             centreon_storage_mod_bam_reporting_ba_ba_name)));
  _dimension_ba_insert.bind_value_as_str(
      2, com::centreon::common::truncate_utf8(
             dba.ba_description(),
             get_centreon_storage_mod_bam_reporting_ba_col_size(
                 centreon_storage_mod_bam_reporting_ba_ba_description)));
  _dimension_ba_insert.bind_value_as_f64(3, dba.sla_month_percent_crit());
  _dimension_ba_insert.bind_value_as_f64(4, dba.sla_month_percent_warn());
  _dimension_ba_insert.bind_value_as_f64(5, dba.sla_duration_crit());
  _dimension_ba_insert.bind_value_as_f64(6, dba.sla_duration_warn());
  _mysql.run_statement(_dimension_ba_insert, database::mysql_error::insert_ba);
}

/**
 *  Process a dimension bv and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_dimension_bv(
    std::shared_ptr<io::data> const& e) {
  bam::dimension_bv_event const& dbv =
      *std::static_pointer_cast<bam::dimension_bv_event const>(e);
  SPDLOG_LOGGER_DEBUG(_logger, "BAM-BI: processing declaration of BV {} ('{}')",
                      dbv.bv_id, dbv.bv_name);

  _dimension_bv_insert.bind_value_as_i32(0, dbv.bv_id);
  _dimension_bv_insert.bind_value_as_str(
      1, com::centreon::common::truncate_utf8(
             dbv.bv_name, get_centreon_storage_mod_bam_reporting_bv_col_size(
                              centreon_storage_mod_bam_reporting_bv_bv_name)));
  _dimension_bv_insert.bind_value_as_str(
      2, com::centreon::common::truncate_utf8(
             dbv.bv_description,
             get_centreon_storage_mod_bam_reporting_bv_col_size(
                 centreon_storage_mod_bam_reporting_bv_bv_description)));
  _mysql.run_statement(_dimension_bv_insert, database::mysql_error::insert_bv);
}

/**
 *  Process a dimension bv and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_pb_dimension_bv(
    std::shared_ptr<io::data> const& e) {
  const DimensionBvEvent& dbv =
      std::static_pointer_cast<bam::pb_dimension_bv_event const>(e)->obj();
  SPDLOG_LOGGER_DEBUG(_logger,
                      "BAM-BI: processing pb declaration of BV {} ('{}')",
                      dbv.bv_id(), dbv.bv_name());

  _dimension_bv_insert.bind_value_as_i32(0, dbv.bv_id());
  _dimension_bv_insert.bind_value_as_str(
      1,
      com::centreon::common::truncate_utf8(
          dbv.bv_name(), get_centreon_storage_mod_bam_reporting_bv_col_size(
                             centreon_storage_mod_bam_reporting_bv_bv_name)));
  _dimension_bv_insert.bind_value_as_str(
      2, com::centreon::common::truncate_utf8(
             dbv.bv_description(),
             get_centreon_storage_mod_bam_reporting_bv_col_size(
                 centreon_storage_mod_bam_reporting_bv_bv_description)));
  _mysql.run_statement(_dimension_bv_insert, database::mysql_error::insert_bv);
}

/**
 *  Process a dimension ba bv relation and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_dimension_ba_bv_relation(
    std::shared_ptr<io::data> const& e) {
  bam::dimension_ba_bv_relation_event const& dbabv =
      *std::static_pointer_cast<bam::dimension_ba_bv_relation_event const>(e);
  SPDLOG_LOGGER_DEBUG(_logger,
                      "BAM-BI: processing relation between BA {} and BV {}",
                      dbabv.ba_id, dbabv.bv_id);

  _dimension_ba_bv_relation_insert.bind_value_as_i32(0, dbabv.ba_id);
  _dimension_ba_bv_relation_insert.bind_value_as_i32(1, dbabv.bv_id);
  _mysql.run_statement(_dimension_ba_bv_relation_insert,
                       database::mysql_error::insert_dimension_ba_bv);
}

/**
 *  Process a dimension ba bv relation and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_pb_dimension_ba_bv_relation(
    std::shared_ptr<io::data> const& e) {
  const DimensionBaBvRelationEvent& dbabv =
      std::static_pointer_cast<bam::pb_dimension_ba_bv_relation_event const>(e)
          ->obj();
  SPDLOG_LOGGER_DEBUG(_logger,
                      "BAM-BI: processing pb relation between BA {} and BV {}",
                      dbabv.ba_id(), dbabv.bv_id());

  _dimension_ba_bv_relation_insert.bind_value_as_i32(0, dbabv.ba_id());
  _dimension_ba_bv_relation_insert.bind_value_as_i32(1, dbabv.bv_id());
  _mysql.run_statement(_dimension_ba_bv_relation_insert,
                       database::mysql_error::insert_dimension_ba_bv);
}

/**
 *  Cache a dimension event, and commit it on the disk accordingly.
 *
 *  @param e  The event to process.
 */
void reporting_stream::_process_dimension(const std::shared_ptr<io::data>& e) {
  if (_processing_dimensions) {
    // Cache the event until the end of the dimensions dump.
    switch (e->type()) {
      case io::events::data_type<io::bam, bam::de_dimension_ba_event>::value: {
        bam::dimension_ba_event const& dba =
            *std::static_pointer_cast<bam::dimension_ba_event const>(e);
        SPDLOG_LOGGER_DEBUG(_logger,
                            "BAM-BI: preparing ba dimension {} ('{}' '{}')",
                            dba.ba_id, dba.ba_name, dba.ba_description);
      } break;
      case io::events::data_type<io::bam, bam::de_dimension_bv_event>::value: {
        bam::dimension_bv_event const& dbv =
            *std::static_pointer_cast<bam::dimension_bv_event const>(e);
        SPDLOG_LOGGER_DEBUG(_logger, "BAM-BI: preparing bv dimension {} ('{}')",
                            dbv.bv_id, dbv.bv_name);
      } break;
      case io::events::data_type<
          io::bam, bam::de_dimension_ba_bv_relation_event>::value: {
        bam::dimension_ba_bv_relation_event const& dbabv =
            *std::static_pointer_cast<
                bam::dimension_ba_bv_relation_event const>(e);
        SPDLOG_LOGGER_DEBUG(
            _logger, "BAM-BI: preparing relation between ba {} and bv {}",
            dbabv.ba_id, dbabv.bv_id);
      } break;
      case io::events::data_type<io::bam, bam::de_dimension_kpi_event>::value: {
        bam::dimension_kpi_event const& dk{
            *std::static_pointer_cast<bam::dimension_kpi_event const>(e)};
        std::string kpi_name;
        if (!dk.service_description.empty())
          kpi_name =
              fmt::format("svc: {} {}", dk.host_name, dk.service_description);
        else if (!dk.kpi_ba_name.empty())
          kpi_name = fmt::format("ba: {}", dk.kpi_ba_name);
        else if (!dk.boolean_name.empty())
          kpi_name = fmt::format("bool: {}", dk.boolean_name);
        else if (!dk.meta_service_name.empty())
          kpi_name = fmt::format("meta: {}", dk.meta_service_name);
        SPDLOG_LOGGER_DEBUG(_logger,
                            "BAM-BI: preparing declaration of kpi {} ('{}')",
                            dk.kpi_id, kpi_name);
      } break;
      case io::events::data_type<io::bam,
                                 bam::de_dimension_timeperiod>::value: {
        bam::dimension_timeperiod const& tp =
            *std::static_pointer_cast<bam::dimension_timeperiod const>(e);
        SPDLOG_LOGGER_DEBUG(
            _logger, "BAM-BI: preparing declaration of timeperiod {} ('{}')",
            tp.id, tp.name);
      } break;
      case io::events::data_type<
          io::bam, bam::de_dimension_ba_timeperiod_relation>::value: {
        bam::dimension_ba_timeperiod_relation const& r =
            *std::static_pointer_cast<
                bam::dimension_ba_timeperiod_relation const>(e);
        SPDLOG_LOGGER_DEBUG(
            _logger, "BAM-BI: preparing relation of BA {} to timeperiod {}",
            r.ba_id, r.timeperiod_id);
      } break;
      default:
        SPDLOG_LOGGER_DEBUG(_logger, "BAM-BI: preparing event of type {:x}",
                            e->type());
        break;
    }
    _dimension_data_cache.emplace_back(e);

  } else
    SPDLOG_LOGGER_WARN(
        _logger,
        "Dimension of type {:x} not handled because dimension block not "
        "opened.",
        e->type());
}

/**
 *  Cache a dimension event, and commit it on the disk accordingly.
 *
 *  @param e  The event to process.
 */
void reporting_stream::_process_pb_dimension(
    const std::shared_ptr<io::data>& e) {
  if (_processing_dimensions) {
    // Cache the event until the end of the dimensions dump.
    switch (e->type()) {
      case pb_dimension_bv_event::static_type(): {
        const DimensionBvEvent& dbv =
            std::static_pointer_cast<bam::pb_dimension_bv_event const>(e)
                ->obj();
        SPDLOG_LOGGER_DEBUG(_logger, "BAM-BI: preparing bv dimension {} ('{}')",
                            dbv.bv_id(), dbv.bv_name());
      } break;
      case pb_dimension_ba_bv_relation_event::static_type(): {
        const DimensionBaBvRelationEvent& dbabv =
            std::static_pointer_cast<
                bam::pb_dimension_ba_bv_relation_event const>(e)
                ->obj();
        SPDLOG_LOGGER_DEBUG(
            _logger, "BAM-BI: preparing relation between ba {} and bv {}",
            dbabv.ba_id(), dbabv.bv_id());
      } break;
      case bam::pb_dimension_timeperiod::static_type(): {
        bam::pb_dimension_timeperiod const& tp =
            *std::static_pointer_cast<bam::pb_dimension_timeperiod const>(e);
        SPDLOG_LOGGER_DEBUG(
            _logger, "BAM-BI: preparing declaration of timeperiod {} ('{}')",
            tp.obj().id(), tp.obj().name());
      } break;
      case bam::pb_dimension_ba_event::static_type(): {
        const DimensionBaEvent& dba =
            std::static_pointer_cast<bam::pb_dimension_ba_event const>(e)
                ->obj();
        SPDLOG_LOGGER_DEBUG(_logger,
                            "BAM-BI: preparing ba dimension {} ('{}' '{}')",
                            dba.ba_id(), dba.ba_name(), dba.ba_description());
      } break;
      case bam::pb_dimension_kpi_event::static_type(): {
        const DimensionKpiEvent& dk =
            std::static_pointer_cast<bam::pb_dimension_kpi_event const>(e)
                ->obj();
        std::string kpi_name;
        if (!dk.service_description().empty())
          kpi_name = fmt::format("svc: {} {}", dk.host_name(),
                                 dk.service_description());
        else if (!dk.kpi_ba_name().empty())
          kpi_name = fmt::format("ba: {}", dk.kpi_ba_name());
        else if (!dk.boolean_name().empty())
          kpi_name = fmt::format("bool: {}", dk.boolean_name());
        else if (!dk.meta_service_name().empty())
          kpi_name = fmt::format("meta: {}", dk.meta_service_name());
        SPDLOG_LOGGER_DEBUG(_logger,
                            "BAM-BI: preparing declaration of kpi {} ('{}')",
                            dk.kpi_id(), kpi_name);
      } break;
      case bam::pb_dimension_ba_timeperiod_relation::static_type(): {
        const DimensionBaTimeperiodRelation& r =
            std::static_pointer_cast<bam::pb_dimension_ba_timeperiod_relation>(
                e)
                ->obj();
        SPDLOG_LOGGER_DEBUG(
            _logger, "BAM-BI: preparing relation of BA {} to timeperiod {}",
            r.ba_id(), r.timeperiod_id());
      } break;

      default:
        SPDLOG_LOGGER_DEBUG(_logger, "BAM-BI: preparing event of type {:x}",
                            e->type());
        break;
    }
    _dimension_data_cache.emplace_back(e);

  } else
    SPDLOG_LOGGER_WARN(
        _logger,
        "Dimension of type {:x} not handled because dimension block not "
        "opened.",
        e->type());
}

/**
 *  Dispatch a dimension event.
 *
 *  @param[in] data  The dimension event.
 */
void reporting_stream::_dimension_dispatch(
    std::shared_ptr<io::data> const& data) {
  switch (data->type()) {
    case io::events::data_type<io::bam, bam::de_dimension_ba_event>::value:
      _process_dimension_ba(data);
      break;
    case bam::pb_dimension_ba_event::static_type():
      _process_pb_dimension_ba(data);
      break;
    case io::events::data_type<io::bam, bam::de_dimension_bv_event>::value:
      _process_dimension_bv(data);
      break;
    case bam::pb_dimension_bv_event::static_type():
      _process_pb_dimension_bv(data);
      break;
    case io::events::data_type<io::bam,
                               bam::de_dimension_ba_bv_relation_event>::value:
      _process_dimension_ba_bv_relation(data);
      break;
    case bam::pb_dimension_ba_bv_relation_event::static_type():
      _process_pb_dimension_ba_bv_relation(data);
      break;
    case bam::dimension_kpi_event::static_type():
    case bam::pb_dimension_kpi_event::static_type():
      _process_dimension_kpi(data);
      break;
    case io::events::data_type<io::bam, bam::de_dimension_timeperiod>::value:
      _process_dimension_timeperiod(data);
      break;
    case bam::pb_dimension_timeperiod::static_type():
      _process_pb_dimension_timeperiod(data);
      break;
    case io::events::data_type<io::bam,
                               bam::de_dimension_ba_timeperiod_relation>::value:
      _process_dimension_ba_timeperiod_relation(data);
    case bam::pb_dimension_ba_timeperiod_relation::static_type():
      _process_pb_dimension_ba_timeperiod_relation(data);
      break;
    default:
      break;
  }
}

/**
 *  Process a dimension truncate signal and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_dimension_truncate_signal(
    const std::shared_ptr<io::data>& e) {
  const dimension_truncate_table_signal& dtts =
      *std::static_pointer_cast<const dimension_truncate_table_signal>(e);
  _process_dimension_truncate_signal(dtts.update_started);
}

/**
 *  Process a dimension truncate signal and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_pb_dimension_truncate_signal(
    const std::shared_ptr<io::data>& e) {
  _process_dimension_truncate_signal(
      std::static_pointer_cast<const pb_dimension_truncate_table_signal>(e)
          ->obj()
          .update_started());
}

/**
 * @brief Process a dimension truncate signal and write it to the db.
 *
 * @param update_started
 */
void reporting_stream::_process_dimension_truncate_signal(bool update_started) {
  if (update_started) {
    _processing_dimensions = true;
    SPDLOG_LOGGER_DEBUG(_logger,
                        "BAM-BI: processing table truncation signal (opening)");

    _dimension_data_cache.clear();
  } else {
    SPDLOG_LOGGER_DEBUG(_logger,
                        "BAM-BI: processing table truncation signal (closing)");
    // Lock the availability thread.
    std::lock_guard<availability_thread> lock(*_availabilities);

    for (auto& stmt : _dimension_truncate_tables)
      _mysql.run_statement(stmt,
                           database::mysql_error::truncate_dimension_table);
    _timeperiods.clear();

    // XXX : dimension event acknowledgement might not work !!!
    //       For this reason, ignore any db error. We wouldn't
    //       be able to manage it on a stream level.
    try {
      for (auto& e : _dimension_data_cache)
        _dimension_dispatch(e);
    } catch (std::exception const& e) {
      SPDLOG_LOGGER_ERROR(
          _logger, "BAM-BI: ignored dimension insertion failure: {}", e.what());
    }

    _mysql.commit();
    _dimension_data_cache.clear();
    _processing_dimensions = false;
  }
}

/**
 *  Process a dimension KPI and write it to the db.
 *
 *  @param[in] e The event.
 */
void reporting_stream::_process_dimension_kpi(
    std::shared_ptr<io::data> const& e) {
  if (_dimension_kpi_insert->is_bulk())
    _dimension_kpi_insert->add_bulk_row(bulk_dimension_kpi_binder{_logger, e});
  else
    _dimension_kpi_insert->add_multi_row(dimension_kpi_binder{_logger, e});
}

/**
 *  Process a dimension timeperiod and store it in the DB and in the
 *  timeperiod cache.
 *
 *  @param[in] e  The event.
 */
void reporting_stream::_process_pb_dimension_timeperiod(
    std::shared_ptr<io::data> const& e) {
  const DimensionTimeperiod& tp =
      std::static_pointer_cast<bam::pb_dimension_timeperiod>(e)->obj();
  SPDLOG_LOGGER_DEBUG(
      _logger, "BAM-BI: processing pb declaration of timeperiod {} ('{}')",
      tp.id(), tp.name());
  _dimension_timeperiod_insert.bind_value_as_i32(0, tp.id());
  _dimension_timeperiod_insert.bind_value_as_str(
      1, com::centreon::common::truncate_utf8(
             tp.name(),
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_name)));
  _dimension_timeperiod_insert.bind_value_as_str(
      2, com::centreon::common::truncate_utf8(
             tp.sunday(),
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_sunday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      3, com::centreon::common::truncate_utf8(
             tp.monday(),
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_monday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      4, com::centreon::common::truncate_utf8(
             tp.tuesday(),
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_tuesday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      5, com::centreon::common::truncate_utf8(
             tp.wednesday(),
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_wednesday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      6, com::centreon::common::truncate_utf8(
             tp.thursday(),
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_thursday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      7, com::centreon::common::truncate_utf8(
             tp.friday(),
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_friday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      8, com::centreon::common::truncate_utf8(
             tp.saturday(),
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_saturday)));
  _mysql.run_statement(_dimension_timeperiod_insert,
                       database::mysql_error::insert_timeperiod);

  _apply(tp);
}

/**
 *  Process a dimension timeperiod and store it in the DB and in the
 *  timeperiod cache.
 *
 *  @param[in] e  The event.
 */
void reporting_stream::_process_dimension_timeperiod(
    std::shared_ptr<io::data> const& e) {
  bam::dimension_timeperiod const& tp =
      *std::static_pointer_cast<bam::dimension_timeperiod const>(e);
  SPDLOG_LOGGER_DEBUG(_logger,
                      "BAM-BI: processing declaration of timeperiod {} ('{}')",
                      tp.id, tp.name);

  _dimension_timeperiod_insert.bind_value_as_i32(0, tp.id);
  _dimension_timeperiod_insert.bind_value_as_str(
      1,
      com::centreon::common::truncate_utf8(
          tp.name, get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                       centreon_storage_mod_bam_reporting_timeperiods_name)));
  _dimension_timeperiod_insert.bind_value_as_str(
      2, com::centreon::common::truncate_utf8(
             tp.sunday,
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_sunday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      3, com::centreon::common::truncate_utf8(
             tp.monday,
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_monday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      4, com::centreon::common::truncate_utf8(
             tp.tuesday,
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_tuesday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      5, com::centreon::common::truncate_utf8(
             tp.wednesday,
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_wednesday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      6, com::centreon::common::truncate_utf8(
             tp.thursday,
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_thursday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      7, com::centreon::common::truncate_utf8(
             tp.friday,
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_friday)));
  _dimension_timeperiod_insert.bind_value_as_str(
      8, com::centreon::common::truncate_utf8(
             tp.saturday,
             get_centreon_storage_mod_bam_reporting_timeperiods_col_size(
                 centreon_storage_mod_bam_reporting_timeperiods_saturday)));
  _mysql.run_statement(_dimension_timeperiod_insert,
                       database::mysql_error::insert_timeperiod);
  DimensionTimeperiod convert;
  convert.set_id(tp.id);
  convert.set_name(tp.name);
  convert.set_monday(tp.monday);
  convert.set_tuesday(tp.tuesday);
  convert.set_wednesday(tp.wednesday);
  convert.set_thursday(tp.thursday);
  convert.set_friday(tp.friday);
  convert.set_saturday(tp.saturday);
  convert.set_sunday(tp.sunday);
  _apply(convert);
}

/**
 *  Process a dimension ba timeperiod relation and store it in
 *  a relation cache.
 *
 *  @param[in] e  The event.
 */
void reporting_stream::_process_dimension_ba_timeperiod_relation(
    std::shared_ptr<io::data> const& e) {
  bam::dimension_ba_timeperiod_relation const& r =
      *std::static_pointer_cast<bam::dimension_ba_timeperiod_relation const>(e);
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM-BI: processing relation of BA {} to timeperiod {} is_default={}",
      r.ba_id, r.timeperiod_id, r.is_default);

  _dimension_ba_timeperiod_insert.bind_value_as_i32(0, r.ba_id);
  _dimension_ba_timeperiod_insert.bind_value_as_i32(1, r.timeperiod_id);
  _dimension_ba_timeperiod_insert.bind_value_as_bool(2, r.is_default);
  _mysql.run_statement(_dimension_ba_timeperiod_insert,
                       database::mysql_error::insert_relation_ba_timeperiod);
  _timeperiods.add_relation(r.ba_id, r.timeperiod_id, r.is_default);
}

/**
 *  Process a dimension ba timeperiod relation and store it in
 *  a relation cache.
 *
 *  @param[in] e  The event.
 */
void reporting_stream::_process_pb_dimension_ba_timeperiod_relation(
    std::shared_ptr<io::data> const& e) {
  const DimensionBaTimeperiodRelation& r =
      std::static_pointer_cast<bam::pb_dimension_ba_timeperiod_relation>(e)
          ->obj();
  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM-BI: processing relation of BA {} to timeperiod {} is_default={}",
      r.ba_id(), r.timeperiod_id(), r.is_default());

  _dimension_ba_timeperiod_insert.bind_value_as_i32(0, r.ba_id());
  _dimension_ba_timeperiod_insert.bind_value_as_i32(1, r.timeperiod_id());
  _dimension_ba_timeperiod_insert.bind_value_as_bool(2, r.is_default());
  _mysql.run_statement(_dimension_ba_timeperiod_insert,
                       database::mysql_error::insert_relation_ba_timeperiod);
  _timeperiods.add_relation(r.ba_id(), r.timeperiod_id(), r.is_default());
}

/**
 *  @brief Compute and write the duration events associated with a ba event.
 *
 *  The event durations are computed from the associated timeperiods of the
 * BA.
 *
 *  @param[in] ev       The ba_event generating the durations.
 *  @param[in] visitor  A visitor stream.
 */
void reporting_stream::_compute_event_durations(const BaEvent& ev,
                                                io::stream* visitor) {
  if (!visitor)
    return;

  SPDLOG_LOGGER_INFO(
      _logger,
      "BAM-BI: computing durations of event started at {} and ended at {} on "
      "BA {}",
      ev.start_time(), ev.end_time(), ev.ba_id());

  // Find the timeperiods associated with this ba.
  std::vector<std::pair<time::timeperiod::ptr, bool>> timeperiods =
      _timeperiods.get_timeperiods_by_ba_id(ev.ba_id());

  if (timeperiods.empty()) {
    SPDLOG_LOGGER_DEBUG(_logger,
                        "BAM-BI: no reporting period defined for event "
                        "started at {} and ended "
                        "at {} on BA {}",
                        ev.start_time(), ev.end_time(), ev.ba_id());
    return;
  }

  for (std::vector<std::pair<time::timeperiod::ptr, bool>>::const_iterator
           it = timeperiods.begin(),
           end = timeperiods.end();
       it != end; ++it) {
    time::timeperiod::ptr tp = it->first;
    bool is_default = it->second;

    std::shared_ptr<pb_ba_duration_event> to_write{
        std::make_shared<pb_ba_duration_event>()};
    BaDurationEvent& dur_ev(to_write->mut_obj());
    dur_ev.set_ba_id(ev.ba_id());
    dur_ev.set_real_start_time(ev.start_time());
    dur_ev.set_start_time(tp->get_next_valid(ev.start_time()));
    dur_ev.set_end_time(ev.end_time());
    if (dur_ev.start_time() > 0 && dur_ev.end_time() > 0 &&
        (dur_ev.start_time() < dur_ev.end_time())) {
      dur_ev.set_duration(dur_ev.end_time() - dur_ev.start_time());
      dur_ev.set_sla_duration(
          tp->duration_intersect(dur_ev.start_time(), dur_ev.end_time()));
      dur_ev.set_timeperiod_id(tp->get_id());
      dur_ev.set_timeperiod_is_default(is_default);
      SPDLOG_LOGGER_DEBUG(
          _logger,
          "BAM-BI: durations of event started at {} and ended at {} on BA {} "
          "were computed for timeperiod {}, duration is {}s, SLA duration is "
          "{}",
          ev.start_time(), ev.end_time(), ev.ba_id(), tp->get_name(),
          dur_ev.duration(), dur_ev.sla_duration());
      visitor->write(to_write);
    } else
      SPDLOG_LOGGER_DEBUG(
          _logger,
          "BAM-BI: event started at {} and ended at {} on BA {} has no "
          "duration on timeperiod {}",
          ev.start_time(), ev.end_time(), ev.ba_id(), tp->get_name());
  }
}

/**
 *  Process a rebuild signal: Delete the obsolete data in the db and rebuild
 *  ba duration events.
 *
 *  @param[in] e  The event.
 */
void reporting_stream::_process_rebuild(std::shared_ptr<io::data> const& e) {
  const rebuild& r = *std::static_pointer_cast<const rebuild>(e);
  if (r.bas_to_rebuild.empty())
    return;
  SPDLOG_LOGGER_DEBUG(_logger, "BAM-BI: processing rebuild signal");

  _update_status("rebuilding: querying ba events");

  // We block the availability thread to prevent it waking
  // up on truncated event durations.
  try {
    std::lock_guard<availability_thread> lock(*_availabilities);

    // Delete obsolete ba events durations.
    {
      std::string query(
          fmt::format("DELETE a FROM mod_bam_reporting_ba_events_durations as "
                      "a INNER JOIN mod_bam_reporting_ba_events as b ON "
                      "a.ba_event_id = b.ba_event_id WHERE b.ba_id IN ({})",
                      r.bas_to_rebuild));

      SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
      _mysql.run_query(query, database::mysql_error::delete_ba_durations);
    }

    // Get the ba events.
    std::vector<std::shared_ptr<pb_ba_event>> ba_events;
    {
      std::string query(
          fmt::format("SELECT ba_id, start_time, end_time, status, in_downtime "
                      "boolean FROM mod_bam_reporting_ba_events WHERE end_time "
                      "IS NOT NULL AND ba_id IN ({})",
                      r.bas_to_rebuild));
      std::promise<mysql_result> promise;
      std::future<mysql_result> future = promise.get_future();
      SPDLOG_LOGGER_TRACE(_logger, "reporting_stream: query: '{}'", query);
      _mysql.run_query_and_get_result(query, std::move(promise));
      try {
        mysql_result res(future.get());

        while (_mysql.fetch_row(res)) {
          std::shared_ptr<pb_ba_event> baev(new pb_ba_event);
          baev->mut_obj().set_ba_id(res.value_as_i32(0));
          baev->mut_obj().set_start_time(res.value_as_i64(1));
          baev->mut_obj().set_end_time(res.value_as_i64(2));
          baev->mut_obj().set_status(com::centreon::broker::State(
              (com::centreon::broker::bam::state)res.value_as_i32(3)));
          baev->mut_obj().set_in_downtime(res.value_as_bool(4));
          ba_events.push_back(baev);
          SPDLOG_LOGGER_DEBUG(_logger, "BAM-BI: got events of BA {}",
                              baev->obj().ba_id());
        }
      } catch (std::exception const& e) {
        throw msg_fmt("BAM-BI: could not get BA events of {} : {}",
                      r.bas_to_rebuild, e.what());
      }
    }

    SPDLOG_LOGGER_INFO(_logger, "BAM-BI: will now rebuild the event durations");

    size_t ba_events_num = ba_events.size();
    size_t ba_events_curr = 0;

    // Generate new ba events durations for each ba events.
    {
      for (const auto& ev : ba_events) {
        std::string s(fmt::format("rebuilding: ba event {}/{}",
                                  ba_events_curr++, ba_events_num));
        _update_status(s);
        _compute_event_durations(ev->obj(), this);
      }
    }
  } catch (...) {
    _update_status("");
    throw;
  }

  SPDLOG_LOGGER_INFO(
      _logger,
      "BAM-BI: event durations rebuild finished, will rebuild availabilities "
      "now");

  // Ask for the availabilities thread to recompute the availabilities.
  _availabilities->rebuild_availabilities(r.bas_to_rebuild);

  _update_status("");
}

/**
 *  Update status of endpoint.
 *
 *  @param[in] status New status.
 */
void reporting_stream::_update_status(std::string const& status) {
  std::lock_guard<std::mutex> lock(_statusm);
  _status = status;
}

void reporting_stream::_commit() {
  _dimension_kpi_insert->execute(_mysql);
  _kpi_event_update->execute(_mysql);
  _mysql.commit();
  _ack_events += _pending_events;
  _pending_events = 0;
}
