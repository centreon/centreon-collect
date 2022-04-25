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

using namespace com::centreon::broker::bam;

constexpr double eps = 0.000001;

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_and::value_hard() {
  return std::abs(_left_hard) > eps && std::abs(_right_hard) > eps;
}

/**
 *  Get the soft value.
 *
 *  @return Evaluation of the expression with soft values.
 */
double bool_and::value_soft() {
  return std::abs(_left_soft) > eps && std::abs(_right_soft) > eps;
}
