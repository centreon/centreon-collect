/*
** Copyright 2009-2012 Centreon
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

#ifndef CCB_NEB_HOST_STATUS_HH
#define CCB_NEB_HOST_STATUS_HH

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/neb/host_service_status.hh"

namespace com::centreon::broker {

namespace neb {
/**
 *  @class host_status host_status.hh "com/centreon/broker/neb/host_status.hh"
 *  @brief host_status represents a status change of an host.
 *
 *  host_status are generated when the status of an host change.
 *  Appropriate fields should be updated.
 */
class host_status : public host_service_status {
 public:
  host_status(uint32_t type = static_type());
  host_status(host_status const& other);
  virtual ~host_status();
  host_status& operator=(host_status const& other);
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::neb, neb::de_host_status>::value;
  }

  timestamp last_time_down;
  timestamp last_time_unreachable;
  timestamp last_time_up;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;

 private:
  void _internal_copy(host_status const& other);
};
}  // namespace neb

}  // namespace com::centreon::broker

#endif  // !CCB_NEB_HOST_STATUS_HH
