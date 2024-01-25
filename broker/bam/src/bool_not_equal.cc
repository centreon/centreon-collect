/**
 * Copyright 2016-2023 Centreon
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

#include "com/centreon/broker/bam/bool_not_equal.hh"

#include <cmath>

using namespace com::centreon::broker::bam;

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_not_equal::value_hard() const {
  return std::fabs(_left_hard - _right_hard) >= COMPARE_EPSILON ? 1.0 : 0.0;
}

/**
 *  Get the hard value as boolean.
 *
 *  @return Evaluation of the expression with hard values.
 */
bool bool_not_equal::boolean_value() const {
  return std::fabs(_left_hard - _right_hard) >= COMPARE_EPSILON;
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string bool_not_equal::object_info() const {
  return fmt::format(
      "NOT EQUAL {:p}\nknown: {}\nvalue: {}", static_cast<const void*>(this),
      state_known() ? "true" : "false", boolean_value() ? "true" : "false");
}
