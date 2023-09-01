/**
* Copyright 2011-2013,2017 Centreon
*
* This file is part of Centreon Engine.
*
* Centreon Engine is free software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* as published by the Free Software Foundation.
*
* Centreon Engine is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Centreon Engine. If not, see
* <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/configuration/applier/connector.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/macros/misc.hh"
#include "com/centreon/engine/macros/process.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine::configuration;
using com::centreon::common::log_v3::log_v3;

/**
 * @brief Add new connector.
 *
 * @param obj The new connector to add into the monitoring engine.
 */
void applier::connector::add_object(const configuration::Connector& obj) {
  // Logging.
  auto logger = log_v3::instance().get(common::log_v3::log_v2_configuration);
  logger->debug("Creating new connector '{}'.", obj.connector_name());

  // Expand command line.
  nagios_macros* macros = get_global_macros();
  std::string command_line;
  process_macros_r(macros, obj.connector_line(), command_line, 0);

  // Add connector to the global configuration set.
  auto* cfg_cnn = pb_config.add_connectors();
  cfg_cnn->CopyFrom(obj);

  // Create connector.
  auto cmd = std::make_shared<commands::connector>(
      obj.connector_name(), command_line, &checks::checker::instance());
  commands::connector::connectors[obj.connector_name()] = cmd;
}

/**
 *  Add new connector.
 *
 *  @param[in] obj  The new connector to add into the monitoring engine.
 */
void applier::connector::add_object(configuration::connector const& obj) {
  // Logging.
  auto logger = log_v3::instance().get(common::log_v3::log_v2_configuration);
  logger->debug("Creating new connector '{}'.", obj.connector_name());

  // Expand command line.
  nagios_macros* macros(get_global_macros());
  std::string command_line;
  process_macros_r(macros, obj.connector_line(), command_line, 0);
  std::string processed_cmd(command_line);

  // Add connector to the global configuration set.
  config->connectors().insert(obj);

  // Create connector.
  auto cmd = std::make_shared<commands::connector>(
      obj.connector_name(), processed_cmd, &checks::checker::instance());
  commands::connector::connectors[obj.connector_name()] = cmd;
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
  auto logger = log_v3::instance().get(common::log_v3::log_v2_configuration);
  logger->debug("Modifying connector '{}'.", new_obj.connector_name());

  // Find connector object.
  connector_map::iterator it_obj(
      commands::connector::connectors.find(new_obj.connector_name()));
  if (it_obj == commands::connector::connectors.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing connector object '{}'",
        new_obj.connector_name());

  commands::connector* c = it_obj->second.get();

  // Update the global configuration set.
  to_modify->CopyFrom(new_obj);

  // Expand command line.
  nagios_macros* macros(get_global_macros());
  std::string command_line;
  process_macros_r(macros, new_obj.connector_line(), command_line, 0);

  // Set the new command line.
  c->set_command_line(command_line);
}

/**
 *  Modify connector.
 *
 *  @param[in] obj  The connector to modify in the monitoring engine.
 */
void applier::connector::modify_object(configuration::connector const& obj) {
  // Logging.
  auto logger = log_v3::instance().get(common::log_v3::log_v2_configuration);
  logger->debug("Modifying connector '{}'.", obj.connector_name());

  // Find old configuration.
  set_connector::iterator it_cfg(config->connectors_find(obj.key()));
  if (it_cfg == config->connectors().end())
    throw(engine_error() << "Cannot modify non-existing connector '"
                         << obj.connector_name() << "'");

  // Find connector object.
  connector_map::iterator it_obj(
      commands::connector::connectors.find(obj.key()));
  if (it_obj == commands::connector::connectors.end())
    throw(engine_error() << "Could not modify non-existing "
                         << "connector object '" << obj.connector_name()
                         << "'");
  commands::connector* c(it_obj->second.get());

  // Update the global configuration set.
  config->connectors().erase(it_cfg);
  config->connectors().insert(obj);

  // Expand command line.
  nagios_macros* macros(get_global_macros());
  std::string command_line;
  process_macros_r(macros, obj.connector_line(), command_line, 0);
  std::string processed_cmd(command_line);

  // Set the new command line.
  c->set_command_line(processed_cmd);
}

void applier::connector::remove_object(ssize_t idx) {
  // Logging.
  const configuration::Connector& obj = pb_config.connectors()[idx];
  auto logger = log_v3::instance().get(common::log_v3::log_v2_configuration);
  logger->debug("Removing connector '{}'.", obj.connector_name());

  // Find connector.
  connector_map::iterator it =
      commands::connector::connectors.find(obj.connector_name());
  if (it != commands::connector::connectors.end()) {
    // Remove connector object.
    commands::connector::connectors.erase(it);
  }

  // Remove connector from the global configuration set.
  pb_config.mutable_connectors()->DeleteSubrange(idx, 1);
}

/**
 *  Remove old connector.
 *
 *  @param[in] obj  The new connector to remove from the monitoring
 *                  engine.
 */
void applier::connector::remove_object(configuration::connector const& obj) {
  // Logging.
  auto logger = log_v3::instance().get(common::log_v3::log_v2_configuration);
  logger->debug("Removing connector '{}'.", obj.connector_name());

  // Find connector.
  connector_map::iterator it(commands::connector::connectors.find(obj.key()));
  if (it != commands::connector::connectors.end()) {
    // Remove connector object.
    commands::connector::connectors.erase(it);
  }

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
void applier::connector::resolve_object(const configuration::Connector&) {}

/**
 *  @brief Resolve a connector.
 *
 *  Connector objects do not need resolution. Therefore this method does
 *  nothing.
 *
 *  @param[in] obj Unused.
 */
void applier::connector::resolve_object(const configuration::connector&) {}
