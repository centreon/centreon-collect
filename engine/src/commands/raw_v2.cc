/**
 * Copyright 2025 Centreon
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

#include "com/centreon/engine/commands/raw_v2.hh"
#include "com/centreon/common/process/process.hh"
#include "com/centreon/engine/commands/environment.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"

using namespace com::centreon::common;
using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::commands;

/**
 *  Constructor.
 *
 *  @param[in] name         The command name.
 *  @param[in] command_line The command line.
 *  @param[in] listener     The listener who catch events.
 */
raw_v2::raw_v2(const std::shared_ptr<asio::io_context> io_context,
               std::string const& name,
               std::string const& command_line,
               command_listener* listener)
    : command(name, command_line, listener, e_type::raw),
      _io_context(io_context),
      _timeout_timer(*io_context) {
  if (_command_line.empty()) {
    throw exceptions::msg_fmt(
        "Could not create {}'' command: command line is empty", _name);
  }
}

static void _build_argv_macro_environment(
    nagios_macros const& macros,
    boost::process::v2::process_environment& env) {
  for (uint32_t i(0); i < MAX_COMMAND_ARGUMENTS; ++i) {
    env.env_buffer.emplace_back(
        fmt::format(MACRO_ENV_VAR_PREFIX "ARG{}", i + 1), macros.argv[i]);
  }
}

/**
 *  Build contact address environment variables.
 *
 *  @param[in]  macros  The macros data struct.
 *  @param[out] env     The environment to fill.
 */
static void _build_contact_address_environment(
    nagios_macros const& macros,
    boost::process::v2::process_environment& env) {
  if (!macros.contact_ptr)
    return;
  std::vector<std::string> const& address(macros.contact_ptr->get_addresses());
  for (uint32_t i(0); i < address.size(); ++i) {
    env.env_buffer.emplace_back(
        fmt::format(MACRO_ENV_VAR_PREFIX "CONTACTADDRESS{}", i), address[i]);
  }
}

/**
 *  Build custom contact macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
static void _build_custom_contact_macro_environment(
    nagios_macros& macros,
    boost::process::v2::process_environment& env) {
  // Build custom contact variable.
  contact* cntct(macros.contact_ptr);
  if (cntct) {
    std::string name;
    for (auto const& cv : cntct->get_custom_variables()) {
      if (!cv.first.empty()) {
        name = "_CONTACT";
        name.append(cv.first);
        macros.custom_contact_vars[name] = cv.second;
      }
    }
  }
  std::string key;
  // Set custom contact variable into the environement
  for (auto const& cv : macros.custom_contact_vars) {
    if (!cv.first.empty()) {
      std::string value(clean_macro_chars(
          cv.second.value(), STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS));
      key = MACRO_ENV_VAR_PREFIX;
      key.append(cv.first);
      env.env_buffer.emplace_back(key, value);
    }
  }
}

/**
 *  Build custom host macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
static void _build_custom_host_macro_environment(
    nagios_macros& macros,
    boost::process::v2::process_environment& env) {
  // Build custom host variable.
  host* hst(macros.host_ptr);
  if (hst) {
    std::string name;
    for (auto const& cv : hst->custom_variables) {
      if (!cv.first.empty()) {
        name = "_HOST";
        name.append(cv.first);
        macros.custom_host_vars[name] = cv.second;
      }
    }
  }
  std::string key;
  // Set custom host variable into the environement
  for (auto const& cv : macros.custom_host_vars) {
    if (!cv.first.empty()) {
      std::string value(clean_macro_chars(
          cv.second.value(), STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS));
      key = MACRO_ENV_VAR_PREFIX;
      key.append(cv.first);
      env.env_buffer.emplace_back(key, value);
    }
  }
}

/**
 *  Build custom service macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
static void _build_custom_service_macro_environment(
    nagios_macros& macros,
    boost::process::v2::process_environment& env) {
  // Build custom service variable.
  service* svc(macros.service_ptr);
  if (svc) {
    std::string name;
    for (auto const& cv : svc->custom_variables) {
      if (!cv.first.empty()) {
        name = "_SERVICE";
        name.append(cv.first);
        macros.custom_service_vars[name] = cv.second;
      }
    }
  }
  std::string key;
  // Set custom service variable into the environement
  for (auto const& cv : macros.custom_service_vars) {
    if (!cv.first.empty()) {
      std::string value(clean_macro_chars(
          cv.second.value(), STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS));
      std::string key;
      key = MACRO_ENV_VAR_PREFIX;
      key.append(cv.first);
      env.env_buffer.emplace_back(key, value);
    }
  }
}

/**
 *  Build macrox environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
static void _build_macrosx_environment(
    nagios_macros& macros,
    boost::process::v2::process_environment& env) {
  bool use_large_installation_tweaks =
      pb_config.use_large_installation_tweaks();
  std::string key;
  for (uint32_t i = 0; i < MACRO_X_COUNT; ++i) {
    int release_memory(0);

    // Need to grab macros?
    if (macros.x[i].empty()) {
      // Skip summary macro in lage instalation tweaks.
      if (i < MACRO_TOTALHOSTSUP || i > MACRO_TOTALSERVICEPROBLEMSUNHANDLED ||
          !use_large_installation_tweaks) {
        grab_macrox_value_r(&macros, i, "", "", macros.x[i], &release_memory);
      }
    }

    // Add into the environment.
    if (!macro_x_names[i].empty()) {
      std::string line;
      key = MACRO_ENV_VAR_PREFIX;
      key.append(macro_x_names[i]);
      env.env_buffer.emplace_back(key, macros.x[i]);
    }

    // Release memory if necessary.
    if (release_memory) {
      macros.x[i] = "";
    }
  }
}

static std::vector<std::pair<std::string, std::string>> _empty_args;

/**
 *  Build all macro environemnt variable.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void _build_environment_macros(nagios_macros& macros,
                               boost::process::v2::process_environment& env) {
  bool enable_environment_macros = pb_config.enable_environment_macros();
  if (enable_environment_macros) {
    _build_macrosx_environment(macros, env);
    _build_argv_macro_environment(macros, env);
    _build_custom_host_macro_environment(macros, env);
    _build_custom_service_macro_environment(macros, env);
    _build_custom_contact_macro_environment(macros, env);
    _build_contact_address_environment(macros, env);
    env.env = env.build_env(_empty_args);
  }
}

/**
 *  Run a command.
 *
 *  @param[in] processed_cmd     A full command line with arguments.
 *  @param[in] macros  The macros data struct.
 *  @param[in] timeout The command timeout.
 *  @param[in] to_push_to_checker This check_result will be pushed to checher.
 *  @param[in] caller  pointer to the caller
 *
 *  @return The command id.
 */
