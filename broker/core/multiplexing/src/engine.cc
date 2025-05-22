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

#include "com/centreon/broker/multiplexing/engine.hh"

#include <absl/synchronization/mutex.h>
#include <unistd.h>

#include <cassert>

#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

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
      auto muxers_empty = [&m = instance->_muxers,
                           logger = instance->_logger]() {
        logger->debug("Still {} muxers configured in Broker engine", m.size());
        return m.empty();
      };
      instance->_logger->info("Waiting for the destruction of subscribers");
      instance->_kiew_m.Await(absl::Condition(&muxers_empty));
    }

    absl::MutexLock lck(&_load_m);
    instance->stop();

    // Commit the cache file, if needed.
    if (instance->_cache_file) {
      // In case of muxers removed from the Engine and still events in _kiew
      instance->publish(instance->_kiew);
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
  bool have_to_send = false;
  {
    absl::MutexLock lck(&_kiew_m);
    switch (_state) {
      case stopped:
        SPDLOG_LOGGER_TRACE(_logger, "engine::publish one event to file");
        _cache_file->add(e);
        _unprocessed_events++;
        break;
      case not_started:
        SPDLOG_LOGGER_TRACE(_logger, "engine::publish one event to queue");
        _kiew.push_back(e);
        break;
      default:
        SPDLOG_LOGGER_TRACE(_logger, "engine::publish one event to queue_");
        _kiew.push_back(e);
        have_to_send = true;
        break;
    }
  }
  if (have_to_send) {
    _send_to_subscribers(nullptr);
  }
}

