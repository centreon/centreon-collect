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
#include <grpc/impl/codegen/log.h>
#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/syslog_sink.h>

#include <atomic>
#include <initializer_list>

using namespace com::centreon::common::log_v2;
using namespace spdlog;

log_v2* log_v2::_instance;

const std::array<std::string, log_v2::LOGGER_SIZE> log_v2::_logger_name = {
    "core",
    "config",
    "bam",
    "bbdo",
    "lua",
    "influxdb",
    "graphite",
    "rrd",
    "stats",
    "perfdata",
    "processing",
    "sql",
    "neb",
    "tcp",
    "tls",
    "grpc",
    "victoria_metrics",
    "process",
    "functions",
    "events",
    "checks",
    "notifications",
    "eventbroker",
    "external_command",
    "commands",
    "downtimes",
    "comments",
    "macros",
    "runtime"};

/**
 * @brief this function is passed to grpc in order to log grpc layer's events to
 * logv2
 *
 * @param args grpc logging params
 */
static void grpc_logger(gpr_log_func_args* args) {
  auto logger = log_v2::instance().get(log_v2::GRPC);
  if (logger) {
    spdlog::level::level_enum grpc_level = logger->level();
    const char* start;
    if (grpc_level == spdlog::level::off)
      return;
    else if (grpc_level > spdlog::level::debug) {
      start = strstr(args->message, "}: ");
      if (!start)
        return;
      start += 3;
    } else
      start = args->message;
    switch (args->severity) {
      case GPR_LOG_SEVERITY_DEBUG:
        if (grpc_level == spdlog::level::trace ||
            grpc_level == spdlog::level::debug) {
          SPDLOG_LOGGER_DEBUG(logger, "{} ({}:{})", start, args->file,
                              args->line);
        }
        break;
      case GPR_LOG_SEVERITY_INFO:
        if (grpc_level == spdlog::level::trace ||
            grpc_level == spdlog::level::debug ||
            grpc_level == spdlog::level::info) {
          if (start)
            SPDLOG_LOGGER_INFO(logger, "{}", start);
        }
        break;
      case GPR_LOG_SEVERITY_ERROR:
        SPDLOG_LOGGER_ERROR(logger, "{}", start);
        break;
    }
  }
}

/**
 * @brief Initialization of the log_v2 instance. Initialized loggers are given
 * in ilist.
 *
 * @param name The name of the logger.
 * @param ilist The list of loggers to initialize.
 */
void log_v2::load(const std::string& name,
                  std::initializer_list<logger_id> ilist) {
  _instance = new log_v2(name, ilist);
}

/**
 * @brief Destruction of the log_v2 instance. No more call to log_v2 is
 * possible.
 */
void log_v2::unload() {
  if (_instance) {
    delete _instance;
    _instance = nullptr;
    spdlog::drop_all();
    spdlog::shutdown();
  }
}

/**
 * @brief Constructor of the log_v2 class. This constructor is not public since
 * it is called through the load() function.
 *
 * @param name Name of the logger.
 * @param ilist List of loggers to initialize.
 */
log_v2::log_v2(const std::string& name,
               std::initializer_list<log_v2::logger_id> ilist)
    : _log_name{name} {
  for (auto& s : ilist)
    create_logger(s);
}

/**
 * @brief Static method to get the current log_v2 running instance.
 *
 * @return A reference to the log_v2 instance.
 */
log_v2& log_v2::instance() {
  assert(_instance);
  return *_instance;
}

/**
 * @brief Accessor to the flush interval current value.
 *
 * @return An std::chrono::seconds value.
 */
std::chrono::seconds log_v2::flush_interval() {
  return _flush_interval;
}

void log_v2::set_flush_interval(uint32_t second_flush_interval) {
  _flush_interval = std::chrono::seconds(second_flush_interval);
  if (second_flush_interval == 0) {
    for (auto& l : _loggers) {
      if (l && l->level() != level::off)
        l->flush_on(l->level());
    }
  } else {
    for (auto& l : _loggers) {
      if (l && l->level() != level::off)
        l->flush_on(level::warn);
    }
  }
  spdlog::flush_every(_flush_interval);
}

