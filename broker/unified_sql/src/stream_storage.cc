/*
** Copyright 2019-2022 Centreon
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

#include <fmt/format.h>

#include <cfloat>
#include <cstring>
#include <list>
#include <sstream>

#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/database/table_max_size.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/misc/perfdata.hh"
#include "com/centreon/broker/misc/shared_mutex.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/unified_sql/internal.hh"
#include "com/centreon/broker/unified_sql/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::unified_sql;

constexpr int32_t queue_timer_duration = 10;
constexpr int32_t dt_queue_timer_duration = 5;

/**
 *  Check that the floating point values are the same number or are NaN or are
 *  INFINITY at the same time. The idea is to check if a is changed into b, did
 *  it really change?
 *
 *  @param[in] a Floating point value.
 *  @param[in] b Floating point value.
 *
 *  @return true if they are equal, false otherwise.
 */
static inline bool check_equality(double a, double b) {
  static const double eps = 0.000001;
  if (a == b)
    return true;
  if (std::isnan(a) && std::isnan(b))
    return true;
  if (fabs(a - b) < eps)
    return true;
  return false;
}

/**
 *  Process a service status event.
 *
 *  @param[in] e Uncasted service status.
 *
 * @return the number of events sent to the database.
 */
void stream::_unified_sql_process_pb_service_status(
    const std::shared_ptr<io::data>& d) {
  auto s{static_cast<const neb::pb_service_status*>(d.get())};
  auto& ss = s->obj();

  uint64_t host_id = ss.host_id(), service_id = ss.service_id();

  log_v2::perfdata()->debug(
      "unified sql::_unified_sql service_status processing: host_id:{}, "
      "service_id:{}",
      host_id, service_id);
  auto it_index_cache = _index_cache.find({host_id, service_id});
  if (it_index_cache == _index_cache.end()) {
    log_v2::sql()->critical(
        "sql: could not find index for service({}, {}) - maybe the poller with "
        "that service should be restarted",
        host_id, service_id);
    return;
  }

  uint32_t rrd_len;
  int32_t conn =
      _mysql.choose_connection_by_instance(_cache_host_instance[host_id]);
  bool index_locked{false};

  /* Index does not exist */
  uint64_t index_id = it_index_cache->second.index_id;
  rrd_len = it_index_cache->second.rrd_retention;
  index_locked = it_index_cache->second.locked;
  uint32_t interval = it_index_cache->second.interval * _interval_length;
  log_v2::perfdata()->debug(
      "unified sql: host_id:{}, service_id:{} - index already in cache "
      "- index_id {}, rrd_len {}",
      host_id, service_id, index_id, rrd_len);

  if (index_id) {
    /* Generate status event */
    log_v2::perfdata()->debug(
        "unified sql: host_id:{}, service_id:{} - generating status event "
        "with index_id {}, rrd_len: {}",
        host_id, service_id, index_id, rrd_len);
    if (ss.checked()) {
      auto status{std::make_shared<storage::pb_status>()};
      auto& s = status->mut_obj();
      s.set_index_id(index_id);
      s.set_interval(interval);
      s.set_rrd_len(rrd_len);
      s.set_time(ss.last_check());
      s.set_state(ss.last_hard_state());
      multiplexing::publisher().write(status);
    }

    if (!ss.perfdata().empty()) {
      /* Statements preparations */
      if (!_metrics_insert.prepared()) {
        _metrics_insert = _mysql.prepare_query(
            "INSERT INTO metrics "
            "(index_id,metric_name,unit_name,warn,warn_low,"
            "warn_threshold_mode,crit,"
            "crit_low,crit_threshold_mode,min,max,current_value,"
            "data_source_type) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)");
      }

      /* Parse perfdata. */
      _finish_action(-1, actions::metrics);
      std::list<misc::perfdata> pds{misc::parse_perfdata(
          ss.host_id(), ss.service_id(), ss.perfdata().c_str())};

      std::list<std::shared_ptr<io::data>> to_publish;
      for (auto& pd : pds) {
        misc::read_lock rlck(_metric_cache_m);
        auto it_index_cache = _metric_cache.find({index_id, pd.name()});

        /* The cache does not contain this metric */
        uint32_t metric_id;
        bool need_metric_mapping = true;
        if (it_index_cache == _metric_cache.end()) {
          rlck.unlock();
          log_v2::perfdata()->debug(
              "unified sql: no metrics corresponding to index {} and "
              "perfdata '{}' found in cache",
              index_id, pd.name());
          /* Let's insert it */
          _metrics_insert.bind_value_as_u64(0, index_id);
          _metrics_insert.bind_value_as_str(1, pd.name());
          _metrics_insert.bind_value_as_str(2, pd.unit());
          _metrics_insert.bind_value_as_f32(3, pd.warning());
          _metrics_insert.bind_value_as_f32(4, pd.warning_low());
          _metrics_insert.bind_value_as_tiny(5, pd.warning_mode());
          _metrics_insert.bind_value_as_f32(6, pd.critical());
          _metrics_insert.bind_value_as_f32(7, pd.critical_low());
          _metrics_insert.bind_value_as_tiny(8, pd.critical_mode());
          _metrics_insert.bind_value_as_f32(9, pd.min());
          _metrics_insert.bind_value_as_f32(10, pd.max());
          _metrics_insert.bind_value_as_f32(11, pd.value());

          uint32_t type = pd.value_type();
          char t[2];
          t[0] = '0' + type;
          t[1] = 0;
          _metrics_insert.bind_value_as_str(12, t);

          // Execute query.
          std::promise<int> promise;
          std::future<int> future = promise.get_future();
          _mysql.run_statement_and_get_int<int>(
              _metrics_insert, std::move(promise),
              database::mysql_task::LAST_INSERT_ID, conn);
          try {
            metric_id = future.get();

            // Insert metric in cache.
            log_v2::perfdata()->info(
                "unified sql: new metric {} for index {} and perfdata "
                "'{}'",
                metric_id, index_id, pd.name());
            metric_info info{.locked = false,
                             .metric_id = metric_id,
                             .type = type,
                             .value = pd.value(),
                             .unit_name = pd.unit(),
                             .warn = pd.warning(),
                             .warn_low = pd.warning_low(),
                             .warn_mode = pd.warning_mode(),
                             .crit = pd.critical(),
                             .crit_low = pd.critical_low(),
                             .crit_mode = pd.critical_mode(),
                             .min = pd.min(),
                             .max = pd.max(),
                             .metric_mapping_sent =
                                 true};  // It will be done after this block

            std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
            _metric_cache[{index_id, pd.name()}] = info;
          } catch (std::exception const& e) {
            log_v2::perfdata()->error(
                "unified sql: failed to create metric {} with type {}, "
                "value {}, unit_name {}, warn {}, warn_low {}, warn_mode {}, "
                "crit {}, crit_low {}, crit_mode {}, min {} and max {}",
                metric_id, type, pd.value(), pd.unit(), pd.warning(),
                pd.warning_low(), pd.warning_mode(), pd.critical(),
                pd.critical_low(), pd.critical_mode(), pd.min(), pd.max());
            throw msg_fmt(
                "unified_sql: insertion of metric '{}"
                "' of index {} failed: {}",
                pd.name(), index_id, e.what());
          }
        } else {
          rlck.unlock();
          std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
          /* We have the metric in the cache */
          metric_id = it_index_cache->second.metric_id;
          if (!it_index_cache->second.metric_mapping_sent)
            it_index_cache->second.metric_mapping_sent = true;
          else
            need_metric_mapping = false;

          pd.value_type(static_cast<misc::perfdata::data_type>(
              it_index_cache->second.type));

          log_v2::perfdata()->debug(
              "unified sql: metric {} concerning index {}, perfdata "
              "'{}' found in cache",
              it_index_cache->second.metric_id, index_id, pd.name());
          // Should we update metrics ?
          if (!check_equality(it_index_cache->second.value, pd.value()) ||
              it_index_cache->second.unit_name != pd.unit() ||
              !check_equality(it_index_cache->second.warn, pd.warning()) ||
              !check_equality(it_index_cache->second.warn_low,
                              pd.warning_low()) ||
              it_index_cache->second.warn_mode != pd.warning_mode() ||
              !check_equality(it_index_cache->second.crit, pd.critical()) ||
              !check_equality(it_index_cache->second.crit_low,
                              pd.critical_low()) ||
              it_index_cache->second.crit_mode != pd.critical_mode() ||
              !check_equality(it_index_cache->second.min, pd.min()) ||
              !check_equality(it_index_cache->second.max, pd.max())) {
            log_v2::perfdata()->info(
                "unified sql: updating metric {} of index {}, perfdata "
                "'{}' with unit: {}, warning: {}:{}, critical: {}:{}, min: "
                "{}, max: {}",
                it_index_cache->second.metric_id, index_id, pd.name(),
                pd.unit(), pd.warning_low(), pd.warning(), pd.critical_low(),
                pd.critical(), pd.min(), pd.max());
            // Update metrics table.
            it_index_cache->second.unit_name = pd.unit();
            it_index_cache->second.value = pd.value();
            it_index_cache->second.warn = pd.warning();
            it_index_cache->second.warn_low = pd.warning_low();
            it_index_cache->second.crit = pd.critical();
            it_index_cache->second.crit_low = pd.critical_low();
            it_index_cache->second.warn_mode = pd.warning_mode();
            it_index_cache->second.crit_mode = pd.critical_mode();
            it_index_cache->second.min = pd.min();
            it_index_cache->second.max = pd.max();
            {
              std::lock_guard<std::mutex> lck(_queues_m);
              _metrics[it_index_cache->second.metric_id] =
                  &it_index_cache->second;
            }
            log_v2::perfdata()->debug("new metric with metric_id={}",
                                      it_index_cache->second.metric_id);
          }
        }
        if (need_metric_mapping) {
          auto mm{std::make_shared<storage::pb_metric_mapping>()};
          auto& mm_obj = mm->mut_obj();
          mm_obj.set_index_id(index_id);
          mm_obj.set_metric_id(metric_id);
          to_publish.emplace_back(std::move(mm));
        }

        if (_store_in_db) {
          // Append perfdata to queue.
          std::string row;
          if (std::isinf(pd.value()))
            row =
                fmt::format("({},{},'{}',{})", metric_id, ss.last_check(),
                            ss.state(), pd.value() < 0.0 ? -FLT_MAX : FLT_MAX);
          else if (std::isnan(pd.value()))
            row = fmt::format("({},{},'{}',NULL)", metric_id, ss.last_check(),
                              ss.state());
          else
            row = fmt::format("({},{},'{}',{})", metric_id, ss.last_check(),
                              ss.state(), pd.value());
          _perfdata.push_query(row);
        }

        // Send perfdata event to processing.
        if (!index_locked) {
          auto perf{std::make_shared<storage::pb_metric>()};
          auto& m = perf->mut_obj();
          m.set_time(ss.last_check());
          m.set_interval(interval);
          m.set_metric_id(metric_id);
          m.set_rrd_len(rrd_len);
          m.set_value(pd.value());
          m.set_value_type(static_cast<Metric_ValueType>(pd.value_type()));
          log_v2::perfdata()->debug(
              "unified sql: generating perfdata event for metric {} "
              "(name '{}', time {}, value {}, rrd_len {}, data_type {})",
              m.metric_id(), pd.name(), m.time(), m.value(), rrd_len,
              m.value_type());
          to_publish.emplace_back(std::move(perf));
        } else {
          log_v2::perfdata()->trace(
              "unified sql: index {} is locked, so metric {} event not sent "
              "to rrd",
              index_id, metric_id);
        }
      }
      multiplexing::publisher pblshr;
      pblshr.write(to_publish);
    }
  }
}

