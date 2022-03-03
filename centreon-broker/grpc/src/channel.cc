/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/grpc/channel.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/misc/trash.hh"
#include "grpc_stream.pb.h"

using namespace com::centreon::broker::grpc;

static com::centreon::broker::misc::trash<channel> _trash;

namespace com {
namespace centreon {
namespace broker {
namespace stream {
std::ostream& operator<<(std::ostream& st,
                         const centreon_stream::centreon_event& to_dump) {
  if (to_dump.IsInitialized()) {
    if (to_dump.has_buffer()) {
      st << com::centreon::broker::misc::string::debug_buf(
          to_dump.buffer().data(), to_dump.buffer().length(), 20);
    }
  }
  return st;
}
}  // namespace stream
namespace grpc {
std::ostream& operator<<(std::ostream& st,
                         const detail_centreon_event& to_dump) {
  if (to_dump.to_dump.IsInitialized()) {
    if (to_dump.to_dump.has_buffer()) {
      st << com::centreon::broker::misc::string::debug_buf(
          to_dump.to_dump.buffer().data(), to_dump.to_dump.buffer().length(),
          100);
    }
  }
  return st;
}
}  // namespace grpc
}  // namespace broker
}  // namespace centreon
}  // namespace com

constexpr unsigned second_delay_before_delete = 60;

void channel::to_trash() {
  _thrown = true;
  log_v2::grpc()->debug("{} this={:p}", __PRETTY_FUNCTION__,
                        static_cast<void*>(this));
  _trash.to_trash(shared_from_this(),
                  time(nullptr) + second_delay_before_delete);
}
