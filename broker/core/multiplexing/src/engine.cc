/*
** Copyright 2009-2013,2015, 2020-2021-2023 Centreon
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

#include "com/centreon/broker/multiplexing/engine.hh"

#include <unistd.h>

#include <cassert>

#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::multiplexing;
using log_v2 = com::centreon::common::log_v2::log_v2;

// Class instance.
std::unique_ptr<engine> engine::_instance{nullptr};
std::mutex engine::_load_m;

/**
 *  Get engine instance.
 *
 *  @return Class instance.
 */
std::unique_ptr<engine>& engine::instance_ptr() {
  assert(_instance);
  return _instance;
}

/**
 * @brief Load engine instance. The argument is the total size allowed for
 * queue files.
 */
void engine::load() {
  log_v2::instance().get(0)->trace("multiplexing: loading engine");
  std::lock_guard<std::mutex> lk(_load_m);
  if (!_instance)
    _instance.reset(new engine);
}

/**
 *  Unload class instance.
 */
void engine::unload() {
  log_v2::instance().get(0)->trace("multiplexing: unloading engine");
  if (!_instance)
    return;
  if (_instance->_state != stopped)
    _instance->stop();

  std::lock_guard<std::mutex> lk(_load_m);
  // Commit the cache file, if needed.
  if (_instance && _instance->_cache_file)
    _instance->_cache_file->commit();

  _instance.reset();
}

/**
 *  Send an event to all subscribers.
 *
 *  @param[in] e  Event to publish.
 */
