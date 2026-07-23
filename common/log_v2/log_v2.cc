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

#include <absl/base/log_severity.h>
#include <absl/container/flat_hash_set.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <grpc/impl/codegen/log.h>
#include <spdlog/common.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include "centreon_rotating_file_sink-inl.hh"

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
    "otl",
    "cache"};

/**
 * @brief this function is passed to grpc in order to log grpc layer's events to
 * logv2
 *
 * @param args grpc logging params
 */

class GrpcLogSink : public absl::LogSink {
  // std::shared_ptr<spdlog::logger> _logger_grpc =
  //     log_v2::instance().get(log_v2::GRPC);
  // std::shared_ptr<spdlog::logger> _logger_otl =
  //     log_v2::instance().get(log_v2::OTL);

 public:
  void Send(const absl::LogEntry& e) override {
    auto _logger_grpc = log_v2::instance().get(log_v2::GRPC);
    auto _logger_otl = log_v2::instance().get(log_v2::OTL);
    auto lvl = e.log_severity();
    auto min_level = spdlog::level::level_enum::off;  // default
    if (_logger_grpc) {
      min_level = _logger_grpc->level();
    }
    if (_logger_otl) {
      min_level = std::min(min_level, _logger_otl->level());
      if (!_logger_grpc) {
        _logger_grpc = _logger_otl;
      }
    }

    std::string_view msg = e.text_message();

    if (min_level > spdlog::level::debug) {
      auto p = msg.find("}: ");
      if (p != std::string_view::npos)
        msg.remove_prefix(p + 3);
    }
    switch (lvl) {
      case absl::LogSeverity::kInfo:
        if (min_level <= spdlog::level::info)
          SPDLOG_LOGGER_INFO(_logger_grpc, "{}", msg);
        break;
      case absl::LogSeverity::kWarning:
      case absl::LogSeverity::kError:
        if (min_level <= spdlog::level::err)
          SPDLOG_LOGGER_ERROR(_logger_grpc, "{}", msg);
        break;
      case absl::LogSeverity::kFatal:
        SPDLOG_LOGGER_CRITICAL(_logger_grpc, "{}", msg);
        break;
    }
  }
};
static GrpcLogSink _common_grpc_sink;

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
 * @brief Constructor of the log_v2 class. This constructor is not public
 * since it is called through the load() function.
 *
 * @param name Name of the logger.
 * @param ilist List of loggers to initialize.
 */
log_v2::log_v2(std::string name)
    : _log_name{std::move(name)},
      _log_type(config::logger_type::LOGGER_STDOUT) {
  sink_ptr my_sink = std::make_shared<sinks::stdout_color_sink_mt>();

  for (int32_t id = 0; id < LOGGER_SIZE; id++) {
    std::shared_ptr<spdlog::logger> logger;
    logger = std::make_shared<spdlog::logger>(
        std::string(logger_name[id].data(), logger_name[id].size()), my_sink);
    logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
    if (id > 1)
      logger->set_level(level::level_enum::err);
    else
      logger->set_level(level::level_enum::info);
    spdlog::register_logger(logger);
    _loggers[id] = std::move(logger);
  }
  /* Hook for gRPC, not beautiful, but no idea how to do better. */
  absl::AddLogSink(&_common_grpc_sink);
}

/**
 * @brief Destructor.
 */
