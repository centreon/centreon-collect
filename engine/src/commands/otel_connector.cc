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
void otel_connector::create(const std::string& connector_name,
                            const std::string& cmd_line,
                            commands::command_listener* listener) {
  std::shared_ptr<otel_connector> cmd(
      std::make_shared<otel_connector>(connector_name, cmd_line, listener));
  auto iter_res = _commands.emplace(connector_name, cmd);
  if (!iter_res.second) {
    iter_res.first->second = cmd;
  }
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
      _logger(log_v2::instance().get(log_v2::OTEL)) {
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
uint64_t otel_connector::run(const std::string& processed_cmd,
                             nagios_macros& macros,
                             uint32_t timeout,
                             const check_result::pointer& to_push_to_checker,
                             const void* caller) {
  std::shared_ptr<otel::open_telemetry_base> otel =
      otel::open_telemetry_base::instance();

  if (!otel) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "open telemetry module not loaded for connector: {}",
                        get_name());
    throw exceptions::msg_fmt(
        "open telemetry module not loaded for connector: {}", get_name());
  }

  uint64_t command_id(get_uniq_id());

  if (!gest_call_interval(command_id, to_push_to_checker, caller)) {
    return command_id;
  }

  if (!_conv_conf) {
    SPDLOG_LOGGER_ERROR(
        _logger, "{} unable to do a check without a converter configuration",
        get_name());
    throw exceptions::msg_fmt(
        "{} unable to do a check without a converter configuration",
        get_name());
  }
  SPDLOG_LOGGER_TRACE(
      _logger,
      "otel_connector::async_run: connector='{}', command_id={}, "
      "cmd='{}', timeout={}",
      _name, command_id, processed_cmd, timeout);

  result res;
  bool res_available = otel->check(
      processed_cmd, _conv_conf, command_id, macros, timeout, res,
      [me = shared_from_this(), command_id](const result& async_res) {
        SPDLOG_LOGGER_TRACE(
            me->_logger, "otel_connector async_run callback: connector='{}' {}",
            me->_name, async_res);
        me->update_result_cache(command_id, async_res);
        if (me->_listener) {
          (me->_listener->finished)(async_res);
        }
      });

  if (res_available) {
    SPDLOG_LOGGER_TRACE(_logger,
                        "otel_connector data available : connector='{}', "
                        "cmd='{}', {}",
                        _name, processed_cmd, res);
    update_result_cache(command_id, res);
    if (_listener) {
      (_listener->finished)(res);
    }
  }

  return command_id;
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
void otel_connector::run(const std::string& processed_cmd,
                         nagios_macros& macros,
                         uint32_t timeout,
                         result& res) {
  std::shared_ptr<otel::open_telemetry_base> otel =
      otel::open_telemetry_base::instance();
  if (!otel) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "open telemetry module not loaded for connector: {}",
                        get_name());
    throw exceptions::msg_fmt(
        "open telemetry module not loaded for connector: {}", get_name());
  }

  uint64_t command_id(get_uniq_id());

  SPDLOG_LOGGER_TRACE(_logger,
                      "otel_connector::sync_run: connector='{}', cmd='{}', "
                      "command_id={}, timeout={}",
                      _name, processed_cmd, command_id, timeout);

  std::condition_variable cv;
  std::mutex cv_m;

  bool res_available =
      otel->check(processed_cmd, _conv_conf, command_id, macros, timeout, res,
                  [&res, &cv](const result& async_res) {
                    res = async_res;
                    cv.notify_one();
                  });

  // no otl_data_point available => wait util available or timeout
  if (!res_available) {
    std::unique_lock l(cv_m);
    cv.wait(l);
  }
  SPDLOG_LOGGER_TRACE(
      _logger, "otel_connector::end sync_run: connector='{}', cmd='{}', {}",
      _name, processed_cmd, res);
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
    if (!_conv_conf) {
      std::shared_ptr<otel::open_telemetry_base> otel =
          otel::open_telemetry_base::instance();
      if (otel) {
        _conv_conf =
            otel->create_check_result_builder_config(get_command_line());
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
