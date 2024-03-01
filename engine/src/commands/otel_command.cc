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

#include "com/centreon/engine/commands/otel_command.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::engine::commands;

/**
 * @brief singleton used to make the bridge between engine and otel module
 *
 */
std::shared_ptr<com::centreon::engine::commands::otel::open_telemetry_base>
    com::centreon::engine::commands::otel::open_telemetry_base::_instance;

/**
 * @brief list of all otel_command
 *
 */
absl::flat_hash_map<std::string, std::shared_ptr<otel_command>>
    otel_command::_commands;

/**
 * @brief create an otel_command
 *
 * @param connector_name
 * @param cmd_line
 * @param listener
 */
void otel_command::create(const std::string& connector_name,
                          const std::string& cmd_line,
                          commands::command_listener* listener) {
  std::shared_ptr<otel_command> cmd(
      std::make_shared<otel_command>(connector_name, cmd_line, listener));
  auto iter_res = _commands.emplace(connector_name, cmd);
  if (!iter_res.second) {
    iter_res.first->second = cmd;
  }
}

bool otel_command::remove(const std::string& connector_name) {
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
bool otel_command::update(const std::string& connector_name,
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
 * @return std::shared_ptr<otel_command>
 */
std::shared_ptr<otel_command> otel_command::get_otel_command(
    const std::string& connector_name) {
  auto search = _commands.find(connector_name);
  return search != _commands.end() ? search->second
                                   : std::shared_ptr<otel_command>();
}

/**
 * @brief erase all otel commands
 *
 */
void otel_command::clear() {
  _commands.clear();
}

/**
 * @brief to call once otel module have been loaded
 *
 */
void otel_command::init_all() {
  for (auto& to_init : _commands) {
    to_init.second->init();
  }
}

/**
 * @brief to call after otel module unload
 *
 */
void otel_command::reset_all_extractor() {
  for (auto& to_init : _commands) {
    to_init.second->reset_extractor();
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
otel_command::otel_command(const std::string& connector_name,
                           const std::string& cmd_line,
                           commands::command_listener* listener)
    : command(connector_name, cmd_line, listener),
      _host_serv_list(std::make_shared<otel::host_serv_list>()) {
  init();
}

void otel_command::update(const std::string& cmd_line) {
  if (get_command_line() == cmd_line) {
    return;
  }
  set_command_line(cmd_line);
  init();
}

static const char* status_to_string(com::centreon::process::status status) {
  switch (status) {
    case com::centreon::process::status::normal:
      return "normal";
    case com::centreon::process::status::crash:
      return "crash";
    case com::centreon::process::status::timeout:
      return "timeout";
    default:
      return "unknown status";
  }
}

uint64_t otel_command::run(const std::string& processed_cmd,
                           nagios_macros& macros,
                           uint32_t timeout,
                           const check_result::pointer& to_push_to_checker,
                           const void* caller) {
  std::shared_ptr<otel::open_telemetry_base> otel =
      otel::open_telemetry_base::instance();

  if (!otel) {
    SPDLOG_LOGGER_ERROR(log_v2::otl(),
                        "open telemetry module not loaded for connector: {}",
                        get_name());
    throw exceptions::msg_fmt(
        "open telemetry module not loaded for connector: {}", get_name());
  }

  uint64_t command_id(get_uniq_id());

  if (!gest_call_interval(command_id, to_push_to_checker, caller)) {
    return command_id;
  }

  SPDLOG_LOGGER_TRACE(log_v2::otl(),
                      "otel_command::async_run: connector='{}', command_id={}, "
                      "cmd='{}', timeout={}",
                      _name, command_id, processed_cmd, timeout);

  result res;
  bool res_available = otel->check(
      processed_cmd, command_id, macros, timeout, res,
      [me = shared_from_this(), command_id](const result& async_res) {
        SPDLOG_LOGGER_TRACE(log_v2::otl(),
                            "otel_command::end async_run: connector='{}', "
                            " status='{}'",
                            me->_name, status_to_string(async_res.exit_status));
        me->update_result_cache(command_id, async_res);
        if (me->_listener) {
          (me->_listener->finished)(async_res);
        }
      });

  if (res_available) {
    SPDLOG_LOGGER_TRACE(
        log_v2::otl(),
        "otel_command::end async_run: connector='{}', cmd='{}', "
        "status='{}'",
        _name, processed_cmd, status_to_string(res.exit_status));
    update_result_cache(command_id, res);
    if (_listener) {
      (_listener->finished)(res);
    }
  }

  return command_id;
}

void otel_command::run(const std::string& processed_cmd,
                       nagios_macros& macros,
                       uint32_t timeout,
                       result& res) {
  std::shared_ptr<otel::open_telemetry_base> otel =
      otel::open_telemetry_base::instance();
  if (!otel) {
    SPDLOG_LOGGER_ERROR(log_v2::otl(),
                        "open telemetry module not loaded for connector: {}",
                        get_name());
    throw exceptions::msg_fmt(
        "open telemetry module not loaded for connector: {}", get_name());
  }

  SPDLOG_LOGGER_TRACE(
      log_v2::otl(),
      "otel_command::sync_run: connector='{}', cmd='{}', timeout={}", _name,
      processed_cmd, timeout);

  uint64_t command_id(get_uniq_id());

  std::condition_variable cv;
  std::mutex cv_m;

  bool res_available = otel->check(processed_cmd, command_id, macros, timeout,
                                   res, [&res, &cv](const result& async_res) {
                                     res = async_res;
                                     cv.notify_one();
                                   });

  // no data_point available => wait util available or timeout
  if (!res_available) {
    std::unique_lock l(cv_m);
    cv.wait(l);
  }
  SPDLOG_LOGGER_TRACE(log_v2::otl(),
                      "otel_command::end sync_run: connector='{}', cmd='{}', "
                      "status='{}'",
                      _name, processed_cmd, status_to_string(res.exit_status));
}

/**
 * @brief when this object is created, otel module may not have been loaded, so
 * we have to init later this method is called by constructor and is effective
 * if we are not in startup otherwise, it's called by open_telemetry object at
 * otel module loading
 *
 */
void otel_command::init() {
  if (!_extractor) {
    std::shared_ptr<otel::open_telemetry_base> otel =
        otel::open_telemetry_base::instance();
    if (otel) {
      _extractor = otel->create_extractor(get_command_line(), _host_serv_list);
    }
  }
}

/**
 * @brief reset _extractor attribute, called when otel module is unloaded
 *
 */
void otel_command::reset_extractor() {
  _extractor.reset();
}

/**
 * @brief add a host service allowed by the extractor of the connector
 *
 * @param host
 * @param service_description empty if host command
 */
void otel_command::register_host_serv(const std::string& host,
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
void otel_command::unregister_host_serv(
    const std::string& host,
    const std::string& service_description) {
  _host_serv_list->unregister_host_serv(host, service_description);
}
