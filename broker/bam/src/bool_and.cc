/*
** Copyright 2014 Centreon
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

#include "com/centreon/broker/bam/bool_and.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::bam;

constexpr double eps = 0.000001;

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_and::value_hard() {
  return std::abs(_left_hard) > ::eps && std::abs(_right_hard) > ::eps;
}

/**
 *  Get the soft value.
 *
 *  @return Evaluation of the expression with soft values.
 */
double bool_and::value_soft() {
  return std::abs(_left_soft) > ::eps && std::abs(_right_soft) > ::eps;
}

/**
 * @brief Tell if this boolean value has a known state. Since it is a logical
 * and, if one operand is known to be false, we can consider the state to be
 * known because the result will be false.
 *
 * @return a boolean.
 */
bool bool_and::state_known() const {
  bool left_exists = _left && _left->state_known();
  bool right_exists = _right && _right->state_known();
  bool retval = (left_exists && right_exists) ||
                (left_exists && std::abs(_left_hard) < ::eps) ||
                (right_exists && std::abs(_right_hard) < ::eps);
  log_v2::bam()->debug("BAM: bool and: state known {}", retval);
  return retval;
}
