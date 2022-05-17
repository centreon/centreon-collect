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
#ifndef CENTREON_BROKER_CORE_INC_COM_CENTREON_BROKER_LOG_V2_HH_
#define CENTREON_BROKER_CORE_INC_COM_CENTREON_BROKER_LOG_V2_HH_

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

class log_v2 {
  std::string _log_name;
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

  std::array<std::shared_ptr<spdlog::logger>, 17> _log;
  std::atomic_bool _running;
  std::mutex _load_m;

  log_v2();
  ~log_v2();

 public:
  static log_v2& instance();
  void apply(const config::state& conf);
  const std::string& log_name() const;

  static std::shared_ptr<spdlog::logger> bam();
  static std::shared_ptr<spdlog::logger> bbdo();
  static std::shared_ptr<spdlog::logger> config();
  static std::shared_ptr<spdlog::logger> core();
  static std::shared_ptr<spdlog::logger> influxdb();
  static std::shared_ptr<spdlog::logger> graphite();
  static std::shared_ptr<spdlog::logger> notification();
  static std::shared_ptr<spdlog::logger> rrd();
  static std::shared_ptr<spdlog::logger> stats();
  static std::shared_ptr<spdlog::logger> lua();
  static std::shared_ptr<spdlog::logger> neb();
  static std::shared_ptr<spdlog::logger> perfdata();
  static std::shared_ptr<spdlog::logger> processing();
  static std::shared_ptr<spdlog::logger> sql();
  static std::shared_ptr<spdlog::logger> tcp();
  static std::shared_ptr<spdlog::logger> tls();
  static std::shared_ptr<spdlog::logger> grpc();
  static bool contains_logger(const std::string& logger);
  static bool contains_level(const std::string& level);
};

CCB_END();

#endif  // CENTREON_BROKER_CORE_INC_COM_CENTREON_BROKER_LOG_V2_HH_
