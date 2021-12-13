/*
** Copyright 2012-2015,2017,2020-2021 Centreon
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

#include "com/centreon/broker/unified_sql/rebuilder.hh"

#include <fmt/format.h>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "bbdo/storage/metric.hh"
#include "bbdo/storage/rebuild.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/database/mysql_error.hh"
#include "com/centreon/broker/database/mysql_result.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/time.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/unified_sql/internal.hh"
#include "com/centreon/broker/unified_sql/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::unified_sql;

/**
 *  Constructor.
 *
 *  @param[in] db_cfg                  Database configuration.
 *  @param[in] rebuild_check_interval  How often the rebuild thread will
 *                                     check for rebuild.
 *  @param[in] rrd_length              Length of RRD files.
 *  @param[in] interval_length         Length in seconds of a time unit.
 */
rebuilder::rebuilder(const database_config& db_cfg,
                     stream* parent,
                     uint32_t rebuild_check_interval,
                     uint32_t rrd_length,
                     uint32_t interval_length)
    : _timer{pool::io_context()},
      _should_exit{false},
      _db_cfg(db_cfg),
      _interval_length(interval_length),
      _rebuild_check_interval(rebuild_check_interval),
      _rrd_len(rrd_length),
      _parent(parent) {
  _db_cfg.set_connections_count(1);
  _db_cfg.set_queries_per_transaction(1);
  _timer.expires_after(std::chrono::seconds(1));
  _timer.async_wait(std::bind(&rebuilder::_run, this, std::placeholders::_1));
}

/**
 *  Destructor.
 */
rebuilder::~rebuilder() {
  _should_exit = true;
  std::promise<bool> p;
  std::future<bool> f(p.get_future());
  asio::post(_timer.get_executor(), [this, &p] {
    _timer.cancel();
    p.set_value(true);
  });
  f.get();
}

/**
 * @brief This internal function is called by the asio::steady_timer _timer
 * every _rebuild_check_interval seconds. It is the main procedure of the
 * rebuilder.
 *
 * @param ec Contains errors if any. In that case, the timer is stopped.
 * One particular error is asio::error_code::aborted which means the rebuilder
 * is stopped.
 */
void rebuilder::_run(asio::error_code ec) {
  if (ec)
    log_v2::sql()->info("unified_sql: rebuilder timer error: {}", ec.message());
  else {
    try {
      // Open DB.
      mysql ms(_db_cfg);

      // Fetch index to rebuild.
      index_info info;
      _next_index_to_rebuild(info, ms);
      while (!_should_exit && info.index_id) {
        // Get check interval of host/service.
        uint64_t index_id;
        uint32_t host_id;
        uint32_t service_id;
        uint32_t check_interval(0);
        uint32_t rrd_len;
        {
          index_id = info.index_id;
          host_id = info.host_id;
          service_id = info.service_id;
          rrd_len = info.rrd_retention;

          std::string query;
          if (!info.service_id)
            query =
                fmt::format("SELECT check_interval FROM hosts WHERE host_id={}",
                            info.host_id);
          else
            query = fmt::format(
                "SELECT check_interval FROM services WHERE host_id={} AND "
                "service_id={}",
                info.host_id, info.service_id);
          std::promise<database::mysql_result> promise;
          ms.run_query_and_get_result(query, &promise);
          database::mysql_result res(promise.get_future().get());
          if (ms.fetch_row(res))
            check_interval = res.value_as_f64(0) * _interval_length;
          if (!check_interval)
            check_interval = 5 * 60;
        }
        log_v2::sql()->info(
            "unified_sql: rebuilder: index {} (interval {}) will be rebuild",
            index_id, check_interval);

        // Set index as being rebuilt.
        _set_index_rebuild(ms, index_id, 2);

        try {
          // Fetch metrics to rebuild.
          std::list<metric_info> metrics_to_rebuild;
          {
            std::string query{
                fmt::format("SELECT metric_id, metric_name, data_source_type "
                            "FROM metrics WHERE index_id={}",
                            index_id)};

            std::promise<database::mysql_result> promise;
            ms.run_query_and_get_result(query, &promise);
            try {
              database::mysql_result res(promise.get_future().get());

              while (!_should_exit && ms.fetch_row(res)) {
                metric_info info;
                info.metric_id = res.value_as_u32(0);
                info.metric_name = res.value_as_str(1);
                info.metric_type = res.value_as_str(2)[0] - '0';
                metrics_to_rebuild.push_back(info);
              }
            } catch (const std::exception& e) {
              throw msg_fmt(
                  "unified_sql: rebuilder: could not fetch metrics of index {}"
                  ": {}",
                  index_id, e.what());
            }
          }

          // Browse metrics to rebuild.
          while (!_should_exit && !metrics_to_rebuild.empty()) {
            metric_info& info(metrics_to_rebuild.front());
            _rebuild_metric(ms, info.metric_id, host_id, service_id,
                            info.metric_name, info.metric_type, check_interval,
                            rrd_len);
            // We need to update the stream for metrics that could
            // change of type.
            _parent->update_metric_info_cache(
                index_id, info.metric_id, info.metric_name, info.metric_type);
            metrics_to_rebuild.pop_front();
          }

          // Rebuild status.
          _rebuild_status(ms, index_id, check_interval, rrd_len);
        } catch (...) {
          // Set index as to-be-rebuilt.
          _set_index_rebuild(ms, index_id, 1);

          // Rethrow exception.
          throw;
        }

        // Set index as rebuilt or to-be-rebuild
        // if we were interrupted.
        _set_index_rebuild(
            ms, index_id,
            (_should_exit ? static_cast<short>(1) : static_cast<short>(0)));

        // Get next index to rebuild.
        _next_index_to_rebuild(info, ms);
      }
    } catch (const std::exception& e) {
      log_v2::sql()->error(
          "unified_sql: rebuilder: Unable to connect to the database: {}",
          e.what());
    } catch (...) {
      log_v2::sql()->error(
          "unified_sql: rebuilder: Unable to connect to the database: unknown "
          "error");
    }
    if (!_should_exit) {
      _timer.expires_after(std::chrono::seconds(_rebuild_check_interval));
      _timer.async_wait(
          std::bind(&rebuilder::_run, this, std::placeholders::_1));
    }
  }
}

