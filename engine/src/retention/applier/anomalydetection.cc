/**
 * Copyright 2022-2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "com/centreon/engine/retention/applier/anomalydetection.hh"
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/retention/applier/service.hh"
#include "com/centreon/engine/retention/applier/utils.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/timeperiod.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::retention;

/**
 *  Update service list.
 *
 *  @param[in] config                The global configuration.
 *  @param[in] lst                   The service list to update.
 *  @param[in] scheduling_info_is_ok True if the retention is not
 *                                   outdated.
 */
void applier::anomalydetection::apply(const configuration::State& config,
                                      const list_anomalydetection& lst,
                                      bool scheduling_info_is_ok) {
  for (auto& s : lst) {
    try {
      std::pair<uint64_t, uint64_t> id{
          get_host_and_service_id(s->host_name(), s->service_description())};
      engine::service& svc(find_service(id.first, id.second));
      _update(config, *s, dynamic_cast<engine::anomalydetection&>(svc),
              scheduling_info_is_ok);
    } catch (...) {
      // ignore exception for the retention.
    }
  }
}

/**
 *  Update internal service base on service retention.
 *
 *  @param[in]      config                The global configuration.
 *  @param[in]      state                 The service retention state.
 *  @param[in, out] obj                   The anomalydetection to update.
 *  @param[in]      scheduling_info_is_ok True if the retention is
 *                                        not outdated.
 */
void applier::anomalydetection::_update(
    const configuration::State& config,
    const retention::anomalydetection& state,
    engine::anomalydetection& obj,
    bool scheduling_info_is_ok) {
  applier::service::update(config, state, static_cast<engine::service&>(obj),
                           scheduling_info_is_ok);
  if (state.sensitivity().is_set()) {
    obj.set_sensitivity(state.sensitivity());
  }
}
