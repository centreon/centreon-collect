/*
** Copyright 2021-2022 Centreon
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

log_v2::log_v2() : _running{false} {
  auto stdout_sink = std::make_shared<sinks::stdout_sink_mt>();
  auto create_logger = [&stdout_sink](const std::string& name,
                                      level::level_enum lvl) {
    spdlog::drop(name);
    auto log = std::make_shared<spdlog::logger>(name, stdout_sink);
    log->set_level(lvl);
    log->flush_on(lvl);
    log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
    spdlog::register_logger(log);
    return log;
  };

  _log[log_v2::log_functions] =
      create_logger("functions", level::from_str("error"));
  _log[log_v2::log_config] =
      create_logger("configuration", level::from_str("info"));
  _log[log_v2::log_events] = create_logger("events", level::from_str("info"));
  _log[log_v2::log_checks] = create_logger("checks", level::from_str("info"));
  _log[log_v2::log_notifications] =
      create_logger("notifications", level::from_str("error"));
  _log[log_v2::log_eventbroker] =
      create_logger("eventbroker", level::from_str("error"));
  _log[log_v2::log_external_command] =
      create_logger("external_command", level::from_str("error"));
  _log[log_v2::log_commands] =
      create_logger("commands", level::from_str("error"));
  _log[log_v2::log_downtimes] =
      create_logger("downtimes", level::from_str("error"));
  _log[log_v2::log_comments] =
      create_logger("comments", level::from_str("error"));
  _log[log_v2::log_macros] = create_logger("macros", level::from_str("error"));
  _log[log_v2::log_process] = create_logger("process", level::from_str("info"));
  _log[log_v2::log_runtime] =
      create_logger("runtime", level::from_str("error"));
  _running = true;
}

log_v2::~log_v2() noexcept {
  _log[log_v2::log_runtime]->info("log finished");
  _running = false;
  spdlog::shutdown();
  for (auto& l : _log)
    l.reset();
}

#ifdef CONF_PROTO
/**
 * @brief Apply configuration on logApply configuration on logs
 *
 * @param config The protobuf configuration to apply.
 */
void log_v2::apply(const configuration::State& config) {
  if (verify_config || test_scheduling)
    return;

  _running = false;
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

  auto create_logger = [&sinks, log_pid = config.log_pid(),
                        log_file_line = config.log_file_line(),
                        log_flush_period = config.log_flush_period()](
                           const std::string& name, level::level_enum lvl) {
    spdlog::drop(name);
    auto log = std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
    log->set_level(lvl);
    if (log_flush_period)
      log->flush_on(level::warn);
    else
      log->flush_on(lvl);

    if (log_pid) {
      if (log_file_line) {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%s:%#] [%n] [%l] [%P] %v");
      } else {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
      }
    } else {
      if (log_file_line) {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%s:%#] [%n] [%l] %v");
      } else {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
      }
    }
    spdlog::register_logger(log);
    return log;
  };

  _log[log_functions] =
      create_logger("functions", level::from_str(config.log_level_functions()));
  _log[log_config] = create_logger("configuration",
                                   level::from_str(config.log_level_config()));
  _log[log_events] =
      create_logger("events", level::from_str(config.log_level_events()));
  _log[log_checks] =
      create_logger("checks", level::from_str(config.log_level_checks()));
  _log[log_notifications] = create_logger(
      "notifications", level::from_str(config.log_level_notifications()));
  _log[log_eventbroker] = create_logger(
      "eventbroker", level::from_str(config.log_level_eventbroker()));
  _log[log_external_command] = create_logger(
      "external_command", level::from_str(config.log_level_external_command()));
  _log[log_commands] =
      create_logger("commands", level::from_str(config.log_level_commands()));
  _log[log_downtimes] =
      create_logger("downtimes", level::from_str(config.log_level_downtimes()));
  _log[log_comments] =
      create_logger("comments", level::from_str(config.log_level_comments()));
  _log[log_macros] =
      create_logger("macros", level::from_str(config.log_level_macros()));
  _log[log_process] =
      create_logger("process", level::from_str(config.log_level_process()));
  _log[log_runtime] =
      create_logger("runtime", level::from_str(config.log_level_runtime()));

  spdlog::flush_every(std::chrono::seconds(config.log_flush_period()));
  _running = true;
}
#endif