/**
 *  Process a service status event.
 *
 *  @param[in] e Uncasted service status.
 *
 * @return the number of events sent to the database.
 */
void stream::_unified_sql_process_service_status(
    const std::shared_ptr<io::data>& d) {
  neb::service_status const& ss{*static_cast<neb::service_status*>(d.get())};
  uint64_t host_id = ss.host_id, service_id = ss.service_id;

  log_v2::perfdata()->debug(
      "unified sql::_unified_sql_process_service_status(): host_id:{}, "
      "service_id:{}",
      host_id, service_id);
  auto it_index_cache = _index_cache.find({host_id, service_id});
  uint64_t index_id;
  uint32_t rrd_len;
  int32_t conn =
      _mysql.choose_connection_by_instance(_cache_host_instance[ss.host_id]);
  bool index_locked{false};
  bool special{!strncmp(ss.host_name.c_str(), BAM_NAME, sizeof(BAM_NAME) - 1)};

  auto add_metric_in_cache =
      [this](uint64_t index_id, uint64_t host_id, uint64_t service_id,
             neb::service_status const& ss, bool index_locked, bool special,
             uint32_t& rrd_len) -> void {
    if (index_id == 0) {
      throw msg_fmt(
          "unified_sql: could not fetch index_id of newly inserted index ({}"
          ", {})",
          host_id, service_id);
    }

    /* Insert index in cache. */
    log_v2::perfdata()->info(
        "unified sql: add_metric_in_cache: index {}, for host_id {} and "
        "service_id {}",
        index_id, host_id, service_id);
    index_info info{
        .index_id = index_id,
        .host_name = ss.host_name,
        .service_description = ss.service_description,
        .rrd_retention = _rrd_len,
        .interval = static_cast<uint32_t>(ss.check_interval),
        .special = special,
        .locked = index_locked,
    };

    _index_cache[{host_id, service_id}] = std::move(info);
    rrd_len = _rrd_len;
    log_v2::perfdata()->debug(
        "add metric in cache: (host: {}, service: {}, index: {}, returned "
        "rrd_len {}",
        ss.host_name, ss.service_description, index_id, rrd_len);

    /* Create the metric mapping. */
    auto im{std::make_shared<storage::index_mapping>(index_id, host_id,
                                                     service_id)};
    multiplexing::publisher pblshr;
    pblshr.write(im);
  };

  /* Index does not exist */
  if (it_index_cache == _index_cache.end()) {
    _finish_action(-1, actions::index_data);
    log_v2::perfdata()->debug(
        "unified sql::_unified_sql_process_service_status(): host_id:{}, "
        "service_id:{} - index not found in cache",
        host_id, service_id);

    if (!_index_data_insert.prepared())
      _index_data_insert = _mysql.prepare_query(
          "INSERT INTO index_data "
          "(host_id,host_name,service_id,service_description,must_be_rebuild,"
          "special) VALUES (?,?,?,?,?,?)");

    fmt::string_view hv(misc::string::truncate(
        ss.host_name, get_index_data_col_size(index_data_host_name)));
    fmt::string_view sv(misc::string::truncate(
        ss.service_description,
        get_index_data_col_size(index_data_service_description)));
    _index_data_insert.bind_value_as_i32(0, host_id);
    _index_data_insert.bind_value_as_str(1, hv);
    _index_data_insert.bind_value_as_i32(2, service_id);
    _index_data_insert.bind_value_as_str(3, sv);
    _index_data_insert.bind_value_as_str(4, "0");
    _index_data_insert.bind_value_as_str(5, special ? "1" : "0");
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    _mysql.run_statement_and_get_int<uint64_t>(
        _index_data_insert, std::move(promise),
        database::mysql_task::LAST_INSERT_ID, conn);
    try {
      index_id = future.get();
      add_metric_in_cache(index_id, host_id, service_id, ss, index_locked,
                          special, rrd_len);
    } catch (std::exception const& e) {
      try {
        if (!_index_data_query.prepared())
          _index_data_query = _mysql.prepare_query(
              "SELECT id from index_data WHERE host_id=? AND service_id=?");

        _index_data_query.bind_value_as_i32(0, host_id);
        _index_data_query.bind_value_as_i32(1, service_id);
        {
          std::promise<database::mysql_result> promise;
          std::future<database::mysql_result> future = promise.get_future();
          log_v2::sql()->debug(
              "Query for index_data for host_id={} and service_id={}", host_id,
              service_id);
          _mysql.run_statement_and_get_result(_index_data_query,
                                              std::move(promise), conn);

          database::mysql_result res(future.get());
          if (_mysql.fetch_row(res))
            index_id = res.value_as_u64(0);
          else
            index_id = 0;
        }

        if (index_id == 0)
          throw msg_fmt(
              "unified_sql: could not fetch index_id of newly inserted index "
              "({}, "
              "{})",
              host_id, service_id);

        if (!_index_data_update.prepared())
          _index_data_update = _mysql.prepare_query(
              "UPDATE index_data "
              "SET host_name=?, service_description=?, must_be_rebuild=?, "
              "special=? "
              "WHERE id=?");

        log_v2::sql()->debug(
            "Updating index_data for host_id={} and service_id={}", host_id,
            service_id);
        _index_data_update.bind_value_as_str(0, hv);
        _index_data_update.bind_value_as_str(1, sv);
        _index_data_update.bind_value_as_str(2, "0");
        _index_data_update.bind_value_as_str(3, special ? "1" : "0");
        _index_data_update.bind_value_as_u64(4, index_id);
        {
          std::promise<database::mysql_result> promise;
          std::future<database::mysql_result> future = promise.get_future();
          _mysql.run_statement_and_get_result(_index_data_update,
                                              std::move(promise), conn);
          future.get();
        }

        add_metric_in_cache(index_id, host_id, service_id, ss, index_locked,
                            special, rrd_len);
        log_v2::sql()->debug(
            "Index {} stored in cache for host_id={} and service_id={}",
            index_id, host_id, service_id);
      } catch (std::exception const& e) {
        throw msg_fmt(
            "unified_sql: insertion of index ( {}, {}"
            ") failed: {}",
            host_id, service_id, e.what());
      }
    }
  } else {
    index_id = it_index_cache->second.index_id;
    rrd_len = it_index_cache->second.rrd_retention;
    index_locked = it_index_cache->second.locked;
    log_v2::perfdata()->debug(
        "unified sql: host_id:{}, service_id:{} - index already in cache "
        "- index_id {}, rrd_len {}",
        host_id, service_id, index_id, rrd_len);
  }

  if (index_id) {
    /* Generate status event */
    log_v2::perfdata()->debug(
        "unified sql: host_id:{}, service_id:{} - generating status event "
        "with index_id {}, rrd_len: {}",
        host_id, service_id, index_id, rrd_len);
    if (ss.has_been_checked) {
      auto status(std::make_shared<storage::status>(
          ss.last_check, index_id,
          static_cast<uint32_t>(ss.check_interval * _interval_length), false,
          rrd_len, ss.last_hard_state));
      multiplexing::publisher().write(status);
    }

    if (!ss.perf_data.empty()) {
      /* Statements preparations */
      if (!_metrics_insert.prepared()) {
        _metrics_insert = _mysql.prepare_query(
            "INSERT INTO metrics "
            "(index_id,metric_name,unit_name,warn,warn_low,"
            "warn_threshold_mode,crit,"
            "crit_low,crit_threshold_mode,min,max,current_value,"
            "data_source_type) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)");
      }

      /* Parse perfdata. */
      _finish_action(-1, actions::metrics);
      std::list<misc::perfdata> pds{misc::parse_perfdata(
          ss.host_id, ss.service_id, ss.perf_data.c_str())};

      std::list<std::shared_ptr<io::data>> to_publish;
      for (auto& pd : pds) {
        misc::read_lock rlck(_metric_cache_m);
        auto it_index_cache = _metric_cache.find({index_id, pd.name()});

        /* The cache does not contain this metric */
        uint32_t metric_id;
        bool need_metric_mapping = true;
        if (it_index_cache == _metric_cache.end()) {
          rlck.unlock();
          log_v2::perfdata()->debug(
              "unified sql: no metrics corresponding to index {} and "
              "perfdata '{}' found in cache",
              index_id, pd.name());
          /* Let's insert it */
          _metrics_insert.bind_value_as_u64(0, index_id);
          _metrics_insert.bind_value_as_str(1, pd.name());
          _metrics_insert.bind_value_as_str(2, pd.unit());
          _metrics_insert.bind_value_as_f32(3, pd.warning());
          _metrics_insert.bind_value_as_f32(4, pd.warning_low());
          _metrics_insert.bind_value_as_tiny(5, pd.warning_mode());
          _metrics_insert.bind_value_as_f32(6, pd.critical());
          _metrics_insert.bind_value_as_f32(7, pd.critical_low());
          _metrics_insert.bind_value_as_tiny(8, pd.critical_mode());
          _metrics_insert.bind_value_as_f32(9, pd.min());
          _metrics_insert.bind_value_as_f32(10, pd.max());
          _metrics_insert.bind_value_as_f32(11, pd.value());

          uint32_t type = pd.value_type();
          char t[2];
          t[0] = '0' + type;
          t[1] = 0;
          _metrics_insert.bind_value_as_str(12, t);

          // Execute query.
          std::promise<int> promise;
          std::future<int> future = promise.get_future();
          _mysql.run_statement_and_get_int<int>(
              _metrics_insert, std::move(promise),
              database::mysql_task::LAST_INSERT_ID, conn);
          try {
            metric_id = future.get();

            // Insert metric in cache.
            log_v2::perfdata()->info(
                "unified sql: new metric {} for index {} and perfdata "
                "'{}'",
                metric_id, index_id, pd.name());
            metric_info info{.locked = false,
                             .metric_id = metric_id,
                             .type = type,
                             .value = pd.value(),
                             .unit_name = pd.unit(),
                             .warn = pd.warning(),
                             .warn_low = pd.warning_low(),
                             .warn_mode = pd.warning_mode(),
                             .crit = pd.critical(),
                             .crit_low = pd.critical_low(),
                             .crit_mode = pd.critical_mode(),
                             .min = pd.min(),
                             .max = pd.max(),
                             .metric_mapping_sent =
                                 true};  // It will be done after this block

            std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
            _metric_cache[{index_id, pd.name()}] = info;
          } catch (std::exception const& e) {
            log_v2::perfdata()->error(
                "unified sql: failed to create metric {} with type {}, "
                "value {}, unit_name {}, warn {}, warn_low {}, warn_mode {}, "
                "crit {}, crit_low {}, crit_mode {}, min {} and max {}",
                metric_id, type, pd.value(), pd.unit(), pd.warning(),
                pd.warning_low(), pd.warning_mode(), pd.critical(),
                pd.critical_low(), pd.critical_mode(), pd.min(), pd.max());
            throw msg_fmt(
                "unified_sql: insertion of metric '{}"
                "' of index {} failed: {}",
                pd.name(), index_id, e.what());
          }
        } else {
          rlck.unlock();
          std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
          /* We have the metric in the cache */
          metric_id = it_index_cache->second.metric_id;
          if (!it_index_cache->second.metric_mapping_sent)
            it_index_cache->second.metric_mapping_sent = true;
          else
            need_metric_mapping = false;

          pd.value_type(static_cast<misc::perfdata::data_type>(
              it_index_cache->second.type));

          log_v2::perfdata()->debug(
              "unified sql: metric {} concerning index {}, perfdata "
              "'{}' found in cache",
              it_index_cache->second.metric_id, index_id, pd.name());
          // Should we update metrics ?
          if (!check_equality(it_index_cache->second.value, pd.value()) ||
              it_index_cache->second.unit_name != pd.unit() ||
              !check_equality(it_index_cache->second.warn, pd.warning()) ||
              !check_equality(it_index_cache->second.warn_low,
                              pd.warning_low()) ||
              it_index_cache->second.warn_mode != pd.warning_mode() ||
              !check_equality(it_index_cache->second.crit, pd.critical()) ||
              !check_equality(it_index_cache->second.crit_low,
                              pd.critical_low()) ||
              it_index_cache->second.crit_mode != pd.critical_mode() ||
              !check_equality(it_index_cache->second.min, pd.min()) ||
              !check_equality(it_index_cache->second.max, pd.max())) {
            log_v2::perfdata()->info(
                "unified sql: updating metric {} of index {}, perfdata "
                "'{}' with unit: {}, warning: {}:{}, critical: {}:{}, min: "
                "{}, max: {}",
                it_index_cache->second.metric_id, index_id, pd.name(),
                pd.unit(), pd.warning_low(), pd.warning(), pd.critical_low(),
                pd.critical(), pd.min(), pd.max());
            // Update metrics table.
            it_index_cache->second.unit_name = pd.unit();
            it_index_cache->second.value = pd.value();
            it_index_cache->second.warn = pd.warning();
            it_index_cache->second.warn_low = pd.warning_low();
            it_index_cache->second.crit = pd.critical();
            it_index_cache->second.crit_low = pd.critical_low();
            it_index_cache->second.warn_mode = pd.warning_mode();
            it_index_cache->second.crit_mode = pd.critical_mode();
            it_index_cache->second.min = pd.min();
            it_index_cache->second.max = pd.max();
            {
              std::lock_guard<std::mutex> lck(_queues_m);
              _metrics[it_index_cache->second.metric_id] =
                  &it_index_cache->second;
            }
            log_v2::perfdata()->debug("new metric with metric_id={}",
                                      it_index_cache->second.metric_id);
          }
        }
        if (need_metric_mapping)
          to_publish.emplace_back(
              std::make_shared<storage::metric_mapping>(index_id, metric_id));

        if (_store_in_db) {
          // Append perfdata to queue.
          std::string row;
          if (std::isinf(pd.value()))
            row = fmt::format("{},{},'{}',{}", metric_id, ss.last_check,
                              ss.current_state,
                              pd.value() < 0.0 ? -FLT_MAX : FLT_MAX);
          else if (std::isnan(pd.value()))
            row = fmt::format("{},{},'{}',NULL", metric_id, ss.last_check,
                              ss.current_state);
          else
            row = fmt::format("{},{},'{}',{}", metric_id, ss.last_check,
                              ss.current_state, pd.value());
          _perfdata.push_query(row);
        }

        // Send perfdata event to processing.
        if (!index_locked) {
          auto perf{std::make_shared<storage::metric>(
              ss.host_id, ss.service_id, pd.name(), ss.last_check,
              static_cast<uint32_t>(ss.check_interval * _interval_length),
              false, metric_id, rrd_len, pd.value(),
              static_cast<misc::perfdata::data_type>(pd.value_type()))};
          log_v2::perfdata()->debug(
              "unified sql: generating perfdata event for metric {} "
              "(name '{}', time {}, value {}, rrd_len {}, data_type {})",
              perf->metric_id, perf->name, perf->time, perf->value, rrd_len,
              perf->value_type);
          to_publish.emplace_back(perf);
        }
      }
      multiplexing::publisher pblshr;
      pblshr.write(to_publish);
    }
  }
}

