/**
 * Copyright 2021-2024 Centreon
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
#include "com/centreon/broker/unified_sql/stream.hh"

#include <absl/strings/str_split.h>

#include "bbdo/storage/index_mapping.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/unified_sql/internal.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using namespace com::centreon::broker::unified_sql;
using log_v2 = com::centreon::common::log_v2::log_v2;

const std::string stream::_index_data_insert_request(
    "INSERT INTO index_data "
    "(host_id,host_name,service_id,service_description,"
    "check_interval, must_be_rebuild,"
    "special) VALUES (?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE "
    "host_name=VALUES(host_name), "
    "service_description=VALUES(service_description), "
    "check_interval=VALUES(check_interval), special=VALUES(special)");

const std::array<std::string, 5> stream::metric_type_name{
    "GAUGE", "COUNTER", "DERIVE", "ABSOLUTE", "AUTOMATIC"};

const std::array<int, 5> stream::hst_ordered_status{0, 4, 2, 0, 1};
const std::array<int, 5> stream::svc_ordered_status{0, 3, 4, 2, 1};

static constexpr int32_t queue_timer_duration = 10;

constexpr void (stream::*const stream::neb_processing_table[])(
    const std::shared_ptr<io::data>&) = {
    nullptr,
    &stream::_process_acknowledgement,
    &stream::_process_comment,
    &stream::_process_custom_variable,
    &stream::_process_custom_variable_status,
    &stream::_process_downtime,
    nullptr,
    nullptr,
    &stream::_process_host_check,
    nullptr,
    &stream::_process_host_group,
    &stream::_process_host_group_member,
    &stream::_process_host,
    &stream::_process_host_parent,
    &stream::_process_host_status,
    &stream::_process_instance,
    &stream::_process_instance_status,
    &stream::_process_log,
    nullptr,
    &stream::_process_service_check,
    nullptr,
    &stream::_process_service_group,
    &stream::_process_service_group_member,
    &stream::_process_service,
    &stream::_process_service_status,
    nullptr,
    &stream::_process_responsive_instance,
    &stream::_process_pb_service,
    &stream::_process_pb_adaptive_service,
    &stream::_process_pb_service_status,
    &stream::_process_pb_host,
    &stream::_process_pb_adaptive_host,
    &stream::_process_pb_host_status,
    &stream::_process_severity,
    &stream::_process_tag,
    &stream::_process_pb_comment,
    &stream::_process_pb_downtime,
    &stream::_process_pb_custom_variable,
    &stream::_process_pb_custom_variable_status,
    &stream::_process_pb_host_check,
    &stream::_process_pb_service_check,
    &stream::_process_pb_log,
    &stream::_process_pb_instance_status,
    &stream::_process_pb_global_diff_state,
    &stream::_process_pb_instance,
    &stream::_process_pb_acknowledgement,
    &stream::_process_pb_responsive_instance,
    nullptr,
    nullptr,
    &stream::_process_pb_host_group,
    &stream::_process_pb_host_group_member,
    &stream::_process_pb_service_group,
    &stream::_process_pb_service_group_member,
    &stream::_process_pb_host_parent,
    &stream::_process_pb_instance_configuration,
    &stream::_process_pb_adaptive_service_status,
    &stream::_process_pb_adaptive_host_status,
    &stream::_process_agent_stats};

constexpr size_t neb_processing_table_size =
    sizeof(stream::neb_processing_table) /
    sizeof(stream::neb_processing_table[0]);

/**
 * @brief this function return a database_config equal to the parameter dbcfg
 * except that the connections_count is equal to 1
 *
 * @param dbcfg  config to copy to the return object
 * @return database_config  copy of the parameter with connections_count = 1
 */
static database_config one_db_connection_config(const database_config& dbcfg) {
  database_config ret(dbcfg);
  ret.set_connections_count(1);
  return ret;
}

stream::stream(const database_config& dbcfg,
               uint32_t rrd_len,
               uint32_t interval_length,
               uint32_t loop_timeout,
               uint32_t instance_timeout,
               bool store_in_data_bin,
               bool store_in_resources,
               bool store_in_hosts_services)
    : io::stream("unified_sql"),
      _state{not_started},
      _processed{0},
      _ack{0},
      _pending_events{0},
      _count{0},
      _loop_timeout{loop_timeout},
      _max_pending_queries(dbcfg.get_queries_per_transaction()),
      _dbcfg{dbcfg},
      _mysql{one_db_connection_config(dbcfg)},
      _instance_timeout{instance_timeout},
      _rebuilder{dbcfg, rrd_len ? rrd_len : 15552000, interval_length},
      _store_in_db{store_in_data_bin},
      _store_in_resources{store_in_resources},
      _store_in_hosts_services{store_in_hosts_services},
      _rrd_len{rrd_len},
      _interval_length{interval_length},
      _max_perfdata_queries{_max_pending_queries},
      _max_metrics_queries{_max_pending_queries},
      _max_cv_queries{_max_pending_queries},
      _max_log_queries{_max_pending_queries},
      _next_update_metrics{std::time_t(nullptr) + 10},
      _next_loop_timeout{std::time_t(nullptr) + _loop_timeout},
      _queues_timer{com::centreon::common::pool::io_context()},
      _stop_check_queues{false},
      _check_queues_stopped{false},
      _center{config::applier::state::instance().center()},
      _stats{_center->register_conflict_manager()},
      _group_clean_timer{com::centreon::common::pool::io_context()},
      _loop_timer{com::centreon::common::pool::io_context()},
      _logger_sql{log_v2::instance().get(log_v2::SQL)},
      _logger_sto{log_v2::instance().get(log_v2::PERFDATA)},
      _cv(queue_timer_duration,
          _max_pending_queries,
          "INSERT INTO customvariables "
          "(name,host_id,service_id,default_value,modified,type,update_time,"
          "value) VALUES {} "
          " ON DUPLICATE KEY UPDATE "
          "default_value=VALUES(default_VALUE),modified=VALUES(modified),type="
          "VALUES(type),update_time=VALUES(update_time),value=VALUES(value)",
          _logger_sql),
      _cvs(queue_timer_duration,
           _max_pending_queries,
           "INSERT INTO customvariables "
           "(name,host_id,service_id,modified,update_time,value) VALUES {} "
           " ON DUPLICATE KEY UPDATE "
           "modified=VALUES(modified),update_time=VALUES(update_time),value="
           "VALUES(value)",
           _logger_sql),
      _oldest_timestamp{std::numeric_limits<time_t>::max()} {
  SPDLOG_LOGGER_DEBUG(_logger_sql, "unified sql: stream class instanciation");

  // dedicated connections for data_bin and logs?
  unsigned nb_dedicated_connection = 0;
  switch (dbcfg.get_connections_count()) {
    case 1:  // only one connection =>data_bin and logs are filled with the only
             // connection
      break;
    case 2:
      nb_dedicated_connection = 1;
      break;
    default:
      nb_dedicated_connection = store_in_data_bin ? 2 : 1;
      break;
  }

  if (nb_dedicated_connection > 0) {
    SPDLOG_LOGGER_INFO(
        _logger_sql,
        "use of {} dedicated connection for logs and data_bin tables",
        nb_dedicated_connection);
    database_config dedicated_cfg(dbcfg);
    dedicated_cfg.set_category(
        database_config::DATA_BIN_LOGS);  // not shared with bam connection
    dedicated_cfg.set_queries_per_transaction(1);
    dedicated_cfg.set_connections_count(nb_dedicated_connection);
    _dedicated_connections = std::make_unique<mysql>(dedicated_cfg);
  }

  _center->execute([stats = _stats, loop_timeout = _loop_timeout,
                    max_queries = _max_pending_queries] {
    stats->set_loop_timeout(loop_timeout);
    stats->set_max_pending_events(max_queries);
  });
  _state = running;
  _action.resize(_mysql.connections_count());

  _bulk_prepared_statement = _mysql.support_bulk_statement();
  _logger_sql->info("Unified sql stream connected to '{}' Server",
                    _mysql.get_server_version());

  try {
    _init_statements();
    _load_caches();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger_sql, "error while loading caches: {}",
                        e.what());
    throw;
  }
  absl::MutexLock l(&_timer_m);
  _queues_timer.expires_after(std::chrono::seconds(queue_timer_duration));
  _queues_timer.async_wait([this](const boost::system::error_code& err) {
    absl::ReaderMutexLock lck(&_barrier_timer_m);
    _check_queues(err);
  });
  _start_loop_timer();
  SPDLOG_LOGGER_INFO(_logger_sql, "Unified sql stream running loop_interval={}",
                     _loop_timeout);
}

