/**
 * Copyright 2009-2013,2015, 2020-2024 Centreon
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

#include <unistd.h>

#include <cassert>

#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/multiplexing/event_sink.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/common/pool.hh"

namespace asio = boost::asio;

using namespace com::centreon::broker;
using namespace com::centreon::broker::multiplexing;
using log_v2 = com::centreon::common::log_v2::log_v2;

// Class instance.
std::shared_ptr<engine> engine::_instance{nullptr};
absl::Mutex engine::_load_m;

/**
 *  Get engine instance.
 *
 *  @return Class instance.
 */
std::shared_ptr<engine> engine::instance_ptr() {
  return _instance;
}

/**
 * @brief Load engine instance. The argument is the total size allowed for
 * queue files.
 */
void engine::load() {
  auto logger = log_v2::instance().get(log_v2::CORE);
  SPDLOG_LOGGER_TRACE(logger, "multiplexing: loading engine");
  absl::MutexLock lk(&_load_m);
  if (!_instance)
    _instance.reset(new engine(logger));
}

/**
 * @brief Unload the engine class instance.
 */
void engine::unload() {
  SPDLOG_LOGGER_TRACE(log_v2::instance().get(log_v2::CORE),
                      "multiplexing: unloading engine");
  auto instance = instance_ptr();
  if (instance) {
    {
      absl::ReleasableMutexLock lck(&instance->_kiew_m);
      /* Here we wait for all the subscriber muxers to be stopped and removed
       * from the muxers array. Even if they execute asynchronous functions,
       * they have finished after that. */
      auto muxers_empty =
          [&m = instance->_muxers, logger = instance->_logger]()
              ABSL_NO_THREAD_SAFETY_ANALYSIS {
                logger->debug("Still {} muxers configured in Broker engine",
                              m.size());
                return m.empty();
              };
      instance->_logger->info("Waiting for the destruction of subscribers");
      instance->_kiew_m.Await(absl::Condition(&muxers_empty));
    }

    absl::MutexLock lck(&_load_m);
    instance->stop();

    // Commit the cache file, if needed.
    if (instance->_cache_file) {
      // Drain _kiew under lock, then publish the copy outside to satisfy
      // ABSL_LOCKS_EXCLUDED(_kiew_m) on publish().
      std::deque<std::shared_ptr<io::data>> kiew_copy;
      {
        absl::MutexLock kiew_lck(&instance->_kiew_m);
        kiew_copy = std::move(instance->_kiew);
      }
      instance->publish(kiew_copy);
      instance->_cache_file->commit();
    }
    _instance.reset();
  }
}

/**
 *  Send an event to all subscribers.
 *
 *  @param[in] e  Event to publish.
 */
void engine::publish(const std::shared_ptr<io::data>& e) {
  /* A one-past-the-end pointer on the caller's own shared_ptr: the single event
   * path stays free of any container. */
  if (_enqueue(&e, &e + 1))
    _drain();
}

/**
 *  Send several events to all subscribers.
 *
 *  @param[in] to_publish  Events to publish.
 */
void engine::publish(const std::deque<std::shared_ptr<io::data>>& to_publish) {
  if (_enqueue(to_publish.begin(), to_publish.end()))
    _drain();
}

/**
 * @brief Queue events, and say whether the caller now owns the drain loop.
 *
 * The body the two publish() overloads share. Templated on the iterator rather
 * than on a container so that publishing one event does not have to build a
 * deque to carry it.
 *
 * @param first  Beginning of the events to queue.
 * @param last   End of the events to queue.
 *
 * @return true if the caller has to run the drain loop. False when a drain is
 * already running, which is just as good: that loop re-reads _kiew under
 * _kiew_m before giving up, so it cannot leave these events behind.
 */
template <typename It>
bool engine::_enqueue(It first, It last) {
  absl::MutexLock lck(&_kiew_m);
  switch (_state) {
    case stopped:
      SPDLOG_LOGGER_TRACE(_logger, "engine::publish {} event(s) to file",
                          std::distance(first, last));
      /* Null when stop() could not open the file, and when the engine was
       * stopped without ever having started — that path has no file to write
       * to. Dropping is all that is left; the failure was reported where it
       * happened. */
      if (_cache_file)
        for (It it = first; it != last; ++it)
          _cache_file->add(*it);
      else
        SPDLOG_LOGGER_ERROR(_logger,
                            "multiplexing: no cache file, {} event(s) lost",
                            std::distance(first, last));
      return false;
    case not_started:
      SPDLOG_LOGGER_TRACE(_logger, "engine::publish {} event(s) to queue",
                          std::distance(first, last));
      _kiew.insert(_kiew.end(), first, last);
      return false;
    default:
      SPDLOG_LOGGER_TRACE(_logger, "engine::publish {} event(s) to queue",
                          std::distance(first, last));
      _kiew.insert(_kiew.end(), first, last);
      if (_draining || _muxers.empty())
        return false;
      _draining = true;
      return true;
  }
}
/**
 *  Start multiplexing. This function gets back the retention content and
 *  inserts it in front of the engine's queue. Then all this content is
 *  published.
 */
