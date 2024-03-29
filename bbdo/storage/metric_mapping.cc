/**
* Copyright 2015 Centreon
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

#include "bbdo/storage/metric_mapping.hh"

#include <cmath>

using namespace com::centreon::broker;
using namespace com::centreon::broker::storage;

/**
 *  Default constructor.
 */
metric_mapping::metric_mapping()
    : io::data(metric_mapping::static_type()), index_id(0), metric_id(0) {}

/**
 *  Constructor
 *
 * @param index_id
 * @param metric_id
 */
metric_mapping::metric_mapping(uint64_t index_id, uint32_t metric_id)
    : io::data(metric_mapping::static_type()),
      index_id{index_id},
      metric_id{metric_id} {}

/**
 *  Copy constructor.
 *
 *  @param[in] m Object to copy.
 */
metric_mapping::metric_mapping(metric_mapping const& m) : io::data(m) {
  _internal_copy(m);
}

/**
 *  Destructor.
 */
metric_mapping::~metric_mapping() {}

/**
 *  Assignment operator.
 *
 *  @param[in] m Object to copy.
 *
 *  @return This object.
 */
metric_mapping& metric_mapping::operator=(metric_mapping const& m) {
  io::data::operator=(m);
  _internal_copy(m);
  return *this;
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] m Object to copy.
 */
void metric_mapping::_internal_copy(metric_mapping const& m) {
  index_id = m.index_id;
  metric_id = m.metric_id;
}

/**************************************
 *                                     *
 *           Static Objects            *
 *                                     *
 **************************************/

// Mapping.
mapping::entry const metric_mapping::entries[] = {
    mapping::entry(&metric_mapping::index_id,
                   "index_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&metric_mapping::metric_id,
                   "metric_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry()};

// Operations.
static io::data* new_metric_mapping() {
  return new metric_mapping;
}
io::event_info::event_operations const metric_mapping::operations = {
    &new_metric_mapping, nullptr, nullptr};
