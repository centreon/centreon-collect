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
#ifndef CENTREON_BROKER_CORE_INC_COM_CENTREON_BROKER_LOG_V2_HH_
#define CENTREON_BROKER_CORE_INC_COM_CENTREON_BROKER_LOG_V2_HH_

#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/namespace.hh"
#include "com/centreon/engine/log_v2_base.hh"

CCB_BEGIN()

class log_v2 : public com::centreon::engine::log_v2_base {
  std::array<std::shared_ptr<spdlog::logger>, 17> _log;
  std::atomic_bool _running;
  asio::system_timer _flush_timer;
  std::mutex _flush_timer_m;
  bool _flush_timer_active;
  std::shared_ptr<asio::io_context> _io_context;

  static std::shared_ptr<log_v2> _instance;

  enum logger {
    log_bam,
    log_bbdo,
    log_config,
    log_core,
    log_graphite,
    log_grpc,
    log_influxdb,
    log_lua,
    log_neb,
    log_notification,
    log_perfdata,
    log_processing,
    log_rrd,
    log_sql,
    log_stats,
    log_tcp,
    log_tls,
  };

  std::mutex _load_m;

  log_v2(const std::shared_ptr<asio::io_context>& io_context);

  std::shared_ptr<spdlog::logger> get_logger(logger log_type,
                                             const char* log_str);

  void start_flush_timer(spdlog::sink_ptr sink);

 public:
  ~log_v2() noexcept;

  void stop_flush_timer();
  void apply(const config::state& conf);

  static void load(const std::shared_ptr<asio::io_context>& io_context);

  static std::shared_ptr<log_v2> instance();
  void set_flush_interval(unsigned second_flush_interval);

  static inline std::shared_ptr<spdlog::logger> bam() {
    return _instance->get_logger(log_bam, "bam");
  }

  static inline std::shared_ptr<spdlog::logger> bbdo() {
    return _instance->get_logger(log_bbdo, "bbdo");
  }

  static inline std::shared_ptr<spdlog::logger> config() {
    return _instance->get_logger(log_config, "config");
  }

  static inline std::shared_ptr<spdlog::logger> core() {
    return _instance->get_logger(log_core, "core");
  }

  static inline std::shared_ptr<spdlog::logger> influxdb() {
    return _instance->get_logger(log_influxdb, "influxdb");
  }

  static inline std::shared_ptr<spdlog::logger> graphite() {
    return _instance->get_logger(log_graphite, "graphite");
  }

  static inline std::shared_ptr<spdlog::logger> notification() {
    return _instance->get_logger(log_notification, "notification");
  }

  static inline std::shared_ptr<spdlog::logger> rrd() {
    return _instance->get_logger(log_rrd, "rrd");
  }

  static inline std::shared_ptr<spdlog::logger> stats() {
    return _instance->get_logger(log_stats, "stats");
  }

  static inline std::shared_ptr<spdlog::logger> lua() {
    return _instance->get_logger(log_lua, "lua");
  }

  static inline std::shared_ptr<spdlog::logger> neb() {
    return _instance->get_logger(log_neb, "neb");
  }

  static inline std::shared_ptr<spdlog::logger> perfdata() {
    return _instance->get_logger(log_perfdata, "perfdata");
  }

  static inline std::shared_ptr<spdlog::logger> processing() {
    return _instance->get_logger(log_processing, "processing");
  }

  static inline std::shared_ptr<spdlog::logger> sql() {
    return _instance->get_logger(log_sql, "sql");
  }

  static inline std::shared_ptr<spdlog::logger> tcp() {
    return _instance->get_logger(log_tcp, "tcp");
  }

  static inline std::shared_ptr<spdlog::logger> tls() {
    return _instance->get_logger(log_tls, "tls");
  }

  static inline std::shared_ptr<spdlog::logger> grpc() {
    return _instance->get_logger(log_grpc, "grpc");
  }

  static bool contains_logger(const std::string& logger);
  static bool contains_level(const std::string& level);
  std::vector<std::pair<std::string, std::string>> levels() const;
  void set_level(const std::string& logger, const std::string& level);
};

CCB_END();

#endif  // CENTREON_BROKER_CORE_INC_COM_CENTREON_BROKER_LOG_V2_HH_