/**
 *  Get next index to rebuild.
 *
 *  @param[out] info  Information about the next index to rebuild.
 *                    Zero'd if no index is waiting for rebuild.
 *  @param[in]  db    Database object.
 */
void rebuilder::_next_index_to_rebuild(index_info& info, mysql& ms) {
  const std::string query(
      "SELECT id,host_id,service_id,rrd_retention FROM"
      " index_data WHERE must_be_rebuild='1' LIMIT 1");
  std::promise<database::mysql_result> promise;
  ms.run_query_and_get_result(query, &promise);

  try {
    database::mysql_result res(promise.get_future().get());
    if (ms.fetch_row(res)) {
      info.index_id = res.value_as_u64(0);
      info.host_id = res.value_as_u32(1);
      info.service_id = res.value_as_u32(2);
      info.rrd_retention = res.value_as_u32(3);
      if (!info.rrd_retention)
        info.rrd_retention = _rrd_len;
    } else
      memset(&info, 0, sizeof(info));
  } catch (const std::exception& e) {
    throw msg_fmt(
        "unified_sql: rebuilder: could not fetch index to rebuild: {} ",
        e.what());
  }
}

/**
 *  Rebuild a metric.
 *
 *  @param[in] db           Database object.
 *  @param[in] metric_id    Metric ID.
 *  @param[in] host_id      Id of the host this metric belong to.
 *  @param[in] service_id   Id of the service this metric belong to.
 *  @param[in] metric_name  Metric name.
 *  @param[in] type         Metric type.
 *  @param[in] interval     Host/service check interval.
 *  @param[in] length       Metric RRD length in seconds.
 */
void rebuilder::_rebuild_metric(mysql& ms,
                                uint32_t metric_id,
                                uint32_t host_id,
                                uint32_t service_id,
                                std::string const& metric_name,
                                int16_t metric_type,
                                uint32_t interval,
                                int64_t length) {
  // Log.
  log_v2::sql()->info(
      "unified_sql: rebuilder: rebuilding metric {} (name {}, type {}, "
      "interval "
      "{})",
      metric_id, metric_name, metric_type, interval);

  // Send rebuild start event.
  _send_rebuild_event(false, metric_id, false);

  time_t start{time(nullptr) - length};

  try {
    // Get data.
    std::string query{
        fmt::format("SELECT ctime,value FROM data_bin WHERE id_metric={} AND "
                    "ctime>={} ORDER BY ctime ASC",
                    metric_id, start)};
    std::promise<database::mysql_result> promise;
    ms.run_query_and_get_result(query, &promise);
    log_v2::sql()->debug(
        "unified_sql(rebuilder): rebuild of metric {}: SQL query: \"{}\"",
        metric_id, query);

    try {
      database::mysql_result res(promise.get_future().get());
      while (!_should_exit && ms.fetch_row(res)) {
        std::shared_ptr<storage::metric> entry =
            std::make_shared<storage::metric>(
                host_id, service_id, metric_name, res.value_as_u32(0), interval,
                true, metric_id, length, res.value_as_f64(1), metric_type);
        if (entry->value > FLT_MAX * 0.999)
          entry->value = std::numeric_limits<double>::infinity();
        else if (entry->value < -FLT_MAX * 0.999)
          entry->value = -std::numeric_limits<double>::infinity();
        log_v2::perfdata()->trace(
            "unified_sql(rebuilder): Sending metric with host_id {}, "
            "service_id "
            "{}, metric_name {}, ctime {}, interval {}, is_for_rebuild {}, "
            "metric_id {}, rrd_len {}, value {}, value_type{}",
            host_id, service_id, metric_name, res.value_as_u32(0), interval,
            true, metric_id, length, res.value_as_f64(1), metric_type);

        multiplexing::publisher().write(entry);
      }
    } catch (const std::exception& e) {
      throw msg_fmt(
          "unified_sql: rebuilder: cannot fetch data of metric {} : {}",
          metric_id, e.what());
    }
  } catch (...) {
    // Send rebuild end event.
    _send_rebuild_event(true, metric_id, false);
    // Rethrow exception.
    throw;
  }
  // Send rebuild end event.
  _send_rebuild_event(true, metric_id, false);
}

