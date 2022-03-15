/*
** Copyright 2021 Centreon
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
#include "com/centreon/broker/unified_sql/stream.hh"

#include <cassert>
#include <cstring>
#include <thread>

#include "bbdo/remove_graph_message.pb.h"
#include "bbdo/storage/index_mapping.hh"
#include "com/centreon/broker/database/mysql_result.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/perfdata.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/stats/center.hh"
#include "com/centreon/broker/unified_sql/internal.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using namespace com::centreon::broker::unified_sql;

const std::array<std::string, 5> stream::metric_type_name{
    "GAUGE", "COUNTER", "DERIVE", "ABSOLUTE", "AUTOMATIC"};

void (stream::*const stream::_neb_processing_table[])(
    const std::shared_ptr<io::data>&) = {
    nullptr,
    &stream::_process_acknowledgement,
    &stream::_process_comment,
    &stream::_process_custom_variable,
    &stream::_process_custom_variable_status,
    &stream::_process_downtime,
    &stream::_process_event_handler,
    &stream::_process_flapping_status,
    &stream::_process_host_check,
    &stream::_process_host_dependency,
    &stream::_process_host_group,
    &stream::_process_host_group_member,
    &stream::_process_host,
    &stream::_process_host_parent,
    &stream::_process_host_status,
    &stream::_process_instance,
    &stream::_process_instance_status,
    &stream::_process_log,
    &stream::_process_module,
    &stream::_process_service_check,
    &stream::_process_service_dependency,
    &stream::_process_service_group,
    &stream::_process_service_group_member,
    &stream::_process_service,
    &stream::_process_service_status,
    &stream::_process_instance_configuration,
    &stream::_process_responsive_instance,
    &stream::_process_pb_service,
    &stream::_process_pb_service_status,
    nullptr,
    &stream::_process_pb_host_status,
    &stream::_process_severity,
    &stream::_process_tag,
    &stream::_process_pb_service_status_check_result,
};

stream::stream(const database_config& dbcfg,
               uint32_t rrd_len,
               uint32_t interval_length,
               uint32_t loop_timeout,
               uint32_t instance_timeout,
               bool store_in_data_bin)
    : io::stream("unified_sql"),
      _state{not_started},
      _processed{0},
      _ack{0},
      _pending_events{0},
      _count{0},
      _loop_timeout{loop_timeout},
      _max_pending_queries(dbcfg.get_queries_per_transaction()),
      _dbcfg{dbcfg},
      _mysql{dbcfg},
      _instance_timeout{instance_timeout},
      _rebuilder{dbcfg, rrd_len ? rrd_len : 15552000, interval_length},
      _store_in_db{store_in_data_bin},
      _rrd_len{rrd_len},
      _interval_length{interval_length},
      _max_perfdata_queries{_max_pending_queries},
      _max_metrics_queries{_max_pending_queries},
      _max_cv_queries{_max_pending_queries},
      _max_log_queries{_max_pending_queries},
      _next_insert_perfdatas{std::time_t(nullptr) + 10},
      _next_update_metrics{std::time_t(nullptr) + 10},
      _next_loop_timeout{std::time_t(nullptr) + _loop_timeout},
      _timer{pool::io_context()},
      _stats{stats::center::instance().register_conflict_manager()},
      _oldest_timestamp{std::numeric_limits<time_t>::max()} {
  log_v2::sql()->debug("unified sql: stream class instanciation");
  stats::center::instance().execute([
    stats = _stats, loop_timeout = _loop_timeout,
    max_queries = _max_pending_queries
  ] {
    stats->set_loop_timeout(loop_timeout);
    stats->set_max_pending_events(max_queries);
  });
  _state = running;
  _action.resize(_mysql.connections_count());

  try {
    _load_caches();
  } catch (const std::exception& e) {
    log_v2::sql()->error("error while loading caches: {}", e.what());
    throw;
  }
  _timer.expires_after(std::chrono::minutes(5));
  _timer.async_wait(
      std::bind(&stream::_check_deleted_index, this, std::placeholders::_1));
  log_v2::sql()->info("Unified sql stream running");
}

stream::~stream() noexcept {
  log_v2::sql()->debug("unified sql: stream destruction");
}

void stream::_load_deleted_instances() {
  _cache_deleted_instance_id.clear();
  std::string query{"SELECT instance_id FROM instances WHERE deleted=1"};
  std::promise<mysql_result> promise;
  _mysql.run_query_and_get_result(query, &promise);
  try {
    mysql_result res(promise.get_future().get());
    while (_mysql.fetch_row(res))
      _cache_deleted_instance_id.insert(res.value_as_u32(0));
  } catch (std::exception const& e) {
    throw msg_fmt("could not get list of deleted instances: {}", e.what());
  }
}

void stream::_load_caches() {
  // Fill index cache.

  /* get deleted cache of instance ids => _cache_deleted_instance_id */
  _load_deleted_instances();

  /* get all outdated instances from the database => _stored_timestamps */
  {
    std::string query{"SELECT instance_id FROM instances WHERE outdated=TRUE"};
    std::promise<mysql_result> promise;
    _mysql.run_query_and_get_result(query, &promise);
    try {
      mysql_result res(promise.get_future().get());
      while (_mysql.fetch_row(res)) {
        uint32_t instance_id = res.value_as_i32(0);
        _stored_timestamps.insert(
            {instance_id,
             stored_timestamp(instance_id, stored_timestamp::unresponsive)});
        stored_timestamp& ts = _stored_timestamps[instance_id];
        ts.set_timestamp(timestamp(std::numeric_limits<time_t>::max()));
      }
    } catch (std::exception const& e) {
      throw msg_fmt(
          "unified sql: could not get the list of outdated instances: {}",
          e.what());
    }
  }

  /* index_data => _index_cache */
  {
    // Execute query.
    std::promise<database::mysql_result> promise;
    _mysql.run_query_and_get_result(
        "SELECT "
        "id,host_id,service_id,host_name,rrd_retention,service_description,"
        "special,locked FROM index_data",
        &promise);
    try {
      database::mysql_result res(promise.get_future().get());

      // Loop through result set.
      while (_mysql.fetch_row(res)) {
        index_info info{.host_name = res.value_as_str(3),
                        .index_id = res.value_as_u64(0),
                        .locked = res.value_as_bool(7),
                        .rrd_retention = res.value_as_u32(4)
                                             ? res.value_as_u32(4)
                                             : _rrd_len,
                        .service_description = res.value_as_str(5),
                        .special = res.value_as_u32(6) == 2};
        uint32_t host_id(res.value_as_u32(1));
        uint32_t service_id(res.value_as_u32(2));
        log_v2::perfdata()->debug(
            "unified_sql: loaded index {} of ({}, {}) with rrd_len={}",
            info.index_id, host_id, service_id, info.rrd_retention);
        _index_cache[{host_id, service_id}] = std::move(info);

        // Create the metric mapping.
        auto im{std::make_shared<storage::index_mapping>(info.index_id, host_id,
                                                         service_id)};
        multiplexing::publisher pblshr;
        pblshr.write(im);
      }
    } catch (std::exception const& e) {
      throw msg_fmt("unified_sql: could not fetch index list from data DB: {}",
                    e.what());
    }
  }

  /* hosts => _cache_host_instance */
  {
    _cache_host_instance.clear();

    std::promise<mysql_result> promise;
    _mysql.run_query_and_get_result("SELECT host_id,instance_id FROM hosts",
                                    &promise);

    try {
      mysql_result res(promise.get_future().get());
      while (_mysql.fetch_row(res))
        _cache_host_instance[res.value_as_u32(0)] = res.value_as_u32(1);
    } catch (std::exception const& e) {
      throw msg_fmt("SQL: could not get the list of host/instance pairs: {}",
                    e.what());
    }
  }

  /* hostgroups => _hostgroup_cache */
  {
    _hostgroup_cache.clear();

    std::promise<mysql_result> promise;
    _mysql.run_query_and_get_result("SELECT hostgroup_id FROM hostgroups",
                                    &promise);

    try {
      mysql_result res(promise.get_future().get());
      while (_mysql.fetch_row(res))
        _hostgroup_cache.insert(res.value_as_u32(0));
    } catch (std::exception const& e) {
      throw msg_fmt("SQL: could not get the list of hostgroups id: {}",
                    e.what());
    }
  }

  /* servicegroups => _servicegroup_cache */
  {
    _servicegroup_cache.clear();

    std::promise<mysql_result> promise;
    _mysql.run_query_and_get_result("SELECT servicegroup_id FROM servicegroups",
                                    &promise);

    try {
      mysql_result res(promise.get_future().get());
      while (_mysql.fetch_row(res))
        _servicegroup_cache.insert(res.value_as_u32(0));
    } catch (std::exception const& e) {
      throw msg_fmt("SQL: could not get the list of servicegroups id: {}",
                    e.what());
    }
  }

  _cache_svc_cmd.clear();
  _cache_hst_cmd.clear();

  /* metrics => _metric_cache */
  {
    std::lock_guard<std::mutex> lock(_metric_cache_m);
    _metric_cache.clear();
    {
      std::lock_guard<std::mutex> lck(_queues_m);
      _metrics.clear();
    }

    std::promise<mysql_result> promise;
    _mysql.run_query_and_get_result(
        "SELECT "
        "metric_id,index_id,metric_name,unit_name,warn,warn_low,"
        "warn_threshold_mode,crit,crit_low,crit_threshold_mode,min,max,"
        "current_value,data_source_type FROM metrics",
        &promise);

    try {
      mysql_result res{promise.get_future().get()};
      while (_mysql.fetch_row(res)) {
        metric_info info;
        info.metric_id = res.value_as_u32(0);
        info.locked = false;
        info.unit_name = res.value_as_str(3);
        info.warn = res.value_as_f32(4);
        info.warn_low = res.value_as_f32(5);
        info.warn_mode = res.value_as_i32(6);
        info.crit = res.value_as_f32(7);
        info.crit_low = res.value_as_f32(8);
        info.crit_mode = res.value_as_i32(9);
        info.min = res.value_as_f32(10);
        info.max = res.value_as_f32(11);
        info.value = res.value_as_f32(12);
        info.type = res.value_as_str(13)[0] - '0';
        info.metric_mapping_sent = false;
        _metric_cache[{res.value_as_u64(1), res.value_as_str(2)}] = info;
      }
    } catch (std::exception const& e) {
      throw msg_fmt("unified sql: could not get the list of metrics: {}",
                    e.what());
    }
  }
}

