/**
 * Copyright 2009-2013,2015-2017,2019-2024 Centreon
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
#include <absl/time/time.h>

#include <cassert>

#include "com/centreon/broker/bbdo/internal.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/common/time.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::multiplexing;
using log_v2 = com::centreon::common::log_v2::log_v2;

static absl::Mutex _add_bench_point_m;
/**
 * @brief add a bench point to pb_bench event
 * as several muxers can update it at the same time, it's protected by a mutex
 *
 * @param event
 * @param tp_name
 */
static void add_bench_point(bbdo::pb_bench& event,
                            const std::string& muxer_name,
                            const char* funct_name)
    ABSL_LOCKS_EXCLUDED(_add_bench_point_m) {
  absl::MutexLock lck(&_add_bench_point_m);
  com::centreon::broker::TimePoint* muxer_tp = event.mut_obj().add_points();
  muxer_tp->set_name(muxer_name);
  muxer_tp->set_function(funct_name);
  com::centreon::common::time_point_to_google_ts(
      std::chrono::system_clock::now(), *muxer_tp->mutable_time());
}

uint32_t muxer::_event_queue_max_size = std::numeric_limits<uint32_t>::max();

absl::Mutex muxer::_running_muxers_m;
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
      _center{stats::center::instance_ptr()},
      _last_stats{std::time(nullptr)},
      _logger{log_v2::instance().get(log_v2::CORE)} {
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
  absl::EnableMutexInvariantDebugging(true);
  // Load head queue file back in memory.
  absl::MutexLock lck(&_events_m);
  if (_persistent) {
    try {
      auto mf{std::make_unique<persistent_file>(memory_file(_name), nullptr)};
      std::shared_ptr<io::data> e;
      for (;;) {
        e.reset();
        mf->read(e, 0);
        if (e) {
          _events.push_back(std::move(e));
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
    QueueFileStats* stats = _center->muxer_stats(_name)->mutable_queue_file();
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
      _events.push_back(std::move(e));
      ++_events_size;
    } while (_events_size < event_queue_max_size());
  } catch (const exceptions::shutdown& e) {
    // Queue file was entirely read back.
    (void)e;
  }

  _update_stats();

  // Log messages.
  SPDLOG_LOGGER_INFO(
      _logger,
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
    absl::MutexLock lck(&_running_muxers_m);
    absl::erase_if(_running_muxers,
                   [](const std::pair<std::string, std::weak_ptr<muxer>>& p) {
                     return p.second.expired();
                   });
    retval = _running_muxers[name].lock();
    if (retval) {
      log_v2::instance()
          .get(log_v2::CONFIG)
          ->debug("muxer: muxer '{}' already exists, reusing it", name);
      retval->set_read_filter(r_filter);
      retval->set_write_filter(w_filter);
      SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::CORE),
                         "multiplexing: reuse '{}' starts with {} in queue and "
                         "the queue file is {}",
                         name, retval->_events_size,
                         retval->_file ? "enable" : "disable");

    } else {
      log_v2::instance()
          .get(log_v2::CONFIG)
          ->debug("muxer: muxer '{}' unknown, creating it", name);
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
  {
    absl::MutexLock lock(&_events_m);
    SPDLOG_LOGGER_INFO(
        _logger, "Destroying muxer {:p} {}: number of events in the queue: {}",
        static_cast<void*>(this), _name, _events_size);
    _clean();
  }
  /* We must unsubscribe once _clean() is over. This is because _clean() is
   * calling the Broker engine and the engine is unloaded once it has no more
   * muxer in its array. As the destructor may be called asynchronously, we
   * must be sure the Broker engine is used while it exists. */
  unsubscribe();

  // caution, unregister_muxer must be the last center method called at muxer
  // destruction to avoid re create a muxer stat entry
  _center->unregister_muxer(_name);
}

/**
 *  Acknowledge events.
 *
 *  @param[in] count  Number of events to acknowledge.
 */
void muxer::ack_events(int count) {
  // Remove acknowledged events.
  SPDLOG_LOGGER_TRACE(
      _logger,
      "multiplexing: acknowledging {} events from {} event queue size: {}",
      count, _name, _events_size);

  if (count) {
    SPDLOG_LOGGER_DEBUG(
        _logger, "multiplexing: acknowledging {} events from {} event queue",
        count, _name);
    absl::MutexLock lck(&_events_m);
    for (int i = 0; i < count && !_events.empty(); ++i) {
      if (_events.begin() == _pos) {
        _logger->error(
            "multiplexing: attempt to acknowledge more events than available "
            "in {} event queue: {} size: {}, requested, {} acknowledged",
            _name, _events_size, count, i);
        break;
      }
      _events.pop_front();
      --_events_size;
    }
    SPDLOG_LOGGER_TRACE(_logger,
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
        _logger, "multiplexing: acknowledging no events from {} event queue",
        _name);
  }
}

/**
 * @brief Flush the muxer and stop it (in this case, nothing to do to stop it).
 *
 * @return The number of acknowledged events.
 */
int32_t muxer::stop() {
  SPDLOG_LOGGER_INFO(_logger,
                     "Stopping muxer {}: number of events in the queue: {}",
                     _name, _events_size);
  absl::MutexLock lck(&_events_m);
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
 * @brief Execute the data_handler() method if it exists and if its execution is
 * needed, in other words, if there are no data available it is not necessary to
 * execute the data handler.
 */
void muxer::_execute_reader_if_needed() {
  SPDLOG_LOGGER_DEBUG(
      _logger, "muxer '{}' execute reader if needed data_handler", _name);
  bool expected = false;
  if (_reader_running.compare_exchange_strong(expected, true)) {
    com::centreon::common::pool::io_context_ptr()->post(
        [me = shared_from_this(), this] {
          std::shared_ptr<data_handler> to_call;
          {
            absl::MutexLock lck(&_events_m);
            to_call = _data_handler;
          }
          if (to_call) {
            std::vector<std::shared_ptr<io::data>> to_fill;
            to_fill.reserve(_events_size);
            bool still_events_to_read [[maybe_unused]] =
                read(to_fill, _events_size);
            uint32_t written = to_call->on_events(to_fill);
            if (written > 0)
              ack_events(written);
            if (written != to_fill.size()) {
              SPDLOG_LOGGER_ERROR(
                  _logger,
                  "Unable to handle all the incoming events in muxer '{}'",
                  _name);
              clear_action_on_new_data();
            }
            _reader_running.store(false);
          }
        });
  }
}

/**
 *  Add a new event to the internal event list.
 *
 *  @param[in] event Event to add.
 */
void muxer::publish(const std::deque<std::shared_ptr<io::data>>& event_queue) {
  _logger->debug("muxer {:p}:publish on muxer '{}': {} events",
                 static_cast<void*>(this), _name, event_queue.size());
  auto evt = event_queue.begin();
  while (evt != event_queue.end()) {
    bool at_least_one_push_to_queue = false;
    {
      // we stop this first loop when mux queue is full in order to release
      // mutex to let read to do its job before writing to file
      absl::MutexLock lck(&_events_m);
      _logger->trace(
          "muxer::publish ({}) starting the loop to stack events --- "
          "events_size = {} <> {}",
          _name, _events_size, event_queue_max_size());
      for (; evt != event_queue.end() && _events_size < event_queue_max_size();
           ++evt) {
        auto event = *evt;
        if (!_write_filter.allows(event->type())) {
          SPDLOG_LOGGER_TRACE(_logger,
                              "muxer {} event {} rejected by write filter",
                              _name, *event);
          continue;
        }
        if (event->type() == bbdo::pb_bench::static_type()) {
          add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(event),
                          _name, "publish");
          SPDLOG_LOGGER_INFO(_logger, "{} bench publish {}", _name,
                             io::data::dump_json{*event});
        }

        SPDLOG_LOGGER_TRACE(
            _logger, "muxer {} event of type {:x} written --- queue size: {}",
            _name, event->type(), _events_size);

        at_least_one_push_to_queue = true;

        _push_to_queue(event);
      }
      _logger->trace("muxer::publish ({}) loop finished", _name);
      if (at_least_one_push_to_queue ||
          _events_size >= event_queue_max_size())  // async handler waiting?
        _execute_reader_if_needed();
    }

    if (evt == event_queue.end()) {
      absl::MutexLock lck(&_events_m);
      _update_stats();
      return;
    }

    // we have stopped insertion because of full queue => retry
    if (at_least_one_push_to_queue) {
      continue;
    }
    /* The queue is full. The rest is put in the retention file. */
    absl::MutexLock lck(&_events_m);
    for (; evt != event_queue.end(); ++evt) {
      auto event = *evt;
      if (!_write_filter.allows(event->type())) {
        SPDLOG_LOGGER_TRACE(
            _logger, "muxer {} event of type {:x} rejected by write filter",
            _name, event->type());
        continue;
      }
      if (event->type() == bbdo::pb_bench::static_type()) {
        add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(event), _name,
                        "retention_publish");
        SPDLOG_LOGGER_INFO(_logger, "muxer {} bench publish to file {} {}",
                           _name, _queue_file_name,
                           io::data::dump_json{*event});
      }
      if (!_file) {
        QueueFileStats* s = _center->muxer_stats(_name)->mutable_queue_file();
        _file = std::make_unique<persistent_file>(_queue_file_name, s);
      }
      try {
        _file->write(event);
        SPDLOG_LOGGER_TRACE(
            _logger,
            "{} publish one event of type {:x} to file {} queue size:{}", _name,
            event->type(), _queue_file_name, _events_size);
      } catch (const std::exception& ex) {
        // in case of exception, we lost event. It's mandatory to avoid
        // infinite loop in case of permanent disk problem
        SPDLOG_LOGGER_ERROR(_logger, "{} fail to write event to {}: {}", _name,
                            _queue_file_name, ex.what());
        _file.reset();
      }
    }
    _update_stats();
  }
  _logger->debug("muxer {:p}:publish on muxer '{}': finished",
                 static_cast<void*>(this), _name);
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
  _logger->trace("muxer {:p}:read() call", static_cast<void*>(this));
  bool timed_out{false};
  absl::MutexLock lck(&_events_m);

  // No data is directly available.
  if (_pos == _events.end()) {
    // Wait a while if subscriber was not shutdown.
    if ((time_t)-1 == deadline)
      _no_event_cv.Wait(&_events_m);
    else if (!deadline)
      timed_out = true;
    else
      _no_event_cv.WaitWithDeadline(&_events_m, absl::FromTimeT(deadline));

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
    SPDLOG_LOGGER_TRACE(_logger, "{} read {} queue size {}", _name, *event,
                        _events_size);
    if (event->type() == bbdo::pb_bench::static_type()) {
      add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(event), _name,
                      "read");
      SPDLOG_LOGGER_INFO(_logger, "{} bench read {}", _name,
                         io::data::dump_json{*event});
    }
  } else {
    SPDLOG_LOGGER_TRACE(_logger, "{} queue size {} no event available", _name,
                        _events_size);
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
  absl::MutexLock lck(&_events_m);
  return _events_size;
}

/**
 *  Reprocess non-acknowledged events.
 */
void muxer::nack_events() {
  SPDLOG_LOGGER_DEBUG(_logger,
                      "multiplexing: reprocessing unacknowledged events from "
                      "{} event queue with {} waiting events",
                      _name, _events_size);
  absl::MutexLock lck(&_events_m);
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
  absl::MutexLock lck(&_events_m);

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
  _no_event_cv.SignalAll();
}

/**
 *  Send an event to multiplexing.
 *
 *  @param[in] d  Event to multiplex.
 */
int muxer::write(std::shared_ptr<io::data> const& d) {
  _logger->debug("write on muxer '{}'", _name);
  if (!d) {
    return 1;
  }
  if (_read_filter.allows(d->type())) {
    if (d->type() == bbdo::pb_bench::static_type()) {
      add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(d), _name,
                      "write");
      SPDLOG_LOGGER_INFO(_logger, "{} bench write {}", _name,
                         io::data::dump_json{*d});
    }
    _engine->publish(d);
  } else {
    SPDLOG_LOGGER_TRACE(_logger,
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
  _logger->debug("write on muxer '{}' {} events (bulk)", _name,
                 to_publish.size());
  for (auto list_iter = to_publish.begin();
       !to_publish.empty() && list_iter != to_publish.end();) {
    const std::shared_ptr<io::data>& d = *list_iter;
    if (_read_filter.allows(d->type())) {
      if (d->type() == bbdo::pb_bench::static_type()) {
        add_bench_point(*std::static_pointer_cast<bbdo::pb_bench>(d), _name,
                        "write");
        SPDLOG_LOGGER_INFO(_logger, "{} bench write {}", _name,
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
 *  Warning: _events_m must be locked to call this function.
 */
void muxer::_clean() {
  _file.reset();
  _center->clear_muxer_queue_file(_name);
  if (_persistent && !_events.empty()) {
    try {
      SPDLOG_LOGGER_TRACE(_logger, "muxer: sending {} events to {}",
                          _events_size, memory_file(_name));
      auto mf{std::make_unique<persistent_file>(memory_file(_name), nullptr)};
      while (!_events.empty()) {
        mf->write(_events.front());
        _events.pop_front();
        --_events_size;
      }
    } catch (std::exception const& e) {
      _logger->error("multiplexing: could not backup memory queue of '{}': {}",
                     _name, e.what());
    }
  }
  _events.clear();
  _events_size = 0;
  _pos = _events.begin();
  _update_stats();
}

/**
 *  Get event from retention file.
 *  Warning: lock _events_m before using this function.
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
      _center->clear_muxer_queue_file(_name);
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
 *  Push event to queue (_events_m is locked when this method is called).
 *
 *  @param[in] event  New event.
 */
void muxer::_push_to_queue(std::shared_ptr<io::data> const& event) {
  bool pos_has_no_more_to_read(_pos == _events.end());
  SPDLOG_LOGGER_TRACE(_logger, "muxer {} event of type {:x} pushed", _name,
                      event->type());
  _events.push_back(event);
  ++_events_size;

  if (pos_has_no_more_to_read) {
    _pos = --_events.end();
    _no_event_cv.Signal();
  }
}

/**
 * @brief Fill statistics if it happened more than 1 second ago
 *
 * Warning: _events_m must be locked before while calling this function.
 */
void muxer::_update_stats() noexcept {
  std::time_t now{std::time(nullptr)};
  if (now - _last_stats > 0) {
    _last_stats = now;
    /* Since _events_m is locked, we can get interesting values and copy them
     * in the capture. Then the execute() function can put them in the stats
     * object asynchronously. */
    _center->update_muxer(_name, _file ? _queue_file_name : "", _events_size,
                          std::distance(_events.begin(), _pos));
  }
}

/**
 *  Remove all the queue files attached to this muxer.
 */
void muxer::remove_queue_files() {
  SPDLOG_LOGGER_INFO(_logger, "multiplexing: '{}' removed", _queue_file_name);

  /* Here _file is already destroyed */
  QueueFileStats* stats = _center->muxer_stats(_name)->mutable_queue_file();
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
  _logger->trace("multiplexing: '{}' set read filter...", _name);
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
  _logger->trace("multiplexing: '{}' set write filter...", _name);
  _write_filter = w_filter;
  _write_filters_str = misc::dump_filters(w_filter);
}

/**
 * @brief Unsubscribe this muxer from the parent engine.
 */
void muxer::unsubscribe() {
  _logger->debug("multiplexing: unsubscribe '{}'", _name);
  _engine->unsubscribe_muxer(this);
}

void muxer::set_action_on_new_data(
    const std::shared_ptr<data_handler>& handler) {
  absl::MutexLock lck(&_events_m);
  _data_handler = handler;
}

void muxer::clear_action_on_new_data() {
  absl::MutexLock lck(&_events_m);
  _data_handler.reset();
}
