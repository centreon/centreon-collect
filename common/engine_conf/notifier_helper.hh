/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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
#ifndef CCE_CONFIGURATION_NOTIFIER_HELPER
#define CCE_CONFIGURATION_NOTIFIER_HELPER

#include <spdlog/logger.h>

#include <type_traits>

#include "common/engine_conf/message_helper.hh"

namespace com::centreon::engine::configuration {

/**
 * @brief Validate the notifier-common part shared by Host, Service and
 * Anomalydetection.
 *
 * This is the configuration-level counterpart of the former Engine runtime
 * `notifier::resolve()`: it only accumulates warnings/errors into @a err (it
 * never throws) and performs no runtime wiring. It is a template because the
 * three protobuf messages expose the same field accessors without sharing a
 * common C++ base. `Anomalydetection` has neither `check_command` nor
 * `check_period` (it derives them from its dependent service), so those two
 * checks are gated behind `if constexpr` and are simply not instantiated for
 * it.
 *
 * @tparam T Host, Service or Anomalydetection.
 * @param obj The notifier to validate.
 * @param contacts Index of every defined contact name.
 * @param contactgroups Index of every defined contact group name.
 * @param commands Index of every defined command name.
 * @param timeperiods Index of every defined timeperiod name.
 * @param err Warning/error counters, incremented in place.
 * @param log Logger for the diagnostics.
 */
template <typename T>
void notifier_resolve(
    const T& obj,
    const absl::flat_hash_set<std::string_view>& contacts,
    const absl::flat_hash_set<std::string_view>& contactgroups,
    const absl::flat_hash_set<std::string_view>& commands,
    const absl::flat_hash_set<std::string_view>& timeperiods,
    error_cnt& err,
    const std::shared_ptr<spdlog::logger>& log) {
  // A command field holds "name!arg1!arg2..."; only the name (before the first
  // '!') must be defined.
  auto check_command_defined = [&](std::string_view field,
                                   std::string_view what) {
    if (field.empty())
      return;
    std::string_view name = field.substr(0, field.find_first_of('!'));
    if (!commands.contains(name)) {
      err.config_errors++;
      log->error("Error: {} '{}' specified for a notifier is not defined "
                 "anywhere!",
                 what, name);
    }
  };

  // Event handler command.
  check_command_defined(obj.event_handler(), "Event handler command");

  if constexpr (!std::is_same_v<T, Anomalydetection>) {
    // Check command.
    check_command_defined(obj.check_command(), "Notifier check command");

    // Check period.
    if (obj.check_period().empty()) {
      err.config_warnings++;
      log->warn("Warning: Notifier has no check time period defined!");
    } else if (!timeperiods.contains(obj.check_period())) {
      err.config_errors++;
      log->error("Error: Check period '{}' specified for a notifier is not "
                 "defined anywhere!",
                 obj.check_period());
    }
  }

  // Contacts.
  for (auto& c : obj.contacts().data()) {
    if (!contacts.contains(c)) {
      err.config_errors++;
      log->error("Error: Contact '{}' specified in a notifier is not defined "
                 "anywhere!",
                 c);
    }
  }

  // Contact groups.
  for (auto& cg : obj.contactgroups().data()) {
    if (!contactgroups.contains(cg)) {
      err.config_errors++;
      log->error("Error: Contact group '{}' specified in a notifier is not "
                 "defined anywhere!",
                 cg);
    }
  }

  // Notification period.
  if (!obj.notification_period().empty()) {
    if (!timeperiods.contains(obj.notification_period())) {
      err.config_errors++;
      log->error("Error: Notification period '{}' specified for a notifier is "
                 "not defined anywhere!",
                 obj.notification_period());
    }
  } else if (obj.notifications_enabled()) {
    err.config_warnings++;
    log->warn("Warning: Notifier has no notification time period defined!");
  }
}

}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_NOTIFIER_HELPER */