stream::~stream() noexcept {
  {
    absl::MutexLock l(&_timer_m);
    _group_clean_timer.cancel();
    _queues_timer.cancel();
    _loop_timer.cancel();
  }
  /* Let's wait a little if one of the timers is working during the cancellation
   */
  absl::MutexLock lck(&_barrier_timer_m);
  /* If there are data to write, we write them, so we force their readyness. */
  if (_hscr_bind)
    _hscr_bind->force_ready();
  if (_sscr_bind)
    _sscr_bind->force_ready();
  if (_hscr_resources_bind)
    _hscr_resources_bind->force_ready();
  if (_sscr_resources_bind)
    _sscr_resources_bind->force_ready();
  if (_perfdata_query)
    _perfdata_query->force_ready();
  _cv.force_ready();
  _cvs.force_ready();
  if (_downtimes)
    _downtimes->force_ready();
  if (_comments)
    _comments->force_ready();
  if (_logs)
    _logs->force_ready();
  boost::system::error_code ec;
  _check_queues(ec);
  SPDLOG_LOGGER_DEBUG(_logger_sql, "unified sql: stream destruction");
}

void stream::_load_deleted_instances() {
  _cache_deleted_instance_id.clear();
  std::string query{"SELECT instance_id FROM instances WHERE deleted=1"};
  std::promise<mysql_result> promise;
  std::future<mysql_result> future = promise.get_future();
  _mysql.run_query_and_get_result(query, std::move(promise));
  try {
    mysql_result res(future.get());
    while (_mysql.fetch_row(res)) {
      int32_t instance_id = res.value_as_i32(0);
      if (instance_id <= 0)
        SPDLOG_LOGGER_ERROR(
            _logger_sql,
            "unified_sql: The 'instances' table contains rows with instance_id "
            "<= 0 ; you should remove them.");
      else
        _cache_deleted_instance_id.insert(instance_id);
    }
  } catch (const std::exception& e) {
    throw msg_fmt("could not get list of deleted instances: {}", e.what());
  }
}

/**
 * @brief Load the unified_sql cache.
 */
