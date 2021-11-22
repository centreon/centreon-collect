/*
** Copyright 2009-2013,2015, 2020-2021 Centreon
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
#include "com/centreon/broker/log_v2.hh"

#include <unistd.h>

#include <fmt/format.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::multiplexing;

// Class instance.
engine* engine::_instance(nullptr);
std::mutex engine::_load_m;

/**
 *  Clear events stored in the multiplexing engine.
 */
void engine::clear() {
  _kiew.clear();
}

/**
 *  Get engine instance.
 *
 *  @return Class instance.
 */
engine& engine::instance() {
  assert(_instance);
  return *_instance;
}

/**
 *  Load engine instance.
 */
void engine::load() {
  std::lock_guard<std::mutex> lk(_load_m);
  if (!_instance)
    _instance = new engine;
}

/**
 *  Send an event to all subscribers.
 *
 *  @param[in] e  Event to publish.
 */
void engine::publish(const std::shared_ptr<io::data>& e) {
  // Lock mutex.
  std::lock_guard<std::mutex> lock(_engine_m);
  _publish(e);
}

void engine::publish(const std::list<std::shared_ptr<io::data>>& to_publish) {
  std::lock_guard<std::mutex> lock(_engine_m);
  for (auto& e : to_publish)
    _publish(e);
}

/**
 *  Send an event to all subscribers. It must be used from this class, and
 *  _engine_m must be locked previously. Otherwise use engine::publish().
 *
 *  @param[in] e  Event to publish.
 */
void engine::_publish(const std::shared_ptr<io::data>& e) {
  // Store object for further processing.
  _kiew.push_back(e);
  // Processing function.
  (this->*_write_func)(e);
}

/**
 *  Start multiplexing.
 */
void engine::start() {
  if (_write_func != &engine::_write) {
    // Set writing method.
    log_v2::perfdata()->debug("multiplexing: starting");
    _write_func = &engine::_write;
    stats::center::instance().update(&EngineStats::set_mode, _stats,
                                     EngineStats::WRITE);

    std::lock_guard<std::mutex> lock(_engine_m);
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
      log_v2::perfdata()->error("multiplexing: couldn't read cache file: {}",
                                e.what());
    }

    // Copy global event queue to local queue.
    while (!_kiew.empty()) {
      kiew.push_back(_kiew.front());
      _kiew.pop_front();
    }

    // Send events queued while multiplexing was stopped.
    for (auto& e : kiew)
      _publish(e);
  }
}

/**
 *  Stop multiplexing.
 */
void engine::stop() {
  if (_write_func != &engine::_nop) {
    // Notify hooks of multiplexing loop end.
    log_v2::core()->debug("multiplexing: stopping");
    std::unique_lock<std::mutex> lock(_engine_m);

    do {
      // Process events from hooks.
      _send_to_subscribers();

      // Make sure that no more data is available.
      lock.unlock();
      usleep(200000);
      lock.lock();
    } while (!_kiew.empty());

    // Open the cache file and start the transaction.
    // The cache file is used to cache all the events produced
    // while the engine is stopped. It will be replayed next time
    // the engine is started.
    try {
      _cache_file.reset(new persistent_cache(_cache_file_path()));
      _cache_file->transaction();
    } catch (const std::exception& e) {
      log_v2::perfdata()->error("multiplexing: could not open cache file: {}",
                                e.what());
      _cache_file.reset();
    }

    // Set writing method.
    _write_func = &engine::_write_to_cache_file;
    stats::center::instance().update(&EngineStats::set_mode, _stats,
                                     EngineStats::WRITE_TO_CACHE_FILE);
  }
}

/**
 *  Subscribe to the multiplexing engine.
 *
 *  @param[in] subscriber  Subscriber.
 */
void engine::subscribe(muxer* subscriber) {
  std::lock_guard<std::mutex> lock(_muxers_m);
  _muxers.push_back(subscriber);
}

/**
 *  Unload class instance.
 */
void engine::unload() {
  std::lock_guard<std::mutex> lk(_load_m);
  // Commit the cache file, if needed.
  if (_instance && _instance->_cache_file.get())
    _instance->_cache_file->commit();

  delete _instance;
  _instance = nullptr;
}

/**
 *  Unsubscribe from the multiplexing engine.
 *
 *  @param[in] subscriber  Subscriber.
 */
void engine::unsubscribe(muxer* subscriber) {
  std::lock_guard<std::mutex> lock(_muxers_m);
  for (auto it = _muxers.begin(), end = _muxers.end(); it != end; ++it)
    if (*it == subscriber) {
      _muxers.erase(it);
      break;
    }
}

/**
 *  Default constructor.
 */
engine::engine()
    :

      _muxers{},
      _stats{stats::center::instance().register_engine()},
      _unprocessed_events{0u},
      _write_func(&engine::_nop) {
  stats::center::instance().update(&EngineStats::set_mode, _stats,
                                   EngineStats::NOP);
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
 *  Do nothing.
 *
 *  @param[in] d  Unused.
 */
void engine::_nop(std::shared_ptr<io::data> const& d) {
  (void)d;
}

/**
 *  Send queued events to subscribers.
 */
void engine::_send_to_subscribers() {
  // Process all queued events.
  std::deque<std::shared_ptr<io::data>> kiew;
  static uint32_t test_dbo = 0;
  {
    std::lock_guard<std::mutex> lock(_muxers_m);
    if (_kiew.size() > test_dbo) {
      test_dbo = _kiew.size();
      log_v2::sql()->error("TEST TEST max kiew size = {}", test_dbo);
    }
    std::swap(_kiew, kiew);
  }
  for (auto& e : kiew) {
    // Send object to every subscriber.
    for (muxer* m : _muxers)
      m->publish(e);
  }
}

/**
 *  The real event publication is done here. This method is just called by
 *  the publish method. No need of a lock, it is already owned by the publish
 *  method.
 *
 *  @param[in] e  Data to publish.
 */
void engine::_write(std::shared_ptr<io::data> const& e) {
  // Send events to subscribers.
  _send_to_subscribers();
}

/**
 *  Write to a cache file that will be played back at startup.
 *
 *  @param[in] d  Data to write.
 */
void engine::_write_to_cache_file(std::shared_ptr<io::data> const& d) {
  try {
    if (_cache_file) {
      _cache_file->add(d);
      _unprocessed_events++;
    }
  } catch (const std::exception& e) {
    log_v2::perfdata()->error("multiplexing: could not write to cache file: {}",
                              e.what());
  }
}
