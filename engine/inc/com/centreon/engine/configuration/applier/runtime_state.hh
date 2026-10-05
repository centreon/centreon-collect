/**
 * Copyright 2026 Centreon
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
#ifndef CCE_CONFIGURATION_APPLIER_RUNTIME_STATE_HH
#define CCE_CONFIGURATION_APPLIER_RUNTIME_STATE_HH

#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::configuration::applier {

/**
 * @class runtime_state runtime_state.hh
 * "com/centreon/engine/configuration/applier/runtime_state.hh"
 * @brief Apply the runtime snapshot Broker sends in a DiffState.
 *
 * In centralized configuration, Broker holds the runtime part of every
 * resource (restored from the database at its startup, kept across
 * configuration changes) and hands it to the poller in the DiffState sent at
 * connection: this is what retention.dat used to provide at startup. Applied
 * once the objects of the diff exist, where the retention used to apply.
 */
class runtime_state {
 public:
  static void apply(const configuration::RuntimeState& rs);
};

}  // namespace com::centreon::engine::configuration::applier

#endif  // !CCE_CONFIGURATION_APPLIER_RUNTIME_STATE_HH
