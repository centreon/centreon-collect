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

#include <array>
#include <chrono>
#include <memory>
#include <string>

#include "config.hh"

namespace com::centreon::common::log_v2 {

constexpr uint32_t log_v2_core = 0;
constexpr uint32_t log_v2_config = 1;
constexpr uint32_t log_v2_process = 2;
constexpr uint32_t log_v2_configuration = 3;

/**
 * @brief Unified log module for broker and engine.
 *
 * To instance a logger with a name, we call the static internal function
 * create_logger(name). It creates the logger and returns an ID useful to
 * get back the logger. We know that spdlog is able to return the logger from
 * its name, but by this way it gets the logger in a hashtable protected by
 * a mutex which is more expensive.
 *
 * With the ID, we can get the logger with the static internal function get(ID).
 * We could also keep the shared_ptr to the logger, but in case of modification
 * of the logger, this shared_ptr would not be updated, that's why it is
 * needed to call regularly this get() function. So in a function, we can get
 * the logger and then all along the function use directly the shared_ptr.
 *
 * It is possible to change sinks of a logger on the fly, during the changes,
 * some logs may be lost, instead of getting the logger, we get a null logger,
 * and when the changes are done, we newly get the logger. That's why it is
 * important not to store forever the logger's shared_ptr.
 */
class log_v2 {
 public:
  enum logger_id {
    CORE = 0,
    CONFIG = 1,
    BAM = 2,
    BBDO = 3,
    LUA = 4,
    INFLUXDB = 5,
    GRAPHITE = 6,
    RRD = 7,
    STATS = 8,
    PERFDATA = 9,
    PROCESSING = 10,
    SQL = 11,
    NEB = 12,
    TCP = 13,
    TLS = 14,
    GRPC = 15,
    VICTORIA_METRICS = 16,
    PROCESS = 17,
    FUNCTIONS = 18,
    EVENTS = 19,
    CHECKS = 20,
    NOTIFICATIONS = 21,
    EVENTBROKER = 22,
    EXTERNAL_COMMAND = 23,
    COMMANDS = 24,
    DOWNTIMES = 25,
    COMMENTS = 26,
    MACROS = 27,
    RUNTIME = 28,
    LOGGER_SIZE
  };

 private:
  static log_v2* _instance;
  std::string _log_name;
  std::chrono::seconds _flush_interval;
  std::string _file_path;
  const static std::array<std::string, LOGGER_SIZE> _logger_name;
  std::array<std::shared_ptr<spdlog::logger>, LOGGER_SIZE> _loggers;
  std::array<bool, LOGGER_SIZE> _slaves;
  std::atomic<config::logger_type> _current_log_type =
      config::logger_type::LOGGER_STDOUT;
  size_t _current_max_size = 0U;
  bool _log_pid = false;
  bool _log_source = false;

 public:
  static void load(const std::string& name,
                   std::initializer_list<logger_id> ilist);
  static void unload();
  static log_v2& instance();
  log_v2(const std::string& name, std::initializer_list<logger_id> ilist);
  log_v2(const log_v2&) = delete;
  log_v2& operator=(const log_v2&) = delete;
  ~log_v2() noexcept = default;
  logger_id get_id(const std::string& name) const noexcept;

  std::chrono::seconds flush_interval();
  void set_flush_interval(uint32_t second_flush_interval);
  std::shared_ptr<spdlog::logger> create_logger(const logger_id id);
  std::shared_ptr<spdlog::logger> get(const logger_id idx);
  void apply(const config& conf);
  bool contains_logger(std::string_view logger) const;
  bool contains_level(const std::string& level) const;
  const std::string& filename() const { return _file_path; }
  std::vector<std::pair<std::string, spdlog::level::level_enum>> levels() const;
  const std::string& log_name() const;
  void disable();
  void disable(std::initializer_list<logger_id> ilist);
};
}  // namespace com::centreon::common::log_v2
#endif /* !CCC_LOG_V2_HH */