void engine::start() {
  bool have_to_send = false;
  {
    absl::MutexLock lck(&_kiew_m);
    if (_state == not_started) {
      // Set writing method.
      SPDLOG_LOGGER_DEBUG(_logger, "multiplexing: engine starting");
      _state = running;
      _center->update(&EngineStats::set_mode, _stats, EngineStats::RUNNING);

      // Local queue.
      std::deque<std::shared_ptr<io::data>> kiew;
      // Get events from the cache file to the local queue.
      try {
        persistent_cache cache(_cache_file_path(), _logger);
        std::shared_ptr<io::data> d;
        for (;;) {
          cache.get(d);
          if (!d)
            break;
          kiew.push_back(d);
        }
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(_logger,
                            "multiplexing: engine couldn't read cache file: {}",
                            e.what());
      }

      // Copy global event queue to local queue.
      while (!_kiew.empty()) {
        kiew.push_back(_kiew.front());
        _kiew.pop_front();
      }

      // Send events queued while multiplexing was stopped.
      _kiew = std::move(kiew);
      have_to_send = true;
    }
  }
  if (have_to_send) {
    _wake_drainer();
    _wait_drained();
  }

  SPDLOG_LOGGER_INFO(_logger, "multiplexing: engine started");
}

/**
 * @brief Stop multiplexing. After a call to this function, all published events
 * are sent to an unprocessed persistent file. These events are not lost and
 * will be handled at the next cbd start.
 */
void engine::stop() {
  absl::ReleasableMutexLock lck(&_kiew_m);

  if (_state == not_started) {
    _state = stopped;
    return;
  }

  if (_state != stopped) {
    /* Opened before _state flips, and that order is the whole point: from the
     * moment the state is stopped, publish() writes to this file. It used to be
     * created only after the drain below, with _kiew_m released in between, so
     * a publish landing in that window dereferenced a null unique_ptr — and on
     * a first stop the pointer is always null, nothing else ever sets it.
     *
     * The cache file holds what is produced while the engine is stopped; it is
     * replayed at the next start. Opening it can still fail, which is why
     * publish() checks the pointer as well. */
    try {
      _cache_file =
          std::make_unique<persistent_cache>(_cache_file_path(), _logger);
      _cache_file->transaction();
    } catch (const std::exception& e) {
      _logger->error("multiplexing: could not open cache file: {}", e.what());
      _cache_file.reset();
    }

    // Set writing method.
    _state = stopped;
    _center->update(&EngineStats::set_mode, _stats, EngineStats::STOPPED);
    lck.Release();
    // Notify hooks of multiplexing loop end.
    SPDLOG_LOGGER_INFO(_logger, "multiplexing: stopping engine");

    _logger->trace("stop: waiting for sending to subscribers to end in stop");
    _wake_drainer();
    _wait_drained();

    _logger->trace("stop: all events sent to subscribers");

    SPDLOG_LOGGER_DEBUG(_logger, "multiplexing: engine stopped");
  }
}

/**
 *  Subscribe a muxer to the multiplexing engine if not already subscribed.
 *
 *  @param[in] subscriber  A muxer.
 */
void engine::subscribe(const std::shared_ptr<muxer>& subscriber) {
  _logger->debug("engine: muxer {} subscribes to engine", subscriber->name());
  absl::MutexLock lck(&_kiew_m);
  for (auto& m : _muxers)
    if (m.mux.lock() == subscriber) {
      _logger->debug("engine: muxer {} already subscribed", subscriber->name());
      /* muxer::create sets the filters of a reused muxer before subscribing it
       * again, so an already known subscriber may carry a new one. */
      m.filter = subscriber->write_filter();
      return;
    }
  _muxers.push_back(
      {subscriber, subscriber->write_filter(), subscriber->name()});
}

/**
 *  Take note of a muxer's new write filter.
 *
 *  @param[in] subscriber  The muxer whose filter changed.
 *  @param[in] filter      Its new write filter.
 */
void engine::update_write_filter(const muxer* subscriber,
                                 const muxer_filter& filter) {
  absl::MutexLock lck(&_kiew_m);
  for (auto& m : _muxers) {
    auto w = m.mux.lock();
    if (w && w.get() == subscriber) {
      m.filter = filter;
      return;
    }
  }
}

/**
 *  Unsubscribe from the multiplexing engine.
 *
 *  @param[in] subscriber  Subscriber.
 */
