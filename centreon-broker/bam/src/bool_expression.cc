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

#include <ctime>
#include "com/centreon/broker/bam/bool_expression.hh"
#include "com/centreon/broker/bam/bool_value.hh"
#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/logging/logging.hh"

using namespace com::centreon::broker::bam;
using namespace com::centreon::broker;

/**
 *  Default constructor.
 */
bool_expression::bool_expression()
  : _id(0),
    _impact_if(true) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
bool_expression::bool_expression(bool_expression const& other)
  : computable(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
bool_expression::~bool_expression() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
bool_expression& bool_expression::operator=(
                                    bool_expression const& other) {
  if (this != &other) {
    computable::operator=(other);
    _internal_copy(other);
  }
  return (*this);
}

/**
 *  Base boolean expression got updated.
 *
 *  @param[in]  child    Expression that got updated.
 *  @param[out] visitor  Receive events generated by this object.
 *
 *  @return True if the values of this object were modified.
 */
bool bool_expression::child_has_update(
                        computable* child,
                        io::stream* visitor) {
  (void)visitor;
  // It is useless to maintain a cache of expression values in this
  // class, as the bool_* classes already cache most of them.
  if (child == _expression.data()) {
    // Logging.
    logging::debug(logging::low) << "BAM: boolean expression " << _id
      << " is getting notified of child update";
  }
  return (true);
}

/**
 *  Get the boolean expression state.
 *
 *  @return Either OK (0) or CRITICAL (2).
 */
short bool_expression::get_state() const {
  return ((_expression->value_hard() == _impact_if)
          ? 2
          : 0);
}

/**
 *  Get if the state is known, i.e has been computed at least once.
 *
 *  @return  True if the state is known.
 */
bool bool_expression::state_known() const {
  return (_expression->state_known());
}

/**
 *  Get if the boolean expression is in downtime.
 *
 *  @return  True if the boolean expression is in downtime.
 */
bool bool_expression::in_downtime() const {
  return (_expression->in_downtime());
}

/**
 *  Get the expression.
 *
 *  @return  The expression.
 */
misc::shared_ptr<bool_value> bool_expression::get_expression() const {
  return (_expression);
}

/**
 *  Set evaluable boolean expression.
 *
 *  @param[in] expression Boolean expression.
 */
void bool_expression::set_expression(
                        misc::shared_ptr<bool_value> const& expression) {
  _expression = expression;
  return ;
}

/**
 *  Set boolean expression ID.
 *
 *  @param[in] id  Boolean expression ID.
 */
void bool_expression::set_id(unsigned int id) {
  _id = id;
  return ;
}

/**
 *  Set whether we should impact if the expression is true or false.
 *
 *  @param[in] impact_if True if impact is applied if the expression is
 *                       true. False otherwise.
 */
void bool_expression::set_impact_if(bool impact_if) {
  _impact_if = impact_if;
  return ;
}

/**
 *  Copy internal data members.
 *
 *  @param[in] right Object to copy.
 */
void bool_expression::_internal_copy(bool_expression const& right) {
  _expression = right._expression;
  _id = right._id;
  _impact_if = right._impact_if;
  return ;
}
