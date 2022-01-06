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

/**
 *  Constructor.
 *
 *  @param[in] name        Name associated to this muxer. It is used to
 *                         create on-disk files.
 *  @param[in] persistent  Whether or not this muxer should backup
 *                         unprocessed events in a persistent storage.
 */
muxer::muxer(std::string name,
             muxer::filters r_filters,
             muxer::filters w_filters,
             bool persistent)
    : io::stream("muxer"),
      _queue_file_enabled{false},
      _name(std::move(name)),
      _read_filters{std::move(r_filters)},
      _write_filters{std::move(w_filters)},
      _read_filters_str{misc::dump_filters(_read_filters)},
      _write_filters_str{misc::dump_filters(_write_filters)},
      _persistent(persistent),
      _stats{stats::center::instance().register_muxer(name)},
      _clk{std::time(nullptr)} {
  // Load head queue file back in memory.
  if (_persistent) {
    try {
      auto mf{std::make_unique<persistent_file>(_memory_file())};
      std::shared_ptr<io::data> e;
      for (;;) {
        e.reset();
        mf->read(e, 0);
        if (e) {
          _events.push_back(e);
        }
      }
    } catch (const exceptions::shutdown& e) {
      // Memory file was properly read back in memory.
      (void)e;
    }
  }

  // Load queue file back in memory.
  try {
    _file.reset(new persistent_file(queue_file(_name)));
    _queue_file_enabled = true;
    _queue_file_name = queue_file(_name);
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
    } while (_events.size() < event_queue_max_size());
  } catch (const exceptions::shutdown& e) {
    // Queue file was entirely read back.
    (void)e;
  }
  _pos = _events.begin();

  updateStats();

  // Log messages.
  log_v2::perfdata()->info(
      "multiplexing: '{}' starts with {} in queue and the queue file is {}",
      _name, _events.size(), _queue_file_enabled ? "enable" : "disable");

  engine::instance().subscribe(this);
}

/**
 *  Destructor.
 */
muxer::~muxer() noexcept {
  stats::center::instance().unregister_muxer(_name);
  engine::instance().unsubscribe(this);
  log_v2::core()->info("Destroying muxer {}: number of events in the queue: {}",
                       _name, _events.size());
  _clean();
}

/**
 *  Acknowledge events.
 *
 *  @param[in] count  Number of events to acknowledge.
 */
void muxer::ack_events(int count) {
  // Remove acknowledged events.
  log_v2::perfdata()->debug(
      "multiplexing: acknowledging {} events from {} event queue", count,
      _name);
  if (count) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (int i = 0; i < count && !_events.empty(); ++i) {
      if (_events.begin() == _pos) {
        log_v2::perfdata()->error(
            "multiplexing: attempt to acknowledge "
            "more events than available in {} event queue: {} requested, {} "
            "acknowledged",
            _name, count, i);
        break;
      }
      _events.pop_front();
    }
    log_v2::perfdata()->trace("multiplexing: still {} events in {} event queue",
                              _events.size(), _name);

    // Fill memory from file.
    std::shared_ptr<io::data> e;
    while (_events.size() < event_queue_max_size()) {
      _get_event_from_file(e);
      if (!e)
        break;
      _push_to_queue(e);
    }
  }
  updateStats();
}

/**
 * @brief Flush the muxer and stop it (in this case, nothing to do to stop it).
 *
 * @return The number of acknowledged events.
 */
int32_t muxer::stop() {
  log_v2::core()->info("Stopping muxer {}: number of events in the queue: {}",
                       _name, _events.size());
  updateStats();
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
void muxer::publish(const std::shared_ptr<io::data> event) {
  if (event) {
    std::lock_guard<std::mutex> lock(_mutex);
    // Check if we should process this event.
    if (_write_filters.find(event->type()) == _write_filters.end())
      return;
    // Check if the event queue limit is reach.
    if (_events.size() >= event_queue_max_size()) {
      // Try to create file if is necessary.
      if (!_file) {
        _file.reset(new persistent_file(queue_file(_name)));
        _queue_file_enabled = true;
        _queue_file_name = queue_file(_name);
      }
      _file->write(event);
    } else
      _push_to_queue(event);
  }
  updateStats();
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
      lock.unlock();
      if (event)
        timed_out = false;
    } else
      event.reset();
  }
  // Data is available, no need to wait.
  else {
    event = *_pos;
    ++_pos;
    lock.unlock();
  }

  updateStats();

  return !timed_out;
}

