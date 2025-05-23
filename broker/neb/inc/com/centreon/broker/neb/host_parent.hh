/*
** Copyright 2009-2013 Centreon
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

#ifndef CCB_NEB_HOST_PARENT_HH
#define CCB_NEB_HOST_PARENT_HH

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"

namespace com::centreon::broker {

namespace neb {
/**
 *  @class host_parent host_parent.hh "com/centreon/broker/neb/host_parent.hh"
 *  @brief Define a parent of a host.
 *
 *  Define a certain host to be the parent of another host.
 */
class host_parent : public io::data {
 public:
  host_parent();
  host_parent(host_parent const& other);
  ~host_parent();
  host_parent& operator=(host_parent const& other);
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::neb, neb::de_host_parent>::value;
  }

  bool enabled;
  uint32_t host_id;
  uint32_t parent_id;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};
}  // namespace neb

}  // namespace com::centreon::broker

#endif  // !CCB_NEB_HOST_PARENT_HH
