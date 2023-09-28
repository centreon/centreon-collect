/*
** Copyright 2016 Centreon
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

#include "com/centreon/broker/bam/bool_constant.hh"

using namespace com::centreon::broker::bam;

constexpr double eps = 0.000001;

/**
 *  Constructor.
 *
 *  @param[in] val  The constant value to assign.
 */
bool_constant::bool_constant(double val,
                             const std::shared_ptr<spdlog::logger>& logger)
    : bool_value(logger), _value(val), _boolean_value{std::abs(val) > ::eps} {}

/**
 *  Get notified of child update.
 *
 *  @param[in] child    The child.
 *  @param[in] visitor  A visitor.
 *
 *  @return True if the parent was modified.
 */
bool bool_constant::child_has_update(computable* child [[maybe_unused]],
                                     io::stream* visitor [[maybe_unused]]) {
  return true;
}

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_constant::value_hard() {
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

void bool_constant::update_from(computable* child [[maybe_unused]],
                                io::stream* visitor [[maybe_unused]],
                                const std::shared_ptr<spdlog::logger>& logger
                                [[maybe_unused]]) {}
