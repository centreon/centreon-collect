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
#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/otel_connector.hh"
#include "com/centreon/engine/configuration/applier/connector.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros/misc.hh"
#include "com/centreon/engine/macros/process.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::engine::configuration;

constexpr std::string_view _otel_fake_exe("opentelemetry");

/**
 * @brief Add new connector.
 *
 * @param obj The new connector to add into the monitoring engine.
 */
void applier::connector::add_object(const configuration::Connector& obj) {
  // Logging.
  config_logger->debug("Creating new connector '{}'.", obj.connector_name());

  // Expand command line.
  nagios_macros* macros = get_global_macros();
  std::string command_line;
  process_macros_r(macros, obj.connector_line(), command_line, 0);

  // Add connector to the global configuration set.
  auto* cfg_cnn = pb_config.add_connectors();
  cfg_cnn->CopyFrom(obj);

  // Create connector.
  boost::trim(command_line);

  // If executable connector path ends with opentelemetry, it's a fake
  // opentelemetry connector.
  size_t end_path = command_line.find(' ');
  size_t otel_pos = command_line.find(_otel_fake_exe);

  if (otel_pos < end_path) {
    commands::otel_connector::create(
        obj.connector_name(),
        boost::algorithm::trim_copy(
            command_line.substr(otel_pos + _otel_fake_exe.length())),
        &checks::checker::instance());
  } else {
    auto cmd = std::make_shared<commands::connector>(
        obj.connector_name(), command_line, &checks::checker::instance());
    commands::connector::connectors[obj.connector_name()] = cmd;
  }
}

/**
 *  @brief Expand connector.
 *
 *  Connector configuration objects do not need expansion. Therefore
 *  this method only copy obj to expanded.
 *
 *  @param[in] s  Unused.
 */
void applier::connector::expand_objects(configuration::State& s
                                        [[maybe_unused]]) {}

/**
 * @brief Modify connector
 *
 * @param to_modify The current configuration connector
 * @param new_obj The new one.
 */
void applier::connector::modify_object(
    configuration::Connector* to_modify,
    const configuration::Connector& new_obj) {
  // Logging.
  config_logger->debug("Modifying connector '{}'.", new_obj.connector_name());

  // Expand command line.
  nagios_macros* macros(get_global_macros());
  std::string command_line;
  process_macros_r(macros, new_obj.connector_line(), command_line, 0);

  boost::trim(command_line);

  // if executable connector path ends with opentelemetry, it's a fake
  // opentelemetry connector
  size_t end_path = command_line.find(' ');
  size_t otel_pos = command_line.find(_otel_fake_exe);

  connector_map::iterator current_connector(
      commands::connector::connectors.find(new_obj.connector_name()));

  if (otel_pos < end_path) {
    if (!commands::otel_connector::update(new_obj.connector_name(),
                                          command_line)) {
      // connector object becomes an otel fake connector
      if (current_connector != commands::connector::connectors.end()) {
        commands::connector::connectors.erase(current_connector);
        commands::otel_connector::create(new_obj.connector_name(), command_line,
                                         &checks::checker::instance());
      } else {
        throw com::centreon::exceptions::msg_fmt(
            "unknown open telemetry command to update: {}",
            new_obj.connector_name());
      }
    }
  } else {
    if (current_connector != commands::connector::connectors.end()) {
      // Set the new command line.
      current_connector->second->set_command_line(command_line);
    } else {
      // old otel_connector => connector
      if (commands::otel_connector::remove(new_obj.connector_name())) {
        auto cmd = std::make_shared<commands::connector>(
            new_obj.connector_name(), command_line,
            &checks::checker::instance());
        commands::connector::connectors[new_obj.connector_name()] = cmd;

      } else {
        throw com::centreon::exceptions::msg_fmt(
            "unknown connector to update: {}", new_obj.connector_name());
      }
    }
  }

  // Update the global configuration set.
  to_modify->CopyFrom(new_obj);
}

void applier::connector::remove_object(ssize_t idx) {
  // Logging.
  const configuration::Connector& obj = pb_config.connectors()[idx];
  config_logger->debug("Removing connector '{}'.", obj.connector_name());

  // Find connector.
  connector_map::iterator it =
      commands::connector::connectors.find(obj.connector_name());
  if (it != commands::connector::connectors.end()) {
    // Remove connector object.
    commands::connector::connectors.erase(it);
  }

  commands::otel_connector::remove(obj.connector_name());

  // Remove connector from the global configuration set.
  pb_config.mutable_connectors()->DeleteSubrange(idx, 1);
}

/**
 *  @brief Resolve a connector.
 *
 *  Connector objects do not need resolution. Therefore this method does
 *  nothing.
 *
 *  @param[in] obj Unused.
 */
void applier::connector::resolve_object(const configuration::Connector&,
                                        error_cnt& err [[maybe_unused]]) {}
