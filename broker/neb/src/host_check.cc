/**
 * Copyright 2009-2013 Centreon
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

#include "com/centreon/broker/neb/host_check.hh"

#include "com/centreon/broker/sql/table_max_size.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::neb;

/**
 *  Default constructor.
 */
host_check::host_check() : check(host_check::static_type()) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
host_check::host_check(host_check const& other) : check(other) {}

/**
 *  Destructor.
 */
host_check::~host_check() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
host_check& host_check::operator=(host_check const& other) {
  check::operator=(other);
  return *this;
}

/**************************************
 *                                     *
 *           Static Objects            *
 *                                     *
 **************************************/

// Mapping.
mapping::entry const host_check::entries[] = {
    mapping::entry(&host_check::active_checks_enabled, ""),
    mapping::entry(&host_check::check_type, ""),
    mapping::entry(&host_check::host_id,
                   "host_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&host_check::next_check, ""),
    mapping::entry(&host_check::command_line,
                   "command_line",
                   get_centreon_storage_hosts_col_size(
                       centreon_storage_hosts_command_line)),
    mapping::entry()};

// Operations.
static io::data* new_host_check() {
  return new host_check;
}
io::event_info::event_operations const host_check::operations = {
    &new_host_check, nullptr, nullptr};
