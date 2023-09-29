/**
 * Copyright 2014, 2023 Centreon
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

#include "com/centreon/broker/bam/bool_call.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::bam;

/**
 *  Constructor.
 *
 *  @param[in] name  The name of the external expression.
 */
bool_call::bool_call(std::string const& name,
                     const std::shared_ptr<spdlog::logger>& logger)
    : bool_value(logger), _name(name) {}

/**
 *  Get the hard value.
 *
 *  @return Evaluation of the expression with hard values.
 */
double bool_call::value_hard() const {
  if (!_expression)
    return 0;
  else
    return _expression->value_hard();
}

/**
 * @brief Get the current value as a boolean.
 *
 * @return True or false.
 */
bool bool_call::boolean_value() const {
  if (!_expression)
    return false;
  else
    return _expression->boolean_value();
}

/**
 *  Is the state known?
 *
 *  @return  True if the state is known.
 */
bool bool_call::state_known() const {
  if (!_expression)
    return false;
  else
    return _expression->state_known();
}

/**
 *  Get the name of this boolean expression.
 *
 *  @return  The name of this boolean expression.
 */
std::string const& bool_call::get_name() const {
  return _name;
}

/**
 *  Set expression.
 *
 *  @param[in] expression  The expression.
 */
void bool_call::set_expression(std::shared_ptr<bool_value> expression) {
  _expression = std::move(expression);
}

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 * @param logger The logger to use.
 */
void bool_call::update_from(computable* child [[maybe_unused]],
                            io::stream* visitor,
                            const std::shared_ptr<spdlog::logger>& logger) {
  logger->trace("bool_call::update_from");
  if (child == _expression.get())
    notify_parents_of_change(visitor, logger);
}