void engine::unsubscribe_muxer(const muxer* subscriber) {
  /* No waiting for a batch in flight here, on purpose. A batch already being
   * distributed holds its targets in _hold, so it keeps serving this muxer to
   * the end whatever we do to the subscriber list — the safety comes from
   * there, not from waiting. And waiting would be a trap: ~muxer unsubscribes
   * itself, so when the drainer releases the last reference to a muxer, this
   * runs in the drainer's own thread and would wait for the drain it is itself
   * performing. */
  _logger->trace("unsubscribe_muxer: removing muxer {:p} from engine",
                 static_cast<const void*>(subscriber));
  absl::MutexLock lck(&_kiew_m);

  auto logger = log_v2::instance().get(log_v2::CONFIG);
  /* The whole list is walked, and expired entries are dropped along the way
   * rather than being mistaken for the caller. Stopping at the first `!w` used
   * to erase some *other*, already dead muxer and return, leaving `subscriber`
   * subscribed — and a feeder that unsubscribes still holds its muxer alive
   * (feeder::_stop_no_lock), so it went on receiving the very events it had
   * just asked not to receive.
   *
   * Erasing here can drop no muxer: the entries hold weak_ptr, so no ~muxer
   * runs under _kiew_m. */
  for (auto it = _muxers.begin(); it != _muxers.end();) {
    auto w = it->mux.lock();
    if (!w) {
      it = _muxers.erase(it);
    } else if (w.get() == subscriber) {
      /* Our own copy of the name, not subscriber->name(): the caller is very
       * often a muxer running its own destructor. */
      logger->debug("multiplexing: muxer {} unsubscribed from Engine",
                    it->name);
      it = _muxers.erase(it);
    } else {
      ++it;
    }
  }
}

/**
 *  Default constructor.
 */
engine::engine(const std::shared_ptr<spdlog::logger>& logger)
    : _state{not_started},
      _center{config::applier::state::instance().center()},
      _stats{_center->register_engine()},
      _logger{logger} {
  _center->update(&EngineStats::set_mode, _stats, EngineStats::NOT_STARTED);
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
  absl::EnableMutexInvariantDebugging(true);
}

engine::~engine() noexcept {
  /* Muxers should be unsubscribed before arriving here. */
  assert(_muxers.empty());
  SPDLOG_LOGGER_DEBUG(_logger, "core: cbd engine destroyed.");
  DEBUG(fmt::format("DESTRUCTOR engine {:p}", static_cast<void*>(this)));
}

/**
 *  Generate path to the multiplexing engine cache file.
 *
 *  @return Path to the multiplexing engine cache file.
 */
std::string engine::_cache_file_path() const {
  std::string retval(fmt::format(
      "{}.unprocessed", config::applier::state::instance().cache_dir()));
  return retval;
}

/**
 * @brief Start a drain loop, unless one is already running.
 *
 * Nothing is returned and nothing is waited for: a drain already in progress is
 * as good as one we started, since it re-reads the queue before giving up.
 */
void engine::_wake_drainer() {
  bool mine = false;
  {
    absl::MutexLock lck(&_kiew_m);
    if (!_draining && !_kiew.empty() && !_muxers.empty()) {
      _draining = true;
      mine = true;
    }
  }
  if (mine)
    _drain();
}

/**
 * @brief Block until the queue is empty and no batch is in flight.
 *
 * Replaces the promise/future dance of start(), stop() and unsubscribe_muxer().
 * Those used to wait only when _send_to_subscribers returned true, and that
 * return value did not distinguish "nothing to send" from "a batch is already
 * in flight" — so they could carry on while events were still being
 * distributed. Waiting on the state itself has no such hole.
 *
 * Muxer-less is a terminal state too: with no subscriber nobody will ever drain
 * the queue, and waiting for it would hang.
 */
void engine::_wait_drained() {
  absl::MutexLock lck(&_kiew_m);
  auto drained = [this]() ABSL_NO_THREAD_SAFETY_ANALYSIS {
    return !_draining && (_kiew.empty() || _muxers.empty());
  };
  _kiew_m.Await(absl::Condition(&drained));
}

/**
 * @brief Give up one share of the batch in flight.
 *
 * @return true if the caller was the last consumer, and therefore now owns the
 * drain loop.
 */
bool engine::_one_done() {
  return _pending.fetch_sub(1, std::memory_order_acq_rel) == 1;
}

/**
 * @brief Hand batch after batch to the subscribed muxers, until the queue is
 * dry.
 *
 * Runs in the publishing thread on its first turn, then in whichever worker
 * finished last. Only one instance of this loop exists at any time — that is
 * what _draining guarantees — and it takes a new batch only once the previous
 * one has been fully consumed, which is what lets _batch, _hold and _targets be
 * reused members rather than a fresh deque per batch.
 *
 * Muxers are all posted, none published from here: a muxer whose in-memory
 * queue is full writes to its retention file, and that must never land in the
 * thread of whoever published the event — Engine's check loop, typically.
 */
