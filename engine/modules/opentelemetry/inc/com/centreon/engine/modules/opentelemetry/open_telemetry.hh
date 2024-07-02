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
#include "data_point_fifo_container.hh"
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
  asio::system_timer _second_timer;
  std::shared_ptr<otl_server> _otl_server;
  std::shared_ptr<http::server> _telegraf_conf_server;
  std::unique_ptr<centreon_agent::agent_reverse_client> _agent_reverse_client;

  using cmd_line_to_extractor_map =
      absl::btree_map<std::string, std::shared_ptr<host_serv_extractor>>;
  cmd_line_to_extractor_map _extractors;
  data_point_fifo_container _fifo;
  std::string _config_file_path;
  std::unique_ptr<otl_config> _conf;
  std::shared_ptr<spdlog::logger> _logger;

  struct host_serv_getter {
    using result_type = host_serv;
    const result_type& operator()(
        const std::shared_ptr<otl_check_result_builder>& node) const {
      return node->get_host_serv();
    }
  };

  struct time_out_getter {
    using result_type = std::chrono::system_clock::time_point;
    result_type operator()(
        const std::shared_ptr<otl_check_result_builder>& node) const {
      return node->get_time_out();
    }
  };

  /**
   * @brief when check can't return data right now, we have no metrics in fifo,
   * converter is stored in this container. It's indexed by host,serv and by
   * timeout
   *
   */
  using waiting_converter = boost::multi_index::multi_index_container<
      std::shared_ptr<otl_check_result_builder>,
      boost::multi_index::indexed_by<
          boost::multi_index::hashed_non_unique<host_serv_getter>,
          boost::multi_index::ordered_non_unique<time_out_getter>>>;

  waiting_converter _waiting;

  std::shared_ptr<asio::io_context> _io_context;
  mutable std::mutex _protect;

  void _forward_to_broker(const std::vector<otl_data_point>& unknown);

  void _second_timer_handler();

  void _create_telegraf_conf_server(
      const telegraf::conf_server_config::pointer& conf);

 protected:
  virtual void _create_otl_server(
      const grpc_config::pointer& server_conf,
      const centreon_agent::agent_config::pointer& agent_conf);
  void _on_metric(const metric_request_ptr& metric);
  void _reload();
  void _start_second_timer();
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

  bool check(const std::string& processed_cmd,
             const std::shared_ptr<commands::otel::check_result_builder_config>&
                 conv_conf,
             uint64_t command_id,
             nagios_macros& macros,
             uint32_t timeout,
             commands::result& res,
             commands::otel::result_callback&& handler) override;

  std::shared_ptr<commands::otel::host_serv_extractor> create_extractor(
      const std::string& cmdline,
      const commands::otel::host_serv_list::pointer& host_serv_list) override;

  std::shared_ptr<commands::otel::check_result_builder_config>
  create_check_result_builder_config(const std::string& cmd_line) override;
};

}  // namespace com::centreon::engine::modules::opentelemetry

#endif