log_v2::~log_v2() noexcept {
  /* When log_v2 is stopped, grpc mustn't log anymore. */
  absl::RemoveLogSink(&_common_grpc_sink);
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
  absl::MutexLock l(&_loggers_m);
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
 * @brief Accessor to the logger id by its name. If the name does not match
 * any logger, LOGGER_SIZE is returned. This method is used essentially
 * during the configuration because the final user is not aware of internal
 * enums.
 * @param name The logger name.
 *
 * @return A log_v2::logger_id corresponding to the wanted logger or
 * LOGGER_SIZE if not found.
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
 * @brief Accessor to the logger of given ID.
 *
 * @param idx The ID of the logger to get.
 *
 * @return A shared pointer to the logger.
 */
std::shared_ptr<spdlog::logger> log_v2::get(log_v2::logger_id idx) {
  absl::MutexLock l(&_loggers_m);
  if (idx < _loggers.size())
    return _loggers[idx];
  else
    return {};
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
  std::shared_ptr<spdlog::logger> warning_to_log;
  try {
    absl::MutexLock l(&_loggers_m);

    /* This part is about sinks so it is reserved for masters. Recreation must
     * also happen on the first apply() that requests custom sinks, even if the
     * resolved type happens to match the constructor's initial LOGGER_STDOUT,
     * otherwise a config that resolves to stdout on first load would never get
     * its custom sinks (e.g. engine's broker forwarding sink) attached. Outside
     * of that narrow case, a same-type apply() must NOT recreate the loggers:
     * callers may have cached a shared_ptr obtained via get() before this call,
     * and recreating would silently strand them on a stale, unconfigured
     * logger object. */
    if (!log_conf.only_atomic_changes() &&
        (_log_type != log_conf.log_type() ||
         (!_broker_sink_added &&
          !log_conf.loggers_with_custom_sinks().empty()))) {
      // logger sink containers are not thread safe => recreate loggers
      const std::string file_path = log_conf.log_path();
      config::logger_type actual_type = log_conf.log_type();
      switch (log_conf.log_type()) {
        case config::logger_type::LOGGER_FILE:
          if (!file_path.empty()) {
            my_sink = std::make_shared<sinks::centreon_rotating_file_sink_mt>(
                file_path, log_conf.max_size(), 99);
            break;
          }
          // No path configured: fall back to stdout, and reflect that in
          // _log_type so a later apply() with a real path still recreates.
          actual_type = config::logger_type::LOGGER_STDOUT;
          [[fallthrough]];
        case config::logger_type::LOGGER_STDOUT:
          my_sink = std::make_shared<sinks::stdout_color_sink_mt>();
          break;
        case config::logger_type::LOGGER_SYSLOG:
          my_sink =
              std::make_shared<sinks::syslog_sink_mt>(file_path, 0, 0, true);
          break;
      }

      std::vector<spdlog::sink_ptr> sinks;
      for (int32_t id = 0; id < LOGGER_SIZE; id++) {
        sinks.clear();
        /* Little hack to include the broker sink to engine loggers. */
        auto& name = logger_name[id];
        if (log_conf.loggers_with_custom_sinks().contains(name)) {
          sinks = log_conf.custom_sinks();
          _broker_sink_added = true;
        }
        sinks.push_back(my_sink);
        auto logger = std::make_shared<spdlog::logger>(
            std::string(logger_name[id].data(), logger_name[id].size()),
            sinks.begin(), sinks.end());
        spdlog::drop(logger->name());
        spdlog::register_logger(logger);
        if (_loggers[id]) {
          logger->set_level(_loggers[id]->level());
          logger->flush_on(_loggers[id]->flush_level());
        }
        _loggers[id] = logger;
      }
      _log_type = actual_type;
      if (_configured) {
        warning_to_log = _loggers[CONFIG];
      }
    }
    _configured = true;

    _flush_interval = std::chrono::seconds(
        log_conf.flush_interval() > 0 ? log_conf.flush_interval() : 0);
    spdlog::flush_every(_flush_interval);

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

    if (log_conf.allow_change_pattern_and_path()) {
      /* The pattern only depends on the global log_pid()/log_source() flags,
       * not on the logger name, so it applies to every logger. */
      for (auto& logger : _loggers) {
        if (!logger)
          continue;
        if (log_conf.log_pid()) {
          if (log_conf.log_source())
            logger->set_pattern(
                "[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] [%P] %v");
          else
            logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
        } else {
          if (log_conf.log_source())
            logger->set_pattern(
                "[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] %v");
          else
            logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
        }
      }

      if (log_conf.log_type() == config::logger_type::LOGGER_FILE) {
        for (auto& s : _loggers[0]->sinks()) {
          if (auto* rotate_file_sink =
                  dynamic_cast<spdlog::sinks::centreon_rotating_file_sink_mt*>(
                      s.get())) {
            rotate_file_sink->set_filename(log_conf.log_path());
            rotate_file_sink->set_max_size(log_conf.max_size());
          }
        }
      }
    }
    // in case of file rotate is done by rotated, reopen files
    for (auto& s : _loggers[0]->sinks()) {
      if (auto* file_sink =
              dynamic_cast<spdlog::sinks::centreon_rotating_file_sink_mt*>(
                  s.get()))
        file_sink->reopen();
    }
  } catch (const std::exception& e) {
    std::cerr << "Fail to apply log config " << e.what()
              << " config:" << log_conf << std::endl;
    throw;
  }
  if (warning_to_log)
    warning_to_log->warn(
        "log type is modified, we can't update type of existing loggers. "
        "So we will recreate them and older ones will continue to live "
        "until restart");
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
  absl::MutexLock l(&_loggers_m);
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
  absl::MutexLock l(&_loggers_m);
  for (auto& l : _loggers)
    /* Loggers can be not defined in case of legacy logger enabled. */
    if (l)
      l->set_level(spdlog::level::level_enum::off);
}

void log_v2::disable(std::initializer_list<logger_id> ilist) {
  absl::MutexLock l(&_loggers_m);
  for (logger_id id : ilist) {
    /* Loggers can be not defined in case of legacy logger enabled. */
    if (_loggers[id])
      _loggers[id]->set_level(spdlog::level::level_enum::off);
  }
}

std::string log_v2::filename() const {
  absl::MutexLock l(&_loggers_m);
  for (auto& s : _loggers[0]->sinks()) {
    spdlog::sinks::centreon_rotating_file_sink_mt* file_sink =
        dynamic_cast<spdlog::sinks::centreon_rotating_file_sink_mt*>(s.get());
    if (file_sink) {
      return file_sink->filename();
    }
  }
  return "";
}
