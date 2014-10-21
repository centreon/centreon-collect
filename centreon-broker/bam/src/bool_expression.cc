/*
** Copyright 2014 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.

*/

#include "com/centreon/broker/bam/bool_expression.hh"
#include "com/centreon/broker/bam/bool_status.hh"
#include "com/centreon/broker/bam/bool_value.hh"
#include "com/centreon/broker/bam/stream.hh"
#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/logging/logging.hh"

using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 */
bool_expression::bool_expression()
  : _id(0),
    _impact_if(true),
    _impact_hard(0.0),
    _impact_soft(0.0) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
bool_expression::bool_expression(bool_expression const& right)
  : kpi(right) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
bool_expression::~bool_expression() {}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
bool_expression& bool_expression::operator=(
                                    bool_expression const& right) {
  if (this != &right) {
    kpi::operator=(right);
    _internal_copy(right);
  }
  return (*this);
}

/**
 *  @brief Add a KPI ID to this boolean expression.
 *
 *  Boolean expression are simulated as KPI.
 *
 *  @param[in] id  One of the boolean expression KPI ID.
 */
void bool_expression::add_kpi_id(unsigned int id) {
  _kpis.push_back(id);
  return ;
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
                        stream* visitor) {
  // It is useless to maintain a cache of expression values in this
  // class, as the bool_* classes already cache most of them.
  if (child == _expression.data()) {
    // Logging.
    logging::debug(logging::low) << "BAM: boolexp " << _id
      << " is getting notified of child update";

    // Generate status event.
    visit(visitor);
  }
  return true;
}

/**
 *  Get the hard state.
 *
 *  @return Boolean expression hard state.
 */
short bool_expression::get_state_hard() const {
  return ((_expression->value_hard() == _impact_if) ? 2 : 0);
}

/**
 *  Get the soft state.
 *
 *  @return Boolean expression soft state.
 */
short bool_expression::get_state_soft() const {
  return ((_expression->value_soft() == _impact_if) ? 2 : 0);
}

/**
 *  Get the hard impacts.
 *
 *  @param[out] hard_impact Hard impacts.
 */
void bool_expression::impact_hard(impact_values& hard_impact) {
  bool value(_expression->value_hard());
  hard_impact.set_nominal(((value && _impact_if)
                           || (!value && !_impact_if))
                          ? _impact_hard
                          : 0.0);
  hard_impact.set_acknowledgement(0.0);
  hard_impact.set_downtime(0.0);
  return ;
}

/**
 *  Get the soft impacts.
 *
 *  @param[out] soft_impact Soft impacts.
 */
void bool_expression::impact_soft(impact_values& soft_impact) {
  bool value(_expression->value_soft());
  soft_impact.set_nominal(((value && _impact_if)
                           || (!value && !_impact_if))
                          ? _impact_soft
                          : 0.0);
  return ;
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
 *  Set hard impact.
 *
 *  @param[in] impact Hard impact.
 */
void bool_expression::set_impact_hard(double impact) {
  _impact_hard = impact;
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
 *  Set soft impact.
 *
 *  @param[in] impact Soft impact.
 */
void bool_expression::set_impact_soft(double impact) {
  _impact_soft = impact;
  return ;
}

/**
 *  Set the kpi id of this boolean expression.
 *
 *  @param[in] id  The kpi id to set.
 */
void bool_expression::set_kpi_id(unsigned int id) {
  kpi::set_id(id);
}

/**
 *  Visit boolean expression.
 *
 *  @param[out] visitor  Object that will receive status.
 */
void bool_expression::visit(stream* visitor) {
  if (visitor) {
    // Generate status event.
    {
      misc::shared_ptr<bool_status> b(new bool_status);
      b->bool_id = _id;
      b->state = _expression->value_hard();
      visitor->write(b.staticCast<io::data>());
    }

    // Generate BI events.
    {
      // Get impact.
      impact_values impacts;
      impact_hard(impacts);

      // If no event was cached, create one.
      if (_event.isNull()) {
        _open_new_event(visitor);
      }
      // If state changed, close event and open a new one.
      else if (get_state_hard() != _event->status) {
        _event->historic = _is_historical_event(_event->start_time);
        _event->end_time = time(NULL);
        for (std::list<unsigned int>::const_iterator
               it(_kpis.begin()),
               end(_kpis.end());
             it != end;
             ++it) {
          misc::shared_ptr<kpi_event> ke(new kpi_event(*_event));
          ke->kpi_id = *it;
          visitor->write(ke.staticCast<io::data>());
        }
        _event.clear();
        _open_new_event(visitor);
      }
    }
  }
  return ;
}

/**
 *  Copy internal data members.
 *
 *  @param[in] right Object to copy.
 */
void bool_expression::_internal_copy(bool_expression const& right) {
  _event = right._event;
  _expression = right._expression;
  _id = right._id;
  _impact_if = right._impact_if;
  _impact_hard = right._impact_hard;
  _impact_soft = right._impact_soft;
  return ;
}

/**
 *  Open a new event.
 *
 *  @param[out] visitor  Visitor that will receive events.
 */
void bool_expression::_open_new_event(stream* visitor) {
  impact_values impacts;
  impact_hard(impacts);
  _event->impact_level = impacts.get_nominal();
  _event->in_downtime = false;
  _event->output = "BAM boolean expression computed by Centreon Broker";
  _event->perfdata.clear();
  _event->start_time = time(NULL);
  _event->status = get_state_hard();
  if (visitor)
    for (std::list<unsigned int>::const_iterator
           it(_kpis.begin()),
           end(_kpis.end());
         it != end;
         ++it) {
      misc::shared_ptr<kpi_event> ke(new kpi_event(*_event));
      ke->kpi_id = *it;
      visitor->write(ke.staticCast<io::data>());
    }
  return ;
}
