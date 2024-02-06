/**
 * Copyright 2009-2012,2015,2019-2023 Centreon
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

#ifndef CCB_MULTIPLEXING_ENGINE_HH
#define CCB_MULTIPLEXING_ENGINE_HH

#include "com/centreon/broker/persistent_cache.hh"
#include "com/centreon/broker/stats/center.hh"

namespace com::centreon::broker::multiplexing {
// Forward declaration.
class muxer;
namespace detail {
class callback_caller;
}  // namespace detail

/**
 *  @class engine engine.hh "com/centreon/broker/multiplexing/engine.hh"
 *  @brief Multiplexing engine.
 *
 *  Core multiplexing engine. Sends events to and receives events from
 *  muxer objects.
 *
 *  This class has a unique instance. Before calling the instance() method,
 *  we have to call the static load() one. And to close this instance, we
 *  have to call the static method unload().
 *
 *  The instance initialization/deinitialization are guarded by a mutex
 *  _load_m. It is only used for that purpose.
 *
 *  This class is the root of events dispatching. Events arrive from a stream,
 *  are transfered to a muxer and then to engine (at the root of the tree).
 *  This one then sends events to all its children. Each muxer receives
 *  these events and sends them to its stream.
 *
 *  The engine has three states:
 *  * not started. All event that could be received is lost by the engine.
 *    This state is possible only when the engine is started or during tests.
 *  * running, received events are dispatched to all the muxers beside. This
 *    is done asynchronously.
 *  * stopped, the 'write' function points to a _write_to_cache_file() funtion.
 *    When broker is stopped, before it to be totally stopped, events are
 *    written to a cache file ...unprocessed... This file will be re-read at the
 *    next broker start.
 *
 *  @see muxer
 */
class engine {
  static std::mutex _load_m;
  static std::shared_ptr<engine> _instance;

  enum state { not_started, running, stopped };

  using send_to_mux_callback_type = std::function<void()>;

  state _state;

  std::unique_ptr<persistent_cache> _cache_file;

  // Mutex to lock _kiew and _state
  std::mutex _engine_m;

  // Data queue.
  std::deque<std::shared_ptr<io::data>> _kiew;

  // Subscriber.
  std::vector<std::weak_ptr<muxer>> _muxers;

  // Statistics.
  EngineStats* _stats;
  uint32_t _unprocessed_events;

  std::atomic_bool _sending_to_subscribers;

  engine();
  std::string _cache_file_path() const;
  bool _send_to_subscribers(send_to_mux_callback_type&& callback);

  friend class detail::callback_caller;

 public:
  static void load();
  static void unload();
  static std::shared_ptr<engine> instance_ptr();

  engine(const engine&) = delete;
  engine& operator=(const engine&) = delete;
  ~engine() noexcept;

  void clear();
  void publish(const std::shared_ptr<io::data>& d);
  void publish(const std::deque<std::shared_ptr<io::data>>& to_publish);
  void start();
  void stop();
  void subscribe(const std::shared_ptr<muxer>& subscriber);
  void unsubscribe_muxer(const muxer* subscriber);
};
}  // namespace com::centreon::broker::multiplexing

#endif  // !CCB_MULTIPLEXING_ENGINE_HH
