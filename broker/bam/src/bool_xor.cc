/*
** Copyright 2014, 2022 Centreon
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

#include "com/centreon/broker/bam/bool_xor.hh"

using namespace com::centreon::broker::bam;

constexpr double eps = 0.000001;

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_xor::value_hard() {
  bool left = std::abs(_left_hard) > eps;
  bool right = std::abs(_right_hard) > eps;
  return left ^ right;
}

/**
 * @brief Get the current value as a boolean
 *
 * @return True or false.
 */
bool bool_xor::boolean_value() const {
  bool left = std::abs(_left_hard) > eps;
  bool right = std::abs(_right_hard) > eps;
  return left ^ right;
}

void bool_xor::update_from(computable* child,
                           io::stream* visitor,
                           const std::shared_ptr<spdlog::logger>& logger) {
  assert("bool_xor" == 0);
  logger->info("bool_xor: update from {:x}", static_cast<void*>(child));
}
