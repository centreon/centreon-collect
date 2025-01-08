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

#include "com/centreon/common/http/http_server.hh"

#include "com/centreon/engine/commands/otel_interface.hh"

#include "centreon_agent/agent_reverse_client.hh"
#include "host_serv_extractor.hh"
#include "otl_check_result_builder.hh"
#include "otl_config.hh"

namespace com::centreon::engine::modules::opentelemetry {

using host_serv_metric = commands::otel::host_serv_metric;
namespace http = com::centreon::common::http;

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
  std::shared_ptr<otl_server> _otl_server;
  std::shared_ptr<http::server> _telegraf_conf_server;
  std::unique_ptr<centreon_agent::agent_reverse_client> _agent_reverse_client;

  using cmd_line_to_extractor_map =
      absl::btree_map<std::string, std::shared_ptr<host_serv_extractor>>;
  cmd_line_to_extractor_map _extractors;
  std::string _config_file_path;
  std::unique_ptr<otl_config> _conf;
  std::shared_ptr<spdlog::logger> _logger;

  std::shared_ptr<asio::io_context> _io_context;
  mutable std::mutex _protect;

  centreon_agent::agent_stat::pointer _agent_stats;

  void _forward_to_broker(const std::vector<otl_data_point>& unknown);

  void _create_telegraf_conf_server(
      const telegraf::conf_server_config::pointer& conf);

 protected:
  virtual void _create_otl_server(
      const grpc_config::pointer& server_conf,
      const centreon_agent::agent_config::pointer& agent_conf);
  void _reload();
  void _shutdown();

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
};

}  // namespace com::centreon::engine::modules::opentelemetry

#endif
