/**
 * Copyright 2022-2024 Centreon
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
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include "centreon_file_sink.hh"

#include <atomic>
#include <initializer_list>

using namespace com::centreon::common::log_v2;
using namespace spdlog;

log_v2* log_v2::_instance = nullptr;

constexpr std::array<std::string_view, log_v2::LOGGER_SIZE> logger_name{
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
    "runtime",
    "otl"};

/**
 * @brief this function is passed to grpc in order to log grpc layer's events to
 * logv2
 *
 * @param args grpc logging params
 */
static void grpc_logger(gpr_log_func_args* args) {
  auto default_logger = log_v2::instance().get(log_v2::GRPC);
  auto otl_logger = log_v2::instance().get(log_v2::OTL);
  auto min_level = spdlog::level::level_enum::off;  // default
  if (default_logger) {
    min_level = default_logger->level();
  }
  if (otl_logger) {
    min_level = std::min(min_level, otl_logger->level());
    if (!default_logger) {
      default_logger = otl_logger;
    }
  }
  if (min_level != spdlog::level::off) {
    const char* start;
    if (min_level > spdlog::level::debug) {
      start = strstr(args->message, "}: ");
      if (!start)
        return;
      start += 3;
    } else
      start = args->message;
    switch (args->severity) {
      case GPR_LOG_SEVERITY_DEBUG:
        if (min_level == spdlog::level::trace ||
            min_level == spdlog::level::debug) {
          SPDLOG_LOGGER_DEBUG(default_logger, "{} ({}:{})", start, args->file,
                              args->line);
        }
        break;
      case GPR_LOG_SEVERITY_INFO:
        if (min_level == spdlog::level::trace ||
            min_level == spdlog::level::debug ||
            min_level == spdlog::level::info) {
          if (start)
            SPDLOG_LOGGER_INFO(default_logger, "{}", start);
        }
        break;
      case GPR_LOG_SEVERITY_ERROR:
        SPDLOG_LOGGER_ERROR(default_logger, "{}", start);
        break;
    }
  }
}

/**
 * @brief Initialization of the log_v2 instance.
 *
 * @param name The name of the logger.
 */
void log_v2::load(std::string name) {
  if (!_instance)
    _instance = new log_v2(std::move(name));
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
log_v2::log_v2(std::string name) : _log_name{std::move(name)} {
  create_loggers(config::logger_type::LOGGER_STDOUT);
}

/**
 * @brief Destructor.
 */
log_v2::~log_v2() noexcept {
  /* When log_v2 is stopped, grpc mustn't log anymore. */
  gpr_set_log_function(nullptr);
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
  for (retval = 0; retval < logger_name.size(); retval++) {
    if (logger_name[retval] == name)
      return static_cast<logger_id>(retval);
  }
  return LOGGER_SIZE;
}

/**
 * @brief Create all the loggers in log_v2. By default they are writing into
 * stdout. Broker.
 *
 * @param typ The log type, to log in syslog, a file, in stdout.
 * @param length The max length of the log receiver (only used for files).
 */
void log_v2::create_loggers(config::logger_type typ, size_t length) {
  _not_threadsafe_configuration = true;
  sink_ptr my_sink;

  for (int32_t id = 0; id < LOGGER_SIZE; id++)
    assert(!_loggers[id]);

  switch (typ) {
    case config::logger_type::LOGGER_FILE: {
      if (length)
        my_sink = std::make_shared<sinks::rotating_file_sink_mt>(
            _file_path, _current_max_size, 99);
      else
        my_sink = std::make_shared<sinks::centreon_file_sink_mt>(_file_path);
    } break;
    case config::logger_type::LOGGER_SYSLOG:
      my_sink = std::make_shared<sinks::syslog_sink_mt>(_log_name, 0, 0, true);
      break;
    case config::logger_type::LOGGER_STDOUT:
      my_sink = std::make_shared<sinks::stdout_color_sink_mt>();
      break;
  }

  for (int32_t id = 0; id < LOGGER_SIZE; id++) {
    std::shared_ptr<spdlog::logger> logger;
    logger = std::make_shared<spdlog::logger>(
        std::string(logger_name[id].data(), logger_name[id].size()), my_sink);
    if (_log_pid) {
      if (_log_source)
        logger->set_pattern(
            "[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] [%P] %v");
      else
        logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
    } else {
      if (_log_source)
        logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] %v");
      else
        logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
    }
    if (id > 1)
      logger->set_level(level::level_enum::err);
    else
      logger->set_level(level::level_enum::info);
    spdlog::register_logger(logger);
    _loggers[id] = std::move(logger);

    /* Hook for gRPC, not beautiful, but no idea how to do better. */
    if (id == GRPC || id == OTL)
      gpr_set_log_function(grpc_logger);
  }
  _not_threadsafe_configuration = false;
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
  spdlog::sink_ptr my_sink;

  /* This part is about sinks so it is reserved for masters */
  if (!log_conf.only_atomic_changes()) {
    _not_threadsafe_configuration = true;
    _file_path = log_conf.log_path();
    switch (log_conf.log_type()) {
      case config::logger_type::LOGGER_FILE: {
        if (log_conf.max_size())
          my_sink = std::make_shared<sinks::rotating_file_sink_mt>(
              _file_path, log_conf.max_size(), 99);
        else
          my_sink = std::make_shared<sinks::centreon_file_sink_mt>(_file_path);
      } break;
      case config::logger_type::LOGGER_SYSLOG:
        my_sink =
            std::make_shared<sinks::syslog_sink_mt>(_file_path, 0, 0, true);
        break;
      case config::logger_type::LOGGER_STDOUT:
        my_sink = std::make_shared<sinks::stdout_color_sink_mt>();
        break;
    }

    for (int32_t id = 0; id < LOGGER_SIZE; id++) {
      std::vector<spdlog::sink_ptr> sinks;

      /* Little hack to include the broker sink to engine loggers. */
      auto& name = logger_name[id];
      if (log_conf.loggers_with_custom_sinks().contains(name))
        sinks = log_conf.custom_sinks();

      sinks.push_back(my_sink);
      auto logger = _loggers[id];
      logger->sinks() = sinks;
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

      if (name == "grpc" && log_conf.loggers().contains(name)) {
        level::level_enum lvl = level::from_str(log_conf.loggers().at(name));
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
    }
    _not_threadsafe_configuration = false;
  }

  _flush_interval = std::chrono::seconds(
      log_conf.flush_interval() > 0 ? log_conf.flush_interval() : 0);
  spdlog::flush_every(_flush_interval);
  /* This is for all loggers, a slave will overwrite the master configuration
   */
  for (int32_t id = 0; id < LOGGER_SIZE; id++) {
    auto& name = logger_name[id];
    if (log_conf.loggers().contains(name)) {
      auto logger = _loggers[id];
      level::level_enum lvl = level::from_str(log_conf.loggers().at(name));
      logger->set_level(lvl);
      if (log_conf.flush_interval() > 0)
        logger->flush_on(level::warn);
      else
        logger->flush_on(lvl);
    }
  }

  for (auto& s : _loggers[0]->sinks()) {
    spdlog::sinks::centreon_file_sink_mt* file_sink =
        dynamic_cast<spdlog::sinks::centreon_file_sink_mt*>(s.get());
    if (file_sink)
      file_sink->reopen();
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
  for (auto& n : logger_name)
    if (n == logger)
      return true;
  return false;
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