uint64_t raw_v2::run(const std::string& processed_cmd,
                     nagios_macros& macros,
                     uint32_t timeout,
                     const check_result::pointer& to_push_to_checker,
                     const void* caller) {
  engine_logger(dbg_commands, basic)
      << "raw_v2::run: cmd='" << processed_cmd << "', timeout=" << timeout;
  SPDLOG_LOGGER_TRACE(commands_logger, "raw_v2::run: cmd='{}', timeout={}",
                      processed_cmd, timeout);

  bool expected = false;
  if (!_running.compare_exchange_strong(expected, true)) {
    throw exceptions::msg_fmt("a check is yet running for command {}", _name);
  }

  if (_last_processed_cmd != processed_cmd) {
    _process_args = common::process<true>::parse_cmd_line(processed_cmd);
    _last_processed_cmd = processed_cmd;
  }

  uint64_t command_id(get_uniq_id());

  if (!gest_call_interval(command_id, to_push_to_checker, caller)) {
    return command_id;
  }

  std::shared_ptr<boost::process::v2::process_environment> env =
      std::make_shared<boost::process::v2::process_environment>(_empty_args);

  SPDLOG_LOGGER_TRACE(commands_logger, "raw_v2::run: id={}", command_id);
  _build_environment_macros(macros, *env);

  try {
    std::shared_ptr<common::process<true>> p =
        std::make_shared<common::process<true>>(
            g_io_context, commands_logger, _process_args, true, false, env);
    p->start_process(
        [me = shared_from_this(), command_id, start = time(nullptr)](
            const common::process<true>& proc, int exit_code, int exit_status,
            const std::string& std_out, const std::string& std_err) {
          me->_on_complete(command_id, start, exit_code, exit_status, std_out,
                           std_err);
        },
        std::chrono::seconds(timeout));
    SPDLOG_LOGGER_TRACE(commands_logger,
                        "raw_v2::run: start process success: id={}",
                        command_id);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_TRACE(commands_logger,
                        "raw_v2::run: start process failed: id={}, {}: {}",
                        command_id, _name, e.what());
    throw;
  }

  return command_id;
}

/**
 * @brief completion handler called by child process object
 * It fills a result struct. If std_out is not empty, we store it in result.out
 * otherwise we use std_err (use full in case of bad script path by example)
 *
 * @param command_id
 * @param start //start time of child process
 * @param exit_code
 * @param exit_status normal or timeout
 * @param std_out all stdout read from child process
 * @param std_err all stderr read from child process
 */
