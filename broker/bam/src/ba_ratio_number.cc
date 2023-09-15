/*
** Copyright 2022 Centreon
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

/**
 *  Constructor.
 *
 *  @param[in] host_id the id of the associated host.
 *  @param[in] service_id the id of the associated service.
 *  @param[in] id the id of this ba.
 *  @param[in] generate_virtual_status  Whether or not the BA object
 *                                      should generate statuses of
 *                                      virtual hosts and services.
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
bool ba_ratio_number::_apply_impact(kpi* kpi_ptr __attribute__((unused)),
                                    ba::impact_info& impact) {
  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return false;

  bool retval = false;
  if (impact.soft_impact.get_state() == state_critical) {
    _level_soft++;
    retval = true;
  }
  if (impact.hard_impact.get_state() == state_critical) {
    _level_hard++;
    retval = true;
  }
  return retval;
}

/**
 *  Unapply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_ratio_number::_unapply_impact(kpi* kpi_ptr,
                                      ba::impact_info& impact
                                      __attribute__((unused))) {
  _level_soft = 0.;
  _level_hard = 0.;

  // We recompute all impact, except the one to unapply...
  for (std::unordered_map<kpi*, impact_info>::iterator it = _impacts.begin(),
                                                       end = _impacts.end();
       it != end; ++it)
    if (it->first != kpi_ptr)
      _apply_impact(it->first, it->second);
}

void ba_ratio_number::_recompute() {
  _level_hard = 0.0;
  _level_soft = 0.0f;
  for (std::unordered_map<kpi*, impact_info>::iterator it(_impacts.begin()),
       end(_impacts.end());
       it != end; ++it)
    _apply_impact(it->first, it->second);
  _recompute_count = 0;
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
