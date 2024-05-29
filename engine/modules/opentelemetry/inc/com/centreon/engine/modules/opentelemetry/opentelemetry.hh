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

#include "com/centreon/engine/commands/otel_interface.hh"

#include "otl_data_point.hh"
#include "otl_config.hh"

namespace com::centreon::engine::modules::opentelemetry {

using host_serv_metric = commands::otel::host_serv_metric;

class otl_server;

/**
 * @brief This class is the high level main class of otel module
 * It needs a json config file witch path is passed to load and an io_context
 * for second timer
 * The two methods used during runtime is create_extractor and check
 *
 */
class open_telemetry : public commands::otel::open_telemetry_base {
  asio::system_timer _second_timer;
  std::shared_ptr<otl_server> _otl_server;

  std::string _config_file_path;
  std::unique_ptr<otl_config> _conf;
  std::shared_ptr<spdlog::logger> _logger;

  std::shared_ptr<asio::io_context> _io_context;
  mutable std::mutex _protect;

  void _forward_to_broker(const std::vector<data_point>& unknown);

  void _second_timer_handler();

 protected:
  virtual void _create_otl_server(const grpc_config::pointer& server_conf);
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
};

}  // namespace com::centreon::engine::modules::opentelemetry

#endif
