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

#include "com/centreon/broker/bam/ba_impact.hh"

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
ba_impact::ba_impact(uint32_t id,
       uint32_t host_id,
       uint32_t service_id,
       bool generate_virtual_status)
    : ba(id, host_id, service_id, configuration::ba::state_source_impact, generate_virtual_status)
      {}

/**
 *  Get BA hard state.
 *
 *  @return BA hard state.
 */
state ba_impact::get_state_hard() {
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
state ba_impact::get_state_soft() {
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
void ba_impact::_apply_impact(kpi* kpi_ptr __attribute__((unused)),
                       ba::impact_info& impact) {
  // Adjust values.
  _acknowledgement_hard += impact.hard_impact.get_acknowledgement();
  _acknowledgement_soft += impact.soft_impact.get_acknowledgement();
  _downtime_hard += impact.hard_impact.get_downtime();
  _downtime_soft += impact.soft_impact.get_downtime();

  if (_dt_behaviour == configuration::ba::dt_ignore_kpi && impact.in_downtime)
    return;
  _level_hard -= impact.hard_impact.get_nominal();
  _level_soft -= impact.soft_impact.get_nominal();
}

/**
 *  Unapply some impact.
 *
 *  @param[in] impact Impact information.
 */
void ba_impact::_unapply_impact(kpi* kpi_ptr, ba::impact_info& impact) {
  // Prevent derive of values.
      ++_recompute_count;
      if (_recompute_count >= _recompute_limit)
        _recompute();

      // Adjust values.
      _acknowledgement_hard -= impact.hard_impact.get_acknowledgement();
      _acknowledgement_soft -= impact.soft_impact.get_acknowledgement();
      _downtime_hard -= impact.hard_impact.get_downtime();
      _downtime_soft -= impact.soft_impact.get_downtime();
      if (_dt_behaviour == configuration::ba::dt_ignore_kpi &&
          impact.in_downtime)
        return;
      _level_hard += impact.hard_impact.get_nominal();
      _level_soft += impact.soft_impact.get_nominal();
}
