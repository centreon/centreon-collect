/**
 * Copyright 2009-2013,2015-2017,2019-2021 Centreon
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

#include "com/centreon/broker/multiplexing/muxer.hh"

#include <cassert>

#include "com/centreon/broker/bbdo/internal.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/common/time.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::multiplexing;

static std::mutex _add_bench_point_m;
/**
 * @brief add a bench point to pb_bench event
 * as several muxers can update it at the same time, it's protected by a mutex
 *
 * @param event
 * @param tp_name
 */
void add_bench_point(bbdo::pb_bench& event,
                     const std::string& muxer_name,
                     const char* funct_name) {
  std::lock_guard<std::mutex> l(_add_bench_point_m);
  com::centreon::broker::TimePoint* muxer_tp = event.mut_obj().add_points();
  muxer_tp->set_name(muxer_name);
  muxer_tp->set_function(funct_name);
  com::centreon::common::time_point_to_google_ts(
      std::chrono::system_clock::now(), *muxer_tp->mutable_time());
}

uint32_t muxer::_event_queue_max_size = std::numeric_limits<uint32_t>::max();

std::mutex muxer::_running_muxers_m;
absl::flat_hash_map<std::string, std::weak_ptr<muxer>> muxer::_running_muxers;

/**
 * @brief Constructor.
 *
 * @param name            Name associated to this muxer. It is used to create
 *                        on-disk files.
 * @param parent          The engine that relies all the muxers.
 * @param r_filter        The read filter constructed from the stream and the
 *                        user configuration.
 * @param w_filter        The write filter constructed from the stream and the
 *                        user configuration.
 * @param persistent      Wether or not this muxer should backup unprocessed
 *                        events in a persistent storage.
 */
muxer::muxer(std::string name,
             const std::shared_ptr<engine>& parent,
             const muxer_filter& r_filter,
             const muxer_filter& w_filter,
             bool persistent)
    : io::stream("muxer"),
      _name(std::move(name)),
      _engine(parent),
      _queue_file_name{queue_file(_name)},
      _read_filter{r_filter},
      _write_filter{w_filter},
      _read_filters_str{misc::dump_filters(r_filter)},
      _write_filters_str{misc::dump_filters(w_filter)},
      _persistent(persistent),
      _events_size{0u},
      _last_stats{std::time(nullptr)} {
  // Load head queue file back in memory.
  DEBUG(fmt::format("CONSTRUCTOR muxer {:p} {}", static_cast<void*>(this),
                    _name));
  std::lock_guard<std::mutex> lck(_mutex);
  if (_persistent) {
    try {
      auto mf{std::make_unique<persistent_file>(memory_file(_name), nullptr)};
      std::shared_ptr<io::data> e;
      for (;;) {
        e.reset();
        mf->read(e, 0);
        if (e) {
          _events.push_back(e);
          ++_events_size;
        }
      }
    } catch (const exceptions::shutdown& e) {
      // Memory file was properly read back in memory.
      (void)e;
    }
  }

  _pos = _events.begin();
  // Load queue file back in memory.
  try {
    QueueFileStats* stats =
        stats::center::instance().muxer_stats(_name)->mutable_queue_file();
    _file = std::make_unique<persistent_file>(_queue_file_name, stats);
    std::shared_ptr<io::data> e;
    // The following do-while might read an extra event from the queue
    // file back in memory. However this is necessary to ensure that a
    // read() operation was done on the queue file and prevent it from
    // being open in case it is empty.
    do {
      _get_event_from_file(e);
      if (!e)
        break;
      _events.push_back(e);
      ++_events_size;
    } while (_events_size < event_queue_max_size());
  } catch (const exceptions::shutdown& e) {
    // Queue file was entirely read back.
    (void)e;
  }

  _update_stats();

  // Log messages.
  SPDLOG_LOGGER_INFO(
      log_v2::core(),
      "multiplexing: '{}' starts with {} in queue and the queue file is {}",
      _name, _events_size, _file ? "enable" : "disable");
}

/**
 * @brief muxer must be in a shared_ptr
 * so this static method creates it and registers it in engine
 *
 * @param name            Name associated to this muxer. It is used to create
 *                        on-disk files.
 * @param parent          The engine that relies all the muxers.
 * @param r_filter        The read filter constructed from the stream and the
 *                        user configuration.
 * @param w_filter        The write filter constructed from the stream and the
 *                        user configuration.
 * @param persistent      Wether or not this muxer should backup unprocessed
 *                        events in a persistent storage.
 * @return std::shared_ptr<muxer>
 */
