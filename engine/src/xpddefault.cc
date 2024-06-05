/**
 * Copyright 2000-2008 Ethan Galstad
 * Copyright 2011-2013 Merethis
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

#include "com/centreon/engine/xpddefault.hh"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;

static commands::command* xpddefault_host_perfdata_command_ptr(nullptr);
static commands::command* xpddefault_service_perfdata_command_ptr(nullptr);

/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

// initializes performance data.
int xpddefault_initialize_performance_data() {
  char* temp_command_name(nullptr);

  // reset vars.
  xpddefault_host_perfdata_command_ptr = nullptr;
  xpddefault_service_perfdata_command_ptr = nullptr;

  // verify that performance data commands are valid.
  if (!config->host_perfdata_command().empty()) {
    char* temp_buffer(string::dup(config->host_perfdata_command()));

    // get the command name, leave any arguments behind.
    temp_command_name = my_strtok(temp_buffer, "!");

    command_map::iterator cmd_found =
        commands::command::commands.find(temp_command_name);

    if (cmd_found == commands::command::commands.end() || !cmd_found->second) {
      engine_logger(log_runtime_warning, basic)
          << "Warning: Host performance command '" << temp_command_name
          << "' was not found - host performance data will not "
             "be processed!";
      runtime_logger->warn(
          "Warning: Host performance command '{}' was not found - host "
          "performance data will not "
          "be processed!",
          temp_command_name);
    } else
      xpddefault_host_perfdata_command_ptr =
          cmd_found->second.get();  // save the command pointer for later.

    delete[] temp_buffer;
  }

  if (!config->service_perfdata_command().empty()) {
    char* temp_buffer(string::dup(config->service_perfdata_command()));

    // get the command name, leave any arguments behind.
    temp_command_name = my_strtok(temp_buffer, "!");

    command_map::iterator cmd_found =
        commands::command::commands.find(temp_command_name);

    if (cmd_found == commands::command::commands.end() || !cmd_found->second) {
      engine_logger(log_runtime_warning, basic)
          << "Warning: Service performance command '" << temp_command_name
          << "' was not found - service performance data will not "
             "be processed!";
      runtime_logger->warn(
          "Warning: Service performance command '{}' was not found - service "
          "performance data will not "
          "be processed!",
          temp_command_name);
    } else
      xpddefault_service_perfdata_command_ptr = cmd_found->second.get();

    // free memory.
    delete[] temp_buffer;
  }

  return OK;
}

/******************************************************************/
/****************** PERFORMANCE DATA FUNCTIONS ********************/
/******************************************************************/

// updates service performance data.
int xpddefault_update_service_performance_data(
    com::centreon::engine::service* svc) {
  nagios_macros* mac(get_global_macros());

  /*
   * bail early if we've got nothing to do so we don't spend a lot
   * of time calculating macros that never get used
   */
  if (!svc || svc->get_perf_data().empty())
    return OK;
  if (config->service_perfdata_command().empty())
    return OK;

  grab_host_macros_r(mac, svc->get_host_ptr());
  grab_service_macros_r(mac, svc);

  // run the performance data command.
  xpddefault_run_service_performance_data_command(mac, svc);

  // get rid of used memory we won't need anymore.
  clear_argv_macros_r(mac);

  // now free() it all.
  clear_volatile_macros_r(mac);

  return OK;
}

// updates host performance data.
int xpddefault_update_host_performance_data(host* hst) {
  nagios_macros* mac(get_global_macros());

  /*
   * bail early if we've got nothing to do so we don't spend a lot
   * of time calculating macros that never get used
   */
  if (!hst || !hst->get_perf_data().empty())
    return OK;

  // set up macros and get to work.
  grab_host_macros_r(mac, hst);

  // run the performance data command.
  xpddefault_run_host_performance_data_command(mac, hst);

  // no more commands to run, so we won't need this any more.
  clear_argv_macros_r(mac);

  // free() all.
  clear_volatile_macros_r(mac);

  return OK;
}

/******************************************************************/
/************** PERFORMANCE DATA COMMAND FUNCTIONS ****************/
/******************************************************************/

