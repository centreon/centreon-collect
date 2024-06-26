/**
 * Copyright 2009-2013,2015 Centreon
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

#include "com/centreon/broker/neb/instance.hh"

#include "com/centreon/broker/sql/table_max_size.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::neb;

/**
 *  @brief Default constructor.
 *
 *  Initialize members to 0, NULL or equivalent.
 */
instance::instance()
    : io::data(instance::static_type()),
      is_running(true),
      pid(0),
      poller_id(0),
      program_end((time_t)-1),
      program_start((time_t)-1) {}

/**
 *  @brief Copy constructor.
 *
 *  Copy all members of the given object to the current instance.
 *
 *  @param[in] other  Object to copy.
 */
instance::instance(instance const& other) : io::data(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
instance::~instance() {}

/**
 *  @brief Assignment operator.
 *
 *  Copy all members of the given object to the current instance.
 *
 *  @param[in] other  Object to copy.
 */
instance& instance::operator=(instance const& other) {
  if (this != &other) {
    io::data::operator=(other);
    _internal_copy(other);
  }
  return *this;
}

/**************************************
 *                                     *
 *          Private Methods            *
 *                                     *
 **************************************/

/**
 *  @brief Copy internal data of the instance object to the current
 *         instance.
 *
 *  Copy data defined within the instance class. This method is used by
 *  the copy constructor and the assignment operator.
 *
 *  @param[in] other  Object to copy.
 */
void instance::_internal_copy(instance const& other) {
  engine = other.engine;
  is_running = other.is_running;
  name = other.name;
  pid = other.pid;
  poller_id = other.poller_id;
  program_end = other.program_end;
  program_start = other.program_start;
  version = other.version;
}

/**************************************
 *                                     *
 *           Static Objects            *
 *                                     *
 **************************************/

// Mapping.
mapping::entry const instance::entries[] = {
    mapping::entry(&instance::engine,
                   "engine",
                   get_centreon_storage_instances_col_size(
                       centreon_storage_instances_engine)),
    mapping::entry(&instance::poller_id,
                   "instance_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&instance::name,
                   "name",
                   get_centreon_storage_instances_col_size(
                       centreon_storage_instances_name)),
    mapping::entry(&instance::is_running, "running"),
    mapping::entry(&instance::pid, "pid"),
    mapping::entry(&instance::program_end,
                   "end_time",
                   mapping::entry::invalid_on_minus_one),
    mapping::entry(&instance::program_start,
                   "start_time",
                   mapping::entry::invalid_on_minus_one),
    mapping::entry(&instance::version,
                   "version",
                   get_centreon_storage_instances_col_size(
                       centreon_storage_instances_version)),
    mapping::entry()};

// Operations.
static io::data* new_instance() {
  return new instance;
}
io::event_info::event_operations const instance::operations = {
    &new_instance, nullptr, nullptr};