void engine::publish(const std::deque<std::shared_ptr<io::data>>& to_publish) {
  bool have_to_send = false;
  {
    absl::MutexLock lck(&_kiew_m);
    switch (_state) {
      case stopped:
        SPDLOG_LOGGER_TRACE(_logger, "engine::publish {} event to file",
                            to_publish.size());
        for (auto& e : to_publish) {
          _cache_file->add(e);
          _unprocessed_events++;
        }
        break;
      case not_started:
        SPDLOG_LOGGER_TRACE(_logger, "engine::publish {} event to queue",
                            to_publish.size());
        for (auto& e : to_publish)
          _kiew.push_back(e);
        break;
      default:
        SPDLOG_LOGGER_TRACE(_logger, "engine::publish {} event to queue_",
                            to_publish.size());
        for (auto& e : to_publish)
          _kiew.push_back(e);
        have_to_send = true;
        break;
    }
  }
  if (have_to_send) {
    _send_to_subscribers(nullptr);
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
  if (have_to_send)
    _send_to_subscribers(nullptr);

  SPDLOG_LOGGER_INFO(_logger, "multiplexing: engine started");
}

/**
 * @brief Stop multiplexing. After a call to this function, all published events
 * are sent to an unprocessed persistent file. These events are not lost and
 * will be handled at the next cbd start.
 */
void engine::stop() {
  absl::ReleasableMutexLock lck(&_kiew_m);

  if (_state != stopped) {
    // Set writing method.
    _state = stopped;
    _center->update(&EngineStats::set_mode, _stats, EngineStats::STOPPED);
    lck.Release();
    // Notify hooks of multiplexing loop end.
    SPDLOG_LOGGER_INFO(_logger, "multiplexing: stopping engine");

    std::promise<void> promise;
    if (_send_to_subscribers([&promise]() { promise.set_value(); })) {
      promise.get_future().get();
    }  // nothing to send or no muxer

    absl::MutexLock l(&_kiew_m);

    // Open the cache file and start the transaction.
    // The cache file is used to cache all the events produced
    // while the engine is stopped. It will be replayed next time
    // the engine is started.
    try {
      _cache_file =
          std::make_unique<persistent_cache>(_cache_file_path(), _logger);
      _cache_file->transaction();
    } catch (const std::exception& e) {
      _logger->error("multiplexing: could not open cache file: {}", e.what());
      _cache_file.reset();
    }

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
    if (m.lock() == subscriber) {
      _logger->debug("engine: muxer {} already subscribed", subscriber->name());
      return;
    }
  _muxers.push_back(subscriber);
}

/**
 *  Unsubscribe from the multiplexing engine.
 *
 *  @param[in] subscriber  Subscriber.
 */
void engine::unsubscribe_muxer(const muxer* subscriber) {
  std::promise<void> promise;

  if (_send_to_subscribers([&promise]() { promise.set_value(); })) {
    promise.get_future().wait();
  }

  absl::MutexLock lck(&_kiew_m);

  auto logger = log_v2::instance().get(log_v2::CONFIG);
  for (auto it = _muxers.begin(); it != _muxers.end(); ++it) {
    auto w = it->lock();
    if (!w || w.get() == subscriber) {
      logger->debug("multiplexing: muxer {} unsubscribed from Engine",
                    subscriber->name());

      _muxers.erase(it);
      return;
    }
  }
}

/**
 *  Default constructor.
 */
engine::engine(const std::shared_ptr<spdlog::logger>& logger)
    : _state{not_started},
      _unprocessed_events{0u},
      _center{stats::center::instance_ptr()},
      _stats{_center->register_engine()},
      _sending_to_subscribers{false},
      _logger{logger} {
  _center->update(&EngineStats::set_mode, _stats, EngineStats::NOT_STARTED);
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
  absl::EnableMutexInvariantDebugging(true);
}

engine::~engine() noexcept {
  /* Muxers should be unsubscribed before arriving here. */
  assert(_state == stopped);
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

namespace com::centreon::broker::multiplexing::detail {

/**
 * @brief The goal of this class is to do the completion job once all muxer
 * has been fed a shared_ptr of one instance of this class is passed to
 * worker. So when all workers have finished, destructor is called and do the
 * job
 *
 */
class callback_caller {
  engine::send_to_mux_callback_type _callback;
  std::shared_ptr<engine> _parent;

 public:
  callback_caller(engine::send_to_mux_callback_type&& callback,
                  const std::shared_ptr<engine>& parent)
      : _callback(callback), _parent(parent) {}

  /**
   * @brief Destroy the callback caller object and do the completion job
   *
   */
  ~callback_caller() {
    // job is done
    bool expected = true;
    if (_parent->_sending_to_subscribers.compare_exchange_strong(expected,
                                                                 false)) {
      if (_callback) {
        _callback();
      }
    }
  }
};

}  // namespace com::centreon::broker::multiplexing::detail

/**
 * @brief
 *  Send queued events to subscribers. Since events are queued, we use a
 * strand to keep their order. But there are several muxers, so we parallelize
 * the sending of data to each. callback is called only if _kiew is not empty
 * @param callback
 * @return true data sent
 * @return false nothing to send or currently sending.
 */
bool engine::_send_to_subscribers(send_to_mux_callback_type&& callback) {
  // is _send_to_subscriber working? (_sending_to_subscribers=false)
  bool expected = false;
  if (!_sending_to_subscribers.compare_exchange_strong(expected, true)) {
    return false;
  }
  // Now we continue and _sending_to_subscribers is true.

  // Process all queued events.
  std::shared_ptr<std::deque<std::shared_ptr<io::data>>> kiew;
  std::shared_ptr<muxer> first_muxer;
  std::shared_ptr<detail::callback_caller> cb;
  {
    absl::MutexLock lck(&_kiew_m);
    if (_muxers.empty() || _kiew.empty()) {
      // nothing to do true => _sending_to_subscribers
      bool expected = true;
      _sending_to_subscribers.compare_exchange_strong(expected, false);
      return false;
    }

    SPDLOG_LOGGER_TRACE(
        _logger, "engine::_send_to_subscribers send {} events to {} muxers",
        _kiew.size(), _muxers.size());

    kiew = std::make_shared<std::deque<std::shared_ptr<io::data>>>();
    std::swap(_kiew, *kiew);
    // completion object
    // it will be destroyed at the end of the scope of this function and at
    // the end of lambdas posted
    cb = std::make_shared<detail::callback_caller>(std::move(callback),
                                                   _instance);

    // we use all asio threads and current thread to publish event
    // the first not null muxer is used by main thread whereas
    // followed threads use io::context::post to do the job
    // when the last muxer had done his job, cb is destroyed and
    // _sending_to_subscribers is refreshed
    for (auto& mux : _muxers) {
      if (!first_muxer) {
        first_muxer = mux.lock();
      } else {
        std::shared_ptr<muxer> mux_to_publish_in_asio = mux.lock();
        if (mux_to_publish_in_asio) {
          asio::post(com::centreon::common::pool::io_context(),
                     [kiew, mux_to_publish_in_asio, cb, logger = _logger]() {
                       try {
                         mux_to_publish_in_asio->publish(*kiew);
                       }  // pool threads protection
                       catch (const std::exception& ex) {
                         SPDLOG_LOGGER_ERROR(
                             logger, "publish caught exception: {}", ex.what());
                       } catch (...) {
                         SPDLOG_LOGGER_ERROR(
                             logger, "publish caught unknown exception");
                       }
                     });
        }
      }
    }
  }
  if (first_muxer) {
    _center->update(&EngineStats::set_processed_events, _stats,
                    static_cast<uint32_t>(kiew->size()));
    /* The same work but by this thread for the last muxer. */
    first_muxer->publish(*kiew);
    return true;
  } else  // no muxer
    return false;
}

/**
 * @brief Clear events stored in the multiplexing engine.
 */
void engine::clear() {
  absl::MutexLock lck(&_kiew_m);
  _kiew.clear();
}

/**
 * @brief Get a muxer by name. This function returns a shared_ptr to the
 * muxer if it is running. If the muxer is not running or it doesn't exist, it
 * returns a nullptr.
 *
 * @param name Name of the muxer to get.
 *
 * @return A shared_ptr to the muxer if it is running, nullptr otherwise.
 */
std::shared_ptr<muxer> engine::get_muxer(const std::string& name) {
  absl::MutexLock lck(&_running_muxers_m);
  absl::erase_if(_running_muxers,
                 [](const std::pair<std::string, std::weak_ptr<muxer>>& p) {
                   return p.second.expired();
                 });
  return _running_muxers[name].lock();
}

void engine::set_muxer(const std::string& name,
                       const std::shared_ptr<muxer>& muxer) {
  absl::MutexLock lck(&_running_muxers_m);
  _running_muxers[name] = muxer;
}
