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

#ifndef CCE_COMMANDS_OTEL_CONNECTOR_HH
#define CCE_COMMANDS_OTEL_CONNECTOR_HH

#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/engine/commands/otel_interface.hh"

namespace com::centreon::engine::commands {

/***
 * This fake connector is used to convert otel metrics to result
 * Configuration is divided in 2 parts
 * Connector command line configure the object who extract host and service from
 * open telemetry request run command line configure converter who converts
 * data_points to result
 */
class otel_connector : public command {
  otel::host_serv_list::pointer _host_serv_list;

 public:
  using otel_connector_container =
      absl::flat_hash_map<std::string, std::shared_ptr<otel_connector>>;

 private:
  static otel_connector_container _commands;

  std::shared_ptr<otel::host_serv_extractor> _extractor;
  std::shared_ptr<otel::otl_check_result_builder_base> _check_result_builder;

  std::shared_ptr<spdlog::logger> _logger;

  void init();

 public:
  static std::shared_ptr<otel_connector> create(
      const std::string& connector_name,
      const std::string& cmd_line,
      commands::command_listener* listener);

  static bool remove(const std::string& connector_name);

  static bool update(const std::string& connector_name,
                     const std::string& cmd_line);

  static std::shared_ptr<otel_connector> get_otel_connector(
      const std::string& connector_name);

  static std::shared_ptr<otel_connector> get_otel_connector_from_host_serv(
      const std::string_view& host,
      const std::string_view& serv);

  static void clear();

  static void init_all();

  static const otel_connector_container& get_otel_connectors() {
    return _commands;
  }

  otel_connector(const std::string& connector_name,
                 const std::string& cmd_line,
                 commands::command_listener* listener);

  void update(const std::string& cmd_line);

  void process_data_pts(
      const std::string_view& host,
      const std::string_view& serv,
      const modules::opentelemetry::metric_to_datapoints& data_pts);

  virtual uint64_t run(const std::string& processed_cmd,
                       nagios_macros& macros,
                       uint32_t timeout,
                       const check_result::pointer& to_push_to_checker,
                       const void* caller = nullptr) override;

  virtual void run(const std::string& process_cmd,
                   nagios_macros& macros,
                   uint32_t timeout,
                   result& res) override;

  void register_host_serv(const std::string& host,
                          const std::string& service_description) override;

  void unregister_host_serv(const std::string& host,
                            const std::string& service_description) override;
};

}  // namespace com::centreon::engine::commands

#endif