void stream::_update_metrics() {
  std::unordered_map<int32_t, metric_info*> metrics;
  {
    std::lock_guard<std::mutex> lck(_queues_m);
    std::swap(_metrics, metrics);
  }

  std::deque<std::string> m;
  for (auto it = metrics.begin(); it != metrics.end(); ++it) {
    metric_info* metric = it->second;
    m.emplace_back(fmt::format(
        "({},'{}',{},{},'{}',{},{},'{}',{},{},{})", metric->metric_id,
        misc::string::escape(metric->unit_name,
                             get_metrics_col_size(metrics_unit_name)),
        std::isnan(metric->warn) || std::isinf(metric->warn)
            ? "NULL"
            : fmt::format("{}", metric->warn),
        std::isnan(metric->warn_low) || std::isinf(metric->warn_low)
            ? "NULL"
            : fmt::format("{}", metric->warn_low),
        metric->warn_mode ? "1" : "0",
        std::isnan(metric->crit) || std::isinf(metric->crit)
            ? "NULL"
            : fmt::format("{}", metric->crit),
        std::isnan(metric->crit_low) || std::isinf(metric->crit_low)
            ? "NULL"
            : fmt::format("{}", metric->crit_low),
        metric->crit_mode ? "1" : "0",
        std::isnan(metric->min) || std::isinf(metric->min)
            ? "NULL"
            : fmt::format("{}", metric->min),
        std::isnan(metric->max) || std::isinf(metric->max)
            ? "NULL"
            : fmt::format("{}", metric->max),
        std::isnan(metric->value) || std::isinf(metric->value)
            ? "NULL"
            : fmt::format("{}", metric->value)));
  }
  if (!m.empty()) {
    std::string query(fmt::format(
        "INSERT INTO metrics (metric_id, unit_name, warn, warn_low, "
        "warn_threshold_mode, crit, crit_low, crit_threshold_mode, min, max, "
        "current_value) VALUES {} ON DUPLICATE KEY UPDATE "
        "unit_name=VALUES(unit_name), warn=VALUES(warn), "
        "warn_low=VALUES(warn_low), "
        "warn_threshold_mode=VALUES(warn_threshold_mode), crit=VALUES(crit), "
        "crit_low=VALUES(crit_low), "
        "crit_threshold_mode=VALUES(crit_threshold_mode), min=VALUES(min), "
        "max=VALUES(max), current_value=VALUES(current_value)",
        fmt::join(m, ",")));
    int32_t conn = _mysql.choose_best_connection(-1);
    _finish_action(-1, actions::metrics);
    log_v2::sql()->trace("Send query: {}", query);
    _mysql.run_query(query, database::mysql_error::update_metrics, false, conn);
    _add_action(conn, actions::metrics);
  }
}

