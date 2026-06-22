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
#ifndef CCE_TIMEPERIODS_TIMEPERIOD_MANAGER_HH
#define CCE_TIMEPERIODS_TIMEPERIOD_MANAGER_HH

#include "engine/src/timeperiods/timeperiod.hh"

namespace com::centreon::engine {

/**
 * @brief Owns the per-process collection of timeperiods.
 *
 * The timeperiods library does not carry a global registry of its own anymore:
 * the collection is owned by this manager so each host application (engine in
 * centengine, broker in cbd) has its own instance, fed through its own path. The
 * timeperiod value class stays registry-agnostic; cross-referencing operations
 * (exclusion resolution) are served by the manager against this map.
 *
 * It is a Meyers singleton: unlike notification_manager it has no
 * destruction-order hazard (nothing calls into it during global teardown), so
 * lazy construction on first instance() is safe and needs no load()/unload().
 */
class timeperiod_manager {
  timeperiod_map _timeperiods;

  timeperiod_manager() = default;
  ~timeperiod_manager() = default;

 public:
  static timeperiod_manager& instance();

  timeperiod_manager(const timeperiod_manager&) = delete;
  timeperiod_manager& operator=(const timeperiod_manager&) = delete;
  timeperiod_manager(timeperiod_manager&&) = delete;
  timeperiod_manager& operator=(timeperiod_manager&&) = delete;

  timeperiod_map& timeperiods() noexcept { return _timeperiods; }
  const timeperiod_map& timeperiods() const noexcept { return _timeperiods; }

  std::shared_ptr<timeperiod> find(const std::string& name) const;
  bool contains(const std::string& name) const;

  /* Resolve a timeperiod's exclusions against the managed collection. */
  void resolve(timeperiod& tp, uint32_t& w, uint32_t& e);
};

}  // namespace com::centreon::engine

#endif  // !CCE_TIMEPERIODS_TIMEPERIOD_MANAGER_HH
