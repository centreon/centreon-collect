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

#include <cassert>

#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

namespace com::centreon::engine {

timeperiod_manager* timeperiod_manager::_instance = nullptr;

/**
 * @brief Get the unique instance of the timeperiod manager.
 *
 * The manager must have been loaded with load() beforehand.
 *
 * @return A reference to the timeperiod_manager singleton.
 */
timeperiod_manager& timeperiod_manager::instance() {
  assert(_instance);
  return *_instance;
}

/**
 * @brief Create the singleton and inject the logger and forbidden characters.
 *
 * Must be called once at startup, before any timeperiod is registered. The
 * forbidden characters are those known by the host at load time; refresh them
 * with set_illegal_object_chars() when the configuration provides them.
 *
 * @param logger The logger the timeperiods library will log through.
 * @param illegal_chars The characters forbidden in timeperiod names.
 */
void timeperiod_manager::load(const std::shared_ptr<spdlog::logger>& logger,
                              std::string_view illegal_chars) {
  if (!_instance)
    _instance = new timeperiod_manager();
  _instance->_logger = logger;
  _instance->_illegal_chars = illegal_chars;
}

/**
 * @brief Destroy the singleton.
 */
void timeperiod_manager::unload() {
  delete _instance;
  _instance = nullptr;
}

/**
 * @brief The logger the timeperiods library logs through.
 *
 * Null-safe: when the manager is not loaded (e.g. standalone library use or
 * unit tests exercising only the free functions), it returns a process-wide
 * silent logger instead of dereferencing a missing instance.
 *
 * @return A never-null logger.
 */
const std::shared_ptr<spdlog::logger>& timeperiod_manager::logger() {
  if (_instance && _instance->_logger)
    return _instance->_logger;
  static const std::shared_ptr<spdlog::logger> fallback =
      std::make_shared<spdlog::logger>(
          "timeperiods", std::make_shared<spdlog::sinks::null_sink_mt>());
  return fallback;
}

/**
 * @brief Refresh the set of characters forbidden in timeperiod names.
 *
 * No-op when the manager is not loaded.
 *
 * @param illegal_chars The forbidden characters.
 */
void timeperiod_manager::set_illegal_object_chars(
    std::string_view illegal_chars) {
  if (_instance)
    _instance->_illegal_chars = illegal_chars;
}

/**
 * @brief Tell whether a name contains a forbidden character.
 *
 * Null-safe: returns false when the manager is not loaded or when no forbidden
 * character is configured.
 *
 * @param name The name to check.
 *
 * @return True if name contains at least one forbidden character.
 */
bool timeperiod_manager::contains_illegal_chars(std::string_view name) {
  if (!_instance || _instance->_illegal_chars.empty())
    return false;
  return name.find_first_of(_instance->_illegal_chars) != std::string_view::npos;
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

}  // namespace com::centreon::engine