void stream::_check_queues(asio::error_code ec) {
  if (ec)
    log_v2::sql()->error(
        "unified_sql: the queues check encountered an error: {}", ec.message());
  else {
    time_t now = time(nullptr);
    size_t sz_metrics;
    {
      std::lock_guard<std::mutex> lck(_queues_m);
      sz_metrics = _metrics.size();
    }

    bool perfdata_done = false;
    if (_perfdata.ready()) {
      std::string query = _perfdata.get_query();
      // Execute query.
      _mysql.run_query(query, database::mysql_error::insert_data);

      perfdata_done = true;
    }

    bool resources_done = false;
    if (_bulk_prepared_statement) {
      _finish_action(-1, actions::host_parents | actions::comments |
                             actions::downtimes | actions::host_dependencies |
                             actions::service_dependencies);
      if (_store_in_hosts_services) {
        log_v2::sql()->trace(
            "Check if some statements are ready, connections count = {}",
            _sscr_resources_bind->connections_count());
        for (uint32_t conn = 0; conn < _hscr_bind->connections_count();
             conn++) {
          if (_hscr_bind->ready(conn)) {
            log_v2::sql()->trace(
                "Sending {} hosts rows of host status on connection {}",
                _hscr_bind->size(conn), conn);
            // Setting the good bind to the stmt
            _hscr_bind->apply_to_stmt(conn);
            // Executing the stmt
            _mysql.run_statement(*_hscr_update,
                                 database::mysql_error::store_host_status,
                                 false, conn);
            _add_action(conn, actions::hosts);
          }
        }
        for (uint32_t conn = 0; conn < _sscr_bind->connections_count();
             conn++) {
          if (_sscr_bind->ready(conn)) {
            log_v2::sql()->trace(
                "Sending {} services rows of service status on connection {}",
                _sscr_bind->size(conn), conn);
            // Setting the good bind to the stmt
            _sscr_bind->apply_to_stmt(conn);
            // Executing the stmt
            _mysql.run_statement(*_sscr_update,
                                 database::mysql_error::store_service_status,
                                 false, conn);
            _add_action(conn, actions::services);
          }
        }
      }
      if (_store_in_resources) {
        for (uint32_t conn = 0;
             conn < _hscr_resources_bind->connections_count(); conn++) {
          if (_hscr_resources_bind->ready(conn)) {
            log_v2::sql()->trace(
                "Sending {} host rows of resource status on connection {}",
                _hscr_resources_bind->size(conn), conn);
            // Setting the good bind to the stmt
            _hscr_resources_bind->apply_to_stmt(conn);
            // Executing the stmt
            _mysql.run_statement(*_hscr_resources_update,
                                 database::mysql_error::store_host_status,
                                 false, conn);
            _add_action(conn, actions::resources);
          }
        }
        for (uint32_t conn = 0;
             conn < _sscr_resources_bind->connections_count(); conn++) {
          if (_sscr_resources_bind->ready(conn)) {
            log_v2::sql()->trace(
                "Sending {} service rows of resource status on connection {}",
                _sscr_resources_bind->size(conn), conn);
            // Setting the good bind to the stmt
            _sscr_resources_bind->apply_to_stmt(conn);
            // Executing the stmt
            _mysql.run_statement(*_sscr_resources_update,
                                 database::mysql_error::store_service_status,
                                 false, conn);
            _add_action(conn, actions::resources);
          }
        }
      }
      resources_done = true;
    }

    bool metrics_done = false;
    if (now >= _next_update_metrics || sz_metrics >= _max_metrics_queries) {
      _next_update_metrics = now + queue_timer_duration;
      _update_metrics();
      metrics_done = true;
    }

    bool customvar_done = false;
    if (_cv.ready()) {
      log_v2::sql()->debug("{} new custom variables inserted", _cv.size());
      std::string query = _cv.get_query();
      int32_t conn = special_conn::custom_variable % _mysql.connections_count();
      _mysql.run_query(query, database::mysql_error::update_customvariables,
                       false, conn);
      _add_action(conn, actions::custom_variables);
      customvar_done = true;
    }

    if (_cvs.ready()) {
      log_v2::sql()->debug("{} new custom variable status inserted",
                           _cvs.size());
      std::string query = _cvs.get_query();
      int32_t conn = special_conn::custom_variable % _mysql.connections_count();
      _mysql.run_query(query, database::mysql_error::update_customvariables,
                       false, conn);
      _add_action(conn, actions::custom_variables);
      customvar_done = true;
    }

    bool downtimes_done = false;
    if (_downtimes.ready()) {
      log_v2::sql()->debug("{} new downtimes inserted", _downtimes.size());
      std::string query = _downtimes.get_query();
      _finish_action(-1, actions::hosts | actions::instances |
                             actions::downtimes | actions::host_parents |
                             actions::host_dependencies |
                             actions::service_dependencies);
      int32_t conn = special_conn::downtime % _mysql.connections_count();
      _mysql.run_query(query, database::mysql_error::update_customvariables,
                       false, conn);
      _add_action(conn, actions::downtimes);
      downtimes_done = true;
    }

    bool logs_done = false;
    if (_logs.ready()) {
      log_v2::sql()->debug("{} new logs inserted", _logs.size());
      std::string query = _logs.get_query();
      // Execute query.
      int32_t conn = special_conn::log % _mysql.connections_count();
      _mysql.run_query(query, database::mysql_error::update_logs, false, conn);
      logs_done = true;
    }

    // End.
    log_v2::perfdata()->debug(
        "unified_sql: end check_queue - resources: {}, "
        "perfdata: {}, metrics: {}, customvar: "
        "{}, logs: {}, downtimes: {}",
        resources_done, perfdata_done, metrics_done, customvar_done, logs_done,
        downtimes_done);

    if (!_stop_check_queues) {
      std::lock_guard<std::mutex> l(_timer_m);
      _queues_timer.expires_after(std::chrono::seconds(5));
      _queues_timer.async_wait(
          [this](const asio::error_code& err) { _check_queues(err); });
    } else {
      log_v2::sql()->info("SQL: check_queues correctly interrupted.");
      _check_queues_stopped = true;
      _queues_cond_var.notify_all();
    }
  }
}

