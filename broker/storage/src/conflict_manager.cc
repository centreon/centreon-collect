/**
 * Copyright 2019-2024 Centreon
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
#include "com/centreon/broker/storage/conflict_manager.hh"

#include <cassert>

#include "bbdo/events.hh"
#include "bbdo/storage/index_mapping.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/sql/mysql_result.hh"
#include "com/centreon/broker/storage/internal.hh"
#include "com/centreon/common/perfdata.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using namespace com::centreon::broker::storage;
using log_v2 = com::centreon::common::log_v2::log_v2;

const std::array<std::string, 5> conflict_manager::metric_type_name{
    "GAUGE", "COUNTER", "DERIVE", "ABSOLUTE", "AUTOMATIC"};

conflict_manager* conflict_manager::_singleton = nullptr;
conflict_manager::instance_state conflict_manager::_state{
    conflict_manager::not_started};
std::mutex conflict_manager::_init_m;
std::condition_variable conflict_manager::_init_cv;

void (conflict_manager::*const conflict_manager::_neb_processing_table[])(
    std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>&) = {
    nullptr,
    &conflict_manager::_process_acknowledgement,
    &conflict_manager::_process_comment,
    &conflict_manager::_process_custom_variable,
    &conflict_manager::_process_custom_variable_status,
    &conflict_manager::_process_downtime,
    nullptr,
    nullptr,
    &conflict_manager::_process_host_check,
    nullptr,
    &conflict_manager::_process_host_group,
    &conflict_manager::_process_host_group_member,
    &conflict_manager::_process_host,
    &conflict_manager::_process_host_parent,
    &conflict_manager::_process_host_status,
    &conflict_manager::_process_instance,
    &conflict_manager::_process_instance_status,
    &conflict_manager::_process_log,
    nullptr,
    &conflict_manager::_process_service_check,
    nullptr,
    &conflict_manager::_process_service_group,
    &conflict_manager::_process_service_group_member,
    &conflict_manager::_process_service,
    &conflict_manager::_process_service_status,
    nullptr,
    &conflict_manager::_process_responsive_instance,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &conflict_manager::_process_severity,
    &conflict_manager::_process_tag,
};

conflict_manager& conflict_manager::instance() {
  assert(_singleton);
  return *_singleton;
}

conflict_manager::conflict_manager(database_config const& dbcfg,
                                   uint32_t loop_timeout,
                                   uint32_t instance_timeout)
    : _exit{false},
      _broken{false},
      _loop_timeout{loop_timeout},
      _max_pending_queries(dbcfg.get_queries_per_transaction()),
      _mysql{dbcfg},
      _instance_timeout{instance_timeout},
      _center{config::applier::state::instance().center()},
      _stats{_center->register_conflict_manager()},
      _ref_count{0},
      _group_clean_timer{com::centreon::common::pool::io_context()},
      _oldest_timestamp{std::numeric_limits<time_t>::max()},
      _logger_sql{log_v2::instance().get(log_v2::SQL)},
      _logger_storage{log_v2::instance().get(log_v2::PERFDATA)} {
  _logger_sql->debug("conflict_manager: class instanciation");
  _center->update(&ConflictManagerStats::set_loop_timeout, _stats,
                  _loop_timeout);
  _center->update(&ConflictManagerStats::set_max_pending_events, _stats,
                  _max_pending_queries);
}

conflict_manager::~conflict_manager() {
  _logger_sql->debug("conflict_manager: destruction");
  std::lock_guard<std::mutex> l(_group_clean_timer_m);
  _group_clean_timer.cancel();
}

/**
 * For the connector that does not initialize the conflict_manager, this
 * function is useful to wait.
 *
 * @param store_in_db A boolean to specify if perfdata should be stored in
 * database.
 * @param rrd_len The rrd length in seconds
 * @param interval_length The length of an elementary time interval.
 * @param queries_per_transaction The number of perfdata to store before sending
 * them to database.
 *
 * @return true if all went OK.
 */