void engine::publish(const std::shared_ptr<io::data>& e) {
  bool have_to_send = false;
  {
    std::lock_guard<std::mutex> lock(_engine_m);
    switch (_state) {
      case stopped:
        log_v2::instance().get(0)->trace("engine::publish one event to file");
        _cache_file->add(e);
        _unprocessed_events++;
        break;
      case not_started:
        log_v2::instance().get(0)->trace("engine::publish one event to queue");
        _kiew.push_back(e);
        break;
      default:
        log_v2::instance().get(0)->trace("engine::publish one event to queue_");
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
    std::lock_guard<std::mutex> lock(_engine_m);
    switch (_state) {
      case stopped:
        log_v2::instance().get(0)->trace("engine::publish {} event to file",
                                         to_publish.size());
        for (auto& e : to_publish) {
          _cache_file->add(e);
          _unprocessed_events++;
        }
        break;
      case not_started:
        log_v2::instance().get(0)->trace("engine::publish {} event to queue",
                                         to_publish.size());
        for (auto& e : to_publish)
          _kiew.push_back(e);
        break;
      default:
        log_v2::instance().get(0)->trace("engine::publish {} event to queue_",
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
    std::lock_guard<std::mutex> lock(_engine_m);
    if (_state == not_started) {
      // Set writing method.
      log_v2::instance().get(0)->debug("multiplexing: engine starting");
      _state = running;
      stats::center::instance().update(&EngineStats::set_mode, _stats,
                                       EngineStats::RUNNING);

      // Local queue.
      std::deque<std::shared_ptr<io::data>> kiew;
      // Get events from the cache file to the local queue.
      try {
        persistent_cache cache(_cache_file_path());
        std::shared_ptr<io::data> d;
        for (;;) {
          cache.get(d);
          if (!d)
            break;
          kiew.push_back(d);
        }
      } catch (const std::exception& e) {
        log_v2::instance().get(0)->error(
            "multiplexing: engine couldn't read cache file: {}", e.what());
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
    _send_to_subscribers(nullptr);
  }
  log_v2::instance().get(0)->info("multiplexing: engine started");
}

/**
 * @brief Stop multiplexing. After a call to this function, all published events
 * are sent to an unprocessed persistent file. These events are not lost and
 * will be handled at the next cbd start.
 */
void engine::stop() {
  std::unique_lock<std::mutex> lock(_engine_m);
  if (_state != stopped) {
    // Notify hooks of multiplexing loop end.
    log_v2::instance().get(0)->info("multiplexing: stopping engine");

    do {
      // Make sure that no more data is available.
      if (!_sending_to_subscribers) {
        log_v2::instance().get(0)->info(
            "multiplexing: sending events to muxers for the last time {} "
            "events to send",
            _kiew.size());
        _sending_to_subscribers = true;
        lock.unlock();
        std::promise<void> promise;
        if (_send_to_subscribers([&promise]() { promise.set_value(); })) {
          promise.get_future().wait();
        } else {  // nothing to send or no muxer
          break;
        }
        lock.lock();
      }
    } while (!_kiew.empty());

    // Open the cache file and start the transaction.
    // The cache file is used to cache all the events produced
    // while the engine is stopped. It will be replayed next time
    // the engine is started.
    try {
      _cache_file = std::make_unique<persistent_cache>(_cache_file_path());
      _cache_file->transaction();
    } catch (const std::exception& e) {
      log_v2::instance().get(0)->error(
          "multiplexing: could not open cache file: {}", e.what());
      _cache_file.reset();
    }

    // Set writing method.
    _state = stopped;
    stats::center::instance().update(&EngineStats::set_mode, _stats,
                                     EngineStats::STOPPED);
  }
  log_v2::instance().get(0)->debug("multiplexing: engine stopped");
  DEBUG(fmt::format("STOP engine {:p}", static_cast<void*>(this)));
}

/**
 *  Subscribe a muxer to the multiplexing engine if not already subscribed.
 *
 *  @param[in] subscriber  A muxer.
 */
void engine::subscribe(muxer* subscriber) {
  log_v2::instance().get(1)->debug("engine: muxer {} subscribes to engine",
                                   subscriber->name());
  std::lock_guard<std::mutex> l(_engine_m);
  muxer* s = subscriber.get();
  for (auto& m : _muxers)
    if (m.lock() == subscriber) {
      log_v2::instance().get(1)->debug("engine: muxer {} already subscribed",
                                       s->name());
      return;
    }
  _muxers.push_back(s);
}

/**
 *  Unsubscribe from the multiplexing engine.
 *
 *  @param[in] subscriber  Subscriber.
 */
void engine::unsubscribe_muxer(const muxer* subscriber) {
  std::lock_guard<std::mutex> l(_engine_m);
  for (auto it = _muxers.begin(); it != _muxers.end(); ++it) {
    auto w = it->lock();
    if (!w || w.get() == subscriber) {
      log_v2::instance().get(1)->debug(
          "engine: muxer {} unsubscribes to engine", subscriber->name());
      _muxers.erase(it);
      return;
    }
  }
}

/**
 *  Default constructor.
 */
engine::engine()
    : _state{not_started},
      _stats{stats::center::instance().register_engine()},
      _unprocessed_events{0u},
      _sending_to_subscribers{false} {
  DEBUG(fmt::format("CONSTRUCTOR engine {:p}", static_cast<void*>(this)));
  stats::center::instance().update(&EngineStats::set_mode, _stats,
                                   EngineStats::NOT_STARTED);
}

engine::~engine() noexcept {
  /* Muxers should be unsubscribed before arriving here. */
  assert(_muxers.empty());
  log_v2::instance().get(0)->debug("core: cbd engine destroyed.");
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
 * @brief The goal of this class is to do the completion job once all muxer has
 * been fed a shared_ptr of one instance of this class is passed to worker. So
 * when all workers have finished, destructor is called and do the job
 *
 */
class callback_caller {
  std::function<void()> _callback;
  std::unique_ptr<engine>& _parent;

 public:
  callback_caller(std::function<void()>&& callback,
                  std::unique_ptr<engine>& parent)
      : _callback(std::move(callback)), _parent(parent) {}

  /**
   * @brief Destroy the callback caller object and do the completion job
   *
   */
  ~callback_caller() {
    // job is done
    bool expected = true;
    _parent->_sending_to_subscribers.compare_exchange_strong(expected, false);
    // if another data to publish redo the job
    _parent->_send_to_subscribers(nullptr);
    if (_callback) {
      _callback();
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
bool engine::_send_to_subscribers(std::function<void()>&& callback) {
  // is _send_to_subscriber working? (_sending_to_subscribers=false)
  bool expected = false;
  if (!_sending_to_subscribers.compare_exchange_strong(expected, true)) {
    return false;
  }
  // Now we continue and _sending_to_subscribers is true.

  // Process all queued events.
  std::shared_ptr<std::deque<std::shared_ptr<io::data>>> kiew;
  muxer* last_muxer;
  std::shared_ptr<detail::callback_caller> cc;
  {
    std::lock_guard<std::mutex> lck(_engine_m);
    if (_muxers.empty() || _kiew.empty()) {
      // nothing to do true => _sending_to_subscribers
      bool expected = true;
      _sending_to_subscribers.compare_exchange_strong(expected, false);
      return false;
    }

    log_v2::instance().get(0)->trace(
        "engine::_send_to_subscribers send {} events to {} muxers",
        _kiew.size(), _muxers.size());

    kiew = std::make_shared<std::deque<std::shared_ptr<io::data>>>();
    std::swap(_kiew, *kiew);
    // completion object (class callback_caller at the top of this file).
    // This object is stored in a shared_ptr and shared by all the subscribers
    // while data are sent to them. Once the last muxer finishes to handle its
    // publication, cc is destroyed. And it is then that its callback is called.
    cc = std::make_shared<detail::callback_caller>(std::move(callback),
                                                   _instance);
    last_muxer = _muxers.rbegin()->lock();
    if (_muxers.size() > 1) {
      /* Since the sending is parallelized, we use the thread pool for this
       * purpose except for the last muxer where we use this thread. */

      /* We get an iterator to the last muxer */
      auto it_last = --_muxers.end();

      /* We use the thread pool for the muxers from the first one to the
       * second to last */
      for (auto it = _muxers.begin(); it != it_last; ++it) {
        pool::io_context().post([kiew, m = it->lock(), cc]() {
          try {
            m->publish(*kiew);
          }  // pool threads protection
          catch (const std::exception& ex) {
            log_v2::instance().get(0)->error("publish caught exception: {}",
                                             ex.what());
          } catch (...) {
            log_v2::instance().get(0)->error(
                "publish caught unknown exception");
          }
        });
      }
    }
  }
  stats::center::instance().update(&EngineStats::set_processed_events, _stats,
                                   static_cast<uint32_t>(kiew->size()));
  /* The same work but by this thread for the last muxer. */
  last_muxer->publish(*kiew);
  return true;
}

/**
 *  Clear events stored in the multiplexing engine.
 */
void engine::clear() {
  std::lock_guard<std::mutex> lck(_engine_m);
  _kiew.clear();
}