/**
 *  Check for deleted index.
 */
void stream::_check_deleted_index() {
  // Info.
  log_v2::sql()->info("unified_sql: starting DB cleanup");

  std::promise<database::mysql_result> promise;
  std::future<database::mysql_result> future = promise.get_future();
  int32_t conn = _mysql.choose_best_connection(-1);
  std::set<uint64_t> index_to_delete;
  std::set<uint64_t> metrics_to_delete;
  try {
    _mysql.run_query_and_get_result(
        "SELECT id FROM index_data WHERE to_delete=1", std::move(promise),
        conn);
    database::mysql_result res(future.get());

    while (_mysql.fetch_row(res)) {
      index_to_delete.insert(res.value_as_u64(0));
    }

    std::promise<database::mysql_result> promise_metrics;
    std::future<database::mysql_result> future_metrics =
        promise_metrics.get_future();
    _mysql.run_query_and_get_result(
        "SELECT metric_id FROM metrics WHERE to_delete=1",
        std::move(promise_metrics), conn);
    res = future_metrics.get();

    while (_mysql.fetch_row(res)) {
      metrics_to_delete.insert(res.value_as_u64(0));
    }
  } catch (const std::exception& e) {
    log_v2::sql()->error(
        "could not query index / metrics table(s) to get index to delete: "
        "{} ",
        e.what());
  }

  log_v2::sql()->info("Something to remove?");
  if (!metrics_to_delete.empty() || !index_to_delete.empty()) {
    log_v2::sql()->info("YES!!!");
    auto rg = std::make_shared<bbdo::pb_remove_graphs>();
    auto& obj = rg->mut_obj();
    for (auto& m : metrics_to_delete)
      obj.add_metric_ids(m);
    for (auto& i : index_to_delete)
      obj.add_index_ids(i);
    remove_graphs(rg);
  }
}

/**
 *  Check for indexes to rebuild.
 */
void stream::_check_rebuild_index() {
  // Fetch next index to delete.
  std::promise<database::mysql_result> promise;
  std::future<database::mysql_result> future = promise.get_future();
  int32_t conn = _mysql.choose_best_connection(-1);
  std::set<uint64_t> index_to_rebuild;
  try {
    _mysql.run_query_and_get_result(
        "SELECT id FROM index_data WHERE must_be_rebuild='1'",
        std::move(promise), conn);
    database::mysql_result res(future.get());

    while (_mysql.fetch_row(res)) {
      index_to_rebuild.insert(res.value_as_u64(0));
    }

  } catch (const std::exception& e) {
    log_v2::sql()->error(
        "could not query indexes table to get indexes to delete: {}", e.what());
  }

  log_v2::sql()->info("Something to rebuild?");
  if (!index_to_rebuild.empty()) {
    log_v2::sql()->info("YES!!!");
    auto rg = std::make_shared<bbdo::pb_rebuild_graphs>();
    auto& obj = rg->mut_obj();
    for (auto& i : index_to_rebuild)
      obj.add_index_ids(i);
    _rebuilder.rebuild_graphs(rg);
  }
}
