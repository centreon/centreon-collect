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

#include "com/centreon/engine/commands/raw.hh"
#include "com/centreon/engine/commands/environment.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"

using namespace com::centreon;
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
raw::raw(std::string const& name,
         std::string const& command_line,
         command_listener* listener)
    : command(name, command_line, listener, e_type::raw), process_listener() {
  if (_command_line.empty())
    throw engine_error() << "Could not create '" << _name
                         << "' command: command line is empty";
}

/**
 *  Destructor.
 */
raw::~raw() noexcept {
  try {
    std::unique_lock<std::mutex> lock(_lock);
    while (!_processes_busy.empty()) {
      process* p{_processes_busy.begin()->first};
      lock.unlock();
      p->wait();
      lock.lock();
    }
    for (auto p : _processes_free)
      delete p;

  } catch (std::exception const& e) {
    engine_logger(log_runtime_error, basic)
        << "Error: Raw command destructor failed: " << e.what();
    SPDLOG_LOGGER_ERROR(runtime_logger,
                        "Error: Raw command destructor failed: {}", e.what());
  }
}

/**
 *  Run a command.
 *
 *  @param[in] args    The command arguments.
 *  @param[in] macros  The macros data struct.
 *  @param[in] timeout The command timeout.
 *  @param[in] to_push_to_checker This check_result will be pushed to checher.
 *  @param[in] caller  pointer to the caller
 *
 *  @return The command id.
 */
uint64_t raw::run(std::string const& processed_cmd,
                  nagios_macros& macros,
                  uint32_t timeout,
                  const check_result::pointer& to_push_to_checker,
                  const void* caller) {
  engine_logger(dbg_commands, basic)
      << "raw::run: cmd='" << processed_cmd << "', timeout=" << timeout;
  SPDLOG_LOGGER_TRACE(commands_logger, "raw::run: cmd='{}', timeout={}",
                      processed_cmd, timeout);

  // Get process and put into the busy list.
  process* p;
  uint64_t command_id(get_uniq_id());

  if (!gest_call_interval(command_id, to_push_to_checker, caller)) {
    return command_id;
  }
  {
    std::lock_guard<std::mutex> lock(_lock);
    p = _get_free_process();
    _processes_busy[p] = command_id;
  }

  engine_logger(dbg_commands, basic)
      << "raw::run: id=" << command_id << ", process=" << p;
  SPDLOG_LOGGER_TRACE(commands_logger, "raw::run: id={} , process={}",
                      command_id, (void*)p);

  // Setup environnement macros if is necessary.
  environment env;
  _build_environment_macros(macros, env);

  try {
    // Start process.
    p->exec(processed_cmd.c_str(), env.data(), timeout);
    engine_logger(dbg_commands, basic)
        << "raw::run: start process success: id=" << command_id;
    SPDLOG_LOGGER_TRACE(commands_logger,
                        "raw::run: start process success: id={}", command_id);
  } catch (...) {
    engine_logger(dbg_commands, basic)
        << "raw::run: start process failed: id=" << command_id;
    SPDLOG_LOGGER_TRACE(commands_logger,
                        "raw::run: start process failed: id={}", command_id);

    std::lock_guard<std::mutex> lock(_lock);
    _processes_busy.erase(p);
    delete p;
    throw;
  }
  return command_id;
}

/**
 *  Run a command and wait the result.
 *
 *  @param[in]  args    The command arguments.
 *  @param[in]  macros  The macros data struct.
 *  @param[in]  timeout The command timeout.
 *  @param[out] res     The result of the command.
 */