bool conflict_manager::init_storage(bool store_in_db,
                                    uint32_t rrd_len,
                                    uint32_t interval_length,
                                    const database_config& dbcfg) {
  log_v2::instance()
      .get(log_v2::CORE)
      ->debug("conflict_manager: storage stream initialization");
  int count;

  std::unique_lock<std::mutex> lk(_init_m);

  for (count = 0; count < 60; count++) {
    /* Let's wait for 60s for the conflict_manager to be initialized */
    if (_init_cv.wait_for(lk, std::chrono::seconds(1), [&] {
          return _singleton != nullptr || _state == finished ||
                 config::applier::mode == config::applier::finished;
        })) {
      if (_state == finished ||
          config::applier::mode == config::applier::finished) {
        log_v2::instance()
            .get(log_v2::CORE)
            ->info("Conflict manager not started because cbd stopped");
        return false;
      }
      if (_singleton->_mysql.get_config() != dbcfg) {
        log_v2::instance()
            .get(log_v2::CORE)
            ->error(
                "Conflict manager: storage and sql streams do not have the "
                "same "
                "database configuration");
        return false;
      }
      std::lock_guard<std::mutex> lck(_singleton->_loop_m);
      _singleton->_rebuilder = std::make_unique<rebuilder>(
          dbcfg, rrd_len ? rrd_len : 15552000, interval_length);
      _singleton->_store_in_db = store_in_db;
      _singleton->_rrd_len = rrd_len;
      _singleton->_interval_length = interval_length;
      _singleton->_max_perfdata_queries = dbcfg.get_queries_per_transaction();
      _singleton->_max_metrics_queries = dbcfg.get_queries_per_transaction();
      _singleton->_max_cv_queries = dbcfg.get_queries_per_transaction();
      _singleton->_max_log_queries = dbcfg.get_queries_per_transaction();
      _singleton->_max_downtime_queries = dbcfg.get_queries_per_transaction();
      _singleton->_ref_count++;
      _singleton->_thread =
          std::thread(&conflict_manager::_callback, _singleton);
      pthread_setname_np(_singleton->_thread.native_handle(), "conflict_mngr");
      log_v2::instance().get(log_v2::CORE)->info("Conflict manager running");
      return true;
    }
    log_v2::instance()
        .get(log_v2::CORE)
        ->info(
            "conflict_manager: Waiting for the sql stream initialization for "
            "{} "
            "seconds",
            count);
  }
  log_v2::instance()
      .get(log_v2::CORE)
      ->error(
          "conflict_manager: not initialized after 60s. Probably "
          "an issue in the sql output configuration.");
  return false;
}

/**
 * @brief This fonction is the one that initializes the conflict_manager.
 *
 * @param dbcfg The database configuration
 * @param loop_timeout A duration in seconds. During this interval received
 *        events are handled. If there are no more events to handle, new
 *        available ones are taken from the fifo. If none, the loop waits during
 *        500ms. After this loop others things are done, cleanups, etc. And then
 *        the loop is started again.
 * @param instance_timeout A duration in seconds. This interval is used for
 *        sending data in bulk. We wait for this interval at least between two
 *        bulks.
 *
 * @return A boolean true if the function went good, false otherwise.
 */
bool conflict_manager::init_sql(database_config const& dbcfg,
                                uint32_t loop_timeout,
                                uint32_t instance_timeout) {
  log_v2::instance()
      .get(log_v2::CORE)
      ->debug("conflict_manager: sql stream initialization");
  std::lock_guard<std::mutex> lk(_init_m);
  _singleton = new conflict_manager(dbcfg, loop_timeout, instance_timeout);
  if (!_singleton) {
    _state = finished;
    return false;
  }

  _state = running;
  _singleton->_action.resize(_singleton->_mysql.connections_count());
  _init_cv.notify_all();
  _singleton->_ref_count++;
  return true;
}

void conflict_manager::_load_deleted_instances() {
  _cache_deleted_instance_id.clear();
  std::string query{"SELECT instance_id FROM instances WHERE deleted=1"};
  std::promise<mysql_result> promise;
  std::future<database::mysql_result> future = promise.get_future();
  _mysql.run_query_and_get_result(query, std::move(promise));
  try {
    mysql_result res(future.get());
    while (_mysql.fetch_row(res))
      _cache_deleted_instance_id.insert(res.value_as_u32(0));
  } catch (std::exception const& e) {
    throw msg_fmt("could not get list of deleted instances: {}", e.what());
  }
}