/**
 * @brief Accessor to the logger id by its name. If the name does not match any
 * logger, LOGGER_SIZE is returned. This method is used essentially during the
 * configuration because the final user is not aware of internal enums.
 * @param name The logger name.
 *
 * @return A log_v2::logger_id corresponding to the wanted logger or LOGGER_SIZE
 * if not found.
 */
log_v2::logger_id log_v2::get_id(const std::string& name) const noexcept {
  uint32_t retval;
  for (retval = 0; retval < _logger_name.size(); retval++) {
    if (_logger_name[retval] == name)
      return static_cast<logger_id>(retval);
  }
  return LOGGER_SIZE;
}

/**
 * @brief Create a logger from its id. The call of this function is dangerous.
 * We want to avoid mutexes in this library. So if we create a logger, we don't
 * have to use it at the same time.
 * This function is called when a broker module is started and it has to log.
 * Otherwise, it is called by the configuration apply of the log_v2 library.
 * If the logger already exists, the method returns it. The logger is not
 * modified by this method, it is created or returned as is.
 *
 * @param id A logger_id identifying which logger to initialize.
 *
 * @return A shared pointer to the logger.
 */
std::shared_ptr<spdlog::logger> log_v2::create_logger(const logger_id id) {
  sink_ptr my_sink;

  if (_loggers[id])
    return _loggers[id];

  switch (_current_log_type) {
    case config::logger_type::LOGGER_FILE: {
      if (_current_max_size)
        my_sink = std::make_shared<sinks::rotating_file_sink_mt>(
            _file_path, _current_max_size, 99);
      else
        my_sink = std::make_shared<sinks::basic_file_sink_mt>(_file_path);
    } break;
    case config::logger_type::LOGGER_SYSLOG:
      my_sink = std::make_shared<sinks::syslog_sink_mt>(_log_name, 0, 0, true);
      break;
    case config::logger_type::LOGGER_STDOUT:
      my_sink = std::make_shared<sinks::stdout_color_sink_mt>();
      break;
  }

  std::shared_ptr<spdlog::logger> logger;
  logger = std::make_shared<spdlog::logger>(_logger_name[id], my_sink);
  if (_log_pid) {
    if (_log_source)
      logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] [%P] %v");
    else
      logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
  } else {
    if (_log_source)
      logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] %v");
    else
      logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
  }
  logger->set_level(level::level_enum::info);
  spdlog::register_logger(logger);
  _loggers[id] = logger;
  _slaves[id] = false;

  /* Hook for gRPC, not beautiful, but no idea how to do better. */
  if (id == GRPC)
    gpr_set_log_function(grpc_logger);
  return logger;
}

/**
 * @brief Accessor to the logger of given ID.
 *
 * @param idx The ID of the logger to get.
 *
 * @return A shared pointer to the logger.
 */
std::shared_ptr<spdlog::logger> log_v2::get(log_v2::logger_id idx) {
  return _loggers[idx];
}

/**
 * @brief Create the loggers configuration from the given log_conf object.
 * New loggers are created with the good configuration.
 * This function should also be called when no logs are emitted.
 *
 * Two changes for a logger are possible:
 * * its level: change the level of a logger is easy since it is atomic.
 * * its sinks: In that case, things are more complicated. We have to build
 *   a new logger and replace the existing one.
 *
 * @param log_conf The configuration to apply
 */
