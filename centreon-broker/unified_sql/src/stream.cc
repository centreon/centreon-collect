/*
** Copyright 2019-2020 Centreon
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

#include "bbdo/storage/index_mapping.hh"
#include "com/centreon/broker/database/mysql_result.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/perfdata.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/stats/center.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using namespace com::centreon::broker::unified_sql;

void (stream::*const stream::_neb_processing_table[])(
    std::shared_ptr<io::data>&) = {
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
};

stream::stream(const database_config& dbcfg,
               uint32_t rrd_len,
               uint32_t interval_length,
               uint32_t loop_timeout,
               uint32_t instance_timeout,
               uint32_t rebuild_check_interval,
               bool store_in_data_bin)
    : io::stream("unified_sql"),
      _state{not_started},
      _processed{0},
      _ack{0},
      _pending_events{0},
      _exit{false},
      _broken{false},
      _loop_timeout{loop_timeout},
      _max_pending_queries(dbcfg.get_queries_per_transaction()),
      _mysql{dbcfg},
      _instance_timeout{instance_timeout},
      _rebuilder{dbcfg, this, rebuild_check_interval,
                 rrd_len ? rrd_len : 15552000, interval_length},
      _store_in_db{store_in_data_bin},
      _rrd_len{rrd_len},
      _interval_length{interval_length},
      _max_perfdata_queries{_max_pending_queries},
      _max_metrics_queries{_max_pending_queries},
      _max_cv_queries{_max_pending_queries},
      _max_log_queries{_max_pending_queries},
      _stats{stats::center::instance().register_conflict_manager()},
      _events_handled{0},
      _speed{},
      _stats_count_pos{0},
      _oldest_timestamp{std::numeric_limits<time_t>::max()} {
  log_v2::sql()->debug("unified sql: stream class instanciation");
  stats::center::instance().execute([stats = _stats,
                                     loop_timeout = _loop_timeout,
                                     max_queries = _max_pending_queries] {
    stats->set_loop_timeout(loop_timeout);
    stats->set_max_pending_events(max_queries);
  });
  _state = running;
  _action.resize(_mysql.connections_count());

  std::lock_guard<std::mutex> lk(_loop_m);
  _thread = std::thread(&stream::_callback, this);
  pthread_setname_np(_thread.native_handle(), "conflict_mngr");
}

stream::~stream() {
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
  std::lock_guard<std::mutex> lk(_loop_m);

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
    _metrics.clear();

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
        metric_name, metric_id, index_id,
        misc::perfdata::data_type_name[metric_type]);
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
 *  The main loop of the stream
 */
