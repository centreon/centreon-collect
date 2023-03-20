/*
** Copyright 2020-2022 Centreon
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

#include "com/centreon/broker/log_v2.hh"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <grpc/impl/codegen/log.h>

#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace spdlog;

/**
 * @brief this function is passed to grpc in order to log grpc layer's events to
 * logv2
 *
 * @param args grpc logging params
 */
static void grpc_logger(gpr_log_func_args* args) {
  std::shared_ptr<spdlog::logger> logger = log_v2::grpc();
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

std::shared_ptr<log_v2> log_v2::_instance;

void log_v2::load(const std::shared_ptr<asio::io_context>& io_context) {
  _instance.reset(new log_v2(io_context));
}

log_v2& log_v2::instance() {
  return *_instance;
}

log_v2::log_v2(const std::shared_ptr<asio::io_context>& io_context)
    : log_v2_base("broker"),
      _running{false},
      _flush_timer(*io_context),
      _flush_timer_active(true),
      _io_context(io_context) {
  auto stdout_sink = std::make_shared<sinks::stdout_color_sink_mt>();
  auto create_logger = [&](const std::string& name) {
    std::shared_ptr<spdlog::logger> log =
        std::make_shared<com::centreon::engine::log_v2_logger>(name, this,
                                                               stdout_sink);
    log->flush_on(level::info);
    spdlog::register_logger(log);
    return log;
  };

  _log[log_v2::log_bam] = create_logger("bam");
  _log[log_v2::log_bbdo] = create_logger("bbdo");
  _log[log_v2::log_config] = create_logger("config");
  _log[log_v2::log_core] = create_logger("core");
  _log[log_v2::log_graphite] = create_logger("graphite");
  _log[log_v2::log_notification] = create_logger("notification");
  _log[log_v2::log_rrd] = create_logger("rrd");
  _log[log_v2::log_stats] = create_logger("stats");
  _log[log_v2::log_influxdb] = create_logger("influxdb");
  _log[log_v2::log_lua] = create_logger("lua");
  _log[log_v2::log_neb] = create_logger("neb");
  _log[log_v2::log_perfdata] = create_logger("perfdata");
  _log[log_v2::log_processing] = create_logger("processing");
  _log[log_v2::log_sql] = create_logger("sql");
  _log[log_v2::log_tcp] = create_logger("tcp");
  _log[log_v2::log_tls] = create_logger("tls");
  _log[log_v2::log_grpc] = create_logger("grpc");
  gpr_set_log_function(grpc_logger);
  _running = true;
}

log_v2::~log_v2() {
  core()->info("log finished");
  _running = false;
  for (auto& l : _log)
    l.reset();
}

void log_v2::apply(const config::state& conf) {
  std::lock_guard<std::mutex> lock(_load_m);
  _running = false;

  const auto& log = conf.log_conf();

  // reset loggers to null sink
  auto null_sink = std::make_shared<sinks::null_sink_mt>();
  std::shared_ptr<sinks::base_sink<std::mutex>> file_sink;

  _file_path = log.log_path();
  if (log.max_size)
    file_sink = std::make_shared<sinks::rotating_file_sink_mt>(
        _file_path, log.max_size, 99);
  else
    file_sink = std::make_shared<sinks::basic_file_sink_mt>(_file_path);

  auto create_log = [&](const std::string& name, level::level_enum lvl) {
    spdlog::drop(name);
    auto logger = std::make_shared<com::centreon::engine::log_v2_logger>(
        name, this, file_sink);
    logger->set_level(lvl);
    if (lvl != level::off) {
      if (log.flush_period)
        logger->flush_on(level::warn);
      else
        logger->flush_on(level::trace);
      if (log.log_pid) {
        if (log.log_source)
          logger->set_pattern(
              "[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%s:%#] [%P] %v");
        else
          logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] [%P] %v");
      } else {
        if (log.log_source)
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
        case level::level_enum::err:
        case level::level_enum::critical:
        case level::level_enum::off:
          gpr_set_log_verbosity(GPR_LOG_SEVERITY_ERROR);
          break;
      }
    }

    spdlog::register_logger(logger);
    return logger;
  };

  _log[log_v2::log_core] = create_log("core", level::info);
  core()->info("{} : log started", _file_path);

  absl::flat_hash_map<std::string, int32_t> lgs{
      {"bam", log_v2::log_bam},
      {"bbdo", log_v2::log_bbdo},
      {"config", log_v2::log_config},
      {"core", log_v2::log_core},
      {"graphite", log_v2::log_graphite},
      {"grpc", log_v2::log_grpc},
      {"influxdb", log_v2::log_influxdb},
      {"lua", log_v2::log_lua},
      {"neb", log_v2::log_neb},
      {"notification", log_v2::log_notification},
      {"perfdata", log_v2::log_perfdata},
      {"processing", log_v2::log_processing},
      {"rrd", log_v2::log_rrd},
      {"sql", log_v2::log_sql},
      {"stats", log_v2::log_stats},
      {"tcp", log_v2::log_tcp},
      {"tls", log_v2::log_tls},
  };
  for (auto it = log.loggers.begin(), end = log.loggers.end(); it != end;
       ++it) {
    if (lgs.contains(it->first)) {
      _log[lgs[it->first]] = create_log(it->first, level::from_str(it->second));
      lgs.erase(it->first);
    }
  }

  for (auto it = lgs.begin(); it != lgs.end(); ++it) {
    _log[lgs[it->first]] = create_log(it->first, spdlog::level::off);
  }

  _flush_interval =
      std::chrono::seconds(log.flush_period > 0 ? log.flush_period : 2);
  start_flush_timer(file_sink);
  _running = true;
}

