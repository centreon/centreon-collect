/*
** Copyright 2013, 2021 Centreon
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

#ifndef CCB_BBDO_ACK_HH
#define CCB_BBDO_ACK_HH

#include "com/centreon/broker/bbdo/internal.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"

namespace com::centreon::broker::bbdo {
/**
 *  @class ack ack.hh "com/centreon/broker/bbdo/ack.hh"
 *  @brief An ack event sent between two bbdo's endpoint.
 *
 *  This is used for high-level event acknowledgement.
 */
class ack : public io::data {
 public:
  ack();
  explicit ack(uint32_t acknowledged_events);
  ~ack() noexcept = default;
  ack(ack const&) = delete;
  ack& operator=(ack const&) = delete;

  /**
   *  Get the event type.
   *
   *  @return The event type.
   */
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::bbdo, bbdo::de_ack>::value;
  }

  uint32_t acknowledged_events;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};
}  // namespace com::centreon::broker::bbdo

#endif  // !CCB_BBDO_VERSION_RESPONSE_HH