void stream::update_metric_info_cache(uint64_t index_id,
                                      uint32_t metric_id,
                                      std::string const& metric_name,
                                      short metric_type) {
  auto it = _metric_cache.find({index_id, metric_name});
  if (it != _metric_cache.end()) {
    log_v2::perfdata()->info(
        "unified sql: updating metric '{}' of id {} at index {} to "
        "metric_type {}",
        metric_name, metric_id, index_id, metric_type_name[metric_type]);
    std::lock_guard<std::mutex> lock(_metric_cache_m);
    it->second.type = metric_type;
    if (it->second.metric_id != metric_id) {
      it->second.metric_id = metric_id;
      // We need to repopulate a new metric_mapping
      it->second.metric_mapping_sent = false;
    }
  }
}

/**
 *  Take a look if a given action is done on a mysql connection. If it is
 *  done, the method waits for tasks on this connection to be finished and
 *  clear the flag.
 *  In case of a conn < 0, the methods checks all the connections.
 *
 * @param conn The connection number or a negative number to check all the
 *             connections
 * @param action An action.
 */
void stream::_finish_action(int32_t conn, uint32_t action) {
  if (conn < 0) {
    for (std::size_t i = 0; i < _action.size(); i++) {
      if (_action[i] & action) {
        _mysql.commit(i);
        _action[i] = actions::none;
      }
    }
  } else if (_action[conn] & action) {
    _mysql.commit(conn);
    _action[conn] = actions::none;
  }
}