void stream::_load_caches() {
  auto cache_ptr = cache::global_cache::instance_ptr();

  // Fill index cache.

  /* get deleted cache of instance ids => _cache_deleted_instance_id */
  _load_deleted_instances();

  std::promise<mysql_result> promise_instance_id;
  std::promise<database::mysql_result> promise_index_data;
  std::promise<mysql_result> promise_hi;
  std::promise<mysql_result> promise_hg;
  std::promise<mysql_result> promise_sg;
  std::promise<mysql_result> promise_metrics;
  std::promise<mysql_result> promise_resource;
  std::promise<mysql_result> promise_severity;
  std::promise<mysql_result> promise_tags;
  std::future<mysql_result> future_instance_id =
      promise_instance_id.get_future();
  std::future<mysql_result> future_index_data = promise_index_data.get_future();
  std::future<mysql_result> future_hi = promise_hi.get_future();
  std::future<mysql_result> future_hg = promise_hg.get_future();
  std::future<mysql_result> future_sg = promise_sg.get_future();
  std::future<mysql_result> future_metrics = promise_metrics.get_future();
  std::future<mysql_result> future_resource = promise_resource.get_future();
  std::future<mysql_result> future_severity = promise_severity.get_future();
  std::future<mysql_result> future_tags = promise_tags.get_future();

  /* get all outdated instances from the database => _stored_timestamps */
  _mysql.run_query_and_get_result(
      "SELECT instance_id FROM instances WHERE outdated=TRUE",
      std::move(promise_instance_id));

  /* index_data => _index_cache */
  _mysql.run_query_and_get_result(
      "SELECT "
      "id,host_id,service_id,host_name,rrd_retention,check_interval,service_"
      "description,"
      "special,locked FROM index_data",
      std::move(promise_index_data));

  /* hosts => _cache_host_instance */
  _mysql.run_query_and_get_result("SELECT host_id,instance_id FROM hosts",
                                  std::move(promise_hi));

  /* hostgroups => _hostgroup_cache */
  _mysql.run_query_and_get_result("SELECT hostgroup_id FROM hostgroups",
                                  std::move(promise_hg));

  /* servicegroups => _servicegroup_cache */
  _mysql.run_query_and_get_result("SELECT servicegroup_id FROM servicegroups",
                                  std::move(promise_sg));

  /* metrics => _metric_cache */
  _mysql.run_query_and_get_result(
      "SELECT "
      "metric_id,index_id,metric_name,unit_name,warn,warn_low,"
      "warn_threshold_mode,crit,crit_low,crit_threshold_mode,min,max,"
      "current_value,data_source_type FROM metrics",
      std::move(promise_metrics));

  /* resources => _resources_cache */
  _mysql.run_query_and_get_result(
      "SELECT resource_id, id, parent_id FROM resources",
      std::move(promise_resource));

  /* severities => _severity_cache */
  _mysql.run_query_and_get_result(
      "SELECT severity_id, id, type FROM severities",
      std::move(promise_severity));

  /* tags => _tags_cache */
  _mysql.run_query_and_get_result("SELECT tag_id, id, type FROM tags",
                                  std::move(promise_tags));

  /* get all outdated instances from the database => _stored_timestamps */
  try {
    mysql_result res(future_instance_id.get());
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

  /* index_data => _index_cache */
  try {
    database::mysql_result res(future_index_data.get());

    auto bbdo = config::applier::state::instance().get_bbdo_version();
    multiplexing::publisher pblshr;

    // Loop through result set.
    while (_mysql.fetch_row(res)) {
      index_info info{
          .index_id = res.value_as_u64(0),
          .host_name = res.value_as_str(3),
          .service_description = res.value_as_str(6),
          .rrd_retention = res.value_as_u32(4) ? res.value_as_u32(4) : _rrd_len,
          .interval = res.value_as_u32(5),
          .special = res.value_as_bool(7),
          .locked = res.value_as_bool(8),
      };
      int32_t host_id = res.value_as_i32(1);
      int32_t service_id = res.value_as_i32(2);
      if (host_id <= 0 || service_id <= 0) {
        if (host_id <= 0)
          SPDLOG_LOGGER_ERROR(
              _logger_sql,
              "unified_sql: the 'index_data' table contains rows with host_id "
              "<= "
              "0, you should remove them.");
        if (service_id <= 0)
          SPDLOG_LOGGER_ERROR(
              _logger_sql,
              "unified_sql: the 'index_data' table contains rows with "
              "service_id "
              "<= 0, you should remove them.");
      } else {
        _logger_sto->debug(
            "unified_sql: loaded index {} of ({}, {}) with rrd_len={}",
            info.index_id, host_id, service_id, info.rrd_retention);
        _index_cache[{host_id, service_id}] = std::move(info);

        if (cache_ptr) {
          cache_ptr->set_index_mapping(info.index_id, host_id, service_id);
        }

        // Create the metric mapping.
        if (bbdo.major_v < 3) {
          auto im{std::make_shared<storage::index_mapping>(
              info.index_id, host_id, service_id)};
          pblshr.write(im);
        } else {
          auto im{std::make_shared<storage::pb_index_mapping>()};
          auto& im_obj = im->mut_obj();
          im_obj.set_index_id(info.index_id);
          im_obj.set_host_id(host_id);
          im_obj.set_service_id(service_id);
          pblshr.write(im);
        }
      }
    }
  } catch (std::exception const& e) {
    throw msg_fmt("unified_sql: could not fetch index list from data DB: {}",
                  e.what());
  }

  /* hosts => _cache_host_instance */
  _cache_host_instance.clear();
  try {
    mysql_result res(future_hi.get());
    while (_mysql.fetch_row(res)) {
      int32_t host_id = res.value_as_i32(0);
      int32_t instance_id = res.value_as_i32(1);
      if (host_id > 0 && instance_id > 0)
        _cache_host_instance[host_id] = instance_id;
      else {
        if (host_id <= 0)
          SPDLOG_LOGGER_ERROR(
              _logger_sql,
              "unified_sql: the 'hosts' table contains rows with host_id <= 0, "
              "you should remove them.");
        if (instance_id <= 0)
          SPDLOG_LOGGER_ERROR(
              _logger_sql,
              "unified_sql: the 'hosts' table contains rows with instance_id "
              "<= 0, you should remove them.");
      }
    }
  } catch (std::exception const& e) {
    throw msg_fmt("SQL: could not get the list of host/instance pairs: {}",
                  e.what());
  }

  /* hostgroups => _hostgroup_cache */
  _hostgroup_cache.clear();
  try {
    mysql_result res(future_hg.get());
    while (_mysql.fetch_row(res)) {
      int32_t hg_id = res.value_as_i32(0);
      if (hg_id > 0)
        _hostgroup_cache.insert(hg_id);
      else
        SPDLOG_LOGGER_ERROR(
            _logger_sql,
            "unified_sql: the table 'hostgroups' contains rows with "
            "hostgroup_id <= 0, you should remove them.");
    }
  } catch (const std::exception& e) {
    throw msg_fmt("SQL: could not get the list of hostgroups id: {}", e.what());
  }

  /* servicegroups => _servicegroup_cache */
  _servicegroup_cache.clear();
  try {
    mysql_result res(future_sg.get());
    while (_mysql.fetch_row(res)) {
      int32_t sg_id = res.value_as_i32(0);
      if (sg_id <= 0)
        SPDLOG_LOGGER_ERROR(
            _logger_sql,
            "unified_sql: the 'servicegroups' table contains rows with "
            "servicegroup_id <= 0, you should remove them.");
      else
        _servicegroup_cache.insert(sg_id);
    }
  } catch (std::exception const& e) {
    throw msg_fmt("SQL: could not get the list of servicegroups id: {}",
                  e.what());
  }

  _cache_svc_cmd.clear();
  _cache_hst_cmd.clear();

  /* metrics => _metric_cache */
  {
    std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
    _metric_cache.clear();
    {
      std::lock_guard<std::mutex> lck(_queues_m);
      _metrics.clear();
    }

    try {
      mysql_result res{future_metrics.get()};
      while (_mysql.fetch_row(res)) {
        metric_info info;
        int32_t metric_id = res.value_as_i32(0);

        if (metric_id <= 0)
          SPDLOG_LOGGER_ERROR(
              _logger_sql,
              "unified_sql: the 'metrics' table contains row with metric_id "
              "<= 0 ; you should remove them.");
        else {
          uint64_t index_id = res.value_as_u64(1);
          std::string metric_name = res.value_as_str(2);
          info.metric_id = metric_id;
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
          _metric_cache[{index_id, metric_name}] = info;
          if (cache_ptr) {
            cache_ptr->set_metric_info(metric_id, index_id, metric_name,
                                       info.unit_name, info.min, info.max);
          }
        }
      }
    } catch (std::exception const& e) {
      throw msg_fmt("unified sql: could not get the list of metrics: {}",
                    e.what());
    }

    try {
      mysql_result res{future_resource.get()};
      while (_mysql.fetch_row(res)) {
        _resource_cache[{res.value_as_u64(1), res.value_as_u64(2)}] =
            res.value_as_u64(0);
      }
    } catch (const std::exception& e) {
      throw msg_fmt("unified sql: could not get the list of resources: {}",
                    e.what());
    }

    try {
      mysql_result res{future_severity.get()};
      while (_mysql.fetch_row(res)) {
        _severity_cache[{res.value_as_u64(1),
                         static_cast<uint16_t>(res.value_as_u32(2))}] =
            res.value_as_u64(0);
      }
    } catch (const std::exception& e) {
      throw msg_fmt("unified sql: could not get the list of severities: {}",
                    e.what());
    }

    try {
      mysql_result res{future_tags.get()};
      while (_mysql.fetch_row(res)) {
        _tags_cache[{res.value_as_u64(1),
                     static_cast<uint16_t>(res.value_as_u32(2))}] =
            res.value_as_u64(0);
      }
    } catch (const std::exception& e) {
      throw msg_fmt("unified sql: could not get the list of tags: {}",
                    e.what());
    }
  }
}

void stream::update_metric_info_cache(uint64_t index_id,
                                      uint32_t metric_id,
                                      std::string const& metric_name,
                                      short metric_type) {
  misc::read_lock lck(_metric_cache_m);
  auto it = _metric_cache.find({index_id, metric_name});
  if (it != _metric_cache.end()) {
    _logger_sto->info(
        "unified sql: updating metric '{}' of id {} at index {} to "
        "metric_type {}",
        metric_name, metric_id, index_id, metric_type_name[metric_type]);
    std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
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
  SPDLOG_LOGGER_TRACE(_logger_sql, "unified sql: finish actions");
  _mysql.commit();
  for (uint32_t& v : _action)
    v = actions::none;
  _ack += _processed;
  _processed = 0;
  SPDLOG_LOGGER_TRACE(_logger_sql, "finish actions processed = {}",
                      static_cast<int>(_processed));
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
  size_t perfdata = _perfdata_query->row_count();
  size_t sz_metrics;
  size_t sz_logs = _logs->row_count();
  size_t sz_cv = _cv.size();
  size_t sz_cvs = _cvs.size();
  size_t count;
  {
    std::lock_guard<std::mutex> lck(_queues_m);
    sz_metrics = _metrics.size();
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

  SPDLOG_LOGGER_TRACE(
      _logger_sql, "unified sql: write event category:{}, element:{}",
      category_of_type(data->type()), element_of_type(data->type()));

  uint32_t type = data->type();
  uint16_t cat = category_of_type(type);
  uint16_t elem = element_of_type(type);
  if (cat == io::neb) {
    if (elem < neb_processing_table_size && neb_processing_table[elem]) {
      (this->*(neb_processing_table[elem]))(data);
    } else {
      SPDLOG_LOGGER_ERROR(_logger_sql, "unknown neb event type: {}", elem);
    }
  } else if (cat == io::bbdo) {
    switch (elem) {
      case bbdo::de_rebuild_graphs:
        _rebuilder.rebuild_graphs(data, _logger_sql);
        break;
      case bbdo::de_remove_graphs:
        remove_graphs(data);
        break;
      case bbdo::de_remove_poller:
        SPDLOG_LOGGER_INFO(_logger_sql, "remove poller...");
        remove_poller(data);
        break;
      default:
        SPDLOG_LOGGER_TRACE(
            _logger_sql,
            "unified sql: event of category bbdo and type {} thrown away ; no "
            "need to store it in the database.",
            type);
    }
  } else if (cat == io::local) {
    if (elem == local::de_pb_stop) {
      SPDLOG_LOGGER_INFO(_logger_sql, "poller stopped...");
      process_stop(data);
    }
  } else {
    SPDLOG_LOGGER_TRACE(
        _logger_sql,
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

  int32_t retval = _ack;
  _ack -= retval;

  _pending_events -= retval;
  return retval;
}

class unified_muxer_filter : public multiplexing::muxer_filter {
 public:
  constexpr unified_muxer_filter()
      : multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init()) {
    // first neb table
    for (unsigned neb_index = 0; neb_index < neb_processing_table_size;
         ++neb_index) {
      if (stream::neb_processing_table[neb_index]) {
        _mask[io::neb] |= 1ULL << neb_index;
      }
    }
    // others
    _mask[io::bbdo] |= 1ULL << bbdo::de_rebuild_graphs;
    _mask[io::bbdo] |= 1ULL << bbdo::de_remove_graphs;
    _mask[io::bbdo] |= 1ULL << bbdo::de_remove_poller;
    _mask[io::local] |= 1ULL << local::de_pb_stop;
    _mask[io::extcmd] |= 1ULL << extcmd::de_pb_bench;
  }
};

constexpr unified_muxer_filter _muxer_filter;
constexpr multiplexing::muxer_filter _forbidden_filter =
    multiplexing::muxer_filter(_muxer_filter).reverse();

const multiplexing::muxer_filter& stream::get_muxer_filter() {
  return _muxer_filter;
}

const multiplexing::muxer_filter& stream::get_forbidden_filter() {
  return _forbidden_filter;
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
  SPDLOG_LOGGER_TRACE(_logger_sql, "SQL: {} / {} events acknowledged", retval,
                      static_cast<int32_t>(_pending_events));
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
  _logger_sql->trace("unified_sql::stream stop {}", static_cast<void*>(this));
  int32_t retval = flush();
  /* We give the order to stop the check_queues */
  _stop_check_queues = true;
  /* We wait for the check_queues to be really stopped */
  std::unique_lock<std::mutex> lck(_queues_m);
  if (_queues_cond_var.wait_for(lck, std::chrono::seconds(queue_timer_duration),
                                [this] { return _check_queues_stopped; })) {
    SPDLOG_LOGGER_INFO(_logger_sql, "SQL: stream correctly stopped");
  } else {
    SPDLOG_LOGGER_ERROR(_logger_sql,
                        "SQL: stream queues check still running...");
  }

  return retval;
}

/**
 * @brief Function called when a poller is disconnected from Broker. It cleans
 * hosts/services and instances in the storage database.
 *
 * @param d A pb_stop event with the instance ID.
 */
void stream::process_stop(const std::shared_ptr<io::data>& d) {
  auto& stop = static_cast<local::pb_stop*>(d.get())->obj();
  int32_t conn = _mysql.choose_connection_by_instance(stop.poller_id());
  _finish_action(-1, actions::hosts | actions::acknowledgements |
                         actions::modules | actions::downtimes |
                         actions::comments | actions::servicegroups |
                         actions::hostgroups);

  // Log message.
  _logger_sql->info("unified_sql: Disabling poller (id: {}, running: no)",
                    stop.poller_id());

  // Clean tables.
  clean_tables(stop.poller_id());

  // Processing.
  if (_is_valid_poller(stop.poller_id())) {
    // Prepare queries.
    if (!_instance_insupdate.prepared()) {
      std::string query(fmt::format(
          "UPDATE instances SET end_time={}, running=0 WHERE instance_id={}",
          time(nullptr), stop.poller_id()));
      _mysql.run_query(query, database::mysql_error::clean_hosts_services,
                       conn);
      _add_action(conn, actions::instances);
    }
  }
}

/**
 * @brief process a remove graphs message.
 *
 * @param d The BBDO message with all the metrics/indexes to remove.
 */
void stream::remove_graphs(const std::shared_ptr<io::data>& d) {
  SPDLOG_LOGGER_INFO(_logger_sql, "remove graphs call");
  asio::post(com::centreon::common::pool::instance().io_context(), [this,
                                                                    data = d] {
    mysql ms(_dbcfg);
    bbdo::pb_remove_graphs* ids =
        static_cast<bbdo::pb_remove_graphs*>(data.get());

    std::promise<database::mysql_result> promise;
    std::future<mysql_result> future = promise.get_future();
    int32_t conn = ms.choose_best_connection(-1);
    std::set<uint64_t> indexes_to_delete;
    std::set<uint64_t> metrics_to_delete;
    try {
      if (!ids->obj().index_ids().empty()) {
        std::string query{
            fmt::format("SELECT i.id,m.metric_id, m.metric_name,i.host_id,"
                        "i.service_id FROM index_data i LEFT JOIN metrics m ON "
                        "i.id=m.index_id WHERE i.id IN ({})",
                        fmt::join(ids->obj().index_ids(), ","))};
        ms.run_query_and_get_result(query, std::move(promise), conn);
        database::mysql_result res(future.get());

        std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
        while (ms.fetch_row(res)) {
          indexes_to_delete.insert(res.value_as_u64(0));
          int64_t mid = res.value_as_i32(1);
          int32_t host_id = res.value_as_i32(3);
          int32_t service_id = res.value_as_i32(4);
          if (mid <= 0 || host_id <= 0 || service_id <= 0) {
            if (mid <= 0)
              SPDLOG_LOGGER_ERROR(
                  _logger_sql,
                  "unified_sql: the 'metrics' table contains rows with "
                  "metric_id <= 0 ; you should remove them.");
            if (host_id <= 0)
              SPDLOG_LOGGER_ERROR(_logger_sql,
                                  "unified_sql: the 'metrics' table "
                                  "contains rows with host_id "
                                  "<= 0 ; you should remove them.");
            if (service_id <= 0)
              SPDLOG_LOGGER_ERROR(
                  _logger_sql,
                  "unified_sql: the 'metrics' table contains rows with "
                  "service_id <= 0 ; you should remove them.");
          } else {
            metrics_to_delete.insert(mid);

            _metric_cache.erase({res.value_as_u64(0), res.value_as_str(2)});
            _index_cache.erase({host_id, service_id});
          }
        }
      }

      if (!ids->obj().metric_ids().empty()) {
        promise = std::promise<database::mysql_result>();
        std::future<mysql_result> future = promise.get_future();

        std::string query{
            fmt::format("SELECT index_id,metric_id,metric_name FROM metrics "
                        "WHERE metric_id IN ({})",
                        fmt::join(ids->obj().metric_ids(), ","))};
        ms.run_query_and_get_result(query, std::move(promise), conn);
        database::mysql_result res(future.get());

        std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
        while (ms.fetch_row(res)) {
          int32_t metric_id = res.value_as_i32(1);
          if (metric_id <= 0)
            SPDLOG_LOGGER_ERROR(_logger_sql,
                                "unified_sql: the 'metrics' table contains "
                                "rows with metric_id "
                                "<= 0 ; you should remove them.");
          else {
            metrics_to_delete.insert(metric_id);
            _metric_cache.erase({res.value_as_u64(0), res.value_as_str(2)});
          }
        }
      }
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(_logger_sql,
                          "could not query index / metrics table(s) to get "
                          "index to delete: "
                          "{} ",
                          e.what());
    }

    std::string mids_str{fmt::format("{}", fmt::join(metrics_to_delete, ","))};
    if (!metrics_to_delete.empty()) {
      SPDLOG_LOGGER_INFO(_logger_sql, "metrics {} erased from database",
                         mids_str);
      ms.run_query(
          fmt::format("DELETE FROM metrics WHERE metric_id in ({})", mids_str),
          database::mysql_error::delete_metric);
    }
    std::string ids_str{fmt::format("{}", fmt::join(indexes_to_delete, ","))};
    if (!indexes_to_delete.empty()) {
      SPDLOG_LOGGER_INFO(_logger_sql, "indexes {} erased from database",
                         ids_str);
      ms.run_query(
          fmt::format("DELETE FROM index_data WHERE id in ({})", ids_str),
          database::mysql_error::delete_index);
    }

    if (!metrics_to_delete.empty() || !indexes_to_delete.empty()) {
      auto rmg{std::make_shared<storage::pb_remove_graph_message>()};
      for (uint64_t i : metrics_to_delete)
        rmg->mut_obj().add_metric_ids(i);
      for (uint64_t i : indexes_to_delete)
        rmg->mut_obj().add_index_ids(i);
      SPDLOG_LOGGER_INFO(
          _logger_sql,
          "publishing pb remove graph with {} metrics and {} indexes",
          metrics_to_delete.size(), indexes_to_delete.size());
      multiplexing::publisher().write(rmg);
    } else
      SPDLOG_LOGGER_INFO(
          _logger_sql,
          "metrics {} and indexes {} do not appear in the storage database",
          mids_str, ids_str);
  });
}

/**
 * @brief process a remove poller message.
 *
 * @param d The BBDO message with the name or the id of the poller to
 * remove.
 */
void stream::remove_poller(const std::shared_ptr<io::data>& d) {
  const bbdo::pb_remove_poller& poller =
      *static_cast<const bbdo::pb_remove_poller*>(d.get());

  try {
    std::promise<database::mysql_result> promise;
    std::future<mysql_result> future = promise.get_future();
    int32_t conn = _mysql.choose_best_connection(-1);
    std::list<uint64_t> ids;
    uint32_t count = 0;
    if (poller.obj().has_str()) {
      _mysql.run_query_and_get_result(
          fmt::format("SELECT instance_id from instances WHERE name='{}' AND "
                      "(running=0 OR deleted=1)",
                      poller.obj().str()),
          std::move(promise), conn);
      database::mysql_result res(future.get());

      while (_mysql.fetch_row(res)) {
        count++;
        ids.push_back(res.value_as_u64(0));
      }
      if (count == 0) {
        SPDLOG_LOGGER_WARN(
            _logger_sql,
            "Unable to remove poller '{}', {} running found in the database",
            poller.obj().str(), count == 0 ? "none" : "more than one");
        std::promise<database::mysql_result> promise;
        std::future<mysql_result> future = promise.get_future();
        _mysql.run_query_and_get_result(
            fmt::format("SELECT instance_id from instances WHERE name='{}'",
                        poller.obj().str()),
            std::move(promise), conn);
        database::mysql_result res(future.get());

        while (_mysql.fetch_row(res)) {
          if (!config::applier::state::instance().has_connection_from_poller(
                  res.value_as_u64(0))) {
            SPDLOG_LOGGER_WARN(
                _logger_sql,
                "The poller '{}' id {} is not connected (even if it looks "
                "running or not deleted)",
                poller.obj().str(), res.value_as_u64(0));
            count++;
            ids.push_back(res.value_as_u64(0));
          }
        }
      }
    } else {
      _mysql.run_query_and_get_result(
          fmt::format(
              "SELECT instance_id from instances WHERE instance_id={} AND "
              "(running=0 OR deleted=1)",
              poller.obj().idx()),
          std::move(promise), conn);
      database::mysql_result res(future.get());

      while (_mysql.fetch_row(res)) {
        count++;
        ids.push_back(res.value_as_u64(0));
      }
      if (count == 0) {
        SPDLOG_LOGGER_WARN(
            _logger_sql,
            "Unable to remove poller {}, {} running found in the "
            "database",
            poller.obj().idx(), count == 0 ? "none" : "more than one");
        std::promise<database::mysql_result> promise;
        std::future<mysql_result> future = promise.get_future();
        _mysql.run_query_and_get_result(
            fmt::format("SELECT name from instances WHERE instance_id={}",
                        poller.obj().idx()),
            std::move(promise), conn);
        database::mysql_result res(future.get());

        while (_mysql.fetch_row(res)) {
          if (!config::applier::state::instance().has_connection_from_poller(
                  poller.obj().idx())) {
            SPDLOG_LOGGER_WARN(
                _logger_sql,
                "The poller '{}' id {} is not connected (even if it looks "
                "running or not deleted)",
                res.value_as_str(0), poller.obj().idx());
            count++;
            ids.push_back(poller.obj().idx());
          }
        }
      }
    }

    for (uint64_t id : ids) {
      conn = _mysql.choose_connection_by_instance(id);
      SPDLOG_LOGGER_INFO(_logger_sql, "unified sql: removing poller {}", id);
      _mysql.run_query(
          fmt::format("DELETE FROM instances WHERE instance_id={}", id),
          database::mysql_error::delete_poller, conn);
      SPDLOG_LOGGER_TRACE(_logger_sql, "unified sql: removing poller {} hosts",
                          id);
      _mysql.run_query(
          fmt::format("DELETE FROM hosts WHERE instance_id={}", id),
          database::mysql_error::delete_poller, conn);

      SPDLOG_LOGGER_TRACE(_logger_sql,
                          "unified sql: removing poller {} resources", id);
      _mysql.run_query(
          fmt::format("DELETE FROM resources WHERE poller_id={}", id),
          database::mysql_error::delete_poller, conn);
      _cache_deleted_instance_id.insert(id);
    }
    _clear_instances_cache(ids);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(
        _logger_sql, "Error encountered while removing a poller: {}", e.what());
  }
}

void stream::_clear_instances_cache(const std::list<uint64_t>& ids) {
  for (auto it = _cache_host_instance.begin();
       it != _cache_host_instance.end();) {
    if (std::find(ids.begin(), ids.end(), it->second) != ids.end()) {
      uint64_t host_id = it->first;
      _cache_hst_cmd.erase(host_id);
      for (auto itt = _cache_svc_cmd.begin(); itt != _cache_svc_cmd.end();
           ++itt) {
        if (itt->first.first == host_id) {
          uint64_t svc_id = itt->first.second;
          auto ridx_it = _index_cache.find({host_id, svc_id});
          uint64_t index_id = ridx_it->second.index_id;
          for (auto idx_it = _index_cache.begin(); idx_it != _index_cache.end();
               ++idx_it) {
            if (idx_it->first.first == index_id)
              _index_cache.erase(idx_it);
            std::lock_guard<misc::shared_mutex> lock(_metric_cache_m);
            for (auto metric_it = _metric_cache.begin();
                 metric_it != _metric_cache.end(); ++metric_it) {
              if (metric_it->first.first == index_id)
                _metric_cache.erase(metric_it);
            }
          }
          _index_cache.erase(ridx_it);
          _cache_svc_cmd.erase(itt);

          // resources
          auto res_it = _resource_cache.find({svc_id, host_id});
          if (res_it != _resource_cache.end())
            _resource_cache.erase(res_it);
        }
        auto res_it = _resource_cache.find({host_id, 0});
        if (res_it != _resource_cache.end())
          _resource_cache.erase(res_it);
      }
      it = _cache_host_instance.erase(it);
    } else
      ++it;
  }
}

void stream::update() {
  SPDLOG_LOGGER_INFO(_logger_sql, "unified_sql stream update");
  _check_deleted_index();
  _check_rebuild_index();
}

void stream::_start_loop_timer() {
  _loop_timer.expires_from_now(std::chrono::seconds(_loop_timeout));
  _loop_timer.async_wait([this](const boost::system::error_code& err) {
    if (err) {
      return;
    }
    absl::ReaderMutexLock lck(&_barrier_timer_m);
    _update_hosts_and_services_of_unresponsive_instances();
    {
      absl::MutexLock l(&_timer_m);
      _start_loop_timer();
    }
  });
}

/**
 * @brief Initialize prepared statements when they are accessed throw a bulk
 * bind or directly. It is the case for _hscr_update.
 */
void stream::_init_statements() {
  if (_bulk_prepared_statement) {
    _perfdata_query = std::make_unique<database::bulk_or_multi>(
        _dedicated_connections ? *_dedicated_connections : _mysql,
        "INSERT INTO data_bin (id_metric,ctime,status,value) VALUES (?,?,?,?)",
        _max_perfdata_queries, std::chrono::seconds(queue_timer_duration),
        _max_perfdata_queries);
    _logs = std::make_unique<database::bulk_or_multi>(
        _dedicated_connections ? *_dedicated_connections : _mysql,
        "INSERT INTO logs "
        "(ctime,host_id,service_id,host_name,instance_name,type,msg_type,"
        "notification_cmd,notification_contact,retry,service_description,"
        "status,output) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
        _max_pending_queries, std::chrono::seconds(queue_timer_duration),
        _max_pending_queries);
    _downtimes = std::make_unique<database::bulk_or_multi>(
        _mysql,
        "INSERT INTO downtimes (actual_end_time,actual_start_time,author,"
        "type,deletion_time,duration,end_time,entry_time,"
        "fixed,host_id,instance_id,internal_id,service_id,"
        "start_time,triggered_by,cancelled,started,comment_data) VALUES "
        "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"
        " ON DUPLICATE KEY UPDATE "
        "actual_end_time=GREATEST(COALESCE(actual_end_time,-1),VALUES("
        "actual_end_time)),actual_start_time=COALESCE(actual_start_time,"
        "VALUES(actual_start_time)),author=VALUES(author),cancelled=VALUES("
        "cancelled),comment_data=VALUES(comment_data),deletion_time=VALUES("
        "deletion_time),duration=VALUES(duration),end_time=VALUES(end_time),"
        "fixed=VALUES(fixed),start_time=VALUES(start_time),started=VALUES("
        "started),triggered_by=VALUES(triggered_by), type=VALUES(type)",
        _max_pending_queries, std::chrono::seconds(queue_timer_duration),
        _max_pending_queries);
    _comments = std::make_unique<database::bulk_or_multi>(
        _mysql,
        "INSERT INTO comments "
        "(author, type, data, deletion_time, entry_time, entry_type, "
        "expire_time, expires, host_id, internal_id, persistent, instance_id, "
        "service_id, source)"
        " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        " ON DUPLICATE KEY UPDATE "
        "author=VALUES(author), type=VALUES(type), data=VALUES(data),"
        "deletion_time=VALUES(deletion_time), "
        "entry_type=VALUES(entry_type), expire_time=VALUES(expire_time),"
        "expires=VALUES(expires), persistent=VALUES(persistent),"
        "source=VALUES(source)",
        _max_pending_queries, std::chrono::seconds(queue_timer_duration),
        _max_pending_queries);
  } else {
    _perfdata_query = std::make_unique<database::bulk_or_multi>(
        "INSERT INTO data_bin (id_metric,ctime,status,value) VALUES", "",
        std::chrono::seconds(queue_timer_duration), _max_perfdata_queries);
    _logs = std::make_unique<database::bulk_or_multi>(
        "INSERT INTO logs "
        "(ctime,host_id,service_id,host_name,instance_name,type,msg_type,"
        "notification_cmd,notification_contact,retry,service_description,"
        "status,output) VALUES ",
        "", std::chrono::seconds(queue_timer_duration), _max_pending_queries);
    _downtimes = std::make_unique<database::bulk_or_multi>(
        "INSERT INTO downtimes (actual_end_time,actual_start_time,author,"
        "type,deletion_time,duration,end_time,entry_time,"
        "fixed,host_id,instance_id,internal_id,service_id,"
        "start_time,triggered_by,cancelled,started,comment_data) VALUES ",
        " ON DUPLICATE KEY UPDATE "
        "actual_end_time=GREATEST(COALESCE(actual_end_time,-1),VALUES("
        "actual_end_time)),actual_start_time=COALESCE(actual_start_time,"
        "VALUES(actual_start_time)),author=VALUES(author),cancelled=VALUES("
        "cancelled),comment_data=VALUES(comment_data),deletion_time=VALUES("
        "deletion_time),duration=VALUES(duration),end_time=VALUES(end_time),"
        "fixed=VALUES(fixed),start_time=VALUES(start_time),started=VALUES("
        "started),triggered_by=VALUES(triggered_by), type=VALUES(type)",
        std::chrono::seconds(queue_timer_duration), _max_pending_queries);
    _comments = std::make_unique<database::bulk_or_multi>(
        "INSERT INTO comments "
        "(author, type, data, deletion_time, entry_time, entry_type, "
        "expire_time, expires, host_id, internal_id, persistent, instance_id, "
        "service_id, source)"
        " VALUES",
        " ON DUPLICATE KEY UPDATE "
        "author=VALUES(author), type=VALUES(type), data=VALUES(data),"
        "deletion_time=VALUES(deletion_time), "
        "entry_type=VALUES(entry_type), expire_time=VALUES(expire_time),"
        "expires=VALUES(expires), persistent=VALUES(persistent),"
        "source=VALUES(source)",
        std::chrono::seconds(queue_timer_duration), _max_pending_queries);
  }

  const std::string hscr_query(
      "UPDATE hosts SET "
      "checked=?,"                   // 0: has_been_checked
      "check_type=?,"                // 1: check_type
      "state=?,"                     // 2: current_state
      "state_type=?,"                // 3: state_type
      "last_state_change=?,"         // 4: last_state_change
      "last_hard_state=?,"           // 5: last_hard_state
      "last_hard_state_change=?,"    // 6: last_hard_state_change
      "last_time_up=?,"              // 7: last_time_up
      "last_time_down=?,"            // 8: last_time_down
      "last_time_unreachable=?,"     // 9: last_time_unreachable
      "output=?,"                    // 10: output + '\n' + long_output
      "perfdata=?,"                  // 11: perf_data
      "flapping=?,"                  // 12: is_flapping
      "percent_state_change=?,"      // 13: percent_state_change
      "latency=?,"                   // 14: latency
      "execution_time=?,"            // 15: execution_time
      "last_check=?,"                // 16: last_check
      "next_check=?,"                // 17: next_check
      "should_be_scheduled=?,"       // 18: should_be_scheduled
      "check_attempt=?,"             // 19: current_check_attempt
      "notification_number=?,"       // 20: notification_number
      "no_more_notifications=?,"     // 21: no_more_notifications
      "last_notification=?,"         // 22: last_notification
      "next_host_notification=?,"    // 23: next_notification
      "acknowledged=?,"              // 24: acknowledgement_type != NONE
      "acknowledgement_type=?,"      // 25: acknowledgement_type
      "scheduled_downtime_depth=? "  // 26: downtime_depth
      "WHERE host_id=?"              // 27: host_id
  );

  const std::string sscr_query(
      "UPDATE services SET "
      "checked=?,"                          // 0: has_been_checked
      "check_type=?,"                       // 1: check_type
      "state=?,"                            // 2: current_state
      "state_type=?,"                       // 3: state_type
      "last_state_change=?,"                // 4: last_state_change
      "last_hard_state=?,"                  // 5: last_hard_state
      "last_hard_state_change=?,"           // 6: last_hard_state_change
      "last_time_ok=?,"                     // 7: last_time_ok
      "last_time_warning=?,"                // 8: last_time_warning
      "last_time_critical=?,"               // 9: last_time_critical
      "last_time_unknown=?,"                // 10: last_time_unknown
      "output=?,"                           // 11: output + '\n' + long_output
      "perfdata=?,"                         // 12: perf_data
      "flapping=?,"                         // 13: is_flapping
      "percent_state_change=?,"             // 14: percent_state_change
      "latency=?,"                          // 15: latency
      "execution_time=?,"                   // 16: execution_time
      "last_check=?,"                       // 17: last_check
      "next_check=?,"                       // 18: next_check
      "should_be_scheduled=?,"              // 19: should_be_scheduled
      "check_attempt=?,"                    // 20: current_check_attempt
      "notification_number=?,"              // 21: notification_number
      "no_more_notifications=?,"            // 22: no_more_notifications
      "last_notification=?,"                // 23: last_notification
      "next_notification=?,"                // 24: next_notification
      "acknowledged=?,"                     // 25: acknowledgement_type != NONE
      "acknowledgement_type=?,"             // 26: acknowledgement_type
      "scheduled_downtime_depth=? "         // 27: downtime_depth
      "WHERE host_id=? AND service_id=?");  // 28, 29

  const std::string hscr_resources_query(
      "UPDATE resources SET "
      "status=?,"                     // 0: current_state
      "status_ordered=?,"             // 1: obtained from current_state
      "last_status_change=?,"         // 2: last_state_change
      "in_downtime=?,"                // 3: downtime_depth() > 0
      "acknowledged=?,"               // 4: acknowledgement_type != NONE
      "status_confirmed=?,"           // 5: state_type == HARD
      "check_attempts=?,"             // 6: current_check_attempt
      "has_graph=?,"                  // 7: perfdata != ""
      "last_check_type=?,"            // 8: check_type
      "last_check=?,"                 // 9: last_check
      "output=?,"                     // 10: output
      "flapping=?,"                   // 11: is_flapping
      "percent_state_change=? "       // 12: percent_state_change
      "WHERE id=? AND parent_id=0");  // 13: host_id

  const std::string sscr_resources_query(
      "UPDATE resources SET "
      "status=?,"                     // 0: current_state
      "status_ordered=?,"             // 1: obtained from current_state
      "last_status_change=?,"         // 2: last_state_change
      "in_downtime=?,"                // 3: downtime_depth() > 0
      "acknowledged=?,"               // 4: acknowledgement_type != NONE
      "status_confirmed=?,"           // 5: state_type == HARD
      "check_attempts=?,"             // 6: current_check_attempt
      "has_graph=?,"                  // 7: perfdata != ""
      "last_check_type=?,"            // 8: check_type
      "last_check=?,"                 // 9: last_check
      "output=? ,"                    // 10: output
      "flapping=?,"                   // 11: is_flapping
      "percent_state_change=? "       // 12: percent_state_change
      "WHERE id=? AND parent_id=?");  // 13, 14: service_id and host_id
  if (_store_in_hosts_services) {
    if (_bulk_prepared_statement) {
      auto hu = std::make_unique<database::mysql_bulk_stmt>(hscr_query);
      _mysql.prepare_statement(*hu);
      _hscr_bind = std::make_unique<bulk_bind>(
          _dbcfg.get_connections_count(), dt_queue_timer_duration,
          _max_pending_queries, *hu, _logger_sql);
      _hscr_update = std::move(hu);

      auto su = std::make_unique<database::mysql_bulk_stmt>(sscr_query);
      _mysql.prepare_statement(*su);
      _sscr_bind = std::make_unique<bulk_bind>(
          _dbcfg.get_connections_count(), dt_queue_timer_duration,
          _max_pending_queries, *su, _logger_sql);
      _sscr_update = std::move(su);
    } else {
      _hscr_update = std::make_unique<database::mysql_stmt>(hscr_query);
      _mysql.prepare_statement(*_hscr_update);

      _sscr_update = std::make_unique<database::mysql_stmt>(sscr_query);
      _mysql.prepare_statement(*_sscr_update);
    }
  }
  if (_store_in_resources) {
    if (_bulk_prepared_statement) {
      auto hu =
          std::make_unique<database::mysql_bulk_stmt>(hscr_resources_query);
      _mysql.prepare_statement(*hu);
      _hscr_resources_bind = std::make_unique<bulk_bind>(
          _dbcfg.get_connections_count(), dt_queue_timer_duration,
          _max_pending_queries, *hu, _logger_sql);
      _hscr_resources_update = std::move(hu);

      auto su =
          std::make_unique<database::mysql_bulk_stmt>(sscr_resources_query);
      _mysql.prepare_statement(*su);
      _sscr_resources_bind = std::make_unique<bulk_bind>(
          _dbcfg.get_connections_count(), dt_queue_timer_duration,
          _max_pending_queries, *su, _logger_sql);
      _sscr_resources_update = std::move(su);
    } else {
      _hscr_resources_update =
          std::make_unique<database::mysql_stmt>(hscr_resources_query);
      _mysql.prepare_statement(*_hscr_resources_update);

      _sscr_resources_update =
          std::make_unique<database::mysql_stmt>(sscr_resources_query);
      _mysql.prepare_statement(*_sscr_resources_update);
    }
  }
}

mysql& stream::get_mysql() {
  return _mysql;
}

bool stream::supports_bulk_prepared_statements() const {
  return _bulk_prepared_statement;
}
