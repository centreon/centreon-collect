/**
 * Copyright 2009-2012,2015,2019-2024 Centreon
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

#include <absl/base/thread_annotations.h>
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
  static absl::Mutex _load_m;
  static std::shared_ptr<engine> _instance;

  enum state { not_started, running, stopped };

  using send_to_mux_callback_type = std::function<void()>;

  std::unique_ptr<persistent_cache> _cache_file;

  // Data queue _kiew and engine state _state are protected by _kiew_m.
  absl::Mutex _kiew_m;
  state _state ABSL_GUARDED_BY(_kiew_m);
  std::deque<std::shared_ptr<io::data>> _kiew ABSL_GUARDED_BY(_kiew_m);
  uint32_t _unprocessed_events ABSL_GUARDED_BY(_kiew_m);

  // Subscriber.
  std::vector<std::weak_ptr<muxer>> _muxers ABSL_GUARDED_BY(_kiew_m);

  // Statistics.
  std::shared_ptr<stats::center> _center;
  EngineStats* _stats;

  std::atomic_bool _sending_to_subscribers;

  std::shared_ptr<spdlog::logger> _logger;

  /* The map of running muxers with the mutex to protect it. */
  absl::Mutex _running_muxers_m;
  absl::flat_hash_map<std::string, std::weak_ptr<muxer>> _running_muxers
      ABSL_GUARDED_BY(_running_muxers_m);

  engine(const std::shared_ptr<spdlog::logger>& logger);
  std::string _cache_file_path() const;
  bool _send_to_subscribers(send_to_mux_callback_type&& callback);

  friend class detail::callback_caller;

 public:
  static void load() ABSL_LOCKS_EXCLUDED(_load_m);
  static void unload() ABSL_LOCKS_EXCLUDED(_load_m);
  static std::shared_ptr<engine> instance_ptr();

  engine(const engine&) = delete;
  engine& operator=(const engine&) = delete;
  ~engine() noexcept;

  void clear() ABSL_LOCKS_EXCLUDED(_kiew_m);
  void publish(const std::shared_ptr<io::data>& d) ABSL_LOCKS_EXCLUDED(_kiew_m);
  void publish(const std::deque<std::shared_ptr<io::data>>& to_publish)
      ABSL_LOCKS_EXCLUDED(_kiew_m);
  void start() ABSL_LOCKS_EXCLUDED(_kiew_m);
  void stop() ABSL_LOCKS_EXCLUDED(_kiew_m);
  void subscribe(const std::shared_ptr<muxer>& subscriber)
      ABSL_LOCKS_EXCLUDED(_kiew_m);
  void unsubscribe_muxer(const muxer* subscriber) ABSL_LOCKS_EXCLUDED(_kiew_m);
  std::shared_ptr<muxer> get_muxer(const std::string& name)
      ABSL_LOCKS_EXCLUDED(_running_muxers_m);
  void set_muxer(const std::string& name, const std::shared_ptr<muxer>& muxer)
      ABSL_LOCKS_EXCLUDED(_running_muxers_m);
};
}  // namespace com::centreon::broker::multiplexing

#endif  // !CCB_MULTIPLEXING_ENGINE_HH