void log_v2::set_flush_interval(unsigned second_flush_interval) {
  log_v2_base::set_flush_interval(second_flush_interval);
  if (second_flush_interval) {
    for (auto logger : _log) {
      logger->flush_on(level::warn);
    }
  } else {
    for (auto logger : _log) {
      logger->flush_on(level::trace);
    }
  }
}

/**
 * @brief logs are written periodicaly to  disk
 *
 * @param sink
 */
void log_v2::start_flush_timer(spdlog::sink_ptr sink) {
  std::lock_guard<std::mutex> l(_flush_timer_m);
  _flush_timer.expires_after(_flush_interval);
  _flush_timer.async_wait([me = std::static_pointer_cast<log_v2>(_instance),
                           sink](const asio::error_code& err) {
    if (err || !me->_flush_timer_active) {
      return;
    }
    if (me->get_flush_interval().count() > 0) {
      sink->flush();
    }
    me->start_flush_timer(sink);
  });
}

void log_v2::stop_flush_timer() {
  std::lock_guard<std::mutex> l(_flush_timer_m);
  _flush_timer_active = false;
  _flush_timer.cancel();
}

/**
 * @brief Check if the given logger makes part of our loggers
 *
 * @param logger A logger name
 *
 * @return a boolean.
 */
bool log_v2::contains_logger(const std::string& logger) {
  const std::array<std::string, 17> loggers{
      "bam",      "bbdo",         "config", "core",  "lua",      "influxdb",
      "graphite", "notification", "rrd",    "stats", "perfdata", "processing",
      "sql",      "neb",          "tcp",    "tls",   "grpc"};
  return std::find(loggers.begin(), loggers.end(), logger) != loggers.end();
}

/**
 * @brief Accessor to the various levels of loggers
 *
 * @return A vector of pairs of strings. The first string is the logger name and
 * the second string is its level.
 */
std::vector<std::pair<std::string, std::string>> log_v2::levels() const {
  std::vector<std::pair<std::string, std::string>> retval;
  if (_running) {
    retval.reserve(_log.size());
    for (auto& l : _log) {
      spdlog::level::level_enum level = l->level();
      auto& lv = to_string_view(level);
      retval.emplace_back(l->name(), std::string(lv.data(), lv.size()));
    }
  }
  return retval;
}

/**
 * @brief this private static method is used to access a specific logger
 *
 * @param log_type
 * @param log_str
 * @return std::shared_ptr<spdlog::logger>
 */
std::shared_ptr<spdlog::logger> log_v2::get_logger(logger log_type,
                                                   const char* log_str) {
  if (_instance->_running)
    return _instance->_log[log_type];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>(log_str, null_sink);
  }
}

/**
 * @brief Check if the given level makes part of the available levels.
 *
 * @param level A level as a string
 *
 * @return A boolean.
 */
bool log_v2::contains_level(const std::string& level) {
  /* spdlog wants 'off' to disable a log but we tolerate 'disabled' */
  if (level == "disabled" || level == "off")
    return true;

  level::level_enum l = level::from_str(level);
  return l != level::off;
}

/**
 * @brief Set the level of a logger.
 *
 * @param logger The logger name
 * @param level The level as a string
 */
void log_v2::set_level(const std::string& logger, const std::string& level) {
  if (_running) {
    bool found = false;
    for (auto l : _log) {
      if (l->name() == logger) {
        found = true;
        level::level_enum lvl = level::from_str(level);
        if (lvl == level::off && level != "off")
          throw msg_fmt("The '{}' level is unknown", level);
        l->set_level(lvl);
        break;
      }
    }
    if (!found)
      throw msg_fmt("The '{}' logger does not exist", logger);
  } else
    throw msg_fmt(
        "Unable to change '{}' logger level, the logger is not running for now "
        "- try later.",
        logger);
}
