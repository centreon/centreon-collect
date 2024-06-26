/**
 * Copyright 2014, 2023-2024 Centreon
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

#include "com/centreon/broker/bam/bool_more_than.hh"
#include "com/centreon/broker/bam/bool_binary_operator.hh"

using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 *
 *  @param[in] strict  Should the operator be strict?
 *  @param[in] logger  The logger to use.
 */
bool_more_than::bool_more_than(bool strict,
                               const std::shared_ptr<spdlog::logger>& logger)
    : bool_binary_operator(logger), _strict{strict} {}

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_more_than::value_hard() const {
  return _strict ? _left_hard > _right_hard : _left_hard >= _right_hard;
}

/**
 * @brief Get the current value as a boolean.
 *
 * @return True or false.
 */
bool bool_more_than::boolean_value() const {
  return _strict ? _left_hard > _right_hard : _left_hard >= _right_hard;
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string bool_more_than::object_info() const {
  return fmt::format(
      "{} {:p}\nknown: {}\nvalue: {}", static_cast<const void*>(this),
      _strict ? "GREATER THAN" : "GREATER OR EQUAL",
      state_known() ? "true" : "false", boolean_value() ? "true" : "false");
}