std::shared_ptr<muxer> muxer::create(std::string name,
                                     const std::shared_ptr<engine>& parent,
                                     const muxer_filter& r_filter,
                                     const muxer_filter& w_filter,
                                     bool persistent) {
  std::shared_ptr<muxer> retval;
  {
    std::lock_guard<std::mutex> lck(_running_muxers_m);
    absl::erase_if(_running_muxers,
                   [](const std::pair<std::string, std::weak_ptr<muxer>>& p) {
                     return p.second.expired();
                   });
    retval = _running_muxers[name].lock();
    if (retval) {
      log_v2::config()->debug("muxer: muxer '{}' already exists, reusing it",
                              name);
      retval->set_read_filter(r_filter);
      retval->set_write_filter(w_filter);
      SPDLOG_LOGGER_INFO(log_v2::core(),
                         "multiplexing: reuse '{}' starts with {} in queue and "
                         "the queue file is {}",
                         name, retval->_events_size,
                         retval->_file ? "enable" : "disable");

    } else {
      log_v2::config()->debug("muxer: muxer '{}' unknown, creating it", name);
      retval = std::shared_ptr<muxer>(
          new muxer(name, parent, r_filter, w_filter, persistent));
      _running_muxers[name] = retval;
    }
  }

  parent->subscribe(retval);
  return retval;
}

/**
 *  Destructor.
 */
muxer::~muxer() noexcept {
  unsubscribe();
  {
    std::lock_guard<std::mutex> lock(_mutex);
    SPDLOG_LOGGER_INFO(log_v2::core(),
                       "Destroying muxer {}: number of events in the queue: {}",
                       _name, _events_size);
    _clean();
  }
  DEBUG(
      fmt::format("DESTRUCTOR muxer {:p} {}", static_cast<void*>(this), _name));
  // caution, unregister_muxer must be the last center method called at muxer
  // destruction to avoid re create a muxer stat entry
  stats::center::instance().unregister_muxer(_name);
}

/**
 *  Acknowledge events.
 *
 *  @param[in] count  Number of events to acknowledge.
 */
void muxer::ack_events(int count) {
  // Remove acknowledged events.
  SPDLOG_LOGGER_TRACE(
      log_v2::core(),
      "multiplexing: acknowledging {} events from {} event queue size: {}",
      count, _name, _events_size);

  if (count) {
    SPDLOG_LOGGER_DEBUG(
        log_v2::core(),
        "multiplexing: acknowledging {} events from {} event queue", count,
        _name);
    std::lock_guard<std::mutex> lock(_mutex);
    for (int i = 0; i < count && !_events.empty(); ++i) {
      if (_events.begin() == _pos) {
        log_v2::core()->error(
            "multiplexing: attempt to acknowledge "
            "more events than available in {} event queue: {} size: {}, "
            "requested, {} "
            "acknowledged",
            _name, _events_size, count, i);
        break;
      }
      _events.pop_front();
      --_events_size;
    }
    SPDLOG_LOGGER_TRACE(log_v2::core(),
                        "multiplexing: still {} events in {} event queue",
                        _events_size, _name);

    // Fill memory from file.
    std::shared_ptr<io::data> e;
    while (_events_size < event_queue_max_size()) {
      _get_event_from_file(e);
      if (!e)
        break;
      _push_to_queue(e);
    }
    _update_stats();
  } else {
    SPDLOG_LOGGER_TRACE(
        log_v2::core(),
        "multiplexing: acknowledging no events from {} event queue", _name);
  }
}

/**
 * @brief Flush the muxer and stop it (in this case, nothing to do to stop it).
 *
 * @return The number of acknowledged events.
 */
int32_t muxer::stop() {
  SPDLOG_LOGGER_INFO(log_v2::core(),
                     "Stopping muxer {}: number of events in the queue: {}",
                     _name, _events_size);
  std::lock_guard<std::mutex> lck(_mutex);
  _update_stats();
  DEBUG(fmt::format("STOP muxer {:p} {}", static_cast<void*>(this), _name));
  return 0;
}

/**
 *  Set the maximum event queue size.
 *
 *  @param[in] max  The size limit.
 */
void muxer::event_queue_max_size(uint32_t max) noexcept {
  if (!max)
    _event_queue_max_size = std::numeric_limits<uint32_t>::max();
  else
    _event_queue_max_size = max;
}

/**
 *  Get the maximum event queue size.
 *
 *  @return The size limit.
 */
uint32_t muxer::event_queue_max_size() noexcept {
  return _event_queue_max_size;
}

/**
 *  Add a new event to the internal event list.
 *
 *  @param[in] event Event to add.
 */
