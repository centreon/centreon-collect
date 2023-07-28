/**
 * Copyright 2023 Centreon
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
#ifndef CCC_LOG_V2_HH
#define CCC_LOG_V2_HH

#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "config.hh"

namespace com::centreon::common::log_v3 {
class log_v3 {
  const std::string _log_name;
  std::chrono::seconds _flush_interval;
  std::string _file_path;

 public:
  log_v3(const std::string& logger_name) : _log_name(logger_name) {}
  ~log_v3() noexcept = default;

  const std::string& log_name() const;
  std::chrono::seconds flush_interval() const;
  void set_flush_interval(uint32_t second_flush_interval);
  static std::shared_ptr<spdlog::logger> create_logger(const std::string& name);
  static std::shared_ptr<spdlog::logger> get(const std::string& name);
  static void apply(const config& conf);
};
}  // namespace com::centreon::common::log_v3
#endif /* !CCC_LOG_V2_HH */
