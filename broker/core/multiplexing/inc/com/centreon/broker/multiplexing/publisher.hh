/**
 * Copyright 2011-2013, 2020-2021 Centreon
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

#ifndef CCB_MULTIPLEXING_PUBLISHER_HH
#define CCB_MULTIPLEXING_PUBLISHER_HH

#include "com/centreon/broker/io/stream.hh"

namespace com::centreon::broker::multiplexing {
/**
 *  @class publisher publisher.hh
 * "com/centreon/broker/multiplexing/publisher.hh"
 *  @brief Publish events to the multiplexing engine.
 *
 *  Publish events written to the publisher to the multiplexing
 *  engine.
 *
 *  @see engine
 */
class publisher : public io::stream {
 public:
  publisher();
  ~publisher() noexcept = default;
  publisher(const publisher&) = delete;
  publisher& operator=(const publisher&) = delete;
  bool read(std::shared_ptr<io::data>& d,
            time_t deadline = (time_t)-1) override;
  int32_t write(const std::shared_ptr<io::data>& d) override;
  int32_t write(const std::deque<std::shared_ptr<io::data>>& to_publish);
  int32_t stop() override;
};
}  // namespace com::centreon::broker::multiplexing

#endif  // !CCB_MULTIPLEXING_PUBLISHER_HH
