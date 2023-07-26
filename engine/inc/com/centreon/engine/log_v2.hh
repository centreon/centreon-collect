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
#ifndef CCE_LOG_V2_HH
#define CCE_LOG_V2_HH

#include "com/centreon/engine/configuration/state.hh"
#include "common/configuration/state.pb.h"
#include "log_v2_base.hh"

CCE_BEGIN()
class log_v2 : public log_v2_base {
  std::array<std::shared_ptr<spdlog::logger>, 13> _log;
  std::atomic_bool _running;
  asio::system_timer _flush_timer;
  std::mutex _flush_timer_m;
  bool _flush_timer_active;
  std::shared_ptr<asio::io_context> _io_context;

  static std::shared_ptr<log_v2> _instance;

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

  log_v2(const std::shared_ptr<asio::io_context>& io_context);

  std::shared_ptr<spdlog::logger> get_logger(logger log_type,
                                             const char* log_str);

  void start_flush_timer(spdlog::sink_ptr sink);

 public:
  ~log_v2() noexcept;

  void stop_flush_timer();
  void apply(const configuration::State& config);
  void apply(const configuration::state& config);
  void set_flush_interval(unsigned second_flush_interval);
  static bool contains_level(const std::string& level_name);
  static void load(const std::shared_ptr<asio::io_context>& io_context);
  static std::shared_ptr<log_v2> instance();
  static inline std::shared_ptr<spdlog::logger> functions() {
    return _instance->get_logger(log_v2::log_functions, "functions");
  }

  static inline std::shared_ptr<spdlog::logger> config() {
    return _instance->get_logger(log_v2::log_config, "configuration");
  }

  static inline std::shared_ptr<spdlog::logger> events() {
    return _instance->get_logger(log_v2::log_events, "events");
  }

  static inline std::shared_ptr<spdlog::logger> checks() {
    return _instance->get_logger(log_v2::log_checks, "checks");
  }

  static inline std::shared_ptr<spdlog::logger> notifications() {
    return _instance->get_logger(log_v2::log_notifications, "notifications");
  }

  static inline std::shared_ptr<spdlog::logger> eventbroker() {
    return _instance->get_logger(log_v2::log_eventbroker, "eventbroker");
  }

  static inline std::shared_ptr<spdlog::logger> external_command() {
    return _instance->get_logger(log_v2::log_external_command,
                                 "external_command");
  }

  static inline std::shared_ptr<spdlog::logger> commands() {
    return _instance->get_logger(log_v2::log_commands, "commands");
  }

  static inline std::shared_ptr<spdlog::logger> downtimes() {
    return _instance->get_logger(log_v2::log_downtimes, "downtimes");
  }

  static inline std::shared_ptr<spdlog::logger> comments() {
    return _instance->get_logger(log_v2::log_comments, "comments");
  }

  static inline std::shared_ptr<spdlog::logger> macros() {
    return _instance->get_logger(log_v2::log_macros, "macros");
  }

  static inline std::shared_ptr<spdlog::logger> process() {
    return _instance->get_logger(log_v2::log_process, "process");
  }

  static inline std::shared_ptr<spdlog::logger> runtime() {
    return _instance->get_logger(log_v2::log_runtime, "runtime");
  }
};
CCE_END()

#endif /* !CCE_LOG_V2_HH */
