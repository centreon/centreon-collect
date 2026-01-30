/**
 * Copyright 2024 Centreon
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
#ifndef CCE_MOD_OTL_SERVER_OPENTELEMETRY_HH
#define CCE_MOD_OTL_SERVER_OPENTELEMETRY_HH

#include <absl/base/thread_annotations.h>
#include <boost/asio/system_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include "com/centreon/common/http/http_server.hh"

#include "com/centreon/engine/commands/otel_interface.hh"

#include "centreon_agent/agent_reverse_client.hh"
#include "common/crypto/cert_tree.hh"
#include "host_serv_extractor.hh"
#include "otl_check_result_builder.hh"
#include "otl_config.hh"

namespace com::centreon::engine::modules::opentelemetry {

using host_serv_metric = commands::otel::host_serv_metric;
namespace http = com::centreon::common::http;
namespace crypto = com::centreon::common::crypto;

class otl_server;

/**
 * @brief This class is the main high-level class of the otel module.
 * It needs a json configuration file whose path is passed on loading, and an
 * io_context for the second timer. The two methods used at runtime are
 * create_extractor() and check(). a second period timer is also used to process
 * check timeouts
 * All attributes are (timers, _conf, _otl_server) are protected by _protect
 * mutex
 *
 */
class open_telemetry : public commands::otel::open_telemetry_base {
  std::shared_ptr<otl_server> _otl_server ABSL_GUARDED_BY(_protect);
  std::chrono::system_clock::time_point _certificate_ttl
      ABSL_GUARDED_BY(_protect);

  std::unique_ptr<crypto::cert_tree> _server_ca ABSL_GUARDED_BY(_protect);

  std::filesystem::file_time_type _ca_mtime;
  std::filesystem::file_time_type _ca_key_mtime;

  asio::system_timer _minute_timer ABSL_GUARDED_BY(_protect);

  std::shared_ptr<http::server> _telegraf_conf_server;
  std::unique_ptr<centreon_agent::agent_reverse_client> _agent_reverse_client;

  using cmd_line_to_extractor_map =
      absl::btree_map<std::string, std::shared_ptr<host_serv_extractor>>;
  cmd_line_to_extractor_map _extractors ABSL_GUARDED_BY(_protect);
  std::string _config_file_path;
  std::unique_ptr<otl_config> _conf ABSL_GUARDED_BY(_protect);
  std::shared_ptr<spdlog::logger> _logger;

  std::shared_ptr<asio::io_context> _io_context;
  mutable absl::Mutex _protect;

  centreon_agent::agent_stat::pointer _agent_stats;

  void _forward_to_broker(const std::vector<otl_data_point>& unknown);

  void _create_telegraf_conf_server(
      const telegraf::conf_server_config::pointer& conf);

  void _start_minute_timer();
  void _minute_timer_handler();

 protected:
  virtual void _create_otl_server(
      const grpc_config::pointer& server_conf,
      const centreon_agent::agent_config::pointer& agent_conf)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);
  void _reload();
  void _shutdown() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);

  void _shutdown_otl_server() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);

 public:
  open_telemetry(const std::string_view config_file_path,
                 const std::shared_ptr<asio::io_context>& io_context,
                 const std::shared_ptr<spdlog::logger>& logger);

  std::shared_ptr<open_telemetry> shared_from_this() {
    return std::static_pointer_cast<open_telemetry>(
        commands::otel::open_telemetry_base::shared_from_this());
  }

  static std::shared_ptr<open_telemetry> load(
      const std::string_view& config_path,
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger);

  static void reload(const std::shared_ptr<spdlog::logger>& logger);

  static std::shared_ptr<open_telemetry> instance() {
    return std::static_pointer_cast<open_telemetry>(_instance);
  }

  static void unload(const std::shared_ptr<spdlog::logger>& logger);

  void on_metric(const metric_request_ptr& metric);

  std::shared_ptr<commands::otel::host_serv_extractor> create_extractor(
      const std::string& cmdline,
      const commands::otel::host_serv_list::pointer& host_serv_list) override;

  std::shared_ptr<commands::otel::otl_check_result_builder_base>
  create_check_result_builder(const std::string& cmdline) override;

  void force_check(uint64_t host_id, uint64_t serv_id) override;

  certificate_info get_otel_service_certificate_info() override;
};

}  // namespace com::centreon::engine::modules::opentelemetry

#endif