void raw_v2::_on_complete(uint64_t command_id,
                          time_t start,
                          int exit_code,
                          int exit_status,
                          const std::string& std_out,
                          const std::string& std_err) {
  bool expected = true;
  if (!_running.compare_exchange_strong(expected, false)) {
    SPDLOG_LOGGER_ERROR(commands_logger, "_bad running state for {}",
                        get_name());
    return;
  }
  SPDLOG_LOGGER_TRACE(commands_logger, "raw_v2::_on_complete: {} id={}",
                      get_name(), command_id);

  // Build check result.
  result res;
  res.command_id = command_id;
  res.start_time = start;
  res.end_time = time(nullptr);
  res.exit_code = exit_code;
  res.output = !std_out.empty() ? std_out : std_err;
  res.exit_status = static_cast<process::status>(exit_status);

  if (res.exit_status == process::timeout) {
    res.exit_code = service::state_unknown;
    res.output = "(Process Timeout)";
  } else if ((res.exit_status == process::crash) || (res.exit_code < -1) ||
             (res.exit_code > 3))
    res.exit_code = service::state_unknown;

  SPDLOG_LOGGER_TRACE(commands_logger, "raw::finished: id={}, {}", command_id,
                      res);

  update_result_cache(command_id, res);

  try {
    // Forward result to the listener.
    if (_listener)
      _listener->finished(res);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(commands_logger, "{} fail to call listener: {}",
                        get_name(), e.what());
  }
}

/**
 *  Run a command and wait the result.
 * If std_out is not empty, we store it in result.out
 * otherwise we use std_err (use full in case of bad script path by example)
 * It's a fake synchronous method, as process is asynchronous, we use an
 * absl::Mutex to wait child process completion
 *
 *  @param[in]  processed_cmd  A full command line with arguments?.
 *  @param[in]  macros  The macros data struct.
 *  @param[in]  timeout The command timeout.
 *  @param[out] res     The result of the command.
 */
void raw_v2::run(const std::string& processed_cmd,
                 nagios_macros& macros,
                 uint32_t timeout,
                 result& res) {
  engine_logger(dbg_commands, basic)
      << "raw_v2::run: cmd='" << processed_cmd << "', timeout=" << timeout;
  SPDLOG_LOGGER_TRACE(commands_logger, "raw_v2::run: cmd='{}', timeout={}",
                      processed_cmd, timeout);

  bool expected = false;
  if (!_running.compare_exchange_strong(expected, true)) {
    throw exceptions::msg_fmt("a check is yet running for command {}", _name);
  }

  if (_last_processed_cmd != processed_cmd) {
    _process_args = common::process<true>::parse_cmd_line(processed_cmd);
    _last_processed_cmd = processed_cmd;
  }

  uint64_t command_id(get_uniq_id());

  std::shared_ptr<boost::process::v2::process_environment> env =
      std::make_shared<boost::process::v2::process_environment>(_empty_args);

  SPDLOG_LOGGER_TRACE(commands_logger, "raw_v2::run: id={}", command_id);
  _build_environment_macros(macros, *env);

  absl::Mutex waiter;
  bool done = false;

  try {
    std::shared_ptr<common::process<true>> p =
        std::make_shared<common::process<true>>(
            g_io_context, commands_logger, _process_args, true, false, env);
    p->start_process(
        [me = shared_from_this(), command_id, start = time(nullptr), &waiter,
         &done, &res](const common::process<true>& proc, int exit_code,
                      int exit_status, const std::string& std_out,
                      const std::string& std_err) {
          absl::MutexLock lck(&waiter);
          bool expected = true;
          if (!me->_running.compare_exchange_strong(expected, false)) {
            SPDLOG_LOGGER_ERROR(commands_logger,
                                "bad running state while waiting for {}",
                                me->get_name());
            return;
          }
          res.command_id = command_id;
          res.start_time = start;
          res.end_time = time(nullptr);
          res.exit_code = exit_code;
          res.exit_status = static_cast<process::status>(exit_status);
          res.output = !std_out.empty() ? std_out : std_err;
          done = true;
        },
        std::chrono::seconds(timeout));
    SPDLOG_LOGGER_TRACE(commands_logger,
                        "raw_v2::run: start process success: id={}",
                        command_id);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_TRACE(commands_logger,
                        "raw_v2::run: start process failed: id={}, {}: {}",
                        command_id, _name, e.what());
    throw;
  }

  absl::MutexLock lck(&waiter);
  waiter.Await(absl::Condition(&done));

  if (res.exit_status == process::timeout) {
    res.exit_code = service::state_unknown;
    res.output = "(Process Timeout)";
  } else if (res.exit_status == process::crash || res.exit_code < -1 ||
             res.exit_code > 3)
    res.exit_code = service::state_unknown;

  SPDLOG_LOGGER_TRACE(commands_logger,
                      "raw_v2::run: end process: "
                      "id={}, {}",
                      command_id, res);
}
