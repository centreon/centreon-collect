/*
** Copyright 2013 Centreon
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

#ifndef CCB_BBDO_VERSION_RESPONSE_HH
#define CCB_BBDO_VERSION_RESPONSE_HH

#include "bbdo/bbdo/bbdo_version.hh"
#include "broker/core/bbdo/internal.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"

namespace com::centreon::broker::bbdo {
/**
 *  @class version_response version_response.hh
 * "com/centreon/broker/bbdo/version_response.hh"
 *  @brief Send protocol version used by endpoint.
 *
 *  Send protocol version used by endpoint.
 */
class version_response : public io::data {
 public:
  short bbdo_major;
  short bbdo_minor;
  short bbdo_patch;
  std::string extensions;

  version_response();
  version_response(bbdo_version bbdo_version, std::string extensions);
  version_response(const version_response&) = delete;
  ~version_response() noexcept = default;
  version_response& operator=(const version_response&) = delete;

  /**
   *  Get the event type.
   *
   *  @return The event type.
   */
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::bbdo, bbdo::de_version_response>::value;
  }

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};
}  // namespace com::centreon::broker::bbdo

#endif  // !CCB_BBDO_VERSION_RESPONSE_HH
