/**
 * Copyright 2012-2015,2017,2020-2021 Centreon
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

#include "com/centreon/broker/storage/rebuilder.hh"

#include <fmt/format.h>
#include <cfloat>
#include <cmath>

#include "com/centreon/broker/misc/time.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/sql/mysql_error.hh"
#include "com/centreon/broker/sql/mysql_result.hh"
#include "com/centreon/broker/storage/internal.hh"
#include "com/centreon/broker/storage/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::storage;
using com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] db_cfg                  Database configuration.
 *  @param[in] rrd_length              Length of RRD files.
 *  @param[in] interval_length         Length in seconds of a time unit.
 */
rebuilder::rebuilder(const database_config& db_cfg,
                     uint32_t rrd_length,
                     uint32_t interval_length)
    : _db_cfg(db_cfg),
      _interval_length(interval_length),
      _rrd_len(rrd_length),
      _logger{log_v2::instance().get(log_v2::SQL)} {
  _db_cfg.set_connections_count(1);
  _db_cfg.set_queries_per_transaction(1);
}

/**
 * @brief process a rebuild metrics message.
 *
 * @param d The BBDO message with all the metric ids to rebuild.
 */
void rebuilder::rebuild_graphs(const std::shared_ptr<io::data>& d) {
  asio::post(com::centreon::common::pool::io_context(), [this, data = d] {
    const bbdo::pb_rebuild_graphs& ids =
        *static_cast<const bbdo::pb_rebuild_graphs*>(data.get());

    std::string ids_str{
        fmt::format("{}", fmt::join(ids.obj().index_ids(), ","))};
    _logger->debug(
        "Metric rebuild: Rebuild metrics event received for metrics ({})",
        ids_str);

    mysql ms(_db_cfg, _logger);
    ms.run_query(
        fmt::format(
            "UPDATE index_data SET must_be_rebuild='2' WHERE id IN ({})",
            ids_str),
        database::mysql_error::update_index_state, false);

    int32_t conn = ms.choose_best_connection(-1);
    /* Lets' get the metrics to rebuild time in DB */
    std::promise<database::mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    std::string query{fmt::format(
        "SELECT m.metric_id, m.metric_name, m.data_source_type, "
        "i.rrd_retention, s.check_interval, m.index_id FROM metrics m LEFT "
        "JOIN index_data "
        "i ON m.index_id=i.id LEFT JOIN services s ON i.host_id=s.host_id AND "
        "i.service_id=s.service_id WHERE i.id IN ({})",
        ids_str)};
    _logger->trace("Metric rebuild: Executed query << {} >>", query);
    ms.run_query_and_get_result(query, std::move(promise), conn);
    std::map<uint64_t, metric_info> ret_inter;
    std::list<int64_t> mids;
    auto start_rebuild = std::make_shared<storage::pb_rebuild_message>();
    start_rebuild->mut_obj().set_state(RebuildMessage_State_START);
    try {
      database::mysql_result res{future.get()};
      while (ms.fetch_row(res)) {
        uint64_t mid = res.value_as_u64(0);
        uint64_t index_id = res.value_as_u64(5);
        mids.push_back(mid);
        _logger->trace("Metric rebuild: metric {} is sent to rebuild", mid);
        (*start_rebuild->mut_obj().mutable_metric_to_index_id())[mid] =
            index_id;
        auto ret = ret_inter.emplace(mid, metric_info());
        metric_info& v = ret.first->second;
        v.metric_name = res.value_as_str(1);
        v.data_source_type = res.value_as_i32(2);
        v.rrd_retention = res.value_as_i32(3);
        if (!v.rrd_retention)
          v.rrd_retention = _rrd_len;
        v.check_interval = res.value_as_f64(4) * _interval_length;
        if (!v.check_interval)
          v.check_interval = 5 * 60;
      }

      std::string mids_str{fmt::format("{}", fmt::join(mids, ","))};
      multiplexing::publisher().write(start_rebuild);

      std::promise<database::mysql_result> promise_cfg;
      std::future<database::mysql_result> future_cfg = promise_cfg.get_future();
      ms.run_query_and_get_result(
          "SELECT len_storage_mysql,len_storage_rrd FROM config",
          std::move(promise_cfg), conn);
      int32_t db_retention_day = 0;

      res = future_cfg.get();
      if (ms.fetch_row(res)) {
        db_retention_day = res.value_as_i32(0);
        int32_t db_retention_day1 = res.value_as_i32(1);
        if (db_retention_day1 < db_retention_day)
          db_retention_day = db_retention_day1;
        _logger->debug("Storage retention on RRD: {} days", db_retention_day);
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
          while (db_retention_day >= 0) {
            _logger->trace("Metrics rebuild: db_retention_day = {}",
                           db_retention_day);
            tmv.tm_mday++;
            end = mktime(&tmv);
            db_retention_day--;
            std::promise<database::mysql_result> promise;
            std::future<database::mysql_result> future = promise.get_future();
            std::string query{fmt::format(
                "SELECT id_metric,ctime,value,status FROM data_bin WHERE "
                "ctime>={} AND "
                "ctime<{} AND id_metric IN ({}) ORDER BY ctime ASC",
                start, end, mids_str)};
            _logger->trace("Metrics rebuild: Query << {} >> executed", query);
            ms.run_query_and_get_result(query, std::move(promise), conn);
            auto data_rebuild = std::make_shared<storage::pb_rebuild_message>();
            data_rebuild->mut_obj().set_state(RebuildMessage_State_DATA);
            database::mysql_result res(future.get());
            while (ms.fetch_row(res)) {
              uint64_t id_metric = res.value_as_u64(0);
              time_t ctime = res.value_as_u64(1);
              double value = res.value_as_f64(2);
              uint32_t status = res.value_as_u32(3);
              Point* pt =
                  (*data_rebuild->mut_obj().mutable_timeserie())[id_metric]
                      .add_pts();
              pt->set_ctime(ctime);
              pt->set_value(value);
              pt->set_status(status);
            }
            for (auto it = ret_inter.begin(); it != ret_inter.end(); ++it) {
              auto i = it->second;
              auto& m{
                  (*data_rebuild->mut_obj().mutable_timeserie())[it->first]};
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
      _logger->error("Metrics rebuild: Error during metrics rebuild: {}",
                     e.what());
    }
    auto end_rebuild = std::make_shared<storage::pb_rebuild_message>();
    end_rebuild->set_obj(std::move(start_rebuild->mut_obj()));
    end_rebuild->mut_obj().set_state(RebuildMessage_State_END);
    multiplexing::publisher().write(end_rebuild);
    ms.run_query(
        fmt::format(
            "UPDATE index_data SET must_be_rebuild='0' WHERE id IN ({})",
            ids_str),
        database::mysql_error::update_index_state, false);
    _logger->debug(
        "Metric rebuild: Rebuild of metrics from the following indexes ({}) "
        "finished",
        ids_str);
  });
}
