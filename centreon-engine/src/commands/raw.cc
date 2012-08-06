/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/engine/commands/raw.hh"
#include "com/centreon/engine/commands/environment.hh"
#include "com/centreon/engine/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::commands;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] name         The command name.
 *  @param[in] command_line The command line.
 *  @param[in] listener     The listener who catch events.
 */
raw::raw(
       std::string const& name,
       std::string const& command_line,
       command_listener* listener)
  : command(name, command_line, listener) {

}

/**
 *  Copy constructor
 *
 *  @param[in] right Object to copy.
 */
raw::raw(raw const& right)
  : command(right) {

}

/**
 *  Destructor.
 */
raw::~raw() throw () {

}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
raw& raw::operator=(raw const& right) {
  if (this != &right)
    command::operator=(right);
  return (*this);
}

/**
 *  Get a pointer on a copy of the same object.
 *
 *  @return Return a pointer on a copy object.
 */
commands::command* raw::clone() const {
  return (new raw(*this));
}

/**
 *  Run a command.
 *
 *  @param[in] args    The command arguments.
 *  @param[in] macros  The macros data struct.
 *  @param[in] timeout The command timeout.
 *
 *  @return The command id.
 */
unsigned long raw::run(
                     std::string const& processed_cmd,
                     nagios_macros& macros,
                     unsigned int timeout) {
  logger(dbg_functions, basic) << "start " << __func__;

  process* p(NULL);
  unsigned long id(get_uniq_id());
  {
    concurrency::locker lock(&_lock);
    p = _get_free_process();
    _processes_busy[p] = id;
  }

  environment env;
  _build_environment_macros(macros, env);

  try {
    logger(dbg_commands, basic)
      << "raw command (id=" << id
      << ") start '" << processed_cmd << "'";
    p->exec(processed_cmd.c_str(), env.data(), timeout);
  }
  catch (...) {
    concurrency::locker lock(&_lock);
    _processes_busy.erase(p);
    throw;
  }

  logger(dbg_functions, basic) << "end " << __func__;
  return (id);
}

/**
 *  Run a command and wait the result.
 *
 *  @param[in]  args    The command arguments.
 *  @param[in]  macros  The macros data struct.
 *  @param[in]  timeout The command timeout.
 *  @param[out] res     The result of the command.
 */
