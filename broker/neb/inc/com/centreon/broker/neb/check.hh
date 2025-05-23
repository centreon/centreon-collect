/**
 * Copyright 2009-2013, 2025 Centreon
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

#ifndef CCB_NEB_CHECK_HH
#define CCB_NEB_CHECK_HH

#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/timestamp.hh"

namespace com::centreon::broker {

namespace neb {
/**
 *  @class check check.hh "com/centreon/broker/neb/check.hh"
 *  @brief Check that has been executed.
 *
 *  Once a check has been executed (the check itself, not
 *  deduced information), this kind of event is sent.
 *
 *  @see host_check
 *  @see service_check
 */
class check : public io::data {
 public:
  check() = delete;
  check(uint32_t type);
  check(check const& c);
  virtual ~check();
  check& operator=(check const& c);

  bool active_checks_enabled;
  short check_type;
  std::string command_line;
  uint32_t host_id;
  timestamp next_check;

 private:
  void _internal_copy(check const& c);
};
}  // namespace neb

}  // namespace com::centreon::broker

#endif  // !CCB_NEB_CHECK_HH