/**
 *  Get the read filters.
 *
 *  @return  The read filters.
 */
const muxer::filters& muxer::read_filters() const {
  return _read_filters;
}

/**
 *  Get the write filters.
 *
 *  @return  The write filters.
 */
const muxer::filters& muxer::write_filters() const {
  return _write_filters;
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
  return _events.size();
}

/**
 *  Reprocess non-acknowledged events.
 */
void muxer::nack_events() {
  log_v2::perfdata()->debug(
      "multiplexing: reprocessing unacknowledged events from {} event queue",
      _name);
  std::lock_guard<std::mutex> lock(_mutex);
  _pos = _events.begin();
  updateStats();
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
  int unacknowledged = 0;
  for (auto it = _events.begin(); it != _pos; ++it)
    ++unacknowledged;
  tree["unacknowledged_events"] = unacknowledged;
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
  if (d && _read_filters.find(d->type()) != _read_filters.end())
    engine::instance().publish(d);
  return 1;
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
 *  Release all events stored within the internal list.
 */
void muxer::_clean() {
  std::lock_guard<std::mutex> lock(_mutex);
  _file.reset();
  _queue_file_enabled = false;
  _queue_file_name.clear();
  if (_persistent && !_events.empty()) {
    try {
      log_v2::core()->trace("muxer: sending {} events to {}", _events.size(),
                            _memory_file());
      auto mf{std::make_unique<persistent_file>(_memory_file())};
      while (!_events.empty()) {
        mf->write(_events.front());
        _events.pop_front();
      }
    } catch (std::exception const& e) {
      log_v2::perfdata()->error(
          "multiplexing: could not backup memory queue of '{}': {}", _name,
          e.what());
    }
  }
  _events.clear();
  updateStats();
}

/**
 *  Get event from retention file. Warning: lock _mutex before using
 *  this function.
 *
 *  @param[out] event  Last event available. Null if none is available.
 */
void muxer::_get_event_from_file(std::shared_ptr<io::data>& event) {
  event.reset();
  // If file exists, try to get the last event.
  if (_file) {
    _queue_file_name = queue_file(_name);
    try {
      do {
        _file->read(event);
      } while (!event);
    } catch (exceptions::shutdown const& e) {
      // The file end was reach.
      (void)e;
      _file.reset();
      _queue_file_enabled = false;
      _queue_file_name.clear();
    }
  }
  updateStats();
}

/**
 *  Get memory file path.
 *
 *  @return Path to the memory file.
 */
std::string muxer::_memory_file() const {
  return memory_file(_name);
}

/**
 *  Push event to queue (_mutex is locked when this method is called).
 *
 *  @param[in] event  New event.
 */
void muxer::_push_to_queue(std::shared_ptr<io::data> const& event) {
  bool pos_has_no_more_to_read(_pos == _events.end());
  _events.push_back(event);

  if (pos_has_no_more_to_read) {
    _pos = --_events.end();
    _cv.notify_one();
  }

  updateStats();
}

/**
 * @brief Fill statistics if it happened more than 1 second ago
 */
void muxer::updateStats() noexcept {
  auto now(std::time(nullptr));
  if ((now - _clk) > 1000) {
    _clk = now;
    stats::center::instance().execute([this]() {
      _stats->set_queue_file_enabled(_queue_file_enabled);
      _stats->set_queue_file(_queue_file_name);
      _stats->set_unacknowledged_events(
          std::to_string(std::distance(_events.begin(), _pos)));
    });
  }
}

/**
 *  Remove all the queue files attached to this muxer.
 */
void muxer::remove_queue_files() {
  log_v2::perfdata()->info("multiplexing: '{}' removed", queue_file(_name));

  /* Here _file is already destroyed */
  persistent_file file(queue_file(_name));
  file.remove_all_files();
}

const std::string& muxer::name() const {
  return _name;
}
