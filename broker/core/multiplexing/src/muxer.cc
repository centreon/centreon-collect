/*
** Copyright 2009-2013,2015-2017,2019-2021 Centreon
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

#include "com/centreon/broker/multiplexing/muxer.hh"

#include <cassert>

#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/multiplexing/engine.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::multiplexing;

uint32_t muxer::_event_queue_max_size = std::numeric_limits<uint32_t>::max();

std::mutex muxer::_running_muxers_m;
absl::flat_hash_map<std::string, std::weak_ptr<muxer>> muxer::_running_muxers;

static auto log_exclude_include = [](const std::string& filename,
                                     uint32_t type,
                                     const char* subject,
                                     bool excluded) {
  FILE* f = fopen(fmt::format("/tmp/{}.log", filename).c_str(), "a");
  fprintf(f, "%d\t%s\t%s\n", type, subject, excluded ? "FALSE" : "TRUE");
  fclose(f);
};

/**
 * @brief Constructor.
 *
 * @param name            Name associated to this muxer. It is used to create
 *                        on-disk files.
 * @param r_filter        The read filter constructed from the stream and the
 *                        user configuration.
 * @param w_filter        The write filter constructed from the stream and the
 *                        user configuration.
 * @param persistent      Wether or not this muxer should backup unprocessed
 *                        events in a persistent storage.
 */
muxer::muxer(std::string name,
             const muxer_filter& r_filter,
             const muxer_filter& w_filter,
             bool persistent)
    : io::stream("muxer"),
      _name(std::move(name)),
      _queue_file_name{queue_file(_name)},
      _read_filter{r_filter},
      _write_filter{w_filter},
      _read_filters_str{misc::dump_filters(r_filter)},
      _write_filters_str{misc::dump_filters(w_filter)},
      _persistent(persistent),
      _events_size{0u},
      _last_stats{std::time(nullptr)} {
  // Load head queue file back in memory.
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
  log_v2::core()->info(
      "multiplexing: '{}' starts with {} in queue and the queue file is {}",
      _name, _events_size, _file ? "enable" : "disable");
}

/**
 * @brief muxer must be in a shared_ptr
 * so this static method creates it and registers it in engine
 *
 * @param name            Name associated to this muxer. It is used to create
 *                        on-disk files.
 * @param r_filter        The read filter constructed from the stream and the
 *                        user configuration.
 * @param w_filter        The write filter constructed from the stream and the
 *                        user configuration.
 * @param persistent      Wether or not this muxer should backup unprocessed
 *                        events in a persistent storage.
 * @return std::shared_ptr<muxer>
 */
std::shared_ptr<muxer> muxer::create(std::string name,
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
    } else {
      log_v2::config()->debug("muxer: muxer '{}' unknown, creating it", name);
      retval = std::shared_ptr<muxer>(
          new muxer(name, r_filter, w_filter, persistent));
      _running_muxers[name] = retval;
    }
  }

  engine::instance_ptr()->subscribe(retval);
  return retval;
}

/**
 *  Destructor.
 */
muxer::~muxer() noexcept {
  stats::center::instance().unregister_muxer(_name);
  auto eng = engine::instance_ptr();
  if (eng)
    eng->unsubscribe(this);
  std::lock_guard<std::mutex> lock(_mutex);
  log_v2::core()->info("Destroying muxer {}: number of events in the queue: {}",
                       _name, _events_size);
  _clean();
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
      "multiplexing: acknowledging {} events from {} event queue", count,
      _name);
  if (count) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (int i = 0; i < count && !_events.empty(); ++i) {
      if (_events.begin() == _pos) {
        log_v2::core()->error(
            "multiplexing: attempt to acknowledge "
            "more events than available in {} event queue: {} requested, {} "
            "acknowledged",
            _name, count, i);
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
  }
}

/**
 * @brief Flush the muxer and stop it (in this case, nothing to do to stop it).
 *
 * @return The number of acknowledged events.
 */
int32_t muxer::stop() {
  log_v2::core()->info("Stopping muxer {}: number of events in the queue: {}",
                       _name, _events_size);
  std::lock_guard<std::mutex> lck(_mutex);
  _update_stats();
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
    {  // we stop this first loop when mux queue is full on order to release
       // mutex to let read do his job before write to file
      std::lock_guard<std::mutex> lock(_mutex);
      for (; evt != event_queue.end() && _events_size < event_queue_max_size();
           ++evt) {
        if (!_write_filter.allows((*evt)->type())) {
          SPDLOG_LOGGER_TRACE(
              log_v2::core(),
              "muxer {} event of type {:x} rejected by write filter", _name,
              (*evt)->type());
          log_exclude_include(_name, (*evt)->type(), "publish", true);
          continue;
        }
        SPDLOG_LOGGER_TRACE(log_v2::core(),
                            "muxer {} event of type {:x} written", _name,
                            (*evt)->type());
        log_exclude_include(_name, (*evt)->type(), "publish", false);

        at_least_one_push_to_queue = true;
        _push_to_queue(*evt);
      }
    }
    if (evt == event_queue.end()) {
      return;
    }
    // we have stopped insertion because of full queue => retry
    if (at_least_one_push_to_queue) {
      continue;
    }
    // nothing pushed => to file
    std::lock_guard<std::mutex> lock(_mutex);
    for (; evt != event_queue.end(); ++evt) {
      if (!_write_filter.allows((*evt)->type())) {
        SPDLOG_LOGGER_TRACE(
            log_v2::core(),
            "muxer {} event of type {:x} rejected by write filter", _name,
            (*evt)->type());
        log_exclude_include(_name, (*evt)->type(), "publish to file", true);
        continue;
      } else {
        SPDLOG_LOGGER_TRACE(log_v2::core(),
                            "muxer {} event of type {:x} written", _name,
                            (*evt)->type());
        log_exclude_include(_name, (*evt)->type(), "publish to file", false);
      }
      if (!_file) {
        QueueFileStats* s =
            stats::center::instance().muxer_stats(_name)->mutable_queue_file();
        _file = std::make_unique<persistent_file>(_queue_file_name, s);
      }
      try {
        _file->write(*evt);
        SPDLOG_LOGGER_TRACE(log_v2::core(),
                            "muxer::publish {} publish one event to file {}",
                            _name, _queue_file_name);
      } catch (const std::exception& ex) {
        // in case of exception, we lost event. It's mandatory to avoid infinite
        // loop in case of permanent disk problem
        SPDLOG_LOGGER_ERROR(log_v2::core(), "{} fail to write event to {}: {}",
                            _name, _queue_file_name, ex.what());
        _file.reset();
      }
    }
  }
  _update_stats();
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
    else {
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

  if (!timed_out) {
    SPDLOG_LOGGER_TRACE(log_v2::core(), "{} read {}", _name, *event);
  }
  if (event)
    SPDLOG_LOGGER_TRACE(log_v2::core(), "muxer {} event of type {:x} read",
                        _name, event->type());
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
  SPDLOG_LOGGER_DEBUG(
      log_v2::core(),
      "multiplexing: reprocessing unacknowledged events from {} event queue",
      _name);
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
  if (d && _read_filter.allows(d->type())) {
    engine::instance_ptr()->publish(d);
    log_exclude_include(_name, d->type(), "write", false);
  } else {
    SPDLOG_LOGGER_TRACE(log_v2::core(),
                        "muxer {} event of type {:x} rejected by read filter",
                        _name, d->type());
    log_exclude_include(_name, d->type(), "write", true);
  }
  return 1;
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
  log_v2::core()->info("multiplexing: '{}' removed", _queue_file_name);

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
