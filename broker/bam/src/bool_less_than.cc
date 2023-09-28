/*
 * Copyright 2014, 2021-2023 Centreon
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

#include "com/centreon/broker/bam/bool_less_than.hh"

using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 *
 *  @param[in] strict  Should the operator be strict?
 */
bool_less_than::bool_less_than(bool strict,
                               const std::shared_ptr<spdlog::logger>& logger)
    : bool_binary_operator(logger), _strict(strict) {}

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_less_than::value_hard() {
  return _strict ? _left_hard < _right_hard : _left_hard <= _right_hard;
}

/**
 * @brief Get the current value as a boolean
 *
 * @return True or false.
 */
bool bool_less_than::boolean_value() const {
  return _strict ? _left_hard < _right_hard : _left_hard <= _right_hard;
}

void bool_less_than::update_from(
    computable* child,
    io::stream* visitor,
    const std::shared_ptr<spdlog::logger>& logger) {
  logger->info("bool_less_than: update from {:x}", static_cast<void*>(child));
}
