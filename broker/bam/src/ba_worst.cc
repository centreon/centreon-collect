/**
 * Copyright 2022-2024 Centreon
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

#include "com/centreon/broker/bam/ba_worst.hh"

#include <fmt/format.h>

#include <cassert>

#include "bbdo/bam/ba_status.hh"
#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/bam/kpi.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/service_status.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Constructor.
 *
 *  @param[in] id the id of this ba.
 *  @param[in] host_id the id of the associated host.
 *  @param[in] service_id the id of the associated service.
 *  @param[in] generate_virtual_status  Whether or not the BA object
 *                                      should generate statuses of
 *                                      virtual hosts and services.
 *  @param[in] logger The logger to use in this BA.
 */
ba_worst::ba_worst(uint32_t id,
                   uint32_t host_id,
                   uint32_t service_id,
                   bool generate_virtual_status,
                   const std::shared_ptr<spdlog::logger>& logger)
    : ba(id,
         host_id,
         service_id,
         configuration::ba::state_source_worst,
         generate_virtual_status,
         logger) {}

/**
 *  Get BA hard state.
 *
 *  @return BA hard state.
 */
state ba_worst::get_state_hard() const {
  bam::state state;

  auto every_kpi_in_dt =
      [](const std::unordered_map<kpi*, bam::ba::impact_info>& imp) -> bool {
    if (imp.empty())
      return false;

    for (auto it = imp.begin(), end = imp.end(); it != end; ++it) {
      if (!it->first->in_downtime())
        return false;
    }

    return true;
  };

  if (_dt_behaviour == configuration::ba::dt_ignore_kpi &&
      every_kpi_in_dt(_impacts))
    state = state_ok;
  else
    state = _computed_hard_state;

  return state;
}

/**
 *  Get BA soft state.
 *
 *  @return BA soft state.
 */
state ba_worst::get_state_soft() const {
  return _computed_soft_state;
}

/**
 *  Apply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_worst::_apply_impact(kpi* kpi_ptr [[maybe_unused]],
                             ba::impact_info& impact) {
  auto is_state_worse = [](short current_state, short new_state) -> bool {
    const std::array<short, 5> order{0, 3, 4, 2, 1};
    assert((unsigned int)current_state < order.size());
    assert((unsigned int)new_state < order.size());
    return order[new_state] > order[current_state];
  };

  // Adjust values.
  _acknowledgement_count += impact.hard_impact.get_acknowledgement();

  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return;

  if (is_state_worse(_computed_soft_state, impact.soft_impact.get_state()))
    _computed_soft_state = impact.soft_impact.get_state();
  if (is_state_worse(_computed_hard_state, impact.hard_impact.get_state()))
    _computed_hard_state = impact.hard_impact.get_state();
}

/**
 *  @brief Apply some child changes. This method is more complete than
 * _apply_impact(). It takes as argument a child kpi and its impact. This child
 * is already known, so its previous impact is replaced by the new one. In other
 * words, this method makes almost the same work as _unapply_impact() and the
 * _apply_impact() ; the difference is that it returns true if the BA really
 * changed.
 *
 * @param child The kpi that changed
 * @param new_hard_impact The new hard impact for this child.
 * @param new_soft_impact The new soft impact for this child.
 * @param child_in_downtime The child is in downtime (or not).
 *
 * @return True if the BA changes, False otherwise.
 */
bool ba_worst::_apply_changes(kpi* child,
                              const impact_values& new_hard_impact,
                              const impact_values& new_soft_impact,
                              bool child_in_downtime) {
  state previous_state = _computed_hard_state;

  _computed_soft_state = _computed_hard_state = state_ok;

  // We recompute all impacts...
  for (auto it = _impacts.begin(), end = _impacts.end(); it != end; ++it) {
    /* The changed child is updated in _impacts */
    if (it->first == child) {
      it->second.hard_impact = new_hard_impact;
      it->second.soft_impact = new_soft_impact;
      it->second.in_downtime = child_in_downtime;
    }
    _apply_impact(it->first, it->second);
  }

  return _computed_hard_state != previous_state;
}

/**
 *  Unapply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_worst::_unapply_impact(kpi* kpi_ptr,
                               ba::impact_info& impact [[maybe_unused]]) {
  // Prevent derive of values.
  _computed_soft_state = _computed_hard_state = state_ok;

  // Adjust values.
  _acknowledgement_count -= impact.hard_impact.get_acknowledgement();

  // We recompute all impacts, except the one to unapply...
  for (auto it = _impacts.begin(), end = _impacts.end(); it != end; ++it)
    if (it->first != kpi_ptr)
      _apply_impact(it->first, it->second);
}

/**
 *  Get the output.
 *
 *  @return Service output.
 */
std::string ba_worst::get_output() const {
  std::string retval;
  auto not_ok_kpis = [this]() {
    std::list<std::string> retval;
    for (auto it = _impacts.begin(), end = _impacts.end(); it != end; ++it) {
      state state = it->second.hard_impact.get_state();
      assert(static_cast<uint32_t>(state) < 4);
      if (state != state_ok) {
        retval.emplace_back(fmt::format(
            "KPI {} is in {} state", it->first->get_name(), state_str[state]));
      }
    }
    return retval;
  };

  state state = get_state_hard();
  switch (state) {
    case state_unknown:
      retval = fmt::format(
          "Status is UNKNOWN - At least one KPI is in an UNKNOWN state: {}",
          fmt::join(not_ok_kpis(), ", "));
      break;
    case state_ok:
      retval = "Status is OK - All KPIs are in an OK state";
      break;
    case state_warning:
      retval = fmt::format(
          "Status is WARNING - At least one KPI is in a WARNING state: {}",
          fmt::join(not_ok_kpis(), ", "));
      break;
    case state_critical:
      retval = fmt::format(
          "Status is CRITICAL - At least one KPI is in a CRITICAL state: {}",
          fmt::join(not_ok_kpis(), ", "));
      break;
  }
  return retval;
}

/**
 *  Get the performance data.
 *
 *  @return Performance data.
 */
std::string ba_worst::get_perfdata() const {
  return {};
}

std::shared_ptr<pb_ba_status> ba_worst::_generate_ba_status(
    bool state_changed) const {
  auto ret = std::make_shared<pb_ba_status>();
  BaStatus& status = ret->mut_obj();
  status.set_ba_id(get_id());
  status.set_in_downtime(in_downtime());
  if (_event)
    status.set_last_state_change(_event->obj().start_time());
  else
    status.set_last_state_change(get_last_kpi_update());
  status.set_level_acknowledgement(_acknowledgement_count);
  status.set_state(com::centreon::broker::State(get_state_hard()));
  status.set_state_changed(state_changed);
  std::string perfdata = get_perfdata();
  if (perfdata.empty())
    status.set_output(get_output());
  else
    status.set_output(get_output() + "|" + perfdata);

  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM: generating status of worst BA {} '{}' (state {}, in downtime {})",
      get_id(), _name, status.state(), status.in_downtime());
  return ret;
}