/**
 *  The main goal of this method is to commit queries sent to the db.
 *  When the commit is done (all the connections commit), we count how
 *  many events can be acknowledged. So we can also update the number of pending
 *  events.
 */
void stream::_finish_actions() {
  log_v2::sql()->trace("unified sql: finish actions");
  _mysql.commit();
  for (uint32_t& v : _action)
    v = actions::none;
  _ack += _processed;
  _processed = 0;
  log_v2::sql()->trace("finish actions processed = {}", _processed);
}

/**
 *  Add an action on the connection conn in the list of current actions.
 *  If conn < 0, the action is added to all the connections.
 *
 * @param conn The connection number or a negative number to add to all the
 *             connections
 * @param action An action.
 */
void stream::_add_action(int32_t conn, actions action) {
  if (conn < 0) {
    for (uint32_t& v : _action)
      v |= action;
  } else
    _action[conn] |= action;
}

/**
 * @brief Returns statistics about the stream. Those statistics
 * are stored directly in a json tree.
 *
 * @return A nlohmann::json with the statistics.
 */
void stream::statistics(nlohmann::json& tree) const {
  size_t perfdata;
  size_t sz_metrics;
  size_t sz_logs;
  size_t sz_cv;
  size_t sz_cvs;
  size_t count;
  {
    std::lock_guard<std::mutex> lck(_queues_m);
    perfdata = _perfdata_queue.size();
    sz_metrics = _metrics.size();
    sz_logs = _log_queue.size();
    sz_cv = _cv_queue.size();
    sz_cvs = _cvs_queue.size();
    count = _count;
  }

  tree["cv events"] = static_cast<int32_t>(sz_cv);
  tree["cvs events"] = static_cast<int32_t>(sz_cvs);
  tree["logs events"] = static_cast<int32_t>(sz_logs);
  tree["loop timeout"] = static_cast<int32_t>(_loop_timeout);
  tree["max pending events"] = static_cast<int32_t>(_max_pending_queries);
  tree["max perfdata events"] = static_cast<int32_t>(_max_perfdata_queries);
  tree["metrics events"] = static_cast<int32_t>(sz_metrics);
  tree["pending_events"] = static_cast<int32_t>(_pending_events);
  tree["count"] = static_cast<int32_t>(count);
  tree["perfdata events"] = static_cast<int32_t>(perfdata);
  tree["processed_events"] = static_cast<int32_t>(_processed);
}

