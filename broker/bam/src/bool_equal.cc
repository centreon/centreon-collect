/*
 * Copyright 2014-2023 Centreon
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

#include "com/centreon/broker/bam/bool_equal.hh"

#include <cmath>

using namespace com::centreon::broker::bam;

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_equal::value_hard() const {
  bool retval = false;
  if (state_known())
    retval = std::fabs(_left_hard - _right_hard) < COMPARE_EPSILON;

  _logger->trace("BAM: bool_equal: value as double: {}", retval);
  return retval;
}

/**
 * @brief Get the current as a boolean
 *
 * @return True or false.
 */
bool bool_equal::boolean_value() const {
  bool retval = false;
  if (state_known())
    retval = std::fabs(_left_hard - _right_hard) < COMPARE_EPSILON;

  _logger->trace("BAM: bool_equal: value: {}", retval);
  return retval;
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string bool_equal::object_info() const {
  return fmt::format(
      "EQUAL {:p}\nknown: {}\nvalue: {}", static_cast<const void*>(this),
      state_known() ? "true" : "false", boolean_value() ? "true" : "false");
}
