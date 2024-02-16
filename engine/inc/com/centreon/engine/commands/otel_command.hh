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

#ifndef CCE_COMMANDS_OTEL_COMMAND_HH
#define CCE_COMMANDS_OTEL_COMMAND_HH

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
class otel_command : public command,
                     public std::enable_shared_from_this<otel_command> {
 public:
  using otel_command_container =
      absl::flat_hash_map<std::string, std::shared_ptr<otel_command>>;

 private:
  static otel_command_container _commands;

  std::shared_ptr<otel::host_serv_extractor> _extractor;

  void init();
    void reset_extractor();

 public:
  static void create(const std::string& connector_name,
                     const std::string& cmd_line,
                     commands::command_listener* listener);

  static bool remove(const std::string& connector_name);

  static bool update(const std::string& connector_name,
                     const std::string& cmd_line);

  static std::shared_ptr<otel_command> get_otel_command(
      const std::string& connector_name);

  static void clear();

  static void init_all();
  static void reset_all_extractor();

  static const otel_command_container& get_otel_commands() { return _commands; }

  otel_command(const std::string& connector_name,
               const std::string& cmd_line,
               commands::command_listener* listener);

  void update(const std::string& cmd_line);

  virtual uint64_t run(const std::string& processed_cmd,
                       nagios_macros& macros,
                       uint32_t timeout,
                       const check_result::pointer& to_push_to_checker,
                       const void* caller = nullptr) override;

  virtual void run(const std::string& process_cmd,
                   nagios_macros& macros,
                   uint32_t timeout,
                   result& res) override;
};

}  // namespace com::centreon::engine::commands

#endif