void muxer::publish(const std::deque<std::shared_ptr<io::data>>& event_queue) {
  auto evt = event_queue.begin();
  while (evt != event_queue.end()) {
    bool at_least_one_push_to_queue = false;
    read_handler async_handler;
    {
      // we stop this first loop when mux queue is full on order to release
      // mutex to let read do his job before write to file
      std::lock_guard<std::mutex> lock(_mutex);
      for (; evt != event_queue.end() && _events_size < event_queue_max_size();
           ++evt) {
        auto event = *evt;
        if (!_write_filter.allows(event->type())) {
          SPDLOG_LOGGER_TRACE(
              log_v2::core(),
              "muxer {} event of type {:x} rejected by write filter", _name,
              event->type());
          continue;
        }

        if (event->type() == bbdo::pb_bench::static_type()) {
          add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(event),
                          _name, "publish");
          SPDLOG_LOGGER_INFO(log_v2::core(), "{} bench publish {}", _name,
                             io::data::dump_json{*event});
        }

        SPDLOG_LOGGER_TRACE(
            log_v2::core(),
            "muxer {} event of type {:x} written queue size: {}", _name,
            event->type(), _events_size);

        at_least_one_push_to_queue = true;

        _push_to_queue(event);
      }
      if (at_least_one_push_to_queue &&
          _read_handler) {  // async handler waiting?
        async_handler = std::move(_read_handler);
        _read_handler = nullptr;
      }
    }
    if (async_handler) {
      async_handler();
    }

    if (evt == event_queue.end()) {
      std::lock_guard<std::mutex> lock(_mutex);
      _update_stats();
      return;
    }
    // we have stopped insertion because of full queue => retry
    if (at_least_one_push_to_queue) {
      continue;
    }
    // nothing pushed => to file
    std::lock_guard<std::mutex> lock(_mutex);
    for (; evt != event_queue.end(); ++evt) {
      auto event = *evt;
      if (!_write_filter.allows(event->type())) {
        SPDLOG_LOGGER_TRACE(
            log_v2::core(),
            "muxer {} event of type {:x} rejected by write filter", _name,
            event->type());
        continue;
      }
      if (event->type() == bbdo::pb_bench::static_type()) {
        add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(event), _name,
                        "retention_publish");
        SPDLOG_LOGGER_INFO(log_v2::core(),
                           "muxer {} bench publish to file {} {}", _name,
                           _queue_file_name, io::data::dump_json{*event});
      }
      if (!_file) {
        QueueFileStats* s =
            stats::center::instance().muxer_stats(_name)->mutable_queue_file();
        _file = std::make_unique<persistent_file>(_queue_file_name, s);
      }
      try {
        _file->write(event);
        SPDLOG_LOGGER_TRACE(
            log_v2::core(),
            "{} publish one event of type {:x} to file {} queue size:{}", _name,
            event->type(), _queue_file_name, _events_size);
      } catch (const std::exception& ex) {
        // in case of exception, we lost event. It's mandatory to avoid
        // infinite loop in case of permanent disk problem
        SPDLOG_LOGGER_ERROR(log_v2::core(), "{} fail to write event to {}: {}",
                            _name, _queue_file_name, ex.what());
        _file.reset();
      }
    }
    _update_stats();
  }
}

/**
 *  Get the next available event without waiting more than timeout.
 *
 *  @param[out] event      Next available event.
 *  @param[in]  deadline   Date limit.
 *
 *  @return Respect io::stream::read()'s return value.
 */
bool muxer::read(std::shared_ptr<io::data>& event, time_t deadline) {
  bool timed_out{false};
  std::unique_lock<std::mutex> lock(_mutex);

  // No data is directly available.
  if (_pos == _events.end()) {
    // Wait a while if subscriber was not shutdown.
    if ((time_t)-1 == deadline)
      _cv.wait(lock);
    else if (!deadline) {
      timed_out = true;
    } else {
      time_t now(time(nullptr));
      timed_out = _cv.wait_for(lock, std::chrono::seconds(deadline - now)) ==
                  std::cv_status::timeout;
    }
    if (_pos != _events.end()) {
      event = *_pos;
      ++_pos;
      if (event)
        timed_out = false;
    } else
      event.reset();
  }
  // Data is available, no need to wait.
  else {
    event = *_pos;
    ++_pos;
  }

  _update_stats();

  if (event) {
    SPDLOG_LOGGER_TRACE(log_v2::core(), "{} read {} queue size {}", _name,
                        *event, _events_size);
    if (event->type() == bbdo::pb_bench::static_type()) {
      add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(event), _name,
                      "read");
      SPDLOG_LOGGER_INFO(log_v2::core(), "{} bench read {}", _name,
                         io::data::dump_json{*event});
    }
  } else {
    SPDLOG_LOGGER_TRACE(log_v2::core(), "{} queue size {} no event available",
                        _name, _events_size);
  }
  return !timed_out;
}

