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
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/null_sink.h>

using namespace com::centreon::engine;
using namespace spdlog;

const std::array<std::string, 1> log_v2::loggers{
  "config" };

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

  _config_log = std::make_unique<logger>("config", stdout_sink);
  _config_log->set_level(_levels_map["info"]);
  _config_log->flush_on(_levels_map["info"]);
  _config_log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
}

spdlog::logger* log_v2::config() {
  return instance()._config_log.get();
}
