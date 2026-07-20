/**
 * Copyright 2011, 2021-2024 Centreon
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

#ifndef CCB_CONFIG_APPLIER_INIT_HH_
#define CCB_CONFIG_APPLIER_INIT_HH_

#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/cbmod_state.hh"

namespace com::centreon::broker::config::applier {

enum applier_state { not_started, initialized, finished };
extern std::atomic<applier_state> mode;
void deinit();

template <typename State>
void init(const std::string& engine_conf_version,
          size_t n_thread,
          const std::string& name,
          size_t event_queues_total_size);

/**
 * @brief Load necessary structures.
 *
 * @param peer_type The type of peer this broker is.
 * @param engine_conf_version The version of the engine configuration or "" if
 * not applicable.
 * @param conf The configuration used to initialize the all.
 */
template <typename State>
void init(const std::string& engine_conf_version, const config::state& conf) {
  init<State>(engine_conf_version, conf.pool_size(), conf.broker_name(),
              conf.event_queues_total_size());
}

}  // namespace com::centreon::broker::config::applier

#endif /* !CCB_CONFIG_APPLIER_INIT_HH_ */
