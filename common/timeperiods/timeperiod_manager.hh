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

#include <string>
#include <string_view>

#include "common/timeperiods/timeperiod.hh"

namespace spdlog {
class logger;
}

namespace com::centreon::engine {

/**
 * @brief Owns the per-process collection of timeperiods and their logger.
 *
 * The timeperiods library does not carry a global registry of its own anymore:
 * the collection is owned by this manager so each host application (engine in
 * centengine, broker in cbd) has its own instance, fed through its own path.
 * The timeperiod value class stays registry-agnostic; cross-referencing
 * operations (exclusion resolution) are served by the manager against this map.
 *
 * It also owns the logger the library logs through and the set of characters
 * the host application forbids in object names: instead of reaching engine
 * globals (config_logger / functions_logger / illegal_object_chars), the
 * timeperiods code logs and validates names via the injected logger() and
 * contains_illegal_chars() — one less dependency on the host application.
 *
 * Lifetime is controlled explicitly through load()/unload() (the singleton is
 * created by load() and destroyed by unload()). instance() requires a loaded
 * manager, but logger() and contains_illegal_chars() are null-safe: they fall
 * back to silent/permissive defaults when the manager is not loaded, so the
 * standalone library functions (and their unit tests) can run without a load().
 */
class timeperiod_manager {
  static timeperiod_manager* _instance;

  timeperiod_map _timeperiods;
  std::shared_ptr<spdlog::logger> _logger;
  std::string _illegal_chars;

  timeperiod_manager() = default;
  ~timeperiod_manager() = default;

 public:
  static timeperiod_manager& instance();
  /* Create the singleton, inject the logger the library logs through and the
   * characters forbidden in timeperiod names (as known by the host at load
   * time; use set_illegal_object_chars() to refresh them later). */
  static void load(const std::shared_ptr<spdlog::logger>& logger,
                   std::string_view illegal_chars);
  /* Destroy the singleton. */
  static void unload();
  /* The library logger; never null (silent fallback when not loaded). */
  static const std::shared_ptr<spdlog::logger>& logger();
  /* Refresh the forbidden-characters set (e.g. on configuration apply). */
  static void set_illegal_object_chars(std::string_view illegal_chars);
  /* Whether name contains a forbidden character; false when not loaded or when
   * no forbidden character is configured. */
  static bool contains_illegal_chars(std::string_view name);

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
