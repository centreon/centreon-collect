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
#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "com/centreon/broker/persistent_cache.hh"
#include "com/centreon/broker/stats/center.hh"

namespace com::centreon::broker::multiplexing {
// Forward declaration.
class muxer;

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
class event_sink;

class engine {
  static absl::Mutex _load_m;
  static std::shared_ptr<engine> _instance;

  enum state { not_started, running, stopped };

  /**
   * @brief A subscribed muxer, with a copy of its write filter.
   *
   * The filter is held here so that deciding whether a muxer wants a batch is a
   * mask test, done without locking the weak_ptr. A cbd serving 100 pollers has
   * one muxer per connection, almost all of them rejecting the monitoring flow,
   * so this is what keeps the fan-out proportional to the muxers that actually
   * want the events rather than to the muxers that exist. Kept in sync by
   * muxer::set_write_filter, through update_write_filter().
   *
   * The name is a copy for the same reason: it is only needed to journal the
   * events a muxer refuses, and going through the weak_ptr for it would undo
   * what the filter copy buys.
   */
  struct subscriber {
    std::weak_ptr<muxer> mux;
    muxer_filter filter;
    std::string name;
  };

  std::unique_ptr<persistent_cache> _cache_file;

  // Data queue _kiew and engine state _state are protected by _kiew_m.
  absl::Mutex _kiew_m;
  state _state ABSL_GUARDED_BY(_kiew_m);
  std::deque<std::shared_ptr<io::data>> _kiew ABSL_GUARDED_BY(_kiew_m);

  // Subscriber.
  std::vector<subscriber> _muxers ABSL_GUARDED_BY(_kiew_m);

  // Statistics.
  std::shared_ptr<stats::center> _center;
  EngineStats* _stats;

  /* Is a drain loop running. */
  bool _draining ABSL_GUARDED_BY(_kiew_m) = false;

  /* Owned by the drain loop. There is only ever one such loop — that is what
   * _draining guarantees — and it only takes a new batch once the previous one
   * is fully consumed, so these need no mutex and can be reused from batch to
   * batch instead of being allocated each time. */
  std::deque<std::shared_ptr<io::data>> _batch;
  std::vector<std::shared_ptr<muxer>> _hold;
  std::vector<muxer*> _targets;

  /* Consumers left on the batch in flight: one per posted muxer, one for the
   * notification sink when there is one, and one for the drainer itself while
   * it still reads _batch. Whoever brings it to zero owns the loop. */
  std::atomic<size_t> _pending{0};

  /* Optional extra batch consumer, set only when Broker owns the notification
   * decision (notification_mode=broker). nullptr otherwise, so the publish path
   * pays only a pointer test. Set at startup and cleared at teardown; the sink
   * outlives the engine's task draining (see deinit order). */
  event_sink* _notification_sink = nullptr;

  std::shared_ptr<spdlog::logger> _logger;

  engine(const std::shared_ptr<spdlog::logger>& logger);
  std::string _cache_file_path() const;

  /* Shared body of the two publish() overloads. Defined in engine.cc, where
   * both instantiations live. */
  template <typename It>
  bool _enqueue(It first, It last) ABSL_LOCKS_EXCLUDED(_kiew_m);

  void _drain();
  bool _one_done();
  void _wake_drainer() ABSL_LOCKS_EXCLUDED(_kiew_m);
  void _wait_drained() ABSL_LOCKS_EXCLUDED(_kiew_m);

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
  void update_write_filter(const muxer* subscriber, const muxer_filter& filter)
      ABSL_LOCKS_EXCLUDED(_kiew_m);
  void set_notification_sink(event_sink* sink) { _notification_sink = sink; }
};
}  // namespace com::centreon::broker::multiplexing

#endif  // !CCB_MULTIPLEXING_ENGINE_HH
