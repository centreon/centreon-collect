/*
 * Copyright 2014, 2022-2023 Centreon
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

#include "com/centreon/broker/bam/bool_not.hh"
#include <cmath>

using namespace com::centreon::broker::bam;

constexpr double eps = 0.000001;

/**
 *  Constructor.
 *
 *  @param[in] val Value that will be negated.
 */
bool_not::bool_not(bool_value::ptr val,
                   const std::shared_ptr<spdlog::logger>& logger)
    : bool_value(logger), _value(std::move(val)) {}

/**
 *  @brief Notify of the change of value of the child.
 *
 *  This class does not cache values. This method is therefore useless
 *  for this class.
 *
 *  @param[in] child     Unused.
 *  @param[out] visitor  Unused.
 *
 *  @return              True;
 */
bool bool_not::child_has_update(computable* child [[maybe_unused]],
                                io::stream* visitor [[maybe_unused]]) {
  return true;
}

/**
 *  Set value object.
 *
 *  @param[in] value Value object whose value will be negated.
 */
void bool_not::set_value(std::shared_ptr<bool_value>& value) {
  _value = value;
}

/**
 *  Get the hard value.
 *
 *  @return Hard value.
 */
double bool_not::value_hard() {
  return std::abs(_value->value_hard()) < ::eps;
}

/**
 * @brief Get the current value as a boolean
 *
 * @return True or false.
 */
bool bool_not::boolean_value() const {
  return std::abs(_value->value_hard()) < ::eps;
}

/**
 *  Get if the state is known, i.e has been computed at least once.
 *
 *  @return  True if the state is known.
 */
bool bool_not::state_known() const {
  return _value && _value->state_known() != 0;
}

/**
 *  Is this expression in downtime?
 *
 *  @return  True if this expression is in downtime.
 */
bool bool_not::in_downtime() const {
  return _value && _value->in_downtime() != 0;
}

void bool_not::update_from(computable* child,
                           io::stream* visitor,
                           const std::shared_ptr<spdlog::logger>& logger) {
  assert("bool_not" == 0);
  logger->info("bool_not: update from {:x}", static_cast<void*>(child));
}
