/**
 * Copyright 2026 Centreon
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
#include "engine/src/timeperiods/timeperiod_manager.hh"

using namespace com::centreon::engine;

/**
 * @brief Get the unique instance of the timeperiod manager.
 *
 * @return A reference to the timeperiod_manager singleton.
 */
timeperiod_manager& timeperiod_manager::instance() {
  static timeperiod_manager instance;
  return instance;
}

/**
 * @brief Look up a timeperiod by name.
 *
 * @param name The timeperiod name.
 *
 * @return The matching timeperiod, or nullptr if absent.
 */
std::shared_ptr<timeperiod> timeperiod_manager::find(
    const std::string& name) const {
  auto it = _timeperiods.find(name);
  return it != _timeperiods.end() ? it->second : nullptr;
}

/**
 * @brief Tell whether a timeperiod with the given name is registered.
 *
 * @param name The timeperiod name.
 *
 * @return True if present.
 */
bool timeperiod_manager::contains(const std::string& name) const {
  return _timeperiods.contains(name);
}

/**
 * @brief Resolve a timeperiod's exclusions against the managed collection.
 *
 * @param tp The timeperiod to resolve.
 * @param w[out] Number of warnings produced during this resolution.
 * @param e[out] Number of errors produced during this resolution.
 */
void timeperiod_manager::resolve(timeperiod& tp, uint32_t& w, uint32_t& e) {
  tp.resolve(_timeperiods, w, e);
}
