/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "com/centreon/connector/log.hh"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace com::centreon::connector;

log::log() : _log_to_file(false) {
  auto filesink = std::make_shared<spdlog::sinks::null_sink_mt>();
  _core_log = std::make_shared<spdlog::logger>("core", filesink);
  _core_log->info("log started");
  _core_log->set_level(spdlog::level::trace);
  _core_log->flush_on(spdlog::level::trace);
}

log::~log() {}

com::centreon::connector::log& log::instance() {
  static log instance;

  return instance;
}

void log::set_level(spdlog::level::level_enum level) {
  _core_log->set_level(level);
  _core_log->flush_on(level);
}

void log::add_pid_to_log() {
  _core_log->set_pattern("%P %+");
}

void log::switch_to_stdout() {
  _log_to_file = false;
  auto filesink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  spdlog::level::level_enum lvl = _core_log->level();
  _core_log = std::make_shared<spdlog::logger>("core", filesink);
  _core_log->info("log started");
  _core_log->set_level(lvl);
  _core_log->flush_on(lvl);
}

void log::switch_to_file(std::string const& filename) {
  _log_to_file = true;
  auto filesink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename);
  spdlog::level::level_enum lvl = _core_log->level();
  _core_log = std::make_shared<spdlog::logger>("core", filesink);
  _core_log->info("log started");
  _core_log->set_level(lvl);
  _core_log->flush_on(lvl);
}

std::shared_ptr<spdlog::logger> log::core() {
  return log::instance()._core_log;
}