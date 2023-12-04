/**
 * Copyright 2014, 2023 Centreon
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

#include "com/centreon/broker/bam/bool_constant.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::bam;

constexpr double eps = 0.000001;

/**
 *  Constructor.
 *
 *  @param[in] val  The constant value to assign.
 */
bool_constant::bool_constant(double val)
    : _value(val), _boolean_value{std::abs(val) > ::eps} {}

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_constant::value_hard() const {
  return _value;
}

/**
 * @brief Returns the contant of bool_constant but as a boolean value.
 *
 * @return true or false.
 */
bool bool_constant::boolean_value() const {
  return _boolean_value;
}

/**
 *  Is the state known ?
 *
 *  @return  True if the state is known.
 */
bool bool_constant::state_known() const {
  return true;
}

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 */
void bool_constant::update_from(computable* child [[maybe_unused]],
                                io::stream* visitor [[maybe_unused]]) {
  log_v2::bam()->trace("bool_constant::update_from");
}

std::string bool_constant::object_info() const {
  return fmt::format("Constant {:p}\nvalue: {}", static_cast<const void*>(this),
                     value_hard());
}

void bool_constant::dump(std::ofstream& output [[maybe_unused]]) const {
  dump_parents(output);
}