/**
 *  Rebuild a status.
 *
 *  @param[in] db        Database object.
 *  @param[in] index_id  Index ID.
 *  @param[in] interval  Host/service check interval.
 */
void rebuilder::_rebuild_status(mysql& ms,
                                uint64_t index_id,
                                uint32_t interval,
                                int64_t length) {
  // Log.
  log_v2::sql()->info(
      "unified_sql: rebuilder: rebuilding status {} (interval {})", index_id,
      interval);

  // Send rebuild start event.
  _send_rebuild_event(false, index_id, true);

  time_t start{time(nullptr) - length};

  try {
    // Get data.
    std::string query{
        fmt::format("SELECT d.ctime,d.status FROM metrics AS m JOIN data_bin "
                    "AS d ON m.metric_id=d.id_metric WHERE m.index_id={} AND "
                    "ctime>={} ORDER BY d.ctime ASC",
                    index_id, start)};
    std::promise<database::mysql_result> promise;
    ms.run_query_and_get_result(query, &promise);
    try {
      database::mysql_result res(promise.get_future().get());
      while (!_should_exit && ms.fetch_row(res)) {
        std::shared_ptr<storage::status> entry(
            std::make_shared<storage::status>(res.value_as_u32(0), index_id,
                                              interval, true, _rrd_len,
                                              res.value_as_i32(1)));
        multiplexing::publisher().write(entry);
      }
    } catch (const std::exception& e) {
      throw msg_fmt(
          "unified_sql: rebuilder: cannot fetch data of index {} : {}",
          index_id, e.what());
    }
  } catch (...) {
    // Send rebuild end event.
    _send_rebuild_event(true, index_id, true);

    // Rethrow exception.
    throw;
  }
  // Send rebuild end event.
  _send_rebuild_event(true, index_id, true);
}

/**
 *  Send a rebuild event.
 *
 *  @param[in] end      false if rebuild is starting, true if it is ending.
 *  @param[in] id       Index or metric ID.
 *  @param[in] is_index true for an index ID, false for a metric ID.
 */
void rebuilder::_send_rebuild_event(bool end, uint64_t id, bool is_index) {
  std::shared_ptr<storage::rebuild> rb =
      std::make_shared<storage::rebuild>(end, id, is_index);
  multiplexing::publisher().write(rb);
}

/**
 *  Set index rebuild flag.
 *
 *  @param[in] db        Database object.
 *  @param[in] index_id  Index to update.
 *  @param[in] state     Rebuild state (0, 1 or 2).
 */
void rebuilder::_set_index_rebuild(mysql& ms,
                                   uint64_t index_id,
                                   int16_t state) {
  std::string query(
      fmt::format("UPDATE index_data SET must_be_rebuild='{}' WHERE id={}",
                  state, index_id));
  ms.run_query(query, database::mysql_error::update_index_state, false);
}

/**
 * @brief process a rebuild metrics message.
 *
 * @param d The BBDO message with all the metric ids to rebuild.
 */