/**
 *  Get the read filters as a string.
 *
 *  @return  The read filters formatted into a string.
 */
const std::string& muxer::read_filters_as_str() const {
  return _read_filters_str;
}

/**
 *  Get the write filters as a string.
 *
 *  @return  The write filters formatted into a string.
 */
const std::string& muxer::write_filters_as_str() const {
  return _write_filters_str;
}

/**
 *  Get the size of the event queue.
 *
 *  @return  The size of the event queue.
 */
uint32_t muxer::get_event_queue_size() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _events_size;
}

/**
 *  Reprocess non-acknowledged events.
 */
void muxer::nack_events() {
  SPDLOG_LOGGER_DEBUG(log_v2::core(),
                      "multiplexing: reprocessing unacknowledged events from "
                      "{} event queue with {} waiting events",
                      _name, _events_size);
  std::lock_guard<std::mutex> lock(_mutex);
  _pos = _events.begin();
  _update_stats();
}

/**
 *  Generate statistics about the subscriber.
 *
 *  @param[out] buffer Output buffer.
 */
void muxer::statistics(nlohmann::json& tree) const {
  // Lock object.
  std::lock_guard<std::mutex> lock(_mutex);

  // Queue file mode.
  bool queue_file_enabled(_file.get());
  tree["queue_file_enabled"] = queue_file_enabled;
  if (queue_file_enabled) {
    nlohmann::json queue_file;
    _file->statistics(queue_file);
    tree["queue_file"] = queue_file;
  }

  // Unacknowledged events count.
  int32_t count = 0;
  for (auto it = _events.begin(); it != _pos; ++it)
    count++;
  tree["unacknowledged_events"] = count;
}

/**
 *  Wake all threads waiting on this subscriber.
 */
void muxer::wake() {
  std::lock_guard<std::mutex> lock(_mutex);
  _cv.notify_all();
}

/**
 *  Send an event to multiplexing.
 *
 *  @param[in] d  Event to multiplex.
 */
int muxer::write(std::shared_ptr<io::data> const& d) {
  if (!d) {
    return 1;
  }
  if (_read_filter.allows(d->type())) {
    if (d->type() == bbdo::pb_bench::static_type()) {
      add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(d), _name,
                      "write");
      SPDLOG_LOGGER_INFO(log_v2::core(), "{} bench write {}", _name,
                         io::data::dump_json{*d});
    }
    _engine->publish(d);
  } else {
    SPDLOG_LOGGER_TRACE(log_v2::core(),
                        "muxer {} event of type {:x} rejected by read filter",
                        _name, d->type());
  }
  return 1;
}

/**
 * @brief send events to multiplexing
 *
 * @param to_publish list of event where not allowed event will be erased
 */
void muxer::write(std::deque<std::shared_ptr<io::data>>& to_publish) {
  for (auto list_iter = to_publish.begin();
       !to_publish.empty() && list_iter != to_publish.end();) {
    const std::shared_ptr<io::data>& d = *list_iter;
    if (_read_filter.allows(d->type())) {
      if (d->type() == bbdo::pb_bench::static_type()) {
        add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(d), _name,
                        "write");
        SPDLOG_LOGGER_INFO(log_v2::core(), "{} bench write {}", _name,
                           io::data::dump_json{*d});
      }
      ++list_iter;
    } else {
      list_iter = to_publish.erase(list_iter);
    }
  }
  if (!to_publish.empty()) {
    _engine->publish(to_publish);
  }
}

/**
 *  Release all events stored within the internal list.
 *  Warning: _mutex must be locked to call this function.
 */
void muxer::_clean() {
  _file.reset();
  stats::center::instance().clear_muxer_queue_file(_name);
  //  stats::center::instance().execute([name=this->_name] {
  //      stats::center::instance().muxer_stats(name)->mutable_queue_file()->Clear();
  //  });
  if (_persistent && !_events.empty()) {
    try {
      SPDLOG_LOGGER_TRACE(log_v2::core(), "muxer: sending {} events to {}",
                          _events_size, memory_file(_name));
      auto mf{std::make_unique<persistent_file>(memory_file(_name), nullptr)};
      while (!_events.empty()) {
        mf->write(_events.front());
        _events.pop_front();
        --_events_size;
      }
    } catch (std::exception const& e) {
      log_v2::core()->error(
          "multiplexing: could not backup memory queue of '{}': {}", _name,
          e.what());
    }
  }
  _events.clear();
  _events_size = 0;
  _pos = _events.begin();
  _update_stats();
}

