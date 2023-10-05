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

#include "com/centreon/broker/bam/kpi_boolexp.hh"
#include "com/centreon/broker/bam/bool_expression.hh"
#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/bam/internal.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Default constructor.
 */
kpi_boolexp::kpi_boolexp(uint32_t kpi_id,
                         uint32_t ba_id,
                         const std::shared_ptr<spdlog::logger>& logger)
    : kpi(kpi_id, ba_id, logger) {}

/**
 *  Return true if in downtime.
 *
 *  @return  True or false.
 */
bool kpi_boolexp::in_downtime() const {
  if (_boolexp)
    return _boolexp->in_downtime();
  return false;
}

/**
 *  Get the impact introduced when the boolean expression is triggered.
 *
 *  @return Impact if the boolean expression is triggered.
 */
double kpi_boolexp::get_impact() const {
  return _impact;
}

/**
 *  Get the hard impact introduced by the boolean expression.
 *
 *  @param[out] hard_impact  Hard impacts.
 */
void kpi_boolexp::impact_hard(impact_values& hard_impact) {
  _fill_impact(hard_impact);
}

/**
 *  Get the soft impact introduced by the boolean expression.
 *
 *  @param[out] soft_impact  Soft impacts.
 */
void kpi_boolexp::impact_soft(impact_values& soft_impact) {
  _fill_impact(soft_impact);
}

/**
 *  Link the kpi_boolexp with a specific boolean expression (class
 *  bool_expression).
 *
 *  @param[in] my_boolexp  Linked boolean expression.
 */
void kpi_boolexp::link_boolexp(std::shared_ptr<bool_expression>& my_boolexp) {
  _boolexp = my_boolexp;
}

/**
 *  Set impact if the boolean expression is triggered.
 *
 *  @param[in] impact  Impact if the boolean expression is triggered.
 */
void kpi_boolexp::set_impact(double impact) {
  _impact = impact;
}

/**
 *  Unlink from boolean expression.
 */
void kpi_boolexp::unlink_boolexp() {
  _boolexp.reset();
}

/**
 *  Visit boolean expression KPI.
 *
 *  @param[out] visitor  Object that will receive status and events.
 */
void kpi_boolexp::visit(io::stream* visitor) {
  if (visitor) {
    // Commit the initial events saved in the cache.
    commit_initial_events(visitor);

    // Get information (HARD and SOFT values are the same).
    impact_values values;
    impact_hard(values);
    state state = _current_state;

    // Generate BI events.
    {
      // If no event was cached, create one.
      if (!_event)
        _open_new_event(visitor, values.get_nominal(), state);
      // If state changed, close event and open a new one.
      else if (state != _event->status()) {
        _event->set_end_time(::time(nullptr));
        visitor->write(std::make_shared<pb_kpi_event>(std::move(*_event)));
        _event.reset();
        _open_new_event(visitor, values.get_nominal(), state);
      }
    }

    // Generate status event.
    {
      std::shared_ptr<pb_kpi_status> status(std::make_shared<pb_kpi_status>());
      KpiStatus& ev(status->mut_obj());
      ev.set_kpi_id(_id);
      ev.set_in_downtime(in_downtime());
      ev.set_level_acknowledgement_hard(values.get_acknowledgement());
      ev.set_level_acknowledgement_soft(values.get_acknowledgement());
      ev.set_level_downtime_hard(values.get_downtime());
      ev.set_level_downtime_soft(values.get_downtime());
      ev.set_level_nominal_hard(values.get_nominal());
      ev.set_level_nominal_soft(values.get_nominal());
      ev.set_state_hard(State(state));
      ev.set_state_soft(State(state));
      ev.set_last_state_change(get_last_state_change().get_time_t());
      ev.set_last_impact(values.get_nominal());
      visitor->write(std::static_pointer_cast<io::data>(status));
    }
  }
}

/**
 *  Fill impact_values from base values.
 *
 *  @param[out] impact  Impact values.
 */
void kpi_boolexp::_fill_impact(impact_values& impact) {
  // Get nominal impact from state.
  bam::state state = _current_state;
  double nominal;
  if (state_ok == state)
    nominal = 0.0;
  else
    nominal = _impact;
  impact.set_nominal(nominal);
  impact.set_acknowledgement(0.0);
  impact.set_downtime(0.0);
  impact.set_state(state);
}

/**
 *  Open a new event for this KPI.
 *
 *  @param[out] visitor  Visitor that will receive events.
 *  @param[in]  impact   Current impact of this KPI.
 *  @param[in]  state    Boolean expression state.
 */
void kpi_boolexp::_open_new_event(io::stream* visitor,
                                  int impact,
                                  state state) {
  _event_init();
  _event->set_start_time(time(nullptr));
  _event->set_end_time(-1);
  _event->set_impact_level(impact);
  _event->set_in_downtime(false);
  _event->set_output("BAM boolean expression computed by Centreon Broker");
  _event->set_perfdata("");
  _event->set_status(com::centreon::broker::State(state));
  if (visitor) {
    visitor->write(std::make_shared<pb_kpi_event>(*_event));
  }
}

/**
 *  @brief Get the current state of the boolexp and store it into the attribute
 *  _current_state.
 *
 *  A boolean expression can be uninitialized yet, if a service status
 *  has yet to come. If this is the case, the status is the one of the
 *  opened event.
 *
 *  @return  The current state of the boolexp.
 */
void kpi_boolexp::_update_state() {
  uint32_t id = _boolexp->get_id();
  if (_boolexp->state_known()) {
    _current_state = _boolexp->get_state();
    _logger->trace("BAM: kpi {} boolean expression: state (known) value: {}",
                   id, _current_state);
  } else if (_event) {
    _current_state = static_cast<state>(_event->status());
    _logger->trace(
        "BAM: kpi {} boolean expression: state from internal event: {}", id,
        _current_state);
  } else {
    _current_state = _boolexp->get_state();
    _logger->trace(
        "BAM: kpi {} boolean expression: state value still taken from "
        "boolexp: {}",
        id, _current_state);
  }
}

/**
 *  Is this KPI in an ok state?
 *
 *  @return  True if this KPI is in an ok state.
 */
bool kpi_boolexp::ok_state() const {
  return _current_state == state_ok || _current_state == state_unknown;
}

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 * @param logger The logger to use.
 */
void kpi_boolexp::update_from(computable* child,
                              io::stream* visitor,
                              const std::shared_ptr<spdlog::logger>& logger) {
  logger->trace("kpi_boolexp::update_from");
  // It is useless to maintain a cache of boolean expression values in
  // this class, as the bool_expression class already cache most of them.
  if (child == _boolexp.get()) {
    state old_state = _current_state;
    // Generate status event.
    _update_state();
    visit(visitor);
    _logger->debug(
        "BAM: boolean expression KPI {} is getting notified of child update "
        "old_state={}, new_state={}",
        _id, old_state, _current_state);
    if (old_state != _current_state)
      notify_parents_of_change(visitor, logger);
  }
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string kpi_boolexp::object_info() const {
  return fmt::format("KPI {}\nstate: {}\n", get_id(), _current_state);
}

/**
 * @brief Recursive or not method that writes object informations to the
 * output stream. If there are children, each one dump() is then called.
 *
 * @param output An output stream.
 */
void kpi_boolexp::dump(std::ofstream& output) const {
  output << fmt::format("\"{}\" -> \"{}\"\n", object_info(),
                        _boolexp->object_info());
  _boolexp->dump(output);
  dump_parents(output);
}
