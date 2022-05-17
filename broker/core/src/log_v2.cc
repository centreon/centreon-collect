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

#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace spdlog;

log_v2& log_v2::instance() {
  static log_v2 instance;
  return instance;
}

log_v2::log_v2() : _running{false} {
  auto stdout_sink = std::make_shared<sinks::stdout_color_sink_mt>();
  auto create_logger = [&stdout_sink](const std::string& name) {
    std::shared_ptr<spdlog::logger> log =
        std::make_shared<spdlog::logger>(name, stdout_sink);
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
  _running = true;
}

log_v2::~log_v2() {
  core()->info("log finished");
  _running = false;
  spdlog::shutdown();
  for (auto& l : _log)
    l.reset();
}

void log_v2::apply(const config::state& conf) {
  std::lock_guard<std::mutex> lock(_load_m);
  _running = false;

  const auto& log = conf.log_conf();

  _log_name = log.log_path();
  // reset loggers to null sink
  auto null_sink = std::make_shared<sinks::null_sink_mt>();
  std::shared_ptr<sinks::base_sink<std::mutex>> file_sink;

  if (log.max_size)
    file_sink = std::make_shared<sinks::rotating_file_sink_mt>(
        _log_name, log.max_size, 99);
  else
    file_sink = std::make_shared<sinks::basic_file_sink_mt>(_log_name);

  auto create_log = [&file_sink, flush_period = log.flush_period](
                        const std::string& name, level::level_enum lvl) {
    spdlog::drop(name);
    auto log = std::make_shared<spdlog::logger>(name, file_sink);
    log->set_level(lvl);
    if (lvl != level::off) {
      if (flush_period)
        log->flush_on(level::warn);
      else
        log->flush_on(lvl);
      log->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
    }
    spdlog::register_logger(log);
    return log;
  };

  _log[log_v2::log_core] = create_log("core", level::info);
  core()->info("{} : log started", _log_name);

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

  spdlog::flush_every(std::chrono::seconds(log.flush_period));
  for (auto it = lgs.begin(); it != lgs.end(); ++it) {
    spdlog::drop(it->first);
    auto l = std::make_shared<spdlog::logger>(it->first, null_sink);
    spdlog::register_logger(l);
    _log[it->second] = std::move(l);
  }
  _running = true;
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

std::shared_ptr<spdlog::logger> log_v2::bam() {
  if (instance()._running)
    return instance()._log[log_v2::log_bam];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("bam", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::bbdo() {
  if (instance()._running)
    return instance()._log[log_v2::log_bbdo];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("bbdo", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::config() {
  if (instance()._running)
    return instance()._log[log_v2::log_config];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("config", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::core() {
  if (instance()._running)
    return instance()._log[log_v2::log_core];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("core", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::influxdb() {
  if (instance()._running)
    return instance()._log[log_v2::log_influxdb];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("influxdb", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::graphite() {
  if (instance()._running)
    return instance()._log[log_v2::log_graphite];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("graphite", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::notification() {
  if (instance()._running)
    return instance()._log[log_v2::log_notification];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("notification", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::rrd() {
  if (instance()._running)
    return instance()._log[log_v2::log_rrd];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("rrd", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::stats() {
  if (instance()._running)
    return instance()._log[log_v2::log_stats];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("stats", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::lua() {
  if (instance()._running)
    return instance()._log[log_v2::log_lua];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("lua", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::neb() {
  if (instance()._running)
    return instance()._log[log_v2::log_neb];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("neb", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::perfdata() {
  if (instance()._running)
    return instance()._log[log_v2::log_perfdata];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("perfdata", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::processing() {
  if (instance()._running)
    return instance()._log[log_v2::log_processing];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("processing", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::sql() {
  if (instance()._running)
    return instance()._log[log_v2::log_sql];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("sql", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::tcp() {
  if (instance()._running)
    return instance()._log[log_v2::log_tcp];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("tcp", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::tls() {
  if (instance()._running)
    return instance()._log[log_v2::log_tls];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("tls", null_sink);
  }
}

std::shared_ptr<spdlog::logger> log_v2::grpc() {
  if (instance()._running)
    return instance()._log[log_v2::log_grpc];
  else {
    auto null_sink = std::make_shared<sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("grpc", null_sink);
  }
}

const std::string& log_v2::log_name() const {
  return _log_name;
}