/**
 *  Get event from retention file.
 *  Warning: lock _mutex before using this function.
 *
 *  @param[out] event  Last event available. Null if none is available.
 */
void muxer::_get_event_from_file(std::shared_ptr<io::data>& event) {
  event.reset();
  // If file exists, try to get the last event.
  if (_file) {
    try {
      do {
        _file->read(event);
      } while (!event);
    } catch (exceptions::shutdown const& e) {
      // The file end was reach.
      (void)e;
      _file.reset();
      stats::center::instance().clear_muxer_queue_file(_name);
    }
  }
}

/**
 *  Get the memory file name associated with this muxer.
 *
 *  @param[in] name  Name of this muxer.
 *
 *  @return  The memory file name associated with this muxer.
 */
std::string muxer::memory_file(std::string const& name) {
  std::string retval(fmt::format(
      "{}.memory.{}", config::applier::state::instance().cache_dir(), name));
  return retval;
}

/**
 *  Get the queue file name associated with this muxer.
 *
 *  @param[in] name  Name of this muxer.
 *
 *  @return  The queue file name associated with this muxer.
 */
std::string muxer::queue_file(std::string const& name) {
  std::string retval(fmt::format(
      "{}.queue.{}", config::applier::state::instance().cache_dir(), name));
  return retval;
}

/**
 *  Push event to queue (_mutex is locked when this method is called).
 *
 *  @param[in] event  New event.
 */
void muxer::_push_to_queue(std::shared_ptr<io::data> const& event) {
  bool pos_has_no_more_to_read(_pos == _events.end());
  SPDLOG_LOGGER_TRACE(log_v2::core(), "muxer {} event of type {:x} pushed",
                      _name, event->type());
  _events.push_back(event);
  ++_events_size;

  if (pos_has_no_more_to_read) {
    _pos = --_events.end();
    _cv.notify_one();
  }
}

/**
 * @brief Fill statistics if it happened more than 1 second ago
 *
 * Warning: _mutex must be locked before while calling this function.
 */
void muxer::_update_stats() noexcept {
  std::time_t now{std::time(nullptr)};
  if (now - _last_stats > 0) {
    _last_stats = now;
    /* Since _mutex is locked, we can get interesting values and copy them
     * in the capture. Then the execute() function can put them in the stats
     * object asynchronously. */
    stats::center::instance().update_muxer(
        _name, _file ? _queue_file_name : "", _events_size,
        std::distance(_events.begin(), _pos));
  }
}

/**
 *  Remove all the queue files attached to this muxer.
 */
void muxer::remove_queue_files() {
  SPDLOG_LOGGER_INFO(log_v2::core(), "multiplexing: '{}' removed",
                     _queue_file_name);

  /* Here _file is already destroyed */
  QueueFileStats* stats =
      stats::center::instance().muxer_stats(_name)->mutable_queue_file();
  persistent_file file(_queue_file_name, stats);
  file.remove_all_files();
}

/**
 * @brief Muxer name accessor.
 *
 * @return a reference to the name.
 */
const std::string& muxer::name() const {
  return _name;
}

/**
 * @brief In case of a muxer reused by a failover, we have to update its
 * filters. This function updates the read filter.
 *
 * @param r_filter        The read filter.
 */
void muxer::set_read_filter(const muxer_filter& r_filter) {
  log_v2::config()->trace("multiplexing: '{}' set read filter...", _name);
  _read_filter = r_filter;
  _read_filters_str = misc::dump_filters(r_filter);
}

/**
 * @brief In case of a muxer reused by a failover, we have to update its
 * filters. This function updates the write filter.
 *
 * @param r_filter        The write filter.
 */
void muxer::set_write_filter(const muxer_filter& w_filter) {
  log_v2::config()->trace("multiplexing: '{}' set write filter...", _name);
  _write_filter = w_filter;
  _write_filters_str = misc::dump_filters(w_filter);
}

/**
 * @brief clear readhandler in case of caller owning this object has terminate
 *
 */
void muxer::clear_read_handler() {
  std::unique_lock<std::mutex> lock(_mutex);
  _read_handler = nullptr;
}

/**
 * @brief Unsubscribe this muxer from the parent engine.
 */
void muxer::unsubscribe() {
  _engine->unsubscribe_muxer(this);
}
