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

#include "com/centreon/broker/bam/ba_worst.hh"

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
ba_worst::ba_worst(uint32_t id,
       uint32_t host_id,
       uint32_t service_id,
       bool generate_virtual_status)
    : ba(id, host_id, service_id, configuration::ba::state_source_worst, generate_virtual_status)
      {}

/**
 *  Get BA hard state.
 *
 *  @return BA hard state.
 */
state ba_worst::get_state_hard() {
  bam::state state;

  auto every_kpi_in_dt =
      [](std::unordered_map<kpi*, bam::ba::impact_info>& imp) -> bool {
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
state ba_worst::get_state_soft() {
  bam::state state;

  auto update_state = [&](float num_critical, float level_crit,
                          float level_warning) -> bam::state {
    if (num_critical >= level_crit)
      return state_critical;
    else if (num_critical >= level_warning)
      return state_warning;
    return state_ok;
  };

    state = _computed_soft_state;
  return state;
}

/**
 *  Apply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_worst::_apply_impact(kpi* kpi_ptr __attribute__((unused)),
                       ba::impact_info& impact) {
  const std::array<short, 5> order{0, 3, 4, 2, 1};
  auto is_state_worse = [&](short current_state, short new_state) -> bool {
    assert((unsigned int)current_state < order.size());
    assert((unsigned int)new_state < order.size());
    return order[new_state] > order[current_state];
  };

  auto is_state_better = [&](short current_state, short new_state) -> bool {
    assert((unsigned int)current_state < order.size());
    assert((unsigned int)new_state < order.size());
    return order[new_state] < order[current_state];
  };

  // Adjust values.
  _acknowledgement_hard += impact.hard_impact.get_acknowledgement();
  _acknowledgement_soft += impact.soft_impact.get_acknowledgement();
  _downtime_hard += impact.hard_impact.get_downtime();
  _downtime_soft += impact.soft_impact.get_downtime();

  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return;
  _level_hard -= impact.hard_impact.get_nominal();
  _level_soft -= impact.soft_impact.get_nominal();

    if (is_state_worse(_computed_soft_state, impact.soft_impact.get_state()))
      _computed_soft_state = impact.soft_impact.get_state();
    if (is_state_worse(_computed_hard_state, impact.hard_impact.get_state()))
      _computed_hard_state = impact.hard_impact.get_state();
}

/**
 *  Unapply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_worst::_unapply_impact(kpi* kpi_ptr, ba::impact_info& impact) {
  // Prevent derive of values.
      _computed_soft_state = _computed_hard_state = state_ok;

  // We recompute all impact, except the one to unapply...
  for (std::unordered_map<kpi*, impact_info>::iterator it(_impacts.begin()),
       end(_impacts.end());
       it != end; ++it)
    if (it->first != kpi_ptr)
      _apply_impact(it->first, it->second);
}
