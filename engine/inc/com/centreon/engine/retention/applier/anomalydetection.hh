/**
 * Copyright 2022-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#ifndef CCE_RETENTION_APPLIER_ANOMALYDETECTION_HH
#define CCE_RETENTION_APPLIER_ANOMALYDETECTION_HH

#include "com/centreon/engine/retention/anomalydetection.hh"

// Forward declaration.

namespace com::centreon::engine {
class anomalydetection;

// Forward declaration.
namespace configuration {
#ifdef LEGACY_CONF
class state;
#else
class State;
#endif
}  // namespace configuration

namespace retention {
namespace applier {
class anomalydetection {
 public:
#ifdef LEGACY_CONF
  static void apply(configuration::state const& config,
                    list_anomalydetection const& lst,
                    bool scheduling_info_is_ok);
#else
  static void apply(const configuration::State& config,
                    const list_anomalydetection& lst,
                    bool scheduling_info_is_ok);
#endif
 private:
#ifdef LEGACY_CONF
  static void _update(configuration::state const& config,
                      retention::anomalydetection const& state,
                      com::centreon::engine::anomalydetection& obj,
                      bool scheduling_info_is_ok);
#else
  static void _update(const configuration::State& config,
                      const retention::anomalydetection& state,
                      com::centreon::engine::anomalydetection& obj,
                      bool scheduling_info_is_ok);
#endif
};
}  // namespace applier
}  // namespace retention

}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_APPLIER_ANOMALYDETECTION_HH
