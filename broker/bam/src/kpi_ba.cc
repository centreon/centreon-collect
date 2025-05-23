/**
 * Copyright 2014-2015, 2021-2024 Centreon
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

#include "com/centreon/broker/bam/kpi_ba.hh"

#include "com/centreon/broker/bam/ba.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Default constructor.
 */
kpi_ba::kpi_ba(uint32_t kpi_id,
               uint32_t ba_id,
               const std::string& ba_name,
               const std::shared_ptr<spdlog::logger>& logger)
    : kpi(kpi_id, ba_id, ba_name, logger) {}

/**
 *  Get the impact introduced by a CRITICAL state of the BA.
 *
 *  @return Impact if BA is CRITICAL.
 */
double kpi_ba::get_impact_critical() const {
  return _impact_critical;
}

/**
 *  Get the impact introduced by a WARNING state of the BA.
 *
 *  @return Impact if BA is WARNING.
 */
double kpi_ba::get_impact_warning() const {
  return _impact_warning;
}

/**
 *  Get the hard impact introduced by the BA.
 *
 *  @param[out] hard_impact Hard impacts.
 */
void kpi_ba::impact_hard(impact_values& hard_impact) {
  _fill_impact(hard_impact, _ba->get_state_hard(), _ba->get_ack_impact_hard(),
               _ba->get_downtime_impact_hard());
}

/**
 *  Get the soft impact introduced by the BA.
 *
 *  @param[out] soft_impact Soft impacts.
 */
void kpi_ba::impact_soft(impact_values& soft_impact) {
  _fill_impact(soft_impact, _ba->get_state_soft(), 0,
               _ba->get_downtime_impact_soft());
}

/**
 *  Link the kpi_ba with a specific BA (class ba).
 *
 *  @param[in] my_ba Linked BA.
 */
void kpi_ba::link_ba(std::shared_ptr<ba>& my_ba) {
  _logger->trace("kpi ba ({}, {}) linked to ba {} {}", _id, _ba_id,
                 my_ba->get_name(), my_ba->get_id());
  _ba = my_ba;
}

/**
 *  Set impact if BA is CRITICAL.
 *
 *  @param[in] impact Impact if BA is CRITICAL.
 */
void kpi_ba::set_impact_critical(double impact) {
  _impact_critical = impact;
}

/**
 *  Set impact if BA is WARNING.
 *
 *  @param[in] impact Impact if BA is WARNING.
 */
void kpi_ba::set_impact_warning(double impact) {
  _impact_warning = impact;
}

/**
 *  Set impact if BA is UNKNOWN.
 *
 *  @param[in] impact Impact if BA is UNKNOWN.
 */
void kpi_ba::set_impact_unknown(double impact) {
  _impact_unknown = impact;
}

/**
 *  Unlink from BA.
 */
void kpi_ba::unlink_ba() {
  _logger->trace("kpi ba ({}, {}) unlinked from ba {} {}", _id, _ba_id,
                 _ba->get_name(), _ba->get_id());
  _ba.reset();
}

/**
 *  Visit BA KPI.
 *
 *  @param[out] visitor  Object that will receive status and events.
 */
void kpi_ba::visit(io::stream* visitor) {
  if (visitor) {
    // Commit the initial events saved in the cache.
    commit_initial_events(visitor);

    // Get information.
    impact_values hard_values;
    impact_values soft_values;
    impact_hard(hard_values);
    impact_soft(soft_values);

    // Generate BI events.
    {
      // BA event state.
      std::shared_ptr<pb_ba_event> bae(_ba->get_ba_event());
      com::centreon::broker::State ba_state =
          bae ? bae->obj().status() : com::centreon::broker::State::OK;
      timestamp last_ba_update(bae ? bae->obj().start_time() : time(nullptr));

      // If no event was cached, create one.
      if (!_event) {
        if (!last_ba_update.is_null())
          _open_new_event(visitor, hard_values.get_nominal(), ba_state,
                          last_ba_update);
      }
      // If state changed, close event and open a new one.
      else if (_ba->in_downtime() != _event->in_downtime() ||
               ba_state != _event->status()) {
        _event->set_end_time(last_ba_update.get_time_t());
        visitor->write(std::make_shared<pb_kpi_event>(*_event));
        _event.reset();
        _open_new_event(visitor, hard_values.get_nominal(), ba_state,
                        last_ba_update);
      }
    }

    // Generate status event.
    {
      _logger->debug("Generating kpi status {} for BA {}", _id, _ba_id);
      std::shared_ptr<pb_kpi_status> status{std::make_shared<pb_kpi_status>()};
      KpiStatus& ev(status->mut_obj());
      ev.set_kpi_id(_id);
      ev.set_in_downtime(in_downtime());
      ev.set_level_acknowledgement_hard(hard_values.get_acknowledgement());
      ev.set_level_acknowledgement_soft(soft_values.get_acknowledgement());
      ev.set_level_downtime_hard(hard_values.get_downtime());
      ev.set_level_downtime_soft(soft_values.get_downtime());
      ev.set_level_nominal_hard(hard_values.get_nominal());
      ev.set_level_nominal_soft(soft_values.get_nominal());
      ev.set_state_hard(State(_ba->get_state_hard()));
      ev.set_state_soft(State(_ba->get_state_soft()));
      ev.set_last_state_change(get_last_state_change().get_time_t());
      ev.set_last_impact(hard_values.get_nominal());
      visitor->write(std::static_pointer_cast<io::data>(status));
    }
  }
}