void raw::run(std::string const& processed_cmd,
              nagios_macros& macros,
              uint32_t timeout,
              result& res) {
  engine_logger(dbg_commands, basic)
      << "raw::run: cmd='" << processed_cmd << "', timeout=" << timeout;
  SPDLOG_LOGGER_TRACE(commands_logger, "raw::run: cmd='{}', timeout={}",
                      processed_cmd, timeout);

  // Get process.
  process p;
  uint64_t command_id(get_uniq_id());

  engine_logger(dbg_commands, basic)
      << "raw::run: id=" << command_id << ", process=" << &p;
  SPDLOG_LOGGER_TRACE(commands_logger, "raw::run: id={}, process={}",
                      command_id, (void*)&p);

  // Setup environement macros if is necessary.
  environment env;
  _build_environment_macros(macros, env);

  // Start process.
  try {
    p.exec(processed_cmd.c_str(), env.data(), timeout);
    engine_logger(dbg_commands, basic)
        << "raw::run: start process success: id=" << command_id;
    SPDLOG_LOGGER_TRACE(commands_logger,
                        "raw::run: start process success: id={}", command_id);
  } catch (...) {
    engine_logger(dbg_commands, basic)
        << "raw::run: start process failed: id=" << command_id;
    SPDLOG_LOGGER_TRACE(commands_logger,
                        "raw::run: start process failed: id={}", command_id);
    throw;
  }

  // Wait for completion.
  p.wait();

  // Get process output.
  p.read(res.output);

  // Set result informations.
  res.command_id = command_id;
  res.start_time = p.start_time();
  res.end_time = p.end_time();
  res.exit_code = p.exit_code();
  res.exit_status = p.exit_status();

  if (res.exit_status == process::timeout) {
    res.exit_code = service::state_unknown;
    res.output = "(Process Timeout)";
  } else if (res.exit_status == process::crash || res.exit_code < -1 ||
             res.exit_code > 3)
    res.exit_code = service::state_unknown;

  engine_logger(dbg_commands, basic) << "raw::run: end process: "
                                        "id="
                                     << command_id
                                     << ", "
                                        "start_time="
                                     << res.start_time.to_mseconds()
                                     << ", "
                                        "end_time="
                                     << res.end_time.to_mseconds()
                                     << ", "
                                        "exit_code="
                                     << res.exit_code
                                     << ", "
                                        "exit_status="
                                     << res.exit_status
                                     << ", "
                                        "output='"
                                     << res.output << "'";
  SPDLOG_LOGGER_TRACE(commands_logger,
                      "raw::run: end process: "
                      "id={}, {}",
                      command_id, res);
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Provide by process_listener interface but not used.
 *
 *  @param[in] p  Unused.
 */
void raw::data_is_available(process& p) noexcept {
  (void)p;
}

/**
 *  Provide by process_listener interface but not used.
 *
 *  @param[in] p  Unused.
 */
void raw::data_is_available_err(process& p) noexcept {
  (void)p;
}

/**
 *  Provide by process_listener interface. Call at the end
 *  of the process execution.
 *
 *  @param[in] p  The process to finished.
 */
void raw::finished(process& p) noexcept {
  try {
    engine_logger(dbg_commands, basic) << "raw::finished: process=" << &p;
    SPDLOG_LOGGER_TRACE(commands_logger, "raw::finished: process={}",
                        (void*)&p);

    uint64_t command_id(0);
    {
      std::unique_lock<std::mutex> lock(_lock);
      // Find process from the busy list.
      std::unordered_map<process*, uint64_t>::iterator it =
          _processes_busy.find(&p);
      if (it == _processes_busy.end()) {
        // Put the process into the free list.
        _processes_free.push_back(&p);
        lock.unlock();

        engine_logger(log_runtime_warning, basic)
            << "Warning: Invalid process pointer: "
               "process not found into process busy list";
        SPDLOG_LOGGER_WARN(runtime_logger,
                           "Warning: Invalid process pointer: "
                           "process not found into process busy list");
        return;
      }
      // Get command_id and remove the process from the busy list.
      command_id = it->second;
      _processes_busy.erase(it);
    }

    engine_logger(dbg_commands, basic) << "raw::finished: id=" << command_id;
    SPDLOG_LOGGER_TRACE(commands_logger, "raw::finished: id={}", command_id);

    // Build check result.
    result res;

    // Get process output.
    p.read(res.output);

    // Release process, put into the free list.
    {
      std::lock_guard<std::mutex> lock(_lock);
      _processes_free.push_back(&p);
    }

    // Set result informations.
    res.command_id = command_id;
    res.start_time = p.start_time();
    res.end_time = p.end_time();
    res.exit_code = p.exit_code();
    res.exit_status = p.exit_status();

    if (res.exit_status == process::timeout) {
      res.exit_code = service::state_unknown;
      res.output = "(Process Timeout)";
    } else if ((res.exit_status == process::crash) || (res.exit_code < -1) ||
               (res.exit_code > 3))
      res.exit_code = service::state_unknown;

    engine_logger(dbg_commands, basic)
        << "raw::finished: id=" << command_id
        << ", start_time=" << res.start_time.to_mseconds()
        << ", end_time=" << res.end_time.to_mseconds()
        << ", exit_code=" << res.exit_code
        << ", exit_status=" << res.exit_status << ", output='" << res.output
        << "'";
    SPDLOG_LOGGER_TRACE(commands_logger, "raw::finished: id={}, {}", command_id,
                        res);

    update_result_cache(command_id, res);

    // Forward result to the listener.
    if (_listener)
      _listener->finished(res);
  } catch (std::exception const& e) {
    engine_logger(log_runtime_warning, basic)
        << "Warning: Raw process termination routine failed: " << e.what();
    SPDLOG_LOGGER_WARN(runtime_logger,
                       "Warning: Raw process termination routine failed: {}",
                       e.what());

    // Release process, put into the free list.
    std::lock_guard<std::mutex> lock(_lock);
    _processes_free.push_back(&p);
  }
}

/**
 *  Build argv macro environment variables.
 *
 *  @param[in]  macros  The macros data struct.
 *  @param[out] env     The environment to fill.
 */
void raw::_build_argv_macro_environment(nagios_macros const& macros,
                                        environment& env) {
  std::string line;
  for (uint32_t i(0); i < MAX_COMMAND_ARGUMENTS; ++i) {
    line = fmt::format(MACRO_ENV_VAR_PREFIX "ARG{}={}", i + 1, macros.argv[i]);
    env.add(line);
  }
}

/**
 *  Build contact address environment variables.
 *
 *  @param[in]  macros  The macros data struct.
 *  @param[out] env     The environment to fill.
 */
void raw::_build_contact_address_environment(nagios_macros const& macros,
                                             environment& env) {
  if (!macros.contact_ptr)
    return;
  std::vector<std::string> const& address(macros.contact_ptr->get_addresses());
  std::string line;
  for (uint32_t i(0); i < address.size(); ++i) {
    line =
        fmt::format(MACRO_ENV_VAR_PREFIX "CONTACTADDRESS{}={}", i, address[i]);
    env.add(line);
  }
}

/**
 *  Build custom contact macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_custom_contact_macro_environment(nagios_macros& macros,
                                                  environment& env) {
  // Build custom contact variable.
  contact* cntct(macros.contact_ptr);
  if (cntct) {
    for (auto const& cv : cntct->get_custom_variables()) {
      if (!cv.first.empty()) {
        std::string name("_CONTACT");
        name.append(cv.first);
        macros.custom_contact_vars[name] = cv.second;
      }
    }
  }
  // Set custom contact variable into the environement
  for (auto const& cv : macros.custom_contact_vars) {
    if (!cv.first.empty()) {
      std::string value(clean_macro_chars(
          cv.second.value(), STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS));
      std::string line;
      line.append(MACRO_ENV_VAR_PREFIX);
      line.append(cv.first);
      line.append("=");
      line.append(value);
      env.add(line);
    }
  }
}

/**
 *  Build custom host macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_custom_host_macro_environment(nagios_macros& macros,
                                               environment& env) {
  // Build custom host variable.
  host* hst(macros.host_ptr);
  if (hst) {
    for (auto const& cv : hst->custom_variables) {
      if (!cv.first.empty()) {
        std::string name("_HOST");
        name.append(cv.first);
        macros.custom_host_vars[name] = cv.second;
      }
    }
  }
  // Set custom host variable into the environement
  for (auto const& cv : macros.custom_host_vars) {
    if (!cv.first.empty()) {
      std::string value(clean_macro_chars(
          cv.second.value(), STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS));
      std::string line;
      line.append(MACRO_ENV_VAR_PREFIX);
      line.append(cv.first);
      line.append("=");
      line.append(value);
      env.add(line);
    }
  }
}

/**
 *  Build custom service macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_custom_service_macro_environment(nagios_macros& macros,
                                                  environment& env) {
  // Build custom service variable.
  service* svc(macros.service_ptr);
  if (svc) {
    for (auto const& cv : svc->custom_variables) {
      if (!cv.first.empty()) {
        std::string name("_SERVICE");
        name.append(cv.first);
        macros.custom_service_vars[name] = cv.second;
      }
    }
  }
  // Set custom service variable into the environement
  for (auto const& cv : macros.custom_service_vars) {
    if (!cv.first.empty()) {
      std::string value(clean_macro_chars(
          cv.second.value(), STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS));
      std::string line;
      line.append(MACRO_ENV_VAR_PREFIX);
      line.append(cv.first);
      line.append("=");
      line.append(value);
      env.add(line);
    }
  }
}

/**
 *  Build all macro environemnt variable.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_environment_macros(nagios_macros& macros, environment& env) {
  bool enable_environment_macros = pb_config.enable_environment_macros();
  if (enable_environment_macros) {
    _build_macrosx_environment(macros, env);
    _build_argv_macro_environment(macros, env);
    _build_custom_host_macro_environment(macros, env);
    _build_custom_service_macro_environment(macros, env);
    _build_custom_contact_macro_environment(macros, env);
    _build_contact_address_environment(macros, env);
  }
}

/**
 *  Build macrox environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_macrosx_environment(nagios_macros& macros, environment& env) {
  bool use_large_installation_tweaks =
      pb_config.use_large_installation_tweaks();
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
      line.append(MACRO_ENV_VAR_PREFIX);
      line.append(macro_x_names[i]);
      line.append("=");
      line.append(macros.x[i]);
      env.add(line);
    }

    // Release memory if necessary.
    if (release_memory) {
      macros.x[i] = "";
    }
  }
}

/**
 *  Get one process to execute command.
 *
 *  @return A process.
 */
process* raw::_get_free_process() {
  // If any process are available, create new one.
  if (_processes_free.empty()) {
    /* Only the out stream is open */
    process* p = new process(this, false, true, false);
    p->setpgid_on_exec(pb_config.use_setpgid());
    return p;
  }
  // Get a free process.
  process* p = _processes_free.front();
  _processes_free.pop_front();
  return p;
}
