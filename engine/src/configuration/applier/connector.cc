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
 *  Add new connector.
 *
 *  @param[in] obj  The new connector to add into the monitoring engine.
 */
void applier::connector::add_object(configuration::connector const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new connector '" << obj.connector_name() << "'.";
  config_logger->debug("Creating new connector '{}'.", obj.connector_name());

  // Expand command line.
  nagios_macros* macros(get_global_macros());
  std::string command_line;
  process_macros_r(macros, obj.connector_line(), command_line, 0);
  std::string processed_cmd(command_line);

  // Add connector to the global configuration set.
  config->connectors().insert(obj);

  // Create connector.
  boost::trim(processed_cmd);

  // if executable connector path ends with opentelemetry, it's a fake
  // opentelemetry connector
  size_t end_path = processed_cmd.find(' ');
  size_t otel_pos = processed_cmd.find(_otel_fake_exe);

  if (otel_pos < end_path) {
    commands::otel_connector::create(
        obj.connector_name(),
        boost::algorithm::trim_copy(
            processed_cmd.substr(otel_pos + _otel_fake_exe.length())),
        &checks::checker::instance());
  } else {
    auto cmd = std::make_shared<commands::connector>(
        obj.connector_name(), processed_cmd, &checks::checker::instance());
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
void applier::connector::expand_objects(configuration::state& s) {
  (void)s;
}

/**
 *  Modify connector.
 *
 *  @param[in] obj  The connector to modify in the monitoring engine.
 */
void applier::connector::modify_object(configuration::connector const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Modifying connector '" << obj.connector_name() << "'.";
  config_logger->debug("Modifying connector '{}'.", obj.connector_name());

  // Find old configuration.
  set_connector::iterator it_cfg(config->connectors_find(obj.key()));
  if (it_cfg == config->connectors().end())
    throw(engine_error() << "Cannot modify non-existing connector '"
                         << obj.connector_name() << "'");

  // Expand command line.
  nagios_macros* macros(get_global_macros());
  std::string command_line;
  process_macros_r(macros, obj.connector_line(), command_line, 0);
  std::string processed_cmd(command_line);

  boost::trim(processed_cmd);

  // if executable connector path ends with opentelemetry, it's a fake
  // opentelemetry connector
  size_t end_path = processed_cmd.find(' ');
  size_t otel_pos = processed_cmd.find(_otel_fake_exe);

  connector_map::iterator exist_connector(
      commands::connector::connectors.find(obj.key()));

  if (otel_pos < end_path) {
    std::string otel_cmdline = boost::algorithm::trim_copy(
        processed_cmd.substr(otel_pos + _otel_fake_exe.length()));

    if (!commands::otel_connector::update(obj.key(), processed_cmd)) {
      // connector object become an otel fake connector
      if (exist_connector != commands::connector::connectors.end()) {
        commands::connector::connectors.erase(exist_connector);
        commands::otel_connector::create(obj.key(), processed_cmd,
                                         &checks::checker::instance());
      } else {
        throw com::centreon::exceptions::msg_fmt(
            "unknown open telemetry command to update: {}", obj.key());
      }
    }
  } else {
    if (exist_connector != commands::connector::connectors.end()) {
      // Set the new command line.
      exist_connector->second->set_command_line(processed_cmd);
    } else {
      // old otel_connector => connector
      if (commands::otel_connector::remove(obj.key())) {
        auto cmd = std::make_shared<commands::connector>(
            obj.connector_name(), processed_cmd, &checks::checker::instance());
        commands::connector::connectors[obj.connector_name()] = cmd;

      } else {
        throw com::centreon::exceptions::msg_fmt(
            "unknown connector to update: {}", obj.key());
      }
    }
  }

  // Update the global configuration set.
  config->connectors().erase(it_cfg);
  config->connectors().insert(obj);
}

/**
 *  Remove old connector.
 *
 *  @param[in] obj  The new connector to remove from the monitoring
 *                  engine.
 */
void applier::connector::remove_object(configuration::connector const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing connector '" << obj.connector_name() << "'.";
  config_logger->debug("Removing connector '{}'.", obj.connector_name());

  // Find connector.
  connector_map::iterator it(commands::connector::connectors.find(obj.key()));
  if (it != commands::connector::connectors.end()) {
    // Remove connector object.
    commands::connector::connectors.erase(it);
  }

  commands::otel_connector::remove(obj.key());

  // Remove connector from the global configuration set.
  config->connectors().erase(obj);
}

/**
 *  @brief Resolve a connector.
 *
 *  Connector objects do not need resolution. Therefore this method does
 *  nothing.
 *
 *  @param[in] obj Unused.
 */
void applier::connector::resolve_object(configuration::connector const& obj
                                        [[maybe_unused]],
                                        error_cnt& err [[maybe_unused]]) {}