int32_t stream::write(const std::shared_ptr<io::data>& data) {
  ++_pending_events;
  assert(data);

  log_v2::sql()->trace("unified sql: write event category:{}, element:{}",
                       category_of_type(data->type()),
                       element_of_type(data->type()));

  uint32_t type = data->type();
  uint16_t cat = category_of_type(type);
  uint16_t elem = element_of_type(type);
  if (cat == io::neb) {
    (this->*(_neb_processing_table[elem]))(data);
    if (type == neb::pb_service::static_type())
      _unified_sql_process_pb_service_status(data);
    if (type == neb::service_status::static_type())
      _unified_sql_process_service_status(data);
  } else if (type == make_type(io::bbdo, bbdo::de_rebuild_rrd_graphs))
    _rebuilder.rebuild_rrd_graphs(data);
  else if (type == make_type(io::bbdo, bbdo::de_remove_graphs))
    remove_graphs(data);
  else {
    log_v2::sql()->trace(
        "unified sql: event of type {} thrown away ; no need to store it in "
        "the database.",
        type);
  }
  _processed++;
  _count++;

  time_t now = std::time(nullptr);
  if (now >= _next_loop_timeout || _count >= _max_pending_queries) {
    _count = 0;
    _next_loop_timeout = now + 10;
    _finish_actions();
  }

  size_t sz_perfdatas;
  size_t sz_metrics;
  size_t sz_cv, sz_cvs;
  size_t sz_logs;
  {
    std::lock_guard<std::mutex> lck(_queues_m);
    sz_perfdatas = _perfdata_queue.size();
    sz_metrics = _metrics.size();
    sz_cv = _cv_queue.size();
    sz_cvs = _cvs_queue.size();
    sz_logs = _log_queue.size();
  }

  if (now >= _next_insert_perfdatas || sz_perfdatas >= _max_perfdata_queries) {
    _next_insert_perfdatas = now + 10;
    asio::post(pool::instance().io_context(),
               std::bind(&stream::_insert_perfdatas, this));
  }

  if (now >= _next_update_metrics || sz_metrics >= _max_metrics_queries) {
    _next_update_metrics = now + 10;
    asio::post(pool::instance().io_context(),
               std::bind(&stream::_update_metrics, this));
  }

  if (now >= _next_update_cv || sz_cv >= _max_cv_queries ||
      sz_cvs >= _max_cv_queries) {
    _next_update_cv = now + 10;
    asio::post(pool::instance().io_context(),
               std::bind(&stream::_update_customvariables, this));
  }

  if (now >= _next_insert_logs || sz_logs >= _max_log_queries) {
    _next_insert_logs = now + 10;
    asio::post(pool::instance().io_context(),
               std::bind(&stream::_insert_logs, this));
  }

  int32_t retval = _ack;
  _ack -= retval;

  _pending_events -= retval;
  return retval;
}

/**
 * @brief Flush the stream.
 *
 * @return Number of acknowledged events.
 */
int32_t stream::flush() {
  if (!_ack)
    _finish_actions();
  int32_t retval = _ack;
  _ack -= retval;
  _pending_events -= retval;
  // Event acknowledgement.
  log_v2::sql()->debug("SQL: {} / {} events acknowledged", retval,
                       _pending_events);
  return retval;
}

