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

#include "com/centreon/broker/neb/module.hh"

#include "com/centreon/broker/database/table_max_size.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::neb;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
module::module()
    : io::data(module::static_type()),
      enabled(true),
      loaded(false),
      poller_id(0),
      should_be_loaded(false) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
module::module(module const& other) : io::data(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
module::~module() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
module& module::operator=(module const& other) {
  if (this != &other) {
    io::data::operator=(other);
    _internal_copy(other);
  }
  return *this;
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Copy data members.
 *
 *  @param[in] other  Object to copy.
 */
void module::_internal_copy(module const& other) {
  args = other.args;
  enabled = other.enabled;
  filename = other.filename;
  loaded = other.loaded;
  poller_id = other.poller_id;
  should_be_loaded = other.should_be_loaded;
}

/**************************************
 *                                     *
 *           Static Objects            *
 *                                     *
 **************************************/

// Mapping.
mapping::entry const module::entries[] = {
    mapping::entry(&module::args, "args", get_modules_col_size(modules_args)),
    mapping::entry(&module::enabled, ""),
    mapping::entry(&module::filename,
                   "filename",
                   get_modules_col_size(modules_filename)),
    mapping::entry(&module::poller_id,
                   "instance_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&module::loaded, "loaded"),
    mapping::entry(&module::should_be_loaded, "should_be_loaded"),
    mapping::entry()};

// Operations.
static io::data* new_module() {
  return new module;
}
io::event_info::event_operations const module::operations = {&new_module,
                                                             nullptr, nullptr};
