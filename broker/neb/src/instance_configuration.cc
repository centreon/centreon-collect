/**
 * Copyright 2016 Centreon
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

#include "com/centreon/broker/neb/instance_configuration.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::neb;

/**
 *  @brief Default constructor.
 *
 *  Initialize members to 0, NULL or equivalent.
 */
instance_configuration::instance_configuration()
    : io::data(instance_configuration::static_type()),
      loaded(false),
      poller_id(0) {}

/**
 *  @brief Copy constructor.
 *
 *  Copy all members of the given object to the current instance.
 *
 *  @param[in] i Object to copy.
 */
instance_configuration::instance_configuration(instance_configuration const& i)
    : io::data(i) {
  _internal_copy(i);
}

/**
 *  Destructor.
 */
instance_configuration::~instance_configuration() {}

/**
 *  @brief Assignment operator.
 *
 *  Copy all members of the given object to the current instance.
 *
 *  @param[in] i Object to copy.
 */
instance_configuration& instance_configuration::operator=(
    instance_configuration const& i) {
  if (this != &i) {
    io::data::operator=(i);
    _internal_copy(i);
  }
  return (*this);
}

/**************************************
 *                                     *
 *           Static Objects            *
 *                                     *
 **************************************/

// Mapping.
mapping::entry const instance_configuration::entries[] = {
    mapping::entry(&instance_configuration::loaded, "loaded"),
    mapping::entry(&instance_configuration::poller_id, "poller_id"),
    mapping::entry()};

// Operations.
static io::data* new_ic() {
  return (new instance_configuration);
}
io::event_info::event_operations const instance_configuration::operations = {
    &new_ic, nullptr, nullptr};

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
 *  @param[in] i Object to copy.
 */
void instance_configuration::_internal_copy(instance_configuration const& i) {
  loaded = i.loaded;
  poller_id = i.poller_id;
  return;
}
