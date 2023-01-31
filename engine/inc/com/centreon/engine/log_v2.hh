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

#include "com/centreon/engine/configuration/state.hh"
#include "configuration/state.pb.h"

CCE_BEGIN()
class log_v2 {
  std::string _log_name;
  std::array<std::shared_ptr<spdlog::logger>, 13> _log;
  std::atomic_bool _running;
  enum logger {
    log_config,
    log_functions,
    log_events,
    log_checks,
    log_notifications,
    log_eventbroker,
    log_external_command,
    log_commands,
    log_downtimes,
    log_comments,
    log_macros,
    log_process,
    log_runtime,
  };

  log_v2();
  ~log_v2() noexcept;

 public:
  void apply(const configuration::State& config);
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
