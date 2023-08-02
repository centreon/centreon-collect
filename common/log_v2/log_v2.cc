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
#include <absl/container/flat_hash_set.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace com::centreon::common::log_v3;
using namespace spdlog;

log_v3* log_v3::_instance = nullptr;

void log_v3::load(std::initializer_list<std::string> ilist) {
  _instance = new log_v3(ilist);
}

void log_v3::unload() {
  if (_instance) {
    delete _instance;
    _instance = nullptr;
    spdlog::drop_all();
    spdlog::shutdown();
  }
}

log_v3::log_v3(std::initializer_list<std::string> ilist) {
  for (auto& s : ilist)
    create_logger_or_get_id(s, true);
}

log_v3::~log_v3() noexcept {}

log_v3& log_v3::instance() {
  assert(_instance);
  return *_instance;
}

// std::chrono::seconds log_v3::flush_interval() {
//   return _flush_interval;
// }

void log_v3::set_flush_interval(uint32_t second_flush_interval) {
  _flush_interval = std::chrono::seconds(second_flush_interval);
  std::lock_guard lck(_loggers_m);
  if (second_flush_interval == 0) {
    for (auto& l : _loggers) {
      if (l->level() != level::off)
        l->flush_on(l->level());
    }
  } else {
    for (auto& l : _loggers) {
      if (l->level() != level::off)
        l->flush_on(level::warn);
    }
  }
  _flush_interval = std::chrono::seconds(second_flush_interval);
  /* if _flush_interval is 0, the flush worker is stopped. */
  spdlog::flush_every(_flush_interval);
}

/**
 * @brief The first use of this function is to create a logger. It should be
 * called before the configuration is applied so any logger created here logs
 * to the console. The second use of this function is once the configuration is
 * applied. And it just returns the id of the logger with the given name.
 *
 * @param name The name of the logger.
 * @param activate If True, the logger is activated at its creation. It is only
 * activated when the load() function is called, otherwise by default, the
 * boolean is false.
 *
 * @return The ID of the logger (created or found in the existing
 * configuration).
 */
uint32_t log_v3::create_logger_or_get_id(const std::string& name,
                                         bool activate) {
  std::lock_guard<std::shared_mutex> lck(_loggers_m);
  uint32_t idx;
  for (idx = 0; idx < _loggers.size(); ++idx) {
    if (_loggers[idx]->name() == name)
      return idx;
  }
  std::shared_ptr<spdlog::logger> logger;
  if (activate) {
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    logger = std::make_shared<spdlog::logger>(name, stdout_sink);
  } else {
    auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    logger = std::make_shared<spdlog::logger>(name, null_sink);
  }
  spdlog::register_logger(logger);
  _loggers.push_back(logger);
  return idx;
}

/**
 * @brief Accessor to the logger of given ID. This function uses a shared mutex
 * because the configuraton can change. We can't store the shared_ptr one time
 * forever since the configuration can change, but it is recommanded to keep it
 * while we stay in a function.
 *
 * @param idx The ID of the logger to get.
 *
 * @return A shared pointer to the logger.
 */
std::shared_ptr<spdlog::logger> log_v3::get(const uint32_t idx) {
  std::shared_lock lck(_loggers_m);
  return _loggers[idx];
}

/**
 * @brief Accessor to the logger from its name. This accessor may be useful in
 * several case but I don't want to see some check to see if the returned value
 * is not null. This function is bad, the get(ID) is far better but in some
 * cases, it is useful.
 *
 * @param name The name of the wanted logger.
 *
 * @return a shared pointer to the logger.
 */
std::shared_ptr<spdlog::logger> log_v3::get(const std::string& name) {
  return spdlog::get(name);
}

/**
 * @brief Create the loggers configuration from the given log_conf object.
 * New loggers are created with the good configuration, and if a logger already
 * exists and is missing in the configuration, it is disabled.
 *
 * @param log_conf
 */
void log_v3::apply(const config& log_conf) {
  auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
  std::shared_ptr<sinks::base_sink<std::mutex>> file_sink;

  std::string _file_path = log_conf.log_path();
  if (log_conf.max_size())
    file_sink = std::make_shared<sinks::rotating_file_sink_mt>(
        _file_path, log_conf.max_size(), 99);
  else
    file_sink = std::make_shared<sinks::basic_file_sink_mt>(_file_path);

  auto update_logger = [&](const std::string& name, level::level_enum lvl) {
    auto logger = std::make_shared<spdlog::logger>(name, file_sink);
    logger->set_level(lvl);
    if (lvl != level::off) {
      if (log_conf.flush_interval() > 0)
        logger->flush_on(level::warn);
      else
        logger->flush_on(lvl);
      if (log_conf.log_pid()) {
        if (log_conf.log_source())
          logger->set_pattern(
              "[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] [%P] %v");
        else
          logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
      } else {
        if (log_conf.log_source())
          logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] %v");
        else
          logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
      }
    }

    //    if (name == "grpc") {
    //      switch (lvl) {
    //        case level::level_enum::trace:
    //        case level::level_enum::debug:
    //          gpr_set_log_verbosity(GPR_LOG_SEVERITY_DEBUG);
    //          break;
    //        case level::level_enum::info:
    //        case level::level_enum::warn:
    //          gpr_set_log_verbosity(GPR_LOG_SEVERITY_INFO);
    //          break;
    //        default:
    //          gpr_set_log_verbosity(GPR_LOG_SEVERITY_ERROR);
    //          break;
    //      }
    //    }

    bool done = false;
    for (auto it = _loggers.begin(); it != _loggers.end(); ++it) {
      if ((*it)->name() == name) {
        drop(name);
        *it = logger;
        done = true;
        break;
      }
    }
    if (!done)
      _loggers.push_back(logger);
    spdlog::register_logger(logger);
    return logger;
  };

  absl::flat_hash_set<std::string> logger_names;

  std::lock_guard<std::shared_mutex> lck(_loggers_m);

  for (auto& l : _loggers)
    logger_names.insert(l->name());

  for (auto it = log_conf.loggers().begin(), end = log_conf.loggers().end();
       it != end; ++it) {
    update_logger(it->first, level::from_str(it->second));
    if (logger_names.contains(it->first))
      logger_names.erase(it->first);
  }

  for (auto& n : logger_names)
    update_logger(n, level::off);

  _flush_interval = std::chrono::seconds(
      log_conf.flush_interval() > 0 ? log_conf.flush_interval() : 0);
  /* if _flush_interval is 0, the flush worker is stopped. */
  spdlog::flush_every(_flush_interval);
}