void conflict_manager::_load_caches() {
  // Fill index cache.
  std::lock_guard<std::mutex> lk(_loop_m);

  /* get deleted cache of instance ids => _cache_deleted_instance_id */
  _load_deleted_instances();

  /* get all outdated instances from the database => _stored_timestamps */
  std::string query{"SELECT instance_id FROM instances WHERE outdated=TRUE"};
  std::promise<mysql_result> promise_inst;
  std::future<database::mysql_result> future_inst = promise_inst.get_future();
  _mysql.run_query_and_get_result(query, std::move(promise_inst));

  /* index_data => _index_cache */
  std::promise<database::mysql_result> promise_ind;
  std::future<database::mysql_result> future_ind = promise_ind.get_future();
  _mysql.run_query_and_get_result(
      "SELECT "
      "id,host_id,service_id,host_name,rrd_retention,service_description,"
      "special,locked FROM index_data",
      std::move(promise_ind));

  /* hosts => _cache_host_instance */
  _cache_host_instance.clear();

  std::promise<mysql_result> promise_hst;
  std::future<database::mysql_result> future_hst = promise_hst.get_future();
  _mysql.run_query_and_get_result("SELECT host_id,instance_id FROM hosts",
                                  std::move(promise_hst));

  /* hostgroups => _hostgroup_cache */
  _hostgroup_cache.clear();

  std::promise<mysql_result> promise_hg;
  std::future<database::mysql_result> future_hg = promise_hg.get_future();
  _mysql.run_query_and_get_result("SELECT hostgroup_id FROM hostgroups",
                                  std::move(promise_hg));

  /* servicegroups => _servicegroup_cache */
  _servicegroup_cache.clear();

  std::promise<mysql_result> promise_sg;
  std::future<database::mysql_result> future_sg = promise_sg.get_future();
  _mysql.run_query_and_get_result("SELECT servicegroup_id FROM servicegroups",
                                  std::move(promise_sg));

  _cache_svc_cmd.clear();
  _cache_hst_cmd.clear();

  /* metrics => _metric_cache */
  {
    std::lock_guard<std::mutex> lock(_metric_cache_m);
    _metric_cache.clear();
    _metrics.clear();
  }

  std::promise<mysql_result> promise_met;
  std::future<database::mysql_result> future_met = promise_met.get_future();
  _mysql.run_query_and_get_result(
      "SELECT "
      "metric_id,index_id,metric_name,unit_name,warn,warn_low,warn_threshold_"
      "mode,crit,crit_low,crit_threshold_mode,min,max,current_value,data_"
      "source_type FROM metrics",
      std::move(promise_met));

  /* severities => _severity_cache */
  std::promise<mysql_result> promise_severity;
  std::future<database::mysql_result> future_severity =
      promise_severity.get_future();
  _mysql.run_query_and_get_result(
      "SELECT severity_id, id, type FROM severities",
      std::move(promise_severity));
  /* tags => _tags_cache */
  std::promise<mysql_result> promise_tags;
  std::future<database::mysql_result> future_tags = promise_tags.get_future();
  _mysql.run_query_and_get_result("SELECT tag_id, id, type FROM tags",
                                  std::move(promise_tags));

  /* Result of all outdated instances */
  try {
    mysql_result res(future_inst.get());
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
        "conflict_manager: could not get the list of outdated instances: {}",
        e.what());
  }

  /* Result of index_data => _index_cache */
  try {
    database::mysql_result res(future_ind.get());

    // Loop through result set.
    while (_mysql.fetch_row(res)) {
      index_info info{
          .host_name = res.value_as_str(3),
          .index_id = res.value_as_u64(0),
          .locked = res.value_as_bool(7),
          .rrd_retention = res.value_as_u32(4) ? res.value_as_u32(4) : _rrd_len,
          .service_description = res.value_as_str(5),
          .special = res.value_as_u32(6) == 2};
      uint32_t host_id(res.value_as_u32(1));
      uint32_t service_id(res.value_as_u32(2));
      _logger_storage->debug(
          "storage: loaded index {} of ({}, {}) with rrd_len={}", info.index_id,
          host_id, service_id, info.rrd_retention);
      _index_cache[{host_id, service_id}] = std::move(info);

      // Create the metric mapping.
      auto im{std::make_shared<storage::index_mapping>(info.index_id, host_id,
                                                       service_id)};
      multiplexing::publisher pblshr;
      pblshr.write(im);
    }
  } catch (std::exception const& e) {
    throw msg_fmt("storage: could not fetch index list from data DB: {}",
                  e.what());
  }

  /* Result of hosts => _cache_host_instance */
  try {
    mysql_result res(future_hst.get());
    while (_mysql.fetch_row(res))
      _cache_host_instance[res.value_as_u32(0)] = res.value_as_u32(1);
  } catch (std::exception const& e) {
    throw msg_fmt("SQL: could not get the list of host/instance pairs: {}",
                  e.what());
  }

  /* Result of hostgroups => _hostgroup_cache */
  try {
    mysql_result res(future_hg.get());
    while (_mysql.fetch_row(res))
      _hostgroup_cache.insert(res.value_as_u32(0));
  } catch (std::exception const& e) {
    throw msg_fmt("SQL: could not get the list of hostgroups id: {}", e.what());
  }

  /* Result of servicegroups => _servicegroup_cache */
  try {
    mysql_result res(future_sg.get());
    while (_mysql.fetch_row(res))
      _servicegroup_cache.insert(res.value_as_u32(0));
  } catch (std::exception const& e) {
    throw msg_fmt("SQL: could not get the list of servicegroups id: {}",
                  e.what());
  }

  /* Result of metrics => _metric_cache */
  try {
    mysql_result res{future_met.get()};
    std::lock_guard<std::mutex> lock(_metric_cache_m);
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
    throw msg_fmt("conflict_manager: could not get the list of metrics: {}",
                  e.what());
  }

  try {
    mysql_result res{future_severity.get()};
    while (_mysql.fetch_row(res)) {
      _severity_cache[{res.value_as_u64(2),
                       static_cast<uint16_t>(res.value_as_u32(1))}] =
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
    throw msg_fmt("unified sql: could not get the list of tags: {}", e.what());
  }
}

