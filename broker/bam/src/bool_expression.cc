/**
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

#include "com/centreon/broker/bam/bool_expression.hh"

#include "com/centreon/broker/bam/bool_value.hh"
#include "com/centreon/broker/bam/impact_values.hh"

using namespace com::centreon::broker::bam;
using namespace com::centreon::broker;

/**
 * @brief Constructor of a boolean expression.
 *
 * @param id Id of the boolean expression
 * @param impact_if True if impact is applied if the expression is true.False
 * otherwise.
 */
bool_expression::bool_expression(uint32_t id,
                                 bool impact_if,
                                 const std::shared_ptr<spdlog::logger>& logger)
    : computable(logger), _id(id), _impact_if(impact_if) {}

/**
 *  Get the boolean expression state.
 *
 *  @return Either OK (0) or CRITICAL (2).
 */
state bool_expression::get_state() const {
  bool v = _expression->boolean_value();
  state retval = v == _impact_if ? state_critical : state_ok;
  _logger->debug(
      "BAM: boolean expression {} - impact if: {} - value: {} - state: {}", _id,
      _impact_if, v, retval);
  return retval;
}

/**
 *  Get if the state is known, i.e has been computed at least once.
 *
 *  @return  True if the state is known.
 */
bool bool_expression::state_known() const {
  return _expression->state_known();
}

/**
 *  Get if the boolean expression is in downtime.
 *
 *  @return  True if the boolean expression is in downtime.
 */
bool bool_expression::in_downtime() const {
  return _expression->in_downtime();
}

/**
 *  Get the expression.
 *
 *  @return  The expression.
 */
std::shared_ptr<bool_value> bool_expression::get_expression() const {
  return _expression;
}

/**
 *  Set evaluable boolean expression.
 *
 *  @param[in] expression Boolean expression.
 */
void bool_expression::set_expression(
    const std::shared_ptr<bool_value>& expression) {
  _expression = expression;
}

uint32_t bool_expression::get_id() const {
  return _id;
}

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 * @param logger The logger to use.
 */
void bool_expression::update_from(computable* child, io::stream* visitor, const std::shared_ptr<spdlog::logger>& logger) {
  log_v2::bam()->trace("bool_expression::update_from");
  if (child == _expression.get())
    notify_parents_of_change(visitor);
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string bool_expression::object_info() const {
  return fmt::format("Boolean exp {}\nimpact if: {}\nknown: {}\nstate: {}",
                     get_id(), _impact_if ? "true" : "false", state_known(),
                     get_state());
}

/**
 * @brief Recursive or not method that writes object informations to the
 * output stream. If there are children, each one dump() is then called.
 *
 * @param output An output stream.
 */
void bool_expression::dump(std::ofstream& output) const {
  output << fmt::format("\"{}\" -> \"{}\"\n", object_info(),
                        _expression->object_info());
  _expression->dump(output);
  dump_parents(output);
}
