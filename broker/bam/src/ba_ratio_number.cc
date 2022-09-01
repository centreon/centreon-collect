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
#include "com/centreon/broker/log_v2.hh"
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
                                 bool generate_virtual_status)
    : ba(id,
         host_id,
         service_id,
         configuration::ba::state_source_ratio_number,
         generate_virtual_status),
      _num_soft_critical_children{0.f},
      _num_hard_critical_children{0.f} {}

/**
 *  Get BA hard state.
 *
 *  @return BA hard state.
 */
state ba_ratio_number::get_state_hard() const {
  if (_num_hard_critical_children >= _level_critical)
    return state_critical;
  else if (_num_hard_critical_children >= _level_warning)
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
  if (_num_soft_critical_children >= _level_critical)
    return state_critical;
  else if (_num_soft_critical_children >= _level_warning)
    return state_warning;
  else
    return state_ok;
}

/**
 *  Apply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_ratio_number::_apply_impact(kpi* kpi_ptr __attribute__((unused)),
                                    ba::impact_info& impact) {
  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return;

  if (impact.soft_impact.get_state() == state_critical)
    _num_soft_critical_children++;
  if (impact.hard_impact.get_state() == state_critical)
    _num_hard_critical_children++;
}

/**
 *  Unapply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_ratio_number::_unapply_impact(kpi* kpi_ptr,
                                      ba::impact_info& impact
                                      __attribute__((unused))) {
  _num_soft_critical_children = 0.f;
  _num_hard_critical_children = 0.f;

  // We recompute all impact, except the one to unapply...
  for (std::unordered_map<kpi*, impact_info>::iterator it = _impacts.begin(),
                                                       end = _impacts.end();
       it != end; ++it)
    if (it->first != kpi_ptr)
      _apply_impact(it->first, it->second);
}

void ba_ratio_number::_recompute() {
  _num_hard_critical_children = 0.0f;
  _num_soft_critical_children = 0.0f;
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
  return fmt::format("BA : {} - current_level = {}%", _name,
                     static_cast<int>(_normalize(_level_hard)));
}

/**
 *  Get the performance data.
 *
 *  @return Performance data.
 */
std::string ba_ratio_number::get_perfdata() const {
  return fmt::format("BA_Level={}%;{};{};0;100 BA_Downtime={}",
                     static_cast<int>(_normalize(_level_hard)),
                     static_cast<int>(_level_warning),
                     static_cast<int>(_level_critical),
                     static_cast<int>(_normalize(_downtime_hard)));
}
