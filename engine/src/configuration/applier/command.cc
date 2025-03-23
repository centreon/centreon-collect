/**
 * Copyright 2011-2013,2017-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/forward.hh"
#include "com/centreon/engine/commands/otel_connector.hh"
#include "com/centreon/engine/commands/raw.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

/**
 *  Add new command.
 *
 *  @param[in] obj  The new command to add into the monitoring engine.
 */
void applier::command::add_object(const configuration::Command& obj) {
  // Logging.
  config_logger->debug("Creating new command '{}'.", obj.command_name());

  // Add command to the global configuration set.
  auto* cmd = pb_indexed_config.state().add_commands();
  cmd->CopyFrom(obj);

  if (obj.connector().empty()) {
    auto raw = std::make_shared<commands::raw>(
        obj.command_name(), obj.command_line(), &checks::checker::instance());
    commands::command::commands[raw->get_name()] = std::move(raw);
  } else {
    connector_map::iterator found_con{
        commands::connector::connectors.find(obj.connector())};
    if (found_con != commands::connector::connectors.end() &&
        found_con->second) {
      std::shared_ptr<commands::forward> forward{
          std::make_shared<commands::forward>(
              obj.command_name(), obj.command_line(), found_con->second)};
      commands::command::commands[forward->get_name()] = forward;
    } else {
      std::shared_ptr<commands::otel_connector> otel_cmd =
          commands::otel_connector::get_otel_connector(obj.connector());
      if (otel_cmd) {
        std::shared_ptr<commands::forward> forward{
            std::make_shared<commands::forward>(obj.command_name(),
                                                obj.command_line(), otel_cmd)};
        commands::command::commands[forward->get_name()] = forward;
      } else {
        throw engine_error() << fmt::format(
            "Could not register command '{}': unable to find '{}'",
            obj.command_name(), obj.connector());
      }
    }
  }
}

/**
 *  @brief Expand command.
 *
 *  Command configuration objects do not need expansion. Therefore this
 *  method does nothing.
 *
 *  @param[in] s  Unused.
 */
void applier::command::expand_objects(configuration::State& s
                                      [[maybe_unused]]) {}

/**
 * @brief Modify command.
 *
 * @param obj The new command protobuf configuration for the object to modify
 * in the monitoring engine.
 */
void applier::command::modify_object(configuration::Command* to_modify,
                                     const configuration::Command& new_obj) {
  // Logging.
  config_logger->debug("Modifying command '{}'.", new_obj.command_name());

  // Find command object.
  command_map::iterator it_obj =
      commands::command::commands.find(new_obj.command_name());
  if (it_obj == commands::command::commands.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing command object '{}'",
        new_obj.command_name());

  // Update the global configuration set.
  to_modify->CopyFrom(new_obj);

  // Command will be temporarily removed from the command set but
  // will be added back right after with _create_command. This does
  // not create dangling pointers since commands::command object are
  // not referenced anywhere, only ::command objects are.
  //  commands::command::commands.erase(obj.command_name());
  if (new_obj.connector().empty()) {
    auto raw = std::make_shared<commands::raw>(new_obj.command_name(),
                                               new_obj.command_line(),
                                               &checks::checker::instance());
    it_obj->second = raw;
  } else {
    connector_map::iterator found_con{
        commands::connector::connectors.find(new_obj.connector())};
    if (found_con != commands::connector::connectors.end() &&
        found_con->second) {
      std::shared_ptr<commands::forward> forward{
          std::make_shared<commands::forward>(new_obj.command_name(),
                                              new_obj.command_line(),
                                              found_con->second)};
      it_obj->second = forward;
    } else {
      std::shared_ptr<commands::otel_connector> otel_cmd =
          commands::otel_connector::get_otel_connector(new_obj.connector());
      if (otel_cmd) {
        std::shared_ptr<commands::forward> forward{
            std::make_shared<commands::forward>(
                new_obj.command_name(), new_obj.command_line(), otel_cmd)};
        it_obj->second = forward;
      } else {
        throw engine_error() << fmt::format(
            "Could not register command '{}': unable to find '{}'",
            new_obj.command_name(), new_obj.connector());
      }
    }
  }
}

/**
 * @brief Remove a protobuf command configuration at index idx
 *
 * @param idx The position in configuration of the configuration to remove.
 */
template <>
void applier::command::remove_object(const std::pair<ssize_t, std::string>& p) {
  const configuration::Command& obj =
      pb_indexed_config.state().commands()[p.first];
  // Logging.
  config_logger->debug("Removing command '{}'.", obj.command_name());

  // Find command.
  std::unordered_map<std::string, std::shared_ptr<commands::command> >::iterator
      it = commands::command::commands.find(obj.command_name());
  if (it != commands::command::commands.end()) {
    // Erase command (will effectively delete the object).
    commands::command::commands.erase(it);
  } else
    throw engine_error() << fmt::format(
        "Could not remove command '{}': it does not exist", obj.command_name());

  // Remove command from the global configuration set.
  pb_indexed_config.state().mutable_commands()->DeleteSubrange(p.first, 1);
}

/**
 *  @brief Resolve command.
 *
 *  This method will check for its connector's existence, if command is
 *  configured to use one.
 *
 *  @param[in] obj  Command object.
 */
void applier::command::resolve_object(const configuration::Command& obj,
                                      error_cnt& err [[maybe_unused]]) {
  if (!obj.connector().empty()) {
    connector_map::iterator found =
        commands::connector::connectors.find(obj.connector());
    if (found == commands::connector::connectors.end() || !found->second) {
      if (!commands::otel_connector::get_otel_connector(obj.connector()))
        throw engine_error() << "unknow command " << obj.connector();
    }
  }
}