// runs the service performance data command.
int xpddefault_run_service_performance_data_command(
    nagios_macros* mac,
    com::centreon::engine::service* svc) {
  std::string raw_command_line;
  std::string processed_command_line;
  bool early_timeout = false;
  double exectime;
  int result(OK);
  int macro_options(STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);

  engine_logger(dbg_functions, basic)
      << "run_service_performance_data_command()";
  functions_logger->trace("run_service_performance_data_command()");

  if (svc == nullptr)
    return ERROR;

  // we don't have a command.
  if (config->service_perfdata_command().empty())
    return OK;

  // get the raw command line.
  get_raw_command_line_r(mac, xpddefault_service_perfdata_command_ptr,
                         config->service_perfdata_command().c_str(),
                         raw_command_line, macro_options);
  if (raw_command_line.c_str())
    return ERROR;

  engine_logger(dbg_perfdata, most)
      << "Raw service performance data command line: " << raw_command_line;
  commands_logger->debug("Raw service performance data command line: {}",
                         raw_command_line);

  // process any macros in the raw command line.
  process_macros_r(mac, raw_command_line, processed_command_line,
                   macro_options);
  if (processed_command_line.empty())
    return ERROR;

  engine_logger(dbg_perfdata, most) << "Processed service performance data "
                                       "command line: "
                                    << processed_command_line;
  commands_logger->debug("Processed service performance data command line: {}",
                         processed_command_line);

  // run the command.
  try {
    std::string tmp;
    my_system_r(mac, processed_command_line, config->perfdata_timeout(),
                &early_timeout, &exectime, tmp, 0);
  } catch (std::exception const& e) {
    engine_logger(log_runtime_error, basic)
        << "Error: can't execute service performance data command line '"
        << processed_command_line << "' : " << e.what();
    runtime_logger->error(
        "Error: can't execute service performance data command line '{}' : {}",
        processed_command_line, e.what());
  }

  // check to see if the command timed out.
  if (early_timeout)
    engine_logger(log_runtime_warning, basic)
        << "Warning: Service performance data command '"
        << processed_command_line << "' for service '" << svc->description()
        << "' on host '" << svc->get_hostname() << "' timed out after "
        << config->perfdata_timeout() << " seconds";
  runtime_logger->warn(
      "Warning: Service performance data command '{}' for service '{}' on host "
      "'{}' timed out after {} seconds",
      processed_command_line, svc->description(), svc->get_hostname(),
      config->perfdata_timeout());

  return result;
}

// runs the host performance data command.
int xpddefault_run_host_performance_data_command(nagios_macros* mac,
                                                 host* hst) {
  std::string raw_command_line;
  std::string processed_command_line;
  bool early_timeout = false;
  double exectime;
  int result(OK);
  int macro_options(STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);

  engine_logger(dbg_functions, basic) << "run_host_performance_data_command()";
  functions_logger->trace("run_host_performance_data_command()");

  if (hst == nullptr)
    return ERROR;

  // we don't have a command.
  if (config->host_perfdata_command().empty())
    return OK;

  // get the raw command line.
  get_raw_command_line_r(mac, xpddefault_host_perfdata_command_ptr,
                         config->host_perfdata_command().c_str(),
                         raw_command_line, macro_options);
  if (raw_command_line.empty())
    return ERROR;

  engine_logger(dbg_perfdata, most)
      << "Raw host performance data command line: " << raw_command_line;
  commands_logger->info("Raw host performance data command line: {}",
                        raw_command_line);

  // process any macros in the raw command line.
  process_macros_r(mac, raw_command_line, processed_command_line,
                   macro_options);

  engine_logger(dbg_perfdata, most)
      << "Processed host performance data command line: "
      << processed_command_line;
  commands_logger->info("Processed host performance data command line: {}",
                        processed_command_line);

  // run the command.
  try {
    std::string tmp;
    my_system_r(mac, processed_command_line, config->perfdata_timeout(),
                &early_timeout, &exectime, tmp, 0);
  } catch (std::exception const& e) {
    engine_logger(log_runtime_error, basic)
        << "Error: can't execute host performance data command line '"
        << processed_command_line << "' : " << e.what();
    runtime_logger->error(
        "Error: can't execute host performance data command line '{}' : {}",
        processed_command_line, e.what());
  }

  if (processed_command_line.empty())
    return ERROR;

  // check to see if the command timed out.
  if (early_timeout)
    engine_logger(log_runtime_warning, basic)
        << "Warning: Host performance data command '" << processed_command_line
        << "' for host '" << hst->name() << "' timed out after "
        << config->perfdata_timeout() << " seconds";
  runtime_logger->warn(
      "Warning: Host performance data command '{}' for host '{}' timed out "
      "after {} seconds",
      processed_command_line, hst->name(), config->perfdata_timeout());

  return result;
}

// processes delimiter characters in templates.
void xpddefault_preprocess_file_templates(char* tmpl) {
  if (!tmpl)
    return;
  char *tmp1{tmpl}, *tmp2{tmpl};

  for (; *tmp1 != 0; tmp1++, tmp2++) {
    if (*tmp1 == '\\') {
      switch (tmp1[1]) {
        case 't':
          *tmp2 = '\t';
          tmp1++;
          break;
        case 'r':
          *tmp2 = '\r';
          tmp1++;
          break;
        case 'n':
          *tmp2 = '\n';
          tmp1++;
          break;
        default:
          *tmp2 = *tmp1;
          break;
      }
    } else
      *tmp2 = *tmp1;
  }
  *tmp2 = 0;
}