void rebuilder::rebuild_rrd_graphs(const std::shared_ptr<io::data>& d) {
  asio::post(_timer.get_executor(), [this, data = d] {
    const bbdo::pb_rebuild_rrd_graphs& ids =
        *static_cast<const bbdo::pb_rebuild_rrd_graphs*>(data.get());

    std::string ids_str{fmt::format("{}", fmt::join(ids.obj.index_id(), ","))};
    log_v2::sql()->debug(
        "Metric rebuild: Rebuild metrics event received for metrics ({})",
        ids_str);

    mysql ms(_db_cfg);
    int32_t conn = ms.choose_best_connection(-1);
    /* Lets' get the metrics to rebuild time in DB */
    std::promise<database::mysql_result> promise;
    struct metric_info {
      std::string metric_name;
      int32_t data_source_type;
      int32_t rrd_retention;
      uint32_t check_interval;
    };
    std::string query{fmt::format(
        "SELECT m.metric_id, m.metric_name, m.data_source_type, "
        "i.rrd_retention, s.check_interval FROM metrics m LEFT JOIN index_data "
        "i ON m.index_id=i.id LEFT JOIN services s ON i.host_id=s.host_id AND "
        "i.service_id=s.service_id WHERE i.id IN ({})",
        ids_str)};
    log_v2::sql()->trace("Metric rebuild: Executed query << {} >>", query);
    ms.run_query_and_get_result(query, &promise, conn);
    std::map<uint64_t, metric_info> ret_inter;
    std::list<int64_t> mids;
    auto start_rebuild = std::make_shared<storage::pb_rebuild_message>();
    start_rebuild->obj.set_state(RebuildMessage_State_START);
    try {
      database::mysql_result res{promise.get_future().get()};
      while (ms.fetch_row(res)) {
        uint64_t mid = res.value_as_u64(0);
        mids.push_back(mid);
        log_v2::sql()->trace("Metric rebuild: metric {} is sent to rebuild",
                             mid);
        start_rebuild->obj.add_metric_id(mid);
        auto ret = ret_inter.emplace(mid, metric_info());
        metric_info& v = ret.first->second;
        v.metric_name = res.value_as_str(1);
        v.data_source_type = res.value_as_i32(2);
        v.rrd_retention = res.value_as_i32(3);
        if (!v.rrd_retention)
          v.rrd_retention = _rrd_len;
        uint32_t rrd_retention = res.value_as_u32(1);
        v.check_interval = res.value_as_f64(4) * _interval_length;
        if (!v.check_interval)
          v.check_interval = 5 * 60;
      }

      ids_str = fmt::format("{}", fmt::join(mids, ","));
      multiplexing::publisher().write(start_rebuild);

      promise = std::promise<database::mysql_result>();
      ms.run_query_and_get_result("SELECT len_storage_mysql FROM config",
                                  &promise, conn);
      int64_t db_retention_day = 0;

      res = promise.get_future().get();
      if (ms.fetch_row(res)) {
        db_retention_day = res.value_as_i64(0);
        log_v2::sql()->debug("Storage retention on Mysql: {} days",
                             db_retention_day);
      }
      if (db_retention_day) {
        /* Let's get the start of this day */
        struct tm tmv;
        std::time_t now{std::time(nullptr)};
        std::time_t start, end;
        if (localtime_r(&now, &tmv)) {
          /* Let's compute the beginning and the end of the first day where the
           * rebuild starts. */
          tmv.tm_sec = tmv.tm_min = tmv.tm_hour = 0;
          tmv.tm_mday -= db_retention_day;
          start = mktime(&tmv);
          while (db_retention_day > 0) {
            tmv.tm_mday++;
            end = mktime(&tmv);
            db_retention_day--;
            std::promise<database::mysql_result> promise;
            std::string query{
                fmt::format("SELECT id_metric,ctime,value FROM data_bin WHERE "
                            "ctime>={} AND "
                            "ctime<{} AND id_metric IN ({}) ORDER BY ctime ASC",
                            start, end, ids_str)};
            log_v2::sql()->trace("Metrics rebuild: Query << {} >> executed",
                                 query);
            ms.run_query_and_get_result(query, &promise, conn);
            auto data_rebuild = std::make_shared<storage::pb_rebuild_message>();
            data_rebuild->obj.set_state(RebuildMessage_State_DATA);
            database::mysql_result res(promise.get_future().get());
            while (ms.fetch_row(res)) {
              uint64_t id_metric = res.value_as_u64(0);
              time_t ctime = res.value_as_u64(1);
              double value = res.value_as_f64(2);
              Point* pt =
                  (*data_rebuild->obj.mutable_timeserie())[id_metric].add_pts();
              pt->set_ctime(ctime);
              pt->set_value(value);
            }
            for (auto it = ret_inter.begin(); it != ret_inter.end(); ++it) {
              auto i = it->second;
              auto& m{(*data_rebuild->obj.mutable_timeserie())[it->first]};
              m.set_check_interval(i.check_interval);
              m.set_data_source_type(i.data_source_type);
              m.set_rrd_retention(i.rrd_retention);
            }
            multiplexing::publisher().write(data_rebuild);
            start = end;
          }
        } else
          throw msg_fmt("Metrics rebuild: Cannot get the date structure.");
      }
    } catch (const std::exception& e) {
      log_v2::sql()->error("Metrics rebuild: Error during metrics rebuild: {}",
                           e.what());
    }
    auto end_rebuild = std::make_shared<storage::pb_rebuild_message>();
    end_rebuild->obj = start_rebuild->obj;
    end_rebuild->obj.set_state(RebuildMessage_State_END);
    multiplexing::publisher().write(end_rebuild);
    log_v2::sql()->debug("Metric rebuild: Rebuild of metrics ({}) finished",
                         ids_str);
  });
}