void conflict_manager::update_metric_info_cache(uint64_t index_id,
                                                uint32_t metric_id,
                                                const std::string& metric_name,
                                                short metric_type) {
  auto it = _metric_cache.find({index_id, metric_name});
  if (it != _metric_cache.end()) {
    _logger_storage->info(
        "conflict_manager: updating metric '{}' of id {} at index {} to "
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
 *  The main loop of the conflict_manager
 */
void conflict_manager::_callback() {
  constexpr unsigned neb_table_size =
      sizeof(_neb_processing_table) / sizeof(_neb_processing_table[0]);

  try {
    _load_caches();
  } catch (std::exception const& e) {
    _logger_sql->error("error while loading caches: {}", e.what());
    _broken = true;
  }

  do {
    std::chrono::system_clock::time_point time_to_deleted_index =
        std::chrono::system_clock::now();

    std::deque<std::tuple<std::shared_ptr<io::data>, uint32_t, bool*>> events;
    try {
      size_t pos = 0;
      while (!_should_exit()) {
        /* Time to send perfdatas to rrd ; no lock needed, it is this thread
         * that fill this queue. */
        _insert_perfdatas();

        /* Time to send metrics to database */
        _update_metrics();

        /* Time to send customvariables to database */
        _update_customvariables();

        /* Time to send downtimes to database */
        _update_downtimes();

        /* Time to send logs to database */
        _insert_logs();

        _logger_sql->trace(
            "conflict_manager: main loop initialized with a timeout of {} "
            "seconds.",
            _loop_timeout);

        std::chrono::system_clock::time_point now0 =
            std::chrono::system_clock::now();

        /* Are there index_data to remove? */
        if (now0 >= time_to_deleted_index) {
          try {
            _check_deleted_index();
            time_to_deleted_index += std::chrono::minutes(5);
          } catch (std::exception const& e) {
            _logger_sql->error(
                "conflict_manager: error while checking deleted indexes: {}",
                e.what());
            _broken = true;
            break;
          }
        }
        int32_t count = 0;
        int32_t timeout = 0;
        int32_t timeout_limit = _loop_timeout * 1000;

        /* This variable is incremented 1000 by 1000 and represents
         * milliseconds. Each time the duration reaches this value, we make
         * stuffs. We make then a timer cadenced at 1000ms. */
        int32_t duration = 1000;

        time_t next_insert_perfdatas = time(nullptr);
        time_t next_update_metrics = next_insert_perfdatas;
        time_t next_update_cv = next_insert_perfdatas;
        time_t next_update_log = next_insert_perfdatas;
        time_t next_update_downtime = next_insert_perfdatas;

        auto empty_caches = [this, &next_insert_perfdatas, &next_update_metrics,
                             &next_update_cv, &next_update_log,
                             &next_update_downtime](
                                std::chrono::system_clock::time_point now) {
          /* If there are too many perfdata to send, let's send them... */
          if (std::chrono::system_clock::to_time_t(now) >=
                  next_insert_perfdatas ||
              _perfdata_queue.size() > _max_perfdata_queries) {
            next_insert_perfdatas =
                std::chrono::system_clock::to_time_t(now) + 10;
            _insert_perfdatas();
          }

          /* If there are too many metrics to send, let's send them... */
          if (std::chrono::system_clock::to_time_t(now) >=
                  next_update_metrics ||
              _metrics.size() > _max_metrics_queries) {
            next_update_metrics =
                std::chrono::system_clock::to_time_t(now) + 10;
            _update_metrics();
          }

          /* Time to send customvariables to database */
          if (std::chrono::system_clock::to_time_t(now) >= next_update_cv ||
              _cv_queue.size() + _cvs_queue.size() > _max_cv_queries) {
            next_update_cv = std::chrono::system_clock::to_time_t(now) + 10;
            _update_customvariables();
          }

          /* Time to send downtimes to database */
          if (std::chrono::system_clock::to_time_t(now) >=
                  next_update_downtime ||
              _downtimes_queue.size() > _max_downtime_queries) {
            next_update_downtime =
                std::chrono::system_clock::to_time_t(now) + 5;
            _update_downtimes();
          }

          /* Time to send logs to database */
          if (std::chrono::system_clock::to_time_t(now) >= next_update_log ||
              _log_queue.size() > _max_log_queries) {
            next_update_log = std::chrono::system_clock::to_time_t(now) + 10;
            _insert_logs();
          }
        };

        /* During this loop, connectors still fill the queue when they receive
         * new events.
         * The loop is hold by three conditions that are:
         * - events.empty() no more events to treat.
         * - count < _max_pending_queries: we don't want to commit everytimes,
         *   so we keep this count to know if we reached the
         *   _max_pending_queries parameter.
         * - timeout < timeout_limit: If the loop lives too long, we interrupt
         * it it is necessary for cleanup operations.
         */
        while (count < _max_pending_queries && timeout < timeout_limit) {
          if (events.empty())
            events = _fifo.first_events();
          if (events.empty()) {
            // Let's wait for 500ms.
            if (_should_exit())
              break;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            /* Here, just before looping, we commit. */
            std::chrono::system_clock::time_point now =
                std::chrono::system_clock::now();

            empty_caches(now);
            _finish_actions();
            continue;
          }
          while (!events.empty()) {
            auto tpl = events.front();
            events.pop_front();
            std::shared_ptr<io::data>& d = std::get<0>(tpl);
            uint32_t type{d->type()};
            uint16_t cat{category_of_type(type)};
            if (std::get<1>(tpl) == sql && cat == io::neb) {
              uint16_t elem{element_of_type(type)};
              void (com::centreon::broker::storage::conflict_manager::*fn)(
                  std::tuple<std::shared_ptr<com::centreon::broker::io::data>,
                             unsigned int, bool*>&) =
                  elem >= neb_table_size ? nullptr
                                         : _neb_processing_table[elem];
              if (fn)
                (this->*fn)(tpl);
              else {
                _logger_sql->warn("SQL: no function defined for event {}:{}",
                                  cat, elem);
                *std::get<2>(tpl) = true;
              }
            } else if (std::get<1>(tpl) == storage && cat == io::neb &&
                       type == neb::service_status::static_type())
              _storage_process_service_status(tpl);
            else if (std::get<1>(tpl) == storage &&
                     type == make_type(io::bbdo, bbdo::de_rebuild_graphs)) {
              _rebuilder->rebuild_graphs(d);
              *std::get<2>(tpl) = true;
            } else if (std::get<1>(tpl) == storage &&
                       type == make_type(io::bbdo, bbdo::de_remove_graphs)) {
              remove_graphs(d);
              *std::get<2>(tpl) = true;
            } else if (std::get<1>(tpl) == sql &&
                       type == make_type(io::local, local::de_pb_stop)) {
              _logger_sql->info("poller stopped...");
              process_stop(d);
              *std::get<2>(tpl) = true;
            } else {
              _logger_sql->trace(
                  "conflict_manager: event of type {} from channel '{}' thrown "
                  "away ; no need to "
                  "store it in the database.",
                  type, std::get<1>(tpl) == sql ? "sql" : "storage");
              *std::get<2>(tpl) = true;
            }

            ++count;
            _stats_count[pos]++;

            std::chrono::system_clock::time_point now1 =
                std::chrono::system_clock::now();

            empty_caches(now1);

            timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now1 - now0)
                          .count();

            /* Get some stats each second */
            if (timeout >= duration) {
              do {
                _update_stats(events.size(), _max_perfdata_queries,
                              _fifo.get_events().size(),
                              _fifo.get_timeline(sql).size(),
                              _fifo.get_timeline(storage).size());
                duration += 1000;
                pos++;
                if (pos >= _stats_count.size())
                  pos = 0;
                _stats_count[pos] = 0;
              } while (timeout > duration);

              _events_handled = events.size();
              float s = std::accumulate(_stats_count.begin(),
                                        _stats_count.end(), 0.0f);
              {
                std::lock_guard<std::mutex> lk(_stat_m);
                _speed = s / _stats_count.size();
                _center->execute(
                    [s = this->_stats, spd = _speed] { s->set_speed(spd); });
              }
            }
          }
        }
        _logger_sql->debug("{} new events to treat", count);
        /* Here, just before looping, we commit. */
        _finish_actions();
        if (_fifo.get_pending_elements() == 0)
          _logger_sql->debug(
              "conflict_manager: acknowledgement - no pending events");
        else
          _logger_sql->debug(
              "conflict_manager: acknowledgement - still {} not acknowledged",
              _fifo.get_pending_elements());

        /* Are there unresonsive instances? */
        _update_hosts_and_services_of_unresponsive_instances();

        /* Get some stats */
        _update_stats(events.size(), _max_perfdata_queries,
                      _fifo.get_events().size(), _fifo.get_timeline(sql).size(),
                      _fifo.get_timeline(storage).size());
      }
    } catch (std::exception const& e) {
      _logger_sql->error("conflict_manager: error in the main loop: {}",
                         e.what());
      if (strstr(e.what(), "server has gone away")) {
        // The case where we must restart the connector.
        _broken = true;
      }
    }
  } while (!_should_exit());

  if (_broken) {
    std::unique_lock<std::mutex> lk(_loop_m);
    /* Let's wait for the end */
    _logger_sql->info(
        "conflict_manager: waiting for the end of the conflict manager main "
        "loop.");
    _loop_cv.wait(lk, [this]() { return !_exit; });
  }
}

/**
 *  Tell if the main loop can exit. Two conditions are needed:
 *    * _exit = true
 *    * _events is empty.
 *
 * This methods takes the lock on _loop_m, so don't call it if you already have
 * it.
 *
 * @return True if the loop can be interrupted, false otherwise.
 */
bool conflict_manager::_should_exit() const {
  std::lock_guard<std::mutex> lock(_loop_m);
  return _broken || (_exit && _fifo.get_events().empty());
}

/**
 *  Method to send event to the conflict manager.
 *
 * @param c The connector responsible of the event (sql or storage)
 * @param e The event
 *
 * @return The number of events to ack.
 */
int32_t conflict_manager::send_event(conflict_manager::stream_type c,
                                     std::shared_ptr<io::data> const& e) {
  assert(e);
  if (_broken)
    throw msg_fmt("conflict_manager: events loop interrupted");

  _logger_sql->trace(
      "conflict_manager: send_event category:{}, element:{} from {}",
      e->type() >> 16, e->type() & 0xffff, c == 0 ? "sql" : "storage");

  return _fifo.push(c, e);
}

/**
 *  This method is called from the stream and returns how many events should
 *  be released. By the way, it removes those objects from the queue.
 *
 * @param c a stream_type (we have two kinds of data arriving in the
 * conflict_manager, those from the sql connector and those from the storage
 * connector, so this stream_type is an enum containing those types).
 *
 * @return the number of events to ack.
 */
int32_t conflict_manager::get_acks(stream_type c) {
  if (_broken)
    throw msg_fmt("conflict_manager: events loop interrupted");

  return _fifo.get_acks(c);
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
void conflict_manager::_finish_action(int32_t conn, uint32_t action) {
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
void conflict_manager::_finish_actions() {
  _logger_sql->trace("conflict_manager: finish actions");
  _mysql.commit();
  std::fill(_action.begin(), _action.end(), actions::none);

  _fifo.clean(sql);
  _fifo.clean(storage);

  _logger_sql->debug("conflict_manager: still {} not acknowledged",
                     _fifo.get_pending_elements());
}

/**
 *  Add an action on the connection conn in the list of current actions.
 *  If conn < 0, the action is added to all the connections.
 *
 * @param conn The connection number or a negative number to add to all the
 *             connections
 * @param action An action.
 */
void conflict_manager::_add_action(int32_t conn, actions action) {
  if (conn < 0) {
    for (uint32_t& v : _action)
      v |= action;
  } else
    _action[conn] |= action;
}

void conflict_manager::__exit() {
  {
    std::lock_guard<std::mutex> lock(_loop_m);
    _exit = true;
    _loop_cv.notify_all();
  }
  if (_thread.joinable())
    _thread.join();
}

/**
 * @brief Updates conflict manager protobuf statistics
 *
 * @param size
 * @param mpdq
 * @param ev_size
 * @param sql_size
 * @param stor_size
 *
 */
void conflict_manager::_update_stats(const std::uint32_t size,
                                     const std::size_t mpdq,
                                     const std::size_t ev_size,
                                     const std::size_t sql_size,
                                     const std::size_t stor_size) noexcept {
  _center->execute(
      [s = this->_stats, size, mpdq, ev_size, sql_size, stor_size] {
        s->set_events_handled(size);
        s->set_max_perfdata_events(mpdq);
        s->set_waiting_events(static_cast<int32_t>(ev_size));
        s->set_sql(static_cast<int32_t>(sql_size));
        s->set_storage(static_cast<int32_t>(stor_size));
      });
}

/**
 * @brief Returns statistics about the conflict_manager. Those statistics
 * are stored directly in a json tree.
 *
 * @return A nlohmann::json with the statistics.
 */
nlohmann::json conflict_manager::get_statistics() {
  nlohmann::json retval;
  retval["max pending events"] = static_cast<int32_t>(_max_pending_queries);
  retval["max perfdata events"] = static_cast<int32_t>(_max_perfdata_queries);
  retval["loop timeout"] = static_cast<int32_t>(_loop_timeout);
  if (std::unique_lock<std::mutex>(_stat_m, std::try_to_lock)) {
    retval["waiting_events"] = static_cast<int32_t>(_fifo.get_events().size());
    retval["events_handled"] = _events_handled;
    retval["sql"] = static_cast<int32_t>(_fifo.get_timeline(sql).size());
    retval["storage"] =
        static_cast<int32_t>(_fifo.get_timeline(storage).size());
    retval["speed"] = fmt::format("{} events/s", _speed);
  }
  return retval;
}

/**
 * @brief Delete the conflict_manager singleton.
 */
int32_t conflict_manager::unload(stream_type type) {
  auto logger = _singleton->_logger_sql;
  if (!_singleton) {
    logger->info("conflict_manager: already unloaded.");
    return 0;
  } else {
    uint32_t count = --_singleton->_ref_count;
    int retval;
    if (count == 0) {
      _singleton->__exit();
      retval = _singleton->_fifo.get_acks(type);
      {
        std::lock_guard<std::mutex> lck(_singleton->_init_m);
        _state = finished;
        delete _singleton;
        _singleton = nullptr;
      }
      logger->info("conflict_manager: no more user of the conflict manager.");
    } else {
      logger->info(
          "conflict_manager: still {} stream{} using the conflict manager.",
          count, count > 1 ? "s" : "");
      retval = _singleton->_fifo.get_acks(type);
      logger->info(
          "conflict_manager: still {} events handled but not acknowledged.",
          retval);
    }
    return retval;
  }
}

/**
 * @brief Function called when a poller is disconnected from Broker. It cleans
 * hosts/services and instances in the storage database.
 *
 * @param d A pb_stop event with the instance ID.
 */
void conflict_manager::process_stop(const std::shared_ptr<io::data>& d) {
  auto& stop = static_cast<local::pb_stop*>(d.get())->obj();
  int32_t conn = _mysql.choose_connection_by_instance(stop.poller_id());
  _finish_action(-1, actions::hosts | actions::acknowledgements |
                         actions::modules | actions::downtimes |
                         actions::comments | actions::servicegroups |
                         actions::hostgroups);

  // Log message.
  _logger_sql->info("SQL: Disabling poller (id: {}, running: no)",
                    stop.poller_id());

  // Clean tables.
  _clean_tables(stop.poller_id());

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
void conflict_manager::remove_graphs(const std::shared_ptr<io::data>& d) {
  asio::post(com::centreon::common::pool::instance().io_context(), [this,
                                                                    data = d] {
    mysql ms(_mysql.get_config());
    const bbdo::pb_remove_graphs& ids =
        *static_cast<const bbdo::pb_remove_graphs*>(data.get());

    std::promise<database::mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
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
            std::move(promise), conn);
        database::mysql_result res(future.get());

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
        std::promise<database::mysql_result> promise_metrics;
        std::future<database::mysql_result> future_metrics =
            promise_metrics.get_future();

        ms.run_query_and_get_result(
            fmt::format("SELECT index_id,metric_id,metric_name FROM metrics "
                        "WHERE metric_id IN ({})",
                        fmt::join(ids.obj().metric_ids(), ",")),
            std::move(promise_metrics), conn);
        database::mysql_result res(future_metrics.get());

        std::lock_guard<std::mutex> lock(_metric_cache_m);
        while (ms.fetch_row(res)) {
          metrics_to_delete.insert(res.value_as_u64(1));
          _metric_cache.erase({res.value_as_u64(0), res.value_as_str(2)});
        }
      }
    } catch (const std::exception& e) {
      _logger_sql->error(
          "could not query index / metrics table(s) to get index to delete: "
          "{} ",
          e.what());
    }

    std::string mids_str{fmt::format("{}", fmt::join(metrics_to_delete, ","))};
    if (!metrics_to_delete.empty()) {
      _logger_sql->info("metrics {} erased from database", mids_str);
      ms.run_query(
          fmt::format("DELETE FROM metrics WHERE metric_id in ({})", mids_str),
          database::mysql_error::delete_metric);
    }
    std::string ids_str{fmt::format("{}", fmt::join(indexes_to_delete, ","))};
    if (!indexes_to_delete.empty()) {
      _logger_sql->info("indexes {} erased from database", ids_str);
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
      multiplexing::publisher().write(rmg);
    } else
      _logger_sql->info(
          "metrics {} and indexes {} do not appear in the storage database",
          fmt::join(ids.obj().metric_ids(), ","),
          fmt::join(ids.obj().index_ids(), ","));
  });
}