void log_v2::apply(const config& log_conf) {
  sink_ptr my_sink;
  std::vector<spdlog::sink_ptr> sinks;

  switch (log_conf.log_type()) {
    case config::logger_type::LOGGER_FILE: {
      if (!log_conf.is_slave())
        _file_path = log_conf.log_path();
      if (log_conf.max_size())
        my_sink = std::make_shared<sinks::rotating_file_sink_mt>(
            _file_path, log_conf.max_size(), 99);
      else
        my_sink = std::make_shared<sinks::basic_file_sink_mt>(_file_path);
    } break;
    case config::logger_type::LOGGER_SYSLOG:
      my_sink = std::make_shared<sinks::syslog_sink_mt>(log_conf.filename(), 0,
                                                        0, true);
      break;
    case config::logger_type::LOGGER_STDOUT:
      my_sink = std::make_shared<sinks::stdout_color_sink_mt>();
      break;
  }

  if (!log_conf.custom_sinks().empty()) {
    sinks = log_conf.custom_sinks();
    sinks.push_back(my_sink);
  }

  auto update_sink_logger = [&](const std::string& name,
                                level::level_enum lvl) {
    logger_id id = get_id(name);
    std::shared_ptr<spdlog::logger> logger = _loggers[id];
    if (logger) {
      if (!log_conf.is_slave()) {
        if (!sinks.empty() &&
            log_conf.loggers_with_custom_sinks().contains(name))
          logger->sinks() = sinks;
        else {
          logger->sinks().clear();
          logger->sinks().push_back(my_sink);
        }
      }
    } else {
      if (log_conf.is_slave())
        logger = std::make_shared<spdlog::logger>(name, my_sink);
      else {
        if (!sinks.empty() &&
            log_conf.loggers_with_custom_sinks().contains(name))
          logger =
              std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
        else
          logger = std::make_shared<spdlog::logger>(name, my_sink);
      }
    }
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

    if (name == "grpc") {
      switch (lvl) {
        case level::level_enum::trace:
        case level::level_enum::debug:
          gpr_set_log_verbosity(GPR_LOG_SEVERITY_DEBUG);
          break;
        case level::level_enum::info:
        case level::level_enum::warn:
          gpr_set_log_verbosity(GPR_LOG_SEVERITY_INFO);
          break;
        default:
          gpr_set_log_verbosity(GPR_LOG_SEVERITY_ERROR);
          break;
      }
    }

    if (!_loggers[id]) {
      _loggers[id] = logger;
      _slaves[id] = log_conf.is_slave();

      /* Hook for gRPC, not beautiful, but no idea how to do better. */
      if (name == "grpc")
        gpr_set_log_function(grpc_logger);

      spdlog::register_logger(logger);
    }
    return logger;
  };

  _flush_interval = std::chrono::seconds(
      log_conf.flush_interval() > 0 ? log_conf.flush_interval() : 0);
  spdlog::flush_every(_flush_interval);

  /* Little array to know on which logger we already have made the update */
  absl::FixedArray<bool> applied(_loggers.size());
  /* Initialization of the array */
  memset(applied.data(), 0, applied.size() * sizeof(bool));
  /* We go through the configuration at first */
  for (auto it = log_conf.loggers().begin(), end = log_conf.loggers().end();
       it != end; ++it) {
    update_sink_logger(it->first, level::from_str(it->second));
    applied[get_id(it->first)] = true;
  }
  _current_log_type = log_conf.log_type();
  /* We go through the loggers not already updated. And if needed we update
   * their sinks. */
  for (uint32_t i = 0; i < applied.size(); i++) {
    if (!applied[i]) {
      if (_loggers[i])
        update_sink_logger(_logger_name[i], _loggers[i]->level());
    }
  }
}

/**
 * @brief Check if the given logger makes part of our loggers
 *
 * @param logger A logger name
 *
 * @return a boolean.
 */
bool log_v2::contains_logger(std::string_view logger) const {
  absl::flat_hash_set<std::string> loggers;
  for (auto& n : _logger_name)
    loggers.insert(n);
  return loggers.contains(logger);
}

/**
 * @brief Check if the given level makes part of the available levels.
 *
 * @param level A level as a string
 *
 * @return A boolean.
 */
bool log_v2::contains_level(const std::string& level) const {
  /* spdlog wants 'off' to disable a log but we tolerate 'disabled' */
  if (level == "disabled" || level == "off")
    return true;

  level::level_enum l = level::from_str(level);
  return l != level::off;
}

std::vector<std::pair<std::string, spdlog::level::level_enum>> log_v2::levels()
    const {
  std::vector<std::pair<std::string, spdlog::level::level_enum>> retval;
  retval.reserve(_loggers.size());
  for (auto& l : _loggers) {
    if (l) {
      auto level = l->level();
      retval.emplace_back(l->name(), level);
    }
  }
  return retval;
}

const std::string& log_v2::log_name() const {
  return _log_name;
}

void log_v2::disable() {
  for (auto& l : _loggers)
    /* Loggers can be not defined in case of legacy logger enabled. */
    if (l)
      l->set_level(spdlog::level::level_enum::off);
}

void log_v2::disable(std::initializer_list<logger_id> ilist) {
  for (logger_id id : ilist) {
    /* Loggers can be not defined in case of legacy logger enabled. */
    if (_loggers[id])
      _loggers[id]->set_level(spdlog::level::level_enum::off);
  }
}
