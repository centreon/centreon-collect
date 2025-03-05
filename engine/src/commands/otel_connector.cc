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

#include "com/centreon/engine/commands/otel_connector.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine::commands;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 * @brief static list of all otel_connector
 *
 */
absl::flat_hash_map<std::string, std::shared_ptr<otel_connector>>
    otel_connector::_commands;

/**
 * @brief create an otel_connector
 *
 * @param connector_name
 * @param cmd_line
 * @param listener
 */
std::shared_ptr<otel_connector> otel_connector::create(
    const std::string& connector_name,
    const std::string& cmd_line,
    commands::command_listener* listener) {
  std::shared_ptr<otel_connector> cmd(
      std::make_shared<otel_connector>(connector_name, cmd_line, listener));
  auto iter_res = _commands.emplace(connector_name, cmd);
  if (!iter_res.second) {
    iter_res.first->second = cmd;
  }
  return cmd;
}

/**
 * @brief remove otel command from static list _commands
 *
 * @param connector_name
 * @return true
 * @return false
 */
bool otel_connector::remove(const std::string& connector_name) {
  return _commands.erase(connector_name);
}

/**
 * @brief update open telemetry extractor
 *
 * @param connector_name
 * @param cmd_line new command line
 * @return true the otel command was found and hist update method was called
 * @return false the otel command doesn't exist
 */
bool otel_connector::update(const std::string& connector_name,
                            const std::string& cmd_line) {
  auto search = _commands.find(connector_name);
  if (search == _commands.end()) {
    return false;
  }
  search->second->update(cmd_line);
  return true;
}

/**
 * @brief get otel command from connector name
 *
 * @param connector_name
 * @return std::shared_ptr<otel_connector>
 */
std::shared_ptr<otel_connector> otel_connector::get_otel_connector(
    const std::string& connector_name) {
  auto search = _commands.find(connector_name);
  return search != _commands.end() ? search->second
                                   : std::shared_ptr<otel_connector>();
}

/**
 * @brief get otel command that is used by host serv
 * Caution: This function must be called from engine main thread
 *
 * @param host
 * @param serv
 * @return std::shared_ptr<otel_connector> null if not found
 */
std::shared_ptr<otel_connector>
otel_connector::get_otel_connector_from_host_serv(
    const std::string_view& host,
    const std::string_view& serv) {
  for (const auto& name_to_conn : _commands) {
    if (name_to_conn.second->_host_serv_list->contains(host, serv)) {
      return name_to_conn.second;
    }
  }
  return {};
}

/**
 * @brief erase all otel commands
 *
 */
void otel_connector::clear() {
  _commands.clear();
}

/**
 * @brief to call once otel module have been loaded
 *
 */
void otel_connector::init_all() {
  for (auto& to_init : _commands) {
    to_init.second->init();
  }
}

/**
 * @brief Construct a new otel command::otel command object
 * at engine startup, otel module is not yet loaded
 * So we can't init some members
 *
 * @param connector_name
 * @param cmd_line
 * @param listener
 */
otel_connector::otel_connector(const std::string& connector_name,
                               const std::string& cmd_line,
                               commands::command_listener* listener)
    : command(connector_name, cmd_line, listener, e_type::otel),
      _host_serv_list(std::make_shared<otel::host_serv_list>()),
      _logger(log_v2::instance().get(log_v2::OTL)) {
  init();
}

/**
 * @brief
 *
 * @param cmd_line
 */
void otel_connector::update(const std::string& cmd_line) {
  if (get_command_line() == cmd_line) {
    return;
  }
  set_command_line(cmd_line);
  init();
}

/**
 * @brief asynchronous check
 * this use the engine callback engine, so there is not callback passed in
 * parameter, we use update_result_cache and _listener->finished instead
 * @param processed_cmd
 * @param macros
 * @param timeout
 * @param to_push_to_checker
 * @param caller
 * @return uint64_t
 */
uint64_t otel_connector::run(const std::string& processed_cmd [[maybe_unused]],
                             nagios_macros& macros [[maybe_unused]],
                             uint32_t timeout [[maybe_unused]],
                             const check_result::pointer& to_push_to_checker
                             [[maybe_unused]],
                             const void* caller [[maybe_unused]]) {
  SPDLOG_LOGGER_ERROR(_logger, "open telemetry services must be passive");
  throw exceptions::msg_fmt("open telemetry services must be passive");
}

/**
 * @brief emulate an synchronous check run
 * as all is asynchronous, we use a condition variable to emulate synchronous
 * mode
 *
 * @param processed_cmd
 * @param macros
 * @param timeout timeout in seconds
 * @param res check result
 */
void otel_connector::run(const std::string& processed_cmd [[maybe_unused]],
                         nagios_macros& macros [[maybe_unused]],
                         uint32_t timeout [[maybe_unused]],
                         result& res [[maybe_unused]]) {
  SPDLOG_LOGGER_ERROR(_logger, "open telemetry services must be passive");
  throw exceptions::msg_fmt("open telemetry services must be passive");
}

/**
 * @brief convert opentelemetry datas in check_result and post it to
 * checks::checker::instance() Caution, this function must be called from engine
 * main thread
 *
 * @param host
 * @param serv empty if result of host check
 * @param data_pts opentelemetry data points
 */
void otel_connector::process_data_pts(
    const std::string_view& host,
    const std::string_view& serv,
    const com::centreon::engine::modules::opentelemetry::metric_to_datapoints&
        data_pts) {
  _check_result_builder->process_data_pts(host, serv, data_pts);
}

/**
 * @brief when this object is created, otel module may not have been loaded, so
 * we have to init later this method is called by constructor and is effective
 * if we are not in startup otherwise, it's called by open_telemetry object at
 * otel module loading
 *
 */
void otel_connector::init() {
  try {
    if (!_extractor) {
      std::shared_ptr<otel::open_telemetry_base> otel =
          otel::open_telemetry_base::instance();
      if (otel) {
        _extractor =
            otel->create_extractor(get_command_line(), _host_serv_list);
      }
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_TRACE(_logger,
                        "fail to parse host serv extractor configuration for "
                        "open-telemetry connector {} command line:{}",
                        get_name(), get_command_line());
  }
  try {
    if (!_check_result_builder) {
      std::shared_ptr<otel::open_telemetry_base> otel =
          otel::open_telemetry_base::instance();
      if (otel) {
        _check_result_builder =
            otel->create_check_result_builder(get_command_line());
      }
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_TRACE(_logger,
                        "fail to parse converter configuration for "
                        "open-telemetry connector {} command line:{}",
                        get_name(), get_command_line());
  }
}

/**
 * @brief add a host service allowed by the extractor of the connector
 *
 * @param host
 * @param service_description empty if host command
 */
void otel_connector::register_host_serv(
    const std::string& host,
    const std::string& service_description) {
  _host_serv_list->register_host_serv(host, service_description);
}

/**
 * @brief remove a host service from host service allowed by the extractor of
 * the connector
 *
 * @param host
 * @param service_description empty if host command
 */
void otel_connector::unregister_host_serv(
    const std::string& host,
    const std::string& service_description) {
  _host_serv_list->remove(host, service_description);
}
