/**
 * Copyright 2009-2012,2015 Centreon
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

#ifndef CCB_NEB_HOST_GROUP_HH
#define CCB_NEB_HOST_GROUP_HH

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/neb/group.hh"

namespace com::centreon::broker::neb {

/**
 *  @class host_group host_group.hh "com/centreon/broker/neb/host_group.hh"
 *  @brief Represents a group of hosts.
 *
 *  A group of hosts within the scheduling engine.
 */
class host_group : public group {
 public:
  host_group();
  host_group(host_group const& other);
  ~host_group();
  host_group& operator=(host_group const& other);
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::neb, neb::de_host_group>::value;
  }

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};

}  // namespace com::centreon::broker::neb

#endif  // !CCB_NEB_HOST_GROUP_HH
