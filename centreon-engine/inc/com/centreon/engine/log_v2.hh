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
#ifndef CCE_LOG_V2_HH
#define CCE_LOG_V2_HH

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <array>
#include <map>
#include <memory>

#include "com/centreon/engine/configuration/state.hh"
#include "com/centreon/engine/namespace.hh"

CCE_BEGIN()
class log_v2 {
  static std::map<std::string, spdlog::level::level_enum> _levels_map;
  std::string _log_name;
  std::shared_ptr<spdlog::logger> _config_log;
  std::shared_ptr<spdlog::logger> _process_log;

  log_v2();
  ~log_v2() noexcept = default;

 public:
  void apply(const configuration::state& config);
  static const std::array<std::string, 2> loggers;

  static log_v2& instance();
  static spdlog::logger* config();
  static spdlog::logger* process();
};
CCE_END()

#endif /* !CCE_LOG_V2_HH */