/**
 *  Fill impact_values from base values.
 *
 *  @param[out] impact          Impact values.
 *  @param[in]  state           BA state.
 *  @param[in]  acknowledgement Acknowledgement impact of the BA.
 *  @param[in]  downtime        Downtime impact of the BA.
 */
void kpi_ba::_fill_impact(impact_values& impact,
                          state state,
                          double acknowledgement,
                          double downtime) {
  // Get nominal impact from state.
  double nominal;
  switch (state) {
    case state_ok:
      nominal = 0.0;
      break;
    case state_warning:
      nominal = _impact_warning;
      break;
    case state_critical:
      nominal = _impact_critical;
      break;
    default:
      nominal = _impact_unknown;
      break;
  }
  impact.set_nominal(nominal);

  // Compute acknowledged and downtimed impacts. Acknowledgement and
  // downtime impacts provided as arguments are from the BA. Therefore
  // are used to proportionnaly compute the acknowledged and downtimed
  // impacts, relative to the nominal impact.
  if (acknowledgement < 0.0)
    acknowledgement = 0.0;
  else if (acknowledgement > 100.0)
    acknowledgement = 100.0;
  impact.set_acknowledgement(acknowledgement * nominal / 100.0);
  if (downtime < 0.0)
    downtime = 0.0;
  else if (downtime > 100.0)
    downtime = 100.0;
  impact.set_downtime(downtime * nominal / 100.0);
  impact.set_state(state);
}

/**
 *  Open a new event for this KPI.
 *
 *  @param[out] visitor           Visitor that will receive events.
 *  @param[in]  impact            Current impact of this KPI.
 *  @param[in]  ba_state          BA state.
 *  @param[in]  event_start_time  Event start time.
 */
void kpi_ba::_open_new_event(io::stream* visitor,
                             int impact,
                             com::centreon::broker::State ba_state,
                             const timestamp& event_start_time) {
  _event_init();
  _event->set_start_time(event_start_time.get_time_t());
  _event->set_end_time(-1);
  _event->set_impact_level(impact);
  _event->set_in_downtime(_ba->in_downtime());
  _event->set_output(_ba->get_output());
  _event->set_perfdata(_ba->get_perfdata());
  _event->set_status(com::centreon::broker::State(ba_state));
  if (visitor)
    visitor->write(std::make_shared<pb_kpi_event>(*_event));
}

/**
 *  Is this KPI in an ok state?
 *
 *  @return  True if this KPI is in an ok state.
 */
bool kpi_ba::ok_state() const {
  return _ba->get_state_hard() == 0;
}

/**
 *  Is this KPI in downtime?
 *
 *  @return  True if this KPI is in downtime.
 */
bool kpi_ba::in_downtime() const {
  return _ba->in_downtime();
}

/**
 * @brief Update this computable with the child modifications.
 *
 * @param child The child that changed.
 * @param visitor The visitor to handle events.
 */
void kpi_ba::update_from(computable* child, io::stream* visitor) {
  _logger->trace("kpi_ba::update_from");
  // It is useless to maintain a cache of BA values in this class, as
  // the ba class already cache most of them.
  if (child == _ba.get()) {
    // Logging.
    _logger->debug("BAM: BA {} KPI {} is getting notified of child update",
                   _ba_id, _id);

    // Generate status event.
    visit(visitor);

    notify_parents_of_change(visitor);
  }
}

/**
 * @brief This method is used by the dump() method. It gives a summary of this
 * computable main informations.
 *
 * @return A multiline strings with various informations.
 */
std::string kpi_ba::object_info() const {
  return fmt::format("KPI {} on BA", get_id());
}

/**
 * @brief Recursive or not method that writes object informations to the
 * output stream. If there are children, each one dump() is then called.
 *
 * @param output An output stream.
 */
void kpi_ba::dump(std::ofstream& output) const {
  output << fmt::format("\"{}\" -> \"{}\"", object_info(), _ba->object_info());
  _ba->dump(output);
  dump_parents(output);
}
