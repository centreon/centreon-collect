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
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include "com/centreon/engine/globals.hh"

using namespace com::centreon::engine;
using namespace spdlog;

const std::array<std::string, 2> log_v2::loggers{"config", "process"};

std::map<std::string, level::level_enum> log_v2::_levels_map{
    {"trace", level::trace}, {"debug", level::debug},
    {"info", level::info},   {"warning", level::warn},
    {"error", level::err},   {"critical", level::critical},
    {"disabled", level::off}};

log_v2& log_v2::instance() {
  static log_v2 instance;
  return instance;
}

log_v2::log_v2() {
  auto stdout_sink = std::make_shared<sinks::stdout_color_sink_mt>();
  auto null_sink = std::make_shared<sinks::null_sink_mt>();

  _config_log = std::make_shared<logger>("config", stdout_sink);
  _config_log->set_level(_levels_map["info"]);
  _config_log->flush_on(_levels_map["info"]);
  _config_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
}

void log_v2::apply(const configuration::state& config) {
  if (verify_config || test_scheduling)
    return;

  std::vector<spdlog::sink_ptr> sinks;
  if (config.use_syslog())
    sinks.push_back(
        std::make_shared<sinks::syslog_sink_mt>("centreon-engine", 0, 0, true));

  if (config.log_file() != "")
    sinks.push_back(
        std::make_shared<sinks::basic_file_sink_mt>(config.log_file()));
  else
    sinks.push_back(std::make_shared<sinks::stdout_color_sink_mt>());

  int64_t debug_level = config.debug_level() << 32;
  auto conf_logger = [pid = config.log_pid(), debug_level, &sinks](logging::type_value t, const std::string& name) {
    auto logger =
      std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
    if (debug_level & t) {
      logger->set_level(_levels_map["trace"]);
      logger->flush_on(_levels_map["trace"]);
    }
    else {
      logger->set_level(_levels_map["info"]);
      logger->flush_on(_levels_map["info"]);
    }
    if (pid)
      logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
    else
      logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
    return logger;
  };

  _functions_log = conf_logger(engine::logging::dbg_functions, "functions");
  _config_log =
      std::make_shared<spdlog::logger>("config", begin(sinks), end(sinks));
  _config_log->set_level(_levels_map["info"]);
  _config_log->flush_on(_levels_map["info"]);
  if (config.log_pid())
    _config_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _config_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");

  _process_log =
      std::make_shared<spdlog::logger>("process", begin(sinks), end(sinks));
  _process_log->set_level(_levels_map["info"]);
  _process_log->flush_on(_levels_map["info"]);
  _process_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
  if (config.log_pid())
    _process_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  else
    _process_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
}

spdlog::logger* log_v2::config() {
  return instance()._config_log.get();
}

spdlog::logger* log_v2::process() {
  return instance()._process_log.get();
}

spdlog::logger* log_v2::functions() {
  return instance()._functions_log.get();
}
