/*
** Copyright 2021 Centreon
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

#include "com/centreon/engine/log_v2.hh"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/broker_sink.hh"

using namespace com::centreon::engine;
using namespace spdlog;

log_v2& log_v2::instance() {
  static log_v2 instance;
  return instance;
}

log_v2::log_v2() {
  auto stdout_sink = std::make_shared<sinks::stdout_sink_mt>();
  auto null_sink = std::make_shared<sinks::null_sink_mt>();

  _functions_log = std::make_shared<spdlog::logger>("functions", stdout_sink);
  _functions_log->set_level(level::from_str("error"));
  _functions_log->flush_on(level::from_str("error"));
  _functions_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _config_log = std::make_shared<spdlog::logger>("config", stdout_sink);
  _config_log->set_level(level::from_str("info"));
  _config_log->flush_on(level::from_str("info"));
  _config_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _events_log = std::make_shared<spdlog::logger>("events", stdout_sink);
  _events_log->set_level(level::from_str("info"));
  _events_log->flush_on(level::from_str("info"));
  _events_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _checks_log = std::make_shared<spdlog::logger>("checks", stdout_sink);
  _checks_log->set_level(level::from_str("info"));
  _checks_log->flush_on(level::from_str("info"));
  _checks_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _notifications_log =
      std::make_shared<spdlog::logger>("notifications", stdout_sink);
  _notifications_log->set_level(level::from_str("error"));
  _notifications_log->flush_on(level::from_str("error"));
  _notifications_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _eventbroker_log =
      std::make_shared<spdlog::logger>("eventbroker", stdout_sink);
  _eventbroker_log->set_level(level::from_str("error"));
  _eventbroker_log->flush_on(level::from_str("error"));
  _eventbroker_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _external_command_log =
      std::make_shared<spdlog::logger>("external_command", stdout_sink);
  _external_command_log->set_level(level::from_str("error"));
  _external_command_log->flush_on(level::from_str("error"));
  _external_command_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _commands_log = std::make_shared<spdlog::logger>("commands", stdout_sink);
  _commands_log->set_level(level::from_str("error"));
  _commands_log->flush_on(level::from_str("error"));
  _commands_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _downtimes_log = std::make_shared<spdlog::logger>("downtimes", stdout_sink);
  _downtimes_log->set_level(level::from_str("error"));
  _downtimes_log->flush_on(level::from_str("error"));
  _downtimes_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _comments_log = std::make_shared<spdlog::logger>("comments", stdout_sink);
  _comments_log->set_level(level::from_str("error"));
  _comments_log->flush_on(level::from_str("error"));
  _comments_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _macros_log = std::make_shared<spdlog::logger>("macros", stdout_sink);
  _macros_log->set_level(level::from_str("error"));
  _macros_log->flush_on(level::from_str("error"));
  _macros_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _process_log = std::make_shared<spdlog::logger>("process", stdout_sink);
  _process_log->set_level(level::from_str("info"));
  _process_log->flush_on(level::from_str("info"));
  _process_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _runtime_log = std::make_shared<spdlog::logger>("runtime", stdout_sink);
  _runtime_log->set_level(level::from_str("error"));
  _runtime_log->flush_on(level::from_str("error"));
  _runtime_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
}

void log_v2::apply(const configuration::state& config) {
  if (verify_config || test_scheduling)
    return;

  std::vector<spdlog::sink_ptr> sinks;
  if (config.log_v2_enabled()) {
    if (config.log_v2_logger() == "file") {
      if (config.log_file() != "")
        sinks.push_back(
            std::make_shared<sinks::basic_file_sink_mt>(config.log_file()));
      else {
        log_v2::config()->error("log_file name is empty");
        sinks.push_back(std::make_shared<sinks::stdout_sink_mt>());
      }
    } else if (config.log_v2_logger() == "syslog")
      sinks.push_back(std::make_shared<sinks::syslog_sink_mt>("centreon-engine",
                                                              0, 0, true));
    auto broker_sink = std::make_shared<logging::broker_sink_mt>();
    broker_sink->set_level(spdlog::level::info);
    sinks.push_back(broker_sink);
  } else
    sinks.push_back(std::make_shared<sinks::null_sink_mt>());

  _functions_log =
      std::make_shared<spdlog::logger>("functions", begin(sinks), end(sinks));
  _functions_log->set_level(level::from_str(config.log_level_functions()));
  _functions_log->flush_on(level::from_str(config.log_level_functions()));
  if (config.log_pid())
    _functions_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _functions_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _config_log =
      std::make_shared<spdlog::logger>("config", begin(sinks), end(sinks));
  _config_log->set_level(level::from_str(config.log_level_config()));
  _config_log->flush_on(level::from_str(config.log_level_config()));
  if (config.log_pid())
    _config_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _config_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _events_log =
      std::make_shared<spdlog::logger>("events", begin(sinks), end(sinks));
  _events_log->set_level(level::from_str(config.log_level_events()));
  _events_log->flush_on(level::from_str(config.log_level_events()));
  if (config.log_pid())
    _events_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _events_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _checks_log =
      std::make_shared<spdlog::logger>("checks", begin(sinks), end(sinks));
  _checks_log->set_level(level::from_str(config.log_level_checks()));
  _checks_log->flush_on(level::from_str(config.log_level_checks()));
  if (config.log_pid())
    _checks_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _checks_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _notifications_log = std::make_shared<spdlog::logger>(
      "notifications", begin(sinks), end(sinks));
  _notifications_log->set_level(
      level::from_str(config.log_level_notifications()));
  _notifications_log->flush_on(
      level::from_str(config.log_level_notifications()));
  if (config.log_pid())
    _notifications_log->set_pattern(
        "[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _notifications_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _eventbroker_log =
      std::make_shared<spdlog::logger>("eventbroker", begin(sinks), end(sinks));
  _eventbroker_log->set_level(level::from_str(config.log_level_eventbroker()));
  _eventbroker_log->flush_on(level::from_str(config.log_level_eventbroker()));
  if (config.log_pid())
    _eventbroker_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _eventbroker_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _external_command_log = std::make_shared<spdlog::logger>(
      "external_command", begin(sinks), end(sinks));
  _external_command_log->set_level(
      level::from_str(config.log_level_external_command()));
  _external_command_log->flush_on(
      level::from_str(config.log_level_external_command()));
  if (config.log_pid())
    _external_command_log->set_pattern(
        "[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _external_command_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _commands_log =
      std::make_shared<spdlog::logger>("commands", begin(sinks), end(sinks));
  _commands_log->set_level(level::from_str(config.log_level_commands()));
  _commands_log->flush_on(level::from_str(config.log_level_commands()));
  if (config.log_pid())
    _commands_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _commands_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _downtimes_log =
      std::make_shared<spdlog::logger>("downtimes", begin(sinks), end(sinks));
  _downtimes_log->set_level(level::from_str(config.log_level_downtimes()));
  _downtimes_log->flush_on(level::from_str(config.log_level_downtimes()));
  if (config.log_pid())
    _downtimes_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _downtimes_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _comments_log =
      std::make_shared<spdlog::logger>("comments", begin(sinks), end(sinks));
  _comments_log->set_level(level::from_str(config.log_level_comments()));
  _comments_log->flush_on(level::from_str(config.log_level_comments()));
  if (config.log_pid())
    _comments_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _comments_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _macros_log =
      std::make_shared<spdlog::logger>("macros", begin(sinks), end(sinks));
  _macros_log->set_level(level::from_str(config.log_level_macros()));
  _macros_log->flush_on(level::from_str(config.log_level_macros()));
  if (config.log_pid())
    _macros_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _macros_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _process_log =
      std::make_shared<spdlog::logger>("process", begin(sinks), end(sinks));
  _process_log->set_level(level::from_str(config.log_level_process()));
  _process_log->flush_on(level::from_str(config.log_level_process()));
  if (config.log_pid())
    _process_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _process_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _runtime_log =
      std::make_shared<spdlog::logger>("runtime", begin(sinks), end(sinks));
  _runtime_log->set_level(level::from_str(config.log_level_process()));
  _runtime_log->flush_on(level::from_str(config.log_level_process()));
  if (config.log_pid())
    _runtime_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _runtime_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
}

std::shared_ptr<spdlog::logger> log_v2::functions() {
  return instance()._functions_log;
}

std::shared_ptr<spdlog::logger> log_v2::config() {
  return instance()._config_log;
}

std::shared_ptr<spdlog::logger> log_v2::events() {
  return instance()._events_log;
}

std::shared_ptr<spdlog::logger> log_v2::checks() {
  return instance()._checks_log;
}

std::shared_ptr<spdlog::logger> log_v2::notifications() {
  return instance()._notifications_log;
}

std::shared_ptr<spdlog::logger> log_v2::eventbroker() {
  return instance()._eventbroker_log;
}

std::shared_ptr<spdlog::logger> log_v2::external_command() {
  return instance()._external_command_log;
}

std::shared_ptr<spdlog::logger> log_v2::commands() {
  return instance()._commands_log;
}

std::shared_ptr<spdlog::logger> log_v2::downtimes() {
  return instance()._downtimes_log;
}

std::shared_ptr<spdlog::logger> log_v2::comments() {
  return instance()._comments_log;
}

std::shared_ptr<spdlog::logger> log_v2::macros() {
  return instance()._macros_log;
}

std::shared_ptr<spdlog::logger> log_v2::process() {
  return instance()._process_log;
}

std::shared_ptr<spdlog::logger> log_v2::runtime() {
  return instance()._runtime_log;
}

/**
 * @brief Check if the given level makes part of the available levels.
 *
 * @param level A level as a string
 *
 * @return A boolean.
 */

bool log_v2::contains_level(const std::string& level_name) {
  auto level = level::from_str(level_name);
  // ignore unrecognized level names
  return !(level == level::off && level_name != "off");
}
