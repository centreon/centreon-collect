/*
** Copyright 2020-2023 Centreon
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
#include "com/centreon/broker/config/state.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace spdlog;

std::unique_ptr<log_v2> log_v2::_instance = nullptr;

log_v2::log_v2(const std::shared_ptr<asio::io_context>& io_context)
    : log_v2_base<17>("broker", 0, "", io_context) {
  auto stdout_sink = std::make_shared<sinks::stdout_color_sink_mt>();
  auto create_logger = [&](const std::string& name) {
    auto log = std::make_shared<spdlog::logger>(name, stdout_sink);
    log->flush_on(level::info);
    spdlog::register_logger(log);
    return log;
  };

  set_logger(log_v2::log_bam, create_logger("bam"));
  set_logger(log_v2::log_bbdo, create_logger("bbdo"));
  set_logger(log_v2::log_config, create_logger("config"));
  set_logger(log_v2::log_core, create_logger("core"));
  set_logger(log_v2::log_graphite, create_logger("graphite"));
  set_logger(log_v2::log_notification, create_logger("notification"));
  set_logger(log_v2::log_rrd, create_logger("rrd"));
  set_logger(log_v2::log_stats, create_logger("stats"));
  set_logger(log_v2::log_influxdb, create_logger("influxdb"));
  set_logger(log_v2::log_lua, create_logger("lua"));
  set_logger(log_v2::log_neb, create_logger("neb"));
  set_logger(log_v2::log_perfdata, create_logger("perfdata"));
  set_logger(log_v2::log_processing, create_logger("processing"));
  set_logger(log_v2::log_sql, create_logger("sql"));
  set_logger(log_v2::log_tcp, create_logger("tcp"));
  set_logger(log_v2::log_tls, create_logger("tls"));
  set_logger(log_v2::log_grpc, create_logger("grpc"));
  _running = true;
}

log_v2::~log_v2() noexcept {
  core()->info("log finished");
  stop_flush_timer();
  _running = false;
  for (auto& l : _log)
    l.reset();
}

void log_v2::apply(const config::log& log) {
  std::lock_guard<std::mutex> lock(_apply_m);
  _running = false;

  // reset loggers to null sink
  auto null_sink = std::make_shared<sinks::null_sink_mt>();
  std::shared_ptr<sinks::base_sink<std::mutex>> file_sink;
  std::shared_ptr<sinks::stdout_color_sink_mt> stdout_sink;

  set_file_path(log.log_path());
  if (file_path().empty())
    stdout_sink = std::make_shared<sinks::stdout_color_sink_mt>();
  else {
    if (log.max_size)
      file_sink = std::make_shared<sinks::rotating_file_sink_mt>(
          file_path(), log.max_size, 99);
    else
      file_sink = std::make_shared<sinks::basic_file_sink_mt>(file_path());
  }

  auto create_log = [&](const std::string& name, level::level_enum lvl) {
    spdlog::drop(name);
    std::shared_ptr<spdlog::logger> logger;
    if (stdout_sink)
      logger = std::make_shared<spdlog::logger>(name, stdout_sink);
    else
      logger = std::make_shared<spdlog::logger>(name, file_sink);
    logger->set_level(lvl);
    if (lvl != level::off) {
      if (log.flush_period)
        logger->flush_on(level::warn);
      else
        logger->flush_on(lvl);
      logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%n] [%l] %v");
    }
    spdlog::register_logger(logger);
    return logger;
  };

  _log[log_v2::log_core] = create_log("core", level::info);
  core()->info("{} : log started", log_name());

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
    spdlog::drop(it->first);
    auto l = std::make_shared<spdlog::logger>(it->first, null_sink);
    spdlog::register_logger(l);
    _log[it->second] = std::move(l);
  }

  _flush_interval =
      std::chrono::seconds(log.flush_period > 0 ? log.flush_period : 2);
  start_flush_timer(file_sink);
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

void log_v2::load(const std::shared_ptr<asio::io_context>& io_context) {
  if (!_instance)
    _instance = std::make_unique<log_v2>(io_context);
}
