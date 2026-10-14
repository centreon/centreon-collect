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
#include "com/centreon/engine/commands/environment.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "engine/macros/descriptors.hh"
#include "engine/macros/process.hh"
#include "engine/macros/resolve.hh"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"

using namespace com::centreon::common;
using namespace com::centreon::engine;
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
      _io_context(io_context) {
  if (_command_line.empty()) {
    throw exceptions::msg_fmt(
        "Could not create {}'' command: command line is empty", _name);
  }
}

raw_v2::~raw_v2() {
  if (_running && _process) {
    process_logger->debug("raw_v2::~raw_v2: killing process for command '{}'",
                          _name);
    _process->kill();
  }
}

static void _build_argv_macro_environment(
    const macro_context& macros,
    boost::process::v2::process_environment& env) {
  for (uint32_t i(0); i < MAX_COMMAND_ARGUMENTS; ++i) {
    env.env_buffer.emplace_back(
        fmt::format(MACRO_ENV_VAR_PREFIX "ARG{}={}", i + 1, macros.argv[i]));
  }
}

/**
 *  Build contact address environment variables.
 *
 *  @param[in]  macros  The macros data struct.
 *  @param[out] env     The environment to fill.
 */
static void _build_contact_address_environment(
    const macro_context& macros,
    boost::process::v2::process_environment& env) {
  if (!macros.contact_ptr)
    return;
  std::vector<std::string> const& address(macros.contact_ptr->get_addresses());
  for (uint32_t i(0); i < address.size(); ++i) {
    env.env_buffer.emplace_back(
        fmt::format(MACRO_ENV_VAR_PREFIX "CONTACTADDRESS{}={}", i, address[i]));
  }
}

/**
 *  Build the environment variables of custom variables.
 *
 *  @param[in]  prefix The prefix of the macro names ("_HOST"...).
 *  @param[in]  vars   The custom variables.
 *  @param[out] env    The environment to fill.
 */
static void _build_custom_macro_environment(
    std::string_view prefix,
    const map_customvar& vars,
    boost::process::v2::process_environment& env) {
  for (auto& [name, var] : vars) {
    if (!name.empty()) {
      std::string value(var.value());
      macros::strip_illegal_macro_chars(value);
      env.env_buffer.emplace_back(
          fmt::format(MACRO_ENV_VAR_PREFIX "{}{}={}", prefix, name, value));
    }
  }
}

/**
 *  Build custom contact, host and service macro environment variables.
 *
 *  @param[in]  macros  The macros data struct.
 *  @param[out] env     The environment to fill.
 */
static void _build_custom_macros_environment(
    const macro_context& macros,
    boost::process::v2::process_environment& env) {
  if (macros.host_ptr)
    _build_custom_macro_environment("_HOST", macros.host_ptr->custom_variables,
                                    env);
  if (macros.service_ptr)
    _build_custom_macro_environment("_SERVICE",
                                    macros.service_ptr->custom_variables, env);
  if (macros.contact_ptr)
    _build_custom_macro_environment(
        "_CONTACT", macros.contact_ptr->get_custom_variables(), env);
}

/**
 *  Build macrox environment variables.
 *
 *  @param[in,out] macros  The macros data struct, where the summary macros
 *                         are cached.
 *  @param[out]    env     The environment to fill.
 */
static void _build_macrosx_environment(
    macro_context& macros,
    boost::process::v2::process_environment& env) {
  bool use_large_installation_tweaks =
      pb_indexed_config.state().use_large_installation_tweaks();
  for (auto& desc : com::centreon::engine::macros::all_descriptors()) {
    std::string value;
    /* With the large installation tweaks, the summary macros are not
     * computed for the environment: they are given only when already
     * cached. */
    if (desc.fam != com::centreon::engine::macros::family::summary ||
        !use_large_installation_tweaks || macros.summary) {
      value = macros::grab_macrox_value(macros, desc, "", "");
    }
    env.env_buffer.emplace_back(
        fmt::format(MACRO_ENV_VAR_PREFIX "{}={}", desc.name, value));
  }
}

static std::vector<std::pair<std::string, std::string>> _empty_args;

/**
 *  Build all macro environemnt variable.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void _build_environment_macros(macro_context& macros,
                               boost::process::v2::process_environment& env) {
  bool enable_environment_macros =
      pb_indexed_config.state().enable_environment_macros();
  if (enable_environment_macros) {
    _build_macrosx_environment(macros, env);
    _build_argv_macro_environment(macros, env);
    _build_custom_macros_environment(macros, env);
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
                     macro_context& macros,
                     uint32_t timeout,
                     const check_result::pointer& to_push_to_checker,
                     const void* caller) {
  SPDLOG_LOGGER_TRACE(commands_logger, "raw_v2::run: cmd='{}', timeout={}",
                      processed_cmd, timeout);

  bool expected = false;
  if (!_running.compare_exchange_strong(expected, true)) {
    throw exceptions::msg_fmt("a check is already running for command {}",
                              _name);
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
    _process = std::make_shared<common::process<true>>(
        g_io_context, commands_logger, _process_args, true, false, env);
    // we don't want that lambda own raw because raw could be deleted by lambda
    // exit called by pool thread
    _process->start_process(
        [me = weak_from_this(), command_id, start = time(nullptr)](
            const common::process<true>&, int exit_code, int exit_status,
            const std::string& std_out, const std::string& std_err) {
          std::shared_ptr<raw_v2> parent =
              std::static_pointer_cast<raw_v2>(me.lock());
          if (parent) {
            parent->_on_complete(command_id, start, exit_code, exit_status,
                                 std_out, std_err);
          }
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
                 macro_context& macros,
                 uint32_t timeout,
                 result& res) {
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
    _process = std::make_shared<common::process<true>>(
        g_io_context, commands_logger, _process_args, true, false, env);
    _process->start_process(
        [me = shared_from_this(), command_id, start = time(nullptr), &waiter,
         &done, &res](const common::process<true>&, int exit_code,
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