/**
 * @brief Read from the database.
 *
 * @param d cleared.
 * @param deadline timeout.
 *
 * @return This method throws shutdown exception.
 */
bool stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  (void)deadline;
  d.reset();
  throw broker::exceptions::shutdown("cannot read from a unified sql stream");
  return true;
}

/**
 * @brief The stop() internal function that stops the stream thread and
 * returns last events to ack.
 *
 * @return the number of events to ack.
 */
int32_t stream::stop() {
  flush();

  /* Let's return how many events to ack */
  int32_t retval = _ack;
  _ack -= retval;
  return retval;
}

/**
 * @brief process a remove graphs message.
 *
 * @param d The BBDO message with all the metrics/indexes to remove.
 */
void stream::remove_graphs(const std::shared_ptr<io::data>& d) {
  asio::post(pool::instance().io_context(), [ this, data = d ] {
    mysql ms(_dbcfg);
    const bbdo::pb_remove_graphs& ids =
        *static_cast<const bbdo::pb_remove_graphs*>(data.get());

    std::promise<database::mysql_result> promise;
    int32_t conn = ms.choose_best_connection(-1);
    std::set<uint64_t> indexes_to_delete;
    std::set<uint64_t> metrics_to_delete;
    try {
      if (!ids.obj().index_ids().empty()) {
        ms.run_query_and_get_result(
            fmt::format("SELECT i.id,m.metric_id, m.metric_name,i.host_id,"
                        "i.service_id FROM index_data i LEFT JOIN metrics m ON "
                        "i.id=m.index_id WHERE i.id IN ({})",
                        fmt::join(ids.obj().index_ids(), ",")),
            &promise, conn);
        database::mysql_result res(promise.get_future().get());

        std::lock_guard<std::mutex> lock(_metric_cache_m);
        while (ms.fetch_row(res)) {
          indexes_to_delete.insert(res.value_as_u64(0));
          uint64_t mid = res.value_as_u64(1);
          if (mid)
            metrics_to_delete.insert(mid);

          _metric_cache.erase({res.value_as_u64(0), res.value_as_str(2)});
          _index_cache.erase({res.value_as_u32(3), res.value_as_u32(4)});
        }
      }

      if (!ids.obj().metric_ids().empty()) {
        promise = std::promise<database::mysql_result>();

        ms.run_query_and_get_result(
            fmt::format("SELECT index_id,metric_id,metric_name FROM metrics "
                        "WHERE metric_id IN ({})",
                        fmt::join(ids.obj().metric_ids(), ",")),
            &promise, conn);
        database::mysql_result res(promise.get_future().get());

        std::lock_guard<std::mutex> lock(_metric_cache_m);
        while (ms.fetch_row(res)) {
          metrics_to_delete.insert(res.value_as_u64(1));
          _metric_cache.erase({res.value_as_u64(0), res.value_as_str(2)});
        }
      }
    } catch (const std::exception& e) {
      log_v2::sql()->error(
          "could not query index / metrics table(s) to get index to delete: "
          "{} ",
          e.what());
    }

    std::string mids_str{fmt::format("{}", fmt::join(metrics_to_delete, ","))};
    if (!metrics_to_delete.empty()) {
      log_v2::sql()->info("metrics {} erased from database", mids_str);
      ms.run_query(
          fmt::format("DELETE FROM metrics WHERE metric_id in ({})", mids_str),
          database::mysql_error::delete_metric, false);
    }
    std::string ids_str{fmt::format("{}", fmt::join(indexes_to_delete, ","))};
    if (!indexes_to_delete.empty()) {
      log_v2::sql()->info("indexes {} erased from database", ids_str);
      ms.run_query(
          fmt::format("DELETE FROM index_data WHERE id in ({})", ids_str),
          database::mysql_error::delete_index, false);
    }

    if (!metrics_to_delete.empty() || !indexes_to_delete.empty()) {
      auto rmg{std::make_shared<storage::pb_remove_graph_message>()};
      for (uint64_t i : metrics_to_delete)
        rmg->mut_obj().add_metric_ids(i);
      for (uint64_t i : indexes_to_delete)
        rmg->mut_obj().add_index_ids(i);
      multiplexing::publisher().write(rmg);
    } else
      log_v2::sql()->info(
          "metrics {} and indexes {} do not appear in the storage database",
          mids_str, ids_str);
  });
}
