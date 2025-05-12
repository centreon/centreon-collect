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

#include "com/centreon/broker/bam/ba_impact.hh"

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

constexpr double eps = 0.000001;

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
ba_impact::ba_impact(uint32_t id,
                     uint32_t host_id,
                     uint32_t service_id,
                     bool generate_virtual_status,
                     const std::shared_ptr<spdlog::logger>& logger)
    : ba(id,
         host_id,
         service_id,
         configuration::ba::state_source_impact,
         generate_virtual_status,
         logger) {}

/**
 *  Get BA hard state.
 *
 *  @return BA hard state.
 */
state ba_impact::get_state_hard() const {
  bam::state state;

  if (!_valid)
    state = state_unknown;
  else if (_level_hard <= _level_critical)
    state = state_critical;
  else if (_level_hard <= _level_warning)
    state = state_warning;
  else
    state = state_ok;

  return state;
}

/**
 *  Get BA soft state.
 *
 *  @return BA soft state.
 */
state ba_impact::get_state_soft() const {
  bam::state state;

  if (!_valid)
    state = state_unknown;
  else if (_level_soft <= _level_critical)
    state = state_critical;
  else if (_level_soft <= _level_warning)
    state = state_warning;
  else
    state = state_ok;

  return state;
}

/**
 *  Apply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_impact::_apply_impact(kpi* kpi_ptr [[maybe_unused]],
                              ba::impact_info& impact) {
  // Adjust values.
  _acknowledgement_count += impact.hard_impact.get_acknowledgement();
  _downtime_hard += impact.hard_impact.get_downtime();
  _downtime_soft += impact.soft_impact.get_downtime();

  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return;
  _level_hard -= impact.hard_impact.get_nominal();
  _level_soft -= impact.soft_impact.get_nominal();
}

bool ba_impact::_apply_changes(kpi* child,
                               const impact_values& new_hard_impact,
                               const impact_values& new_soft_impact,
                               bool in_downtime) {
  int32_t previous_level = _level_hard;
  auto it = _impacts.find(child);
  _unapply_impact(child, it->second);
  it->second.hard_impact = new_hard_impact;
  it->second.soft_impact = new_soft_impact;
  it->second.in_downtime = in_downtime;
  _apply_impact(child, it->second);
  return previous_level != _level_hard;
}

/**
 *  Unapply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_impact::_unapply_impact(kpi* kpi_ptr [[maybe_unused]],
                                ba::impact_info& impact) {
  // Prevent derive of values.
  ++_recompute_count;
  if (_recompute_count >= _recompute_limit)
    _recompute();

  // Adjust values.
  _acknowledgement_count -= impact.hard_impact.get_acknowledgement();
  _downtime_hard -= impact.hard_impact.get_downtime();
  _downtime_soft -= impact.soft_impact.get_downtime();
  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return;
  _level_hard += impact.hard_impact.get_nominal();
  _level_soft += impact.soft_impact.get_nominal();
}

/**
 *  Get the output.
 *
 *  @return Service output.
 */
std::string ba_impact::get_output() const {
  auto impacting_kpis = [this]() {
    std::list<std::string> lst;
    for (auto it = _impacts.begin(), end = _impacts.end(); it != end; ++it) {
      if (it->second.hard_impact.get_nominal() > eps) {
        lst.emplace_back(fmt::format(
            "KPI {} (impact: {})", it->first->get_name(),
            static_cast<int32_t>(it->second.hard_impact.get_nominal())));
      }
    }
    return lst;
  };

  state state = get_state_hard();
  std::string retval;
  double level = _normalize(_level_hard);
  uint32_t s = _impacts.size();
  switch (state) {
    case state_unknown:
      retval = "Status is unknown";
      break;
    case state_ok:
      if (level >= 100 - eps) {
        retval = fmt::format(
            "Status is OK - Level = 100 (warn: {} - crit: {}) - none of the {} "
            "KPI{} is impacting the BA right now",
            _level_warning, _level_critical, s, s > 1 ? "s" : "");
      } else {
        auto lst = impacting_kpis();
        uint32_t nb_imp = lst.size();
        retval = fmt::format(
            "Status is OK - Level = {} (warn: {} - crit: {}) - {} KPI{} out of "
            "{} impact{} the BA: {}",
            level, _level_warning, _level_critical, nb_imp,
            nb_imp > 1 ? "s" : "", s, nb_imp == 1 ? "s" : "",
            fmt::join(lst, ", "));
      }
      break;
    case state_warning: {
      auto lst = impacting_kpis();
      uint32_t nb_imp = lst.size();
      retval = fmt::format(
          "Status is WARNING - Level = {} - {} KPI{} out of {} impact{} the BA "
          "for {} points - {}",
          level, nb_imp, nb_imp > 1 ? "s" : "", s, nb_imp == 1 ? "s" : "",
          100 - level, fmt::join(lst, ", "));
    } break;
    case state_critical: {
      auto lst = impacting_kpis();
      uint32_t nb_imp = lst.size();
      retval = fmt::format(
          "Status is CRITICAL - Level = {} - {} KPI{} out of {} impact{} the "
          "BA for {} points - {}",
          level, nb_imp, nb_imp > 1 ? "s" : "", s, nb_imp == 1 ? "s" : "",
          100 - level, fmt::join(lst, ", "));
    } break;
  }
  return retval;
}

/**
 *  Get the performance data.
 *
 *  @return Performance data.
 */
std::string ba_impact::get_perfdata() const {
  return fmt::format(
      "BA_Level={};{};{};0;100", static_cast<int>(_normalize(_level_hard)),
      static_cast<int>(_level_warning), static_cast<int>(_level_critical));
}

/**
 *  @brief Recompute all impacts.
 *
 *  This method was created to prevent the real values to derive to
 *  much from their true value due to the caching system.
 */
void ba_impact::_recompute() {
  _acknowledgement_count = 0.0;
  _downtime_hard = 0.0;
  _downtime_soft = 0.0;
  _level_hard = 100.0;
  _level_soft = 100.0;
  for (std::unordered_map<kpi*, impact_info>::iterator it = _impacts.begin(),
                                                       end = _impacts.end();
       it != end; ++it)
    _apply_impact(it->first, it->second);
  _recompute_count = 0;
}

std::shared_ptr<pb_ba_status> ba_impact::_generate_ba_status(
    bool state_changed) const {
  auto ret{std::make_shared<pb_ba_status>()};
  BaStatus& status = ret->mut_obj();
  status.set_ba_id(get_id());
  status.set_in_downtime(in_downtime());
  if (_event)
    status.set_last_state_change(_event->obj().start_time());
  else
    status.set_last_state_change(get_last_kpi_update());
  status.set_level_acknowledgement(_acknowledgement_count);
  status.set_level_downtime(_normalize(_downtime_hard));
  status.set_level_nominal(_normalize(_level_hard));
  if (status.state() != com::centreon::broker::State(get_state_hard())) {
    status.set_state(com::centreon::broker::State(get_state_hard()));
    status.set_state_changed(state_changed);
  }
  std::string perfdata = get_perfdata();
  if (perfdata.empty())
    status.set_output(get_output());
  else
    status.set_output(get_output() + "|" + perfdata);

  SPDLOG_LOGGER_DEBUG(
      _logger,
      "BAM: generating status of impact BA {} '{}' (state {}, in downtime {}, "
      "level {})",
      get_id(), _name, status.state(), status.in_downtime(),
      status.level_nominal());
  return ret;
}