void engine::_drain() {
  /* The posted lambdas outlive this call. Holding the instance keeps the
   * engine, and the mutexes they touch, alive — the role callback_caller's
   * shared_ptr used to play. */
  std::shared_ptr<engine> keep_alive = _instance;

  for (;;) {
    /* Released outside _kiew_m, and deliberately so: _hold may carry the last
     * reference to a muxer, and ~muxer unsubscribes itself, which takes
     * _kiew_m. absl::Mutex is not recursive, so clearing this under the lock
     * would deadlock the moment a feeder disappears while its batch is in
     * flight. Safe here: the previous batch is fully consumed by now. */
    _batch.clear();
    _hold.clear();
    _targets.clear();

    {
      absl::MutexLock lck(&_kiew_m);
      if (_kiew.empty() || _muxers.empty()) {
        _draining = false;
        return;
      }

      SPDLOG_LOGGER_TRACE(_logger, "engine::_drain send {} events to {} muxers",
                          _kiew.size(), _muxers.size());

      std::swap(_kiew, _batch);

      /* One mask for the whole batch, so each muxer is settled with a single
       * test. This is what keeps a cbd serving 100 pollers from posting to 100
       * muxers when 2 of them want the events: the others used to be posted
       * anyway, walk the batch, and reject all of it. */
      muxer_filter wanted({});
      for (const auto& e : _batch)
        wanted.insert(e->type());

      /* Skipping a muxer is where its write filter now rejects events, so this
       * is where the rejections have to be journalled: muxer::publish, which
       * used to do it, is never reached for a batch nobody wants. Guarded
       * rather than left to the macro, because what costs here is the walk, not
       * the formatting — the point of the mask test above is not to walk the
       * batch once per muxer. */
      const bool log_rejects = _logger->should_log(spdlog::level::trace);

      /* _hold and _targets are emptied in the head of the loop, outside the
       * lock, and must not be emptied again here: dropping the last reference
       * to a muxer runs ~muxer, which unsubscribes, which takes _kiew_m —
       * deadlock on a non-recursive mutex. */
      for (const auto& s : _muxers) {
        if (!s.filter.contains_some_of(wanted)) {
          if (log_rejects)
            for (const auto& e : _batch)
              SPDLOG_LOGGER_TRACE(
                  _logger,
                  "muxer {} event of type {:x} rejected by write filter",
                  s.name, e->type());
          continue;
        }
        if (auto m = s.mux.lock()) {
          _hold.push_back(std::move(m));
          _targets.push_back(_hold.back().get());
        }
      }
    }

    /* Read once and reused below: _notification_sink is a bare pointer cleared
     * at teardown, and counting a consumer we then fail to post would leave
     * _pending stuck above zero — that is, a drain that never ends and an
     * engine that never publishes again. */
    event_sink* sink = _notification_sink;

    /* Armed before anything is posted, or a quick worker could reach zero while
     * we are still posting and a second loop would start. The extra share is
     * the drainer's own, held while it reads _batch below. */
    _pending.store(_targets.size() + (sink ? 1 : 0) + 1,
                   std::memory_order_release);

    for (muxer* m : _targets)
      asio::post(
          com::centreon::common::pool::io_context(), [this, keep_alive, m] {
            try {
              m->publish(_batch);
            }  // pool threads protection
            catch (const std::exception& ex) {
              SPDLOG_LOGGER_ERROR(_logger, "publish caught exception: {}",
                                  ex.what());
            } catch (...) {
              SPDLOG_LOGGER_ERROR(_logger, "publish caught unknown exception");
            }
            if (_one_done())
              _drain();
          });

    _center->update(&EngineStats::set_processed_events, _stats,
                    static_cast<uint32_t>(_batch.size()));
    config::applier::state::instance().cache().publish(_batch);

    /* Notification driver (notification_mode=broker only): posted AFTER
     * cache.publish so the sink sees a cache already up to date with this
     * batch, and counted in _pending so the next batch waits for it
     * (serialization). nullptr in engine mode → nothing posted. */
    if (sink)
      asio::post(com::centreon::common::pool::io_context(),
                 [this, keep_alive, sink] {
                   sink->on_events(_batch);
                   if (_one_done())
                     _drain();
                 });

    /* Released last, so nobody can clear _batch while we were still reading it.
     * If everyone else is already done, we carry on with the next batch here
     * rather than handing the loop over. */
    if (!_one_done())
      return;
  }
}

/**
 * @brief Clear events stored in the multiplexing engine.
 */
void engine::clear() {
  absl::MutexLock lck(&_kiew_m);
  _kiew.clear();
}
