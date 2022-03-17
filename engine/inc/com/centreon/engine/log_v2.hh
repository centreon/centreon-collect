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
  std::string _log_name;
  std::shared_ptr<spdlog::logger> _config_log;
  std::shared_ptr<spdlog::logger> _functions_log;
  std::shared_ptr<spdlog::logger> _events_log;
  std::shared_ptr<spdlog::logger> _checks_log;
  std::shared_ptr<spdlog::logger> _notifications_log;
  std::shared_ptr<spdlog::logger> _eventbroker_log;
  std::shared_ptr<spdlog::logger> _external_command_log;
  std::shared_ptr<spdlog::logger> _commands_log;
  std::shared_ptr<spdlog::logger> _downtimes_log;
  std::shared_ptr<spdlog::logger> _comments_log;
  std::shared_ptr<spdlog::logger> _macros_log;
  std::shared_ptr<spdlog::logger> _process_log;
  std::shared_ptr<spdlog::logger> _runtime_log;

  log_v2();
  ~log_v2() noexcept = default;

 public:
  void apply(const configuration::state& config);
  static bool contains_level(const std::string& level_name);
  static log_v2& instance();
  static std::shared_ptr<spdlog::logger> functions();
  static std::shared_ptr<spdlog::logger> config();
  static std::shared_ptr<spdlog::logger> events();
  static std::shared_ptr<spdlog::logger> checks();
  static std::shared_ptr<spdlog::logger> notifications();
  static std::shared_ptr<spdlog::logger> eventbroker();
  static std::shared_ptr<spdlog::logger> external_command();
  static std::shared_ptr<spdlog::logger> commands();
  static std::shared_ptr<spdlog::logger> downtimes();
  static std::shared_ptr<spdlog::logger> comments();
  static std::shared_ptr<spdlog::logger> macros();
  static std::shared_ptr<spdlog::logger> process();
  static std::shared_ptr<spdlog::logger> runtime();
};
CCE_END()

#endif /* !CCE_LOG_V2_HH */
