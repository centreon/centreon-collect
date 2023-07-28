/**
 * Copyright 2022-2023 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "common/log_v2/log_v2.hh"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace com::centreon::common::log_v3;

const std::string& log_v3::log_name() const {
  return _log_name;
}

std::chrono::seconds log_v3::flush_interval() const {
  return _flush_interval;
}

void log_v3::set_flush_interval(uint32_t second_flush_interval) {
  _flush_interval = std::chrono::seconds(second_flush_interval);
}

std::shared_ptr<spdlog::logger> log_v3::create_logger(const std::string& name) {
  auto retval = spdlog::get(name);
  if (!retval) {
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto retval = std::make_shared<spdlog::logger>(name, stdout_sink);
    spdlog::register_logger(retval);
  }
  return retval;
}

std::shared_ptr<spdlog::logger> log_v3::get(const std::string& name) {
  return spdlog::get(name);
}

void log_v3::apply(const config& conf) {
  //  _file_path = conf.log_path();
  //  _flush_interval = conf.flush_interval();
  //  auto apply_on_logger = [this](const std::shared_ptr<spdlog::logger>&
  //  logger) {
  //    logger->set_level(spdlog::level::err);
  //    std::vector<sink_ptr> sinks = logger->sinks();
  //  };
  //  std::shared_ptr<sinks::base_sink<std::mutex>> file_sink;
  //
  //  if (conf.max_size())
  //    file_sink = std::make_shared<sinks::rotating_file_sink_mt>(_file_path,
  //    conf.max_size(), 99);
  //  else
  //    file_sink = std::make_shared<sinks::basic_file_sink_mt>(_file_path);
  //
  //  spdlog::drop(name);
  //  auto logger = std::make_shared<spdlog::logger>(name, file_sink);
  //  logger->set_level(spdlog::level::err);
  //  logger->set_flush_interval(_flush_interval);
  //  spdlog::register_logger(logger);
}
