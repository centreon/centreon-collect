/*
** Copyright 2021-2023 Centreon
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
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/broker_sink.hh"

using namespace com::centreon::engine;
using namespace spdlog;

std::unique_ptr<log_v2> log_v2::_instance = nullptr;

void log_v2::load(const std::shared_ptr<asio::io_context>& io_context) {
  _instance.reset(new log_v2(io_context));
}

log_v2& log_v2::instance() {
  return *_instance;
}

log_v2::log_v2(const std::shared_ptr<asio::io_context>& io_context)
    : log_v2_base<13>("engine", 0, "", io_context) {
  auto stdout_sink = std::make_shared<sinks::stdout_sink_mt>();
  auto create_logger = [&](const std::string& name, level::level_enum lvl) {
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

  _log[log_v2::log_process]->info("{} : log started", log_name());

  _running = true;
}

log_v2::~log_v2() noexcept {
  _log[log_v2::log_runtime]->info("log finished");
  _running = false;
  for (auto& l : _log)
    l.reset();
}

void log_v2::apply(const configuration::state& config) {
  if (verify_config || test_scheduling)
    return;

  _running = false;
  std::vector<spdlog::sink_ptr> sinks;
  spdlog::sink_ptr sink_to_flush;
  if (config.log_v2_enabled()) {
    if (config.log_v2_logger() == "file") {
      if (config.log_file() != "") {
        set_file_path(config.log_file());
        sink_to_flush =
            std::make_shared<sinks::basic_file_sink_mt>(file_path());
      } else {
        log_v2::config()->error("log_file name is empty");
        sink_to_flush = std::make_shared<sinks::stdout_sink_mt>();
      }
    } else if (config.log_v2_logger() == "syslog")
      sink_to_flush = std::make_shared<sinks::syslog_sink_mt>("centreon-engine",
                                                              0, 0, true);
    if (sink_to_flush) {
      sinks.push_back(sink_to_flush);
    }
    auto broker_sink = std::make_shared<logging::broker_sink_mt>();
    broker_sink->set_level(spdlog::level::info);
    sinks.push_back(broker_sink);
  } else
    sinks.push_back(std::make_shared<sinks::null_sink_mt>());

  auto create_logger = [&](const std::string& name, level::level_enum lvl) {
    spdlog::drop(name);
    auto log = std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
    log->set_level(lvl);
    if (config.log_flush_period())
      log->flush_on(level::warn);
    else
      log->flush_on(lvl);

    if (config.log_pid()) {
      if (config.log_file_line()) {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%s:%#] [%n] [%l] [%P] %v");
      } else {
        log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
      }
    } else {
      if (config.log_file_line()) {
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

  _flush_interval = std::chrono::seconds(config.log_flush_period());

  if (sink_to_flush)
    start_flush_timer();
  else
    stop_flush_timer();
  _running = true;
}

/**
 * @brief Accessor to the various levels of loggers
 *
 * @return A vector of pairs of strings. The first string is the logger name and
 * the second string is its level.
 */
std::vector<std::pair<std::string, std::string>> log_v2::levels() const {
  std::vector<std::pair<std::string, std::string>> retval;
  if (_running) {
    retval.reserve(_log.size());
    for (auto& l : _log) {
      spdlog::level::level_enum level = l->level();
      auto& lv = to_string_view(level);
      retval.emplace_back(l->name(), std::string(lv.data(), lv.size()));
    }
  }
  return retval;
}

/**
 * @brief Set the level of a logger.
 *
 * @param logger The logger name
 * @param level The level as a string
 */
void log_v2::set_level(const std::string& logger, const std::string& level) {
  if (_running) {
    bool found = false;
    for (auto l : _log) {
      if (l->name() == logger) {
        found = true;
        level::level_enum lvl = level::from_str(level);
        if (lvl == level::off && level != "off")
          throw engine_error()
              << fmt::format("The '{}' level is unknown", level);
        l->set_level(lvl);
        break;
      }
    }
    if (!found)
      throw engine_error() << fmt::format("The '{}' logger does not exist",
                                          logger);
  } else
    throw engine_error() << fmt::format(
        "Unable to change '{}' logger level, the logger is not running for now "
        "- try later.",
        logger);
}