#ifdef CONF_LEGACY
void log_v2::apply(const configuration::state& config) {
  if (verify_config || test_scheduling)
    return;

  _running = false;
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

  auto create_logger = [&sinks, log_pid = config.log_pid(),
                        log_file_line = config.log_file_line(),
                        log_flush_period = config.log_flush_period()](
                           const std::string& name, level::level_enum lvl) {
    spdlog::drop(name);
    auto log = std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
    log->set_level(lvl);
    if (log_flush_period)
      log->flush_on(level::warn);
    else
      log->flush_on(lvl);

    if (log_pid) {
      if (log_file_line) {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%s:%#] [%n] [%l] [%P] %v");
      } else {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
      }
    } else {
      if (log_file_line) {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%s:%#] [%n] [%l] %v");
      } else {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
      }
    }
    spdlog::register_logger(log);
    return log;
  };

  _log[log_functions] =
      create_logger("functions", level::from_str(config.log_level_functions()));
  _log[log_config] = create_logger("configuration",
                                   level::from_str(config.log_level_config()));
  _log[log_events] =
      create_logger("events", level::from_str(config.log_level_events()));
  _log[log_checks] =
      create_logger("checks", level::from_str(config.log_level_checks()));
  _log[log_notifications] = create_logger(
      "notifications", level::from_str(config.log_level_notifications()));
  _log[log_eventbroker] = create_logger(
      "eventbroker", level::from_str(config.log_level_eventbroker()));
  _log[log_external_command] = create_logger(
      "external_command", level::from_str(config.log_level_external_command()));
  _log[log_commands] =
      create_logger("commands", level::from_str(config.log_level_commands()));
  _log[log_downtimes] =
      create_logger("downtimes", level::from_str(config.log_level_downtimes()));
  _log[log_comments] =
      create_logger("comments", level::from_str(config.log_level_comments()));
  _log[log_macros] =
      create_logger("macros", level::from_str(config.log_level_macros()));
  _log[log_process] =
      create_logger("process", level::from_str(config.log_level_process()));
  _log[log_runtime] =
      create_logger("runtime", level::from_str(config.log_level_runtime()));

  spdlog::flush_every(std::chrono::seconds(config.log_flush_period()));
  _running = true;
}
#endif

std::shared_ptr<spdlog::logger> log_v2::functions() {
  if (instance()._running)
    return instance()._log[log_v2::log_functions];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("functions", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::config() {
  if (instance()._running)
    return instance()._log[log_v2::log_config];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("configuration", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::events() {
  if (instance()._running)
    return instance()._log[log_v2::log_events];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("events", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::checks() {
  if (instance()._running)
    return instance()._log[log_v2::log_checks];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("checks", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::notifications() {
  if (instance()._running)
    return instance()._log[log_v2::log_notifications];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("notifications", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::eventbroker() {
  if (instance()._running)
    return instance()._log[log_v2::log_eventbroker];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("eventbroker", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::external_command() {
  if (instance()._running)
    return instance()._log[log_v2::log_external_command];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("external_command", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::commands() {
  if (instance()._running)
    return instance()._log[log_v2::log_commands];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("commands", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::downtimes() {
  if (instance()._running)
    return instance()._log[log_v2::log_downtimes];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("downtimes", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::comments() {
  if (instance()._running)
    return instance()._log[log_v2::log_comments];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("comments", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::macros() {
  if (instance()._running)
    return instance()._log[log_v2::log_macros];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("macros", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::process() {
  if (instance()._running)
    return instance()._log[log_v2::log_process];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("process", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::runtime() {
  if (instance()._running)
    return instance()._log[log_v2::log_runtime];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("runtime", null_sink);
  }
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
