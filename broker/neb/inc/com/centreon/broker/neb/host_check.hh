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

#ifndef CCB_NEB_HOST_CHECK_HH
#define CCB_NEB_HOST_CHECK_HH

#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/neb/check.hh"

namespace com::centreon::broker {

namespace neb {
/**
 *  @class host_check host_check.hh "com/centreon/broker/neb/host_check.hh"
 *  @brief Check that has been executed on a host.
 *
 *  Once a check has been executed on a host, an object of this class
 *  is sent.
 */
class host_check : public check {
 public:
  host_check();
  host_check(host_check const& other);
  virtual ~host_check();
  host_check& operator=(host_check const& other);
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::neb, neb::de_host_check>::value;
  }

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;
};  // namespace neb
}  // namespace neb

}

#endif  // !CCB_NEB_HOST_CHECK_HH
