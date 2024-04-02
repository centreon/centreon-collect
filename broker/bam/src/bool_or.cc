/**
 * Copyright 2014, 2021, 2023 Centreon
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

#include "com/centreon/broker/bam/bool_or.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::bam;

constexpr double eps = 0.000001;

/**
 * @brief Check that the children are known and in that case propagate their
 * values to _left_hard and _right_hard. Otherwise these attributes have no
 * meaning.
 *
 * After a call to this internal function, the _state_known attribute has the
 * good value.
 */
void bool_or::_update_state() {
  _logger->trace("bool_or::update_state...");
  if (_left && _left->state_known() && _left->boolean_value()) {
    _logger->trace("bam: bool or left changed to true");
    _left_hard = true;
    _boolean_value = true;
    _state_known = true;
  } else if (_right && _right->state_known() && _right->boolean_value()) {
    _logger->trace("bam: bool or right changed to true");
    _right_hard = true;
    _boolean_value = true;
    _state_known = true;
  } else {
    _logger->trace(
        "bam: bool or: left false or unknown - right false or unknown");
    _boolean_value = false;
    bool_binary_operator::_update_state();
    _logger->trace("bam: bool or generic rule applied: value: {} - known : {}",
                   _boolean_value, _state_known);
  }
}

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_or::value_hard() const {
  return _boolean_value;
}

/**
 *  Get the hard value as a boolean.
 *
 *  @return Evaluation of the expression with hard values.
 */
bool bool_or::boolean_value() const {
  return _boolean_value;
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string bool_or::object_info() const {
  return fmt::format(
      "OR {:p}\nknown: {}\nvalue: {}", static_cast<const void*>(this),
      state_known() ? "true" : "false", boolean_value() ? "true" : "false");
}
