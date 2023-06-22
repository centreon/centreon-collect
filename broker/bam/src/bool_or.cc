/*
** Copyright 2014, 2021 Centreon
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

#include "com/centreon/broker/bam/bool_or.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::bam;

constexpr double eps = 0.000001;

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_or::value_hard() {
  bool left = std::abs(_left_hard) >= ::eps;
  bool right = std::abs(_right_hard) >= ::eps;
  double retval = left || right;
  log_v2::bam()->debug(
      "bool_or: (hard) left: {} | (hard) right: {} = (hard) result: {}", left,
      right, retval);
  return retval;
}

/**
 *  Get the soft value.
 *
 *  @return Evaluation of the expression with soft values.
 */
double bool_or::value_soft() {
  bool left = std::abs(_left_soft) >= ::eps;
  bool right = std::abs(_right_soft) >= ::eps;
  double retval = left || right;
  log_v2::bam()->debug(
      "bool_or: (soft) left: {} | (soft) right: {} = (soft) result: {}", left,
      right, retval);
  return retval;
}

/**
 * @brief Tell if this boolean value has a known state. Since it is a logical
 * or, if one operand is known to be true, we can consider the state to be
 * known because the result will be true.
 *
 * @return a boolean.
 */
bool bool_or::state_known() const {
  bool left_exists = _left && _left->state_known();
  bool right_exists = _right && _right->state_known();
  bool retval = (left_exists && right_exists) ||
                (left_exists && std::abs(_left_hard) >= ::eps) ||
                (right_exists && std::abs(_right_hard) >= ::eps);
  log_v2::bam()->debug("BAM: bool or: state known {}", retval);
  return retval;
}
