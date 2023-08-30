/**
* Copyright 2022 Centreon
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

#include "com/centreon/broker/bam/ba_best.hh"

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
ba_best::ba_best(uint32_t id,
                 uint32_t host_id,
                 uint32_t service_id,
                 bool generate_virtual_status,
                 const std::shared_ptr<spdlog::logger>& logger)
    : ba(id,
         host_id,
         service_id,
         configuration::ba::state_source_best,
         generate_virtual_status,
         logger) {}

/**
 *  Get BA hard state.
 *
 *  @return BA hard state.
 */
state ba_best::get_state_hard() const {
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
state ba_best::get_state_soft() const {
  return _computed_soft_state;
}

/**
 *  Apply some impact.
 *
 *  @param[in] impact Impact information.
 */
bool ba_best::_apply_impact(kpi* kpi_ptr __attribute__((unused)),
                            ba::impact_info& impact) {
  const std::array<short, 5> order{0, 3, 4, 2, 1};

  auto is_state_better = [&](short current_state, short new_state) -> bool {
    assert((unsigned int)current_state < order.size());
    assert((unsigned int)new_state < order.size());
    return order[new_state] < order[current_state];
  };

  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return false;

  bool retval = false;
  if (is_state_better(_computed_soft_state, impact.soft_impact.get_state())) {
    _computed_soft_state = impact.soft_impact.get_state();
    retval = true;
  }
  if (is_state_better(_computed_hard_state, impact.hard_impact.get_state())) {
    _computed_hard_state = impact.hard_impact.get_state();
    retval = true;
  }
  return retval;
}

/**
 *  Unapply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_best::_unapply_impact(kpi* kpi_ptr,
                              ba::impact_info& impact [[maybe_unused]]) {
  // Prevent derive of values.
  _computed_soft_state = _computed_hard_state = state_critical;

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
std::string ba_best::get_output() const {
  state state = get_state_hard();
  std::string retval;
  switch (state) {
    case state_unknown:
      retval =
          "Status is UNKNOWN - All KPIs are in an UNKNOWN state or worse "
          "(WARNING or CRITICAL)";
      break;
    case state_ok:
      retval = "Status is OK - At least one KPI is in an OK state";
      break;
    case state_warning:
      retval =
          "Status is WARNING - All KPIs are in a WARNING state or worse "
          "(CRITICAL)";
      break;
    case state_critical:
      retval = "Status is CRITICAL - All KPIs are in a CRITICAL state";
      break;
  }
  return retval;
}

/**
 *  Get the performance data.
 *
 *  @return Performance data.
 */
std::string ba_best::get_perfdata() const {
  return {};
}

void ba_best::_recompute() {
  _computed_soft_state = state_critical;
  _computed_hard_state = state_critical;
  for (auto it = _impacts.begin(), end = _impacts.end(); it != end; ++it)
    _apply_impact(it->first, it->second);
  _recompute_count = 0;
}
