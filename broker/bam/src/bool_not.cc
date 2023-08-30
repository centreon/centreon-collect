/**
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
#include "com/centreon/broker/log_v2.hh"

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
double bool_not::value_hard() const {
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

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 */
void bool_not::update_from(computable* child [[maybe_unused]],
                           io::stream* visitor) {
  log_v2::bam()->trace("bool_not::update_from");
  if (_value.get() == child)
    notify_parents_of_change(visitor);
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string bool_not::object_info() const {
  return fmt::format(
      "NOT {:x}\nknown: {}\nvalue: {}", static_cast<const void*>(this),
      state_known() ? "true" : "false", boolean_value() ? "true" : "false");
}

/**
 * @brief Recursive or not method that writes object informations to the
 * output stream. If there are children, each one dump() is then called.
 *
 * @param output An output stream.
 */
void bool_not::dump(std::ofstream& output) const {
  output << fmt::format("\"{}\" -> \"{}\"\n", object_info(),
                        _value->object_info());
  dump_parents(output);
}
