/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#ifndef CCB_MULTIPLEXING_EVENT_SINK_HH
#define CCB_MULTIPLEXING_EVENT_SINK_HH

#include <deque>
#include <memory>

namespace com::centreon::broker::io {
class data;
}

namespace com::centreon::broker::multiplexing {

/**
 * @brief A generic consumer of a batch of events, plugged into
 * multiplexing::engine alongside the muxers and the global cache.
 *
 * Unlike a muxer (which enqueues events for a downstream stream), a sink
 * processes the batch directly. The engine hands it each drained batch. It is
 * meant for in-process treatments that are not streams — the Broker-side
 * notification driver being the first, hence the generic name for future reuse.
 */
class event_sink {
 public:
  virtual ~event_sink() = default;

  /**
   * @brief Process a batch of events.
   *
   * @param events The events drained from the engine queue for this batch.
   */
  virtual void on_events(
      const std::deque<std::shared_ptr<io::data>>& events) = 0;
};

}  // namespace com::centreon::broker::multiplexing

#endif  // !CCB_MULTIPLEXING_EVENT_SINK_HH
