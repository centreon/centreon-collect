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

#include "com/centreon/broker/bam/ba_ratio_number.hh"

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

static constexpr double eps = 0.000001;

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
ba_ratio_number::ba_ratio_number(uint32_t id,
                                 uint32_t host_id,
                                 uint32_t service_id,
                                 bool generate_virtual_status,
                                 const std::shared_ptr<spdlog::logger>& logger)
    : ba(id,
         host_id,
         service_id,
         configuration::ba::state_source_ratio_number,
         generate_virtual_status,
         logger) {
  _level_hard = _level_soft = 0;
}

/**
 *  Get BA hard state.
 *
 *  @return BA hard state.
 */
state ba_ratio_number::get_state_hard() const {
  if (_level_hard >= _level_critical)
    return state_critical;
  else if (_level_hard >= _level_warning)
    return state_warning;
  else
    return state_ok;
}

/**
 *  Get BA soft state.
 *
 *  @return BA soft state.
 */
state ba_ratio_number::get_state_soft() const {
  if (_level_soft >= _level_critical)
    return state_critical;
  else if (_level_soft >= _level_warning)
    return state_warning;
  else
    return state_ok;
}

/**
 *  Apply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_ratio_number::_apply_impact(kpi* kpi_ptr [[maybe_unused]],
                                    ba::impact_info& impact) {
  // Adjust values.
  _acknowledgement_count += impact.hard_impact.get_acknowledgement();

  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return;

  if (impact.soft_impact.get_state() == state_critical)
    _level_soft++;
  if (impact.hard_impact.get_state() == state_critical)
    _level_hard++;
}

/**
 *  Unapply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_ratio_number::_unapply_impact(kpi* kpi_ptr,
                                      ba::impact_info& impact
                                      [[maybe_unused]]) {
  _level_soft = 0.;
  _level_hard = 0.;

  // Adjust values.
  _acknowledgement_count -= impact.hard_impact.get_acknowledgement();

  // We recompute all impact, except the one to unapply...
  for (std::unordered_map<kpi*, impact_info>::iterator it = _impacts.begin(),
                                                       end = _impacts.end();
       it != end; ++it)
    if (it->first != kpi_ptr)
      _apply_impact(it->first, it->second);
}

/**
 *  Apply some child changes. This method is more complete than _apply_impact().
 *  It takes as argument a child kpi and its impact. This child is already
 *  known, so its previous impact is replaced by the new one.
 *  In other words, this method makes almost the same work as _unapply_impact()
 *  and the _apply_impact() ; the difference is that it returns true if the BA
 *  really changed.
 *
 *  @param[in] impact Impact information.
 *  @return True if the BA changes, False otherwise.
 */
bool ba_ratio_number::_apply_changes(kpi* child,
                                     const impact_values& new_hard_impact,
                                     const impact_values& new_soft_impact,
                                     bool in_downtime) {
  double previous_level = _level_hard;
  _level_soft = 0.;
  _level_hard = 0.;

  // We recompute all impact, except the one to unapply...
  for (std::unordered_map<kpi*, impact_info>::iterator it = _impacts.begin(),
                                                       end = _impacts.end();
       it != end; ++it) {
    if (it->first == child) {
      it->second.hard_impact = new_hard_impact;
      it->second.soft_impact = new_soft_impact;
      it->second.in_downtime = in_downtime;
    }
    _apply_impact(it->first, it->second);
  }
  return std::abs(previous_level - _level_hard) > eps;
}

/**
 *  Get the output.
 *
 *  @return Service output.
 */
std::string ba_ratio_number::get_output() const {
  state state = get_state_hard();
  uint32_t s = _impacts.size();
  std::string retval;
  switch (state) {
    case state_unknown:
      retval = "Status is UNKNOWN";
      break;
    default:
      retval = fmt::format(
          "Status is {} - {} out of {} KPI{} are in a CRITICAL state (warn: {} "
          "- crit: {})",
          state_str[state], _level_hard, s, s > 1 ? "s" : "", _level_warning,
          _level_critical);
      break;
  }

  return retval;
}

/**
 *  Get the performance data.
 *
 *  @return Performance data.
 */
std::string ba_ratio_number::get_perfdata() const {
  return fmt::format("BA_Level={};{};{};0;{}", static_cast<int>(_level_hard),
                     static_cast<int>(_level_warning),
                     static_cast<int>(_level_critical), _impacts.size());
}

std::shared_ptr<pb_ba_status> ba_ratio_number::_generate_ba_status(
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
  status.set_level_nominal(_normalize(_level_hard));
  status.set_state(com::centreon::broker::State(get_state_hard()));
  status.set_state_changed(state_changed);
  std::string perfdata = get_perfdata();
  if (perfdata.empty())
    status.set_output(get_output());
  else
    status.set_output(get_output() + "|" + perfdata);

  SPDLOG_LOGGER_DEBUG(_logger,
                      "BAM: generating status of ratio number BA {} '{}' "
                      "(state {}, in downtime {}, level {})",
                      get_id(), _name, status.state(), status.in_downtime(),
                      status.level_nominal());
  return ret;
}