void raw::run(
            std::string const& processed_cmd,
            nagios_macros& macros,
            unsigned int timeout,
            result& res) {
  logger(dbg_functions, basic) << "start " << __func__;

  process p;
  unsigned long cmd_id(get_uniq_id());

  environment env;
  _build_environment_macros(macros, env);

  logger(dbg_commands, basic)
    << "raw command (id=" << cmd_id
    << ") start '" << processed_cmd << "'";
  p.exec(processed_cmd.c_str(), env.data(), timeout);

  // Wait for completion.
  p.wait();

  // Get process output.
  p.read(res.output);

  // Set result informations.
  res.command_id = cmd_id;
  res.start_time = p.start_time();
  res.end_time = p.end_time();
  res.exit_code = p.exit_code();
  res.exit_status = p.exit_status();

  logger(dbg_functions, basic) << "end " << __func__;
  return ;
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
void raw::data_is_available(process& p) throw () {
  (void)p;
  return;
}

/**
 *  Provide by process_listener interface but not used.
 *
 *  @param[in] p  Unused.
 */
void raw::data_is_available_err(process& p) throw () {
  (void)p;
  return;
}

/**
 *  Provide by process_listener interface. Call at the end
 *  of the process execution.
 *
 *  @param[in] p  The process to finished.
 */
void raw::finished(process& p) throw () {
  logger(dbg_functions, basic) << "start " << __func__;

  concurrency::locker lock(&_lock);
  umap<process*, unsigned long>::iterator
    it(_processes_busy.find(&p));
  if (it == _processes_busy.end()) {
    logger(log_runtime_warning, basic)
      << "invalid process pointer: "
      "process not found into process busy list";
    return;
  }
  unsigned long cmd_id(it->second);
  _processes_busy.erase(it);

  logger(dbg_commands, basic)
    << "raw command (id=" << cmd_id << ") finished.";

  // Build check result.
  result res;

  // Get process output.
  p.read(res.output);

  // Set result informations.
  res.command_id = cmd_id;
  res.start_time = p.start_time();
  res.end_time = p.end_time();
  res.exit_code = p.exit_code();
  res.exit_status = p.exit_status();

  // Forward result to the listener.
  if (_listener)
    (_listener->finished)(res);

  _processes_free.push_back(&p);

  logger(dbg_functions, basic) << "end " << __func__;
  return;
}

/**
 *  Build argv macro environment variables.
 *
 *  @param[in]  macros  The macros data struct.
 *  @param[out] env     The environment to fill.
 */
void raw::_build_argv_macro_environment(
            nagios_macros const& macros,
            environment& env) {
  for (unsigned int i(0); i < MAX_COMMAND_ARGUMENTS; ++i) {
    char const* value(macros.argv[i] ? macros.argv[i] : "");
    std::ostringstream oss;
    oss << MACRO_ENV_VAR_PREFIX "ARG" << (i + 1) << "=" << value;
    env.add(oss.str());
  }
  return;
}

/**
 *  Build contact address environment variables.
 *
 *  @param[in]  macros  The macros data struct.
 *  @param[out] env     The environment to fill.
 */
void raw::_build_contact_address_environment(
            nagios_macros const& macros,
            environment& env) {
  if (!macros.contact_ptr)
    return;
  for (unsigned int i(0); i < MAX_CONTACT_ADDRESSES; ++i) {
    char const* value(macros.contact_ptr->address[i]);
    if (!value)
      value = "";
    std::ostringstream oss;
    oss << MACRO_ENV_VAR_PREFIX "CONTACTADDRESS" << i << "=" << value;
    env.add(oss.str());
  }
  return;
}

/**
 *  Build custom contact macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_custom_contact_macro_environment(
            nagios_macros& macros,
            environment& env) {
  // Build custom contact variable.
  contact* hst(macros.contact_ptr);
  if (hst) {
    for (customvariablesmember* customvar(hst->custom_variables);
         customvar;
         customvar = customvar->next)
      if (customvar->variable_name) {
        char const* value(customvar->variable_value);
        if (!value)
          value = "";
        std::string name("_CONTACT");
        name.append(customvar->variable_name);
        add_custom_variable_to_object(
          &macros.custom_contact_vars,
          name.c_str(),
          value);
      }
  }
  // Set custom contact variable into the environement
  for (customvariablesmember* customvar(macros.custom_contact_vars);
       customvar;
       customvar = customvar->next)
    if (customvar->variable_name) {
      char const* value("");
      if (customvar->variable_value)
        value = clean_macro_chars(
                  customvar->variable_value,
                  STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
      std::string line;
      line.append(MACRO_ENV_VAR_PREFIX);
      line.append(customvar->variable_name);
      line.append("=");
      line.append(value);
      env.add(line);
    }
  return;
}

/**
 *  Build custom host macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_custom_host_macro_environment(
            nagios_macros& macros,
            environment& env) {
  // Build custom host variable.
  host* hst(macros.host_ptr);
  if (hst) {
    for (customvariablesmember* customvar(hst->custom_variables);
         customvar;
         customvar = customvar->next)
      if (customvar->variable_name) {
        char const* value("");
        if (customvar->variable_value)
          value = customvar->variable_value;
        std::string name("_HOST");
        name.append(customvar->variable_name);
        add_custom_variable_to_object(
          &macros.custom_host_vars,
          name.c_str(),
          value);
      }
  }
  // Set custom host variable into the environement
  for (customvariablesmember* customvar(macros.custom_host_vars);
       customvar;
       customvar = customvar->next)
    if (customvar->variable_name) {
      char const* value("");
      if (customvar->variable_value)
        value = clean_macro_chars(
                  customvar->variable_value,
                  STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
      std::string line;
      line.append(MACRO_ENV_VAR_PREFIX);
      line.append(customvar->variable_name);
      line.append("=");
      line.append(value);
      env.add(line);
    }
  return;
}

/**
 *  Build custom service macro environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_custom_service_macro_environment(
            nagios_macros& macros,
            environment& env) {
  // Build custom service variable.
  service* hst(macros.service_ptr);
  if (hst) {
    for (customvariablesmember* customvar(hst->custom_variables);
         customvar;
         customvar = customvar->next)
      if (customvar->variable_name) {
        char const* value(customvar->variable_value);
        if (!value)
          value = "";
        std::string name("_SERVICE");
        name.append(customvar->variable_name);
        add_custom_variable_to_object(
          &macros.custom_service_vars,
          name.c_str(),
          value);
      }
  }
  // Set custom service variable into the environement
  for (customvariablesmember* customvar(macros.custom_service_vars);
       customvar;
       customvar = customvar->next)
    if (customvar->variable_name) {
      char const* value("");
      if (customvar->variable_value)
        value = clean_macro_chars(
                  customvar->variable_value,
                  STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
      std::string line;
      line.append(MACRO_ENV_VAR_PREFIX);
      line.append(customvar->variable_name);
      line.append("=");
      line.append(value);
      env.add(line);
    }
  return;
}

/**
 *  Build all macro environemnt variable.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_environment_macros(
            nagios_macros& macros,
            environment& env) {
  if (config.get_enable_environment_macros()) {
    _build_macrosx_environment(macros, env);
    _build_argv_macro_environment(macros, env);
    _build_custom_host_macro_environment(macros, env);
    _build_custom_service_macro_environment(macros, env);
    _build_custom_contact_macro_environment(macros, env);
    _build_contact_address_environment(macros, env);
  }
  return;
}

/**
 *  Build macrox environment variables.
 *
 *  @param[in,out] macros  The macros data struct.
 *  @param[out]    env     The environment to fill.
 */
void raw::_build_macrosx_environment(
            nagios_macros& macros,
            environment& env) {
  for (unsigned int i(0); i < MACRO_X_COUNT; ++i) {
    int release_memory(0);

    // Need to grab macros?
    if (!macros.x[i]) {
      // Skip summary macro in lage instalation tweaks.
      if ((i < MACRO_TOTALHOSTSUP
           || i > MACRO_TOTALSERVICEPROBLEMSUNHANDLED)
          && !config.get_use_large_installation_tweaks()) {
        grab_macrox_value_r(
          &macros,
          i,
          NULL,
          NULL,
          &macros.x[i],
          &release_memory);
      }
    }

    // Add into the environment.
    if (macro_x_names[i]) {
      std::string line;
      line.append(MACRO_ENV_VAR_PREFIX);
      line.append(macro_x_names[i]);
      line.append("=");
      line.append(macros.x[i] ? macros.x[i] : "");
      env.add(line);
    }

    // Release memory if necessary.
    if (release_memory) {
      delete[] macros.x[i];
      macros.x[i] = NULL;
    }
  }
  return;
}

/**
 *  Get one process to execute command.
 *
 *  @return A process.
 */
process* raw::_get_free_process() {
  if (_processes_free.empty()) {
    process* p(new process(this));
    p->enable_stream(process::err, false);
    return (p);
  }
  process* p(_processes_free.front());
  _processes_free.pop_front();
  return (p);
}