void stream::_callback() {
  try {
    _load_caches();
  } catch (std::exception const& e) {
    log_v2::sql()->error("error while loading caches: {}", e.what());
    _broken = true;
  }

  do {
    std::chrono::system_clock::time_point time_to_deleted_index =
        std::chrono::system_clock::now();

    size_t pos = 0;
    std::deque<std::shared_ptr<io::data>> events;
    try {
      while (!_should_exit()) {
        /* Time to send perfdatas to rrd ; no lock needed, it is this thread
         * that fill this queue. */
        _insert_perfdatas();

        /* Time to send metrics to database */
        _update_metrics();

        /* Time to send customvariables to database */
        _update_customvariables();

        /* Time to send logs to database */
        _insert_logs();

        log_v2::sql()->trace(
            "unified sql: main loop initialized with a timeout of {} "
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
            log_v2::sql()->error(
                "unified sql: error while checking deleted indexes: {}",
                e.what());
            _broken = true;
            break;
          }
        }
        uint32_t count = 0;
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

        auto empty_caches = [this, &next_insert_perfdatas, &next_update_metrics,
                             &next_update_cv, &next_update_log](
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
          if (events.empty()) {
            std::lock_guard<std::mutex> lck(_fifo_m);
            std::swap(_fifo, events);
          }
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
            auto d = events.front();
            uint32_t type = d->type();
            uint16_t cat = category_of_type(type);
            uint16_t elem = element_of_type(type);
            if (cat == io::neb) {
              (this->*(_neb_processing_table[elem]))(d);
              if (type == neb::service_status::static_type())
                _unified_sql_process_service_status(d);
            } else {
              log_v2::sql()->trace(
                  "unified sql: event of type {} thrown away ; no need to "
                  "store it in the database.",
                  type);
            }
            events.pop_front();
            log_v2::sql()->trace("processed = {}", _processed);
            _processed++;

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
                duration += 1000;
                pos++;
                if (pos >= _stats_count.size())
                  pos = 0;
                _stats_count[pos] = 0;
              } while (timeout > duration);

              _events_handled = events.size();
              float s = 0.0f;
              for (const auto& c : _stats_count)
                s += c;

              std::lock_guard<std::mutex> lk(_stat_m);
              _speed = s / _stats_count.size();
              stats::center::instance().update(&ConflictManagerStats::set_speed,
                                               _stats,
                                               static_cast<double>(_speed));
            }
          }
        }
        log_v2::sql()->debug("{} new events to treat", count);
        /* Here, just before looping, we commit. */
        _finish_actions();
        int32_t fifo_size;
        {
          std::lock_guard<std::mutex> lk(_fifo_m);
          fifo_size = _fifo.size();
        }
        if (fifo_size == 0)
          log_v2::sql()->debug(
              "unified sql: acknowledgement - no pending events");
        else {
          log_v2::sql()->debug(
              "unified sql: acknowledgement - still {} not acknowledged",
              fifo_size);
        }

        /* Are there unresonsive instances? */
        _update_hosts_and_services_of_unresponsive_instances();

        /* Get some stats */
        {
          std::lock_guard<std::mutex> lk(_stat_m);
          _events_handled = events.size();
          stats::center::instance().execute([stats = _stats, handled = _events_handled, fifo_size]
              {
              stats->set_events_handled(handled);
              stats->set_waiting_events(fifo_size);
              });
//          stats::center::instance().update(
//              &ConflictManagerStats::set_events_handled, _stats,
//              _events_handled);
//          stats::center::instance().update(
//              &ConflictManagerStats::set_max_perfdata_events, _stats,
//              _max_perfdata_queries);
//          stats::center::instance().update(
//              &ConflictManagerStats::set_waiting_events, _stats, fifo_size);
        }
      }
    } catch (std::exception const& e) {
      log_v2::sql()->error("unified sql: error in the main loop: {}", e.what());
      if (strstr(e.what(), "server has gone away")) {
        // The case where we must restart the connector.
        _broken = true;
      }
    }
  } while (!_should_exit());

  if (_broken) {
    std::unique_lock<std::mutex> lk(_loop_m);
    /* Let's wait for the end */
    log_v2::sql()->info(
        "unified sql: waiting for the end of the conflict manager main "
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
bool stream::_should_exit() const {
  std::lock_guard<std::mutex> lock(_loop_m);
  bool fifo_empty;
  {
    std::lock_guard<std::mutex> lck(_fifo_m);
    fifo_empty = _fifo.empty();
  }
  return _broken || (_exit && fifo_empty);
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

  std::lock_guard<std::mutex> lck(_fifo_m);
  log_v2::sql()->debug("unified sql: still {} not acknowledged", _fifo.size());
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

void stream::__exit() {
  {
    std::lock_guard<std::mutex> lock(_loop_m);
    _exit = true;
    _loop_cv.notify_all();
  }
  if (_thread.joinable())
    _thread.join();
}

/**
 * @brief Returns statistics about the stream. Those statistics
 * are stored directly in a json tree.
 *
 * @return A nlohmann::json with the statistics.
 */
void stream::statistics(nlohmann::json& tree) const {
  int32_t fifo_size;
  int32_t processed;
  int32_t pending;
  {
    std::lock_guard<std::mutex> lck(_fifo_m);
    fifo_size = _fifo.size();
    processed = _processed;
    pending = _pending_events;
  }
  tree["max pending events"] = static_cast<int32_t>(_max_pending_queries);
  tree["max perfdata events"] = static_cast<int32_t>(_max_perfdata_queries);
  tree["loop timeout"] = static_cast<int32_t>(_loop_timeout);
  if (auto lock = std::unique_lock<std::mutex>(_stat_m, std::try_to_lock)) {
    tree["waiting_events"] = fifo_size;
    tree["processed_events"] = processed;
    tree["acked_events"] = static_cast<int32_t>(_ack);
    tree["events_handled"] = _events_handled;
    tree["speed"] = fmt::format("{:.2f} events/s", _speed);
  }
}

/**
 * @brief Delete the stream singleton.
 */
// int32_t stream::unload(stream_type type) {
//  if (!_singleton) {
//    log_v2::sql()->info("unified sql: already unloaded.");
//    return 0;
//  } else {
//    uint32_t count = --_singleton->_ref_count;
//    int retval;
//    if (count == 0) {
//      __exit();
//      retval = _fifo.get_acks(type);
//      {
//        std::lock_guard<std::mutex> lck(_init_m);
//        _state = finished;
//        delete _singleton;
//        _singleton = nullptr;
//      }
//      log_v2::sql()->info("unified sql: no more user of the conflict
//      manager.");
//    } else {
//      log_v2::sql()->info(
//          "unified sql: still {} stream{} using the conflict manager.", count,
//          count > 1 ? "s" : "");
//      retval = _fifo.get_acks(type);
//      log_v2::sql()->info(
//          "unified sql: still {} events handled but not acknowledged.",
//          retval);
//    }
//    return retval;
//  }
//}

int32_t stream::write(const std::shared_ptr<io::data>& data) {
  ++_pending_events;
  assert(data);
  if (_broken)
    throw msg_fmt("unified sql: events loop interrupted");

  log_v2::sql()->trace("unified sql: write event category:{}, element:{}",
                       category_of_type(data->type()),
                       element_of_type(data->type()));
  int32_t retval;
  {
    std::lock_guard<std::mutex> lck(_fifo_m);
    _fifo.push_back(data);
    retval = _ack;
    _ack -= retval;
  }

  _pending_events -= retval;
  return retval;
}

/**
 * @brief Flush the stream.
 *
 * @return Number of acknowledged events.
 */
int32_t stream::flush() {
  int32_t retval = _ack;
  _ack -= retval;
  _pending_events -= retval;
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
  /* Let's stop the thread */
  {
    std::lock_guard<std::mutex> lck(_loop_m);
    _exit = true;
    _loop_cv.notify_all();
  }
  if (_thread.joinable())
    _thread.join();

  /* Let's return how many events to ack */
  int32_t retval = _ack;
  _ack -= retval;
  return retval;
}
