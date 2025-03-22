/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
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

#include "com/centreon/engine/sehandlers.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/checkable.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/downtimes/downtime.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/neberrors.hh"
#include "com/centreon/engine/utils.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::logging;

/******************************************************************/
/************* OBSESSIVE COMPULSIVE HANDLER FUNCTIONS *************/
/******************************************************************/

/* handles host check results in an obsessive compulsive manner... */
int obsessive_compulsive_host_check_processor(
    com::centreon::engine::host* hst) {
  std::string raw_command;
  std::string processed_command;
  bool early_timeout = false;
  double exectime = 0.0;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
  nagios_macros* mac(get_global_macros());

  bool obsess_over_hosts;
  uint32_t ochp_timeout;
  obsess_over_hosts = pb_indexed_config.state().obsess_over_hosts();
  const std::string& ochp_command = pb_indexed_config.state().ochp_command();
  ochp_timeout = pb_indexed_config.state().ochp_timeout();

  engine_logger(dbg_functions, basic)
      << "obsessive_compulsive_host_check_processor()";
  functions_logger->trace("obsessive_compulsive_host_check_processor()");

  if (hst == nullptr)
    return ERROR;

  /* bail out if we shouldn't be obsessing */
  if (!obsess_over_hosts)
    return OK;
  if (!hst->obsess_over())
    return OK;

  /* if there is no valid command, exit */
  if (ochp_command.empty())
    return ERROR;

  /* update macros */
  grab_host_macros_r(mac, hst);

  /* get the raw command line */
  get_raw_command_line_r(mac, ochp_command_ptr, ochp_command.c_str(),
                         raw_command, macro_options);
  if (raw_command.empty()) {
    clear_volatile_macros_r(mac);
    return ERROR;
  }

  engine_logger(dbg_checks, most)
      << "Raw obsessive compulsive host processor command line: "
      << raw_command;
  checks_logger->debug(
      "Raw obsessive compulsive host processor command line: {}", raw_command);

  /* process any macros in the raw command line */
  process_macros_r(mac, raw_command, processed_command, macro_options);
  if (processed_command.empty()) {
    clear_volatile_macros_r(mac);
    return ERROR;
  }

  engine_logger(dbg_checks, most)
      << "Processed obsessive compulsive host processor "
         "command line: "
      << processed_command;
  checks_logger->debug(
      "Processed obsessive compulsive host processor "
      "command line: {}",
      processed_command);

  if (hst->command_is_allowed_by_whitelist(processed_command,
                                           checkable::OBSESS_TYPE)) {
    /* run the command */
    try {
      std::string tmp;
      my_system_r(mac, processed_command, ochp_timeout, &early_timeout,
                  &exectime, tmp, 0);
    } catch (std::exception const& e) {
      engine_logger(log_runtime_error, basic)
          << "Error: can't execute compulsive host processor command line '"
          << processed_command << "' : " << e.what();
      runtime_logger->error(
          "Error: can't execute compulsive host processor command line '{}' : "
          "{}",
          processed_command, e.what());
    }
  } else {
    runtime_logger->error(
        "Error: can't execute compulsive host processor command line '{}' : it "
        "is not allowed by the whitelist",
        processed_command);
  }
  clear_volatile_macros_r(mac);

  /* check to see if the command timed out */
  if (early_timeout)
    engine_logger(log_runtime_warning, basic)
        << "Warning: OCHP command '" << processed_command << "' for host '"
        << hst->name() << "' timed out after " << ochp_timeout << " seconds";
  runtime_logger->warn(
      "Warning: OCHP command '{}' for host '{}' timed out after {} seconds",
      processed_command, hst->name(), ochp_timeout);

  return OK;
}

/******************************************************************/
/**************** SERVICE EVENT HANDLER FUNCTIONS *****************/
/******************************************************************/

/* runs the global service event handler */
int run_global_service_event_handler(nagios_macros* mac,
                                     com::centreon::engine::service* svc) {
  std::string raw_command;
  std::string processed_command;
  std::string processed_logentry;
  std::string command_output;
  bool early_timeout = false;
  double exectime = 0.0;
  struct timeval start_time;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

  engine_logger(dbg_functions, basic) << "run_global_service_event_handler()";
  functions_logger->trace("run_global_service_event_handler()");

  bool enable_event_handlers;
  bool log_event_handlers;
  uint32_t event_handler_timeout;
  enable_event_handlers = pb_indexed_config.state().enable_event_handlers();
  const std::string& global_service_event_handler =
      pb_indexed_config.state().global_service_event_handler();
  log_event_handlers = pb_indexed_config.state().log_event_handlers();
  event_handler_timeout = pb_indexed_config.state().event_handler_timeout();

  if (svc == nullptr)
    return ERROR;

  /* bail out if we shouldn't be running event handlers */
  if (!enable_event_handlers)
    return OK;

  /* a global service event handler command has not been defined */
  if (global_service_event_handler.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, more)
      << "Running global event handler for service '" << svc->description()
      << "' on host '" << svc->get_hostname() << "'...";
  events_logger->debug(
      "Running global event handler for service '{}' on host '{}'...",
      svc->description(), svc->get_hostname());

  /* get start time */
  gettimeofday(&start_time, nullptr);

  /* get the raw command line */
  get_raw_command_line_r(mac, global_service_event_handler_ptr,
                         global_service_event_handler.c_str(), raw_command,
                         macro_options);
  if (raw_command.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, most)
      << "Raw global service event handler command line: " << raw_command;
  events_logger->debug("Raw global service event handler command line: {}",
                       raw_command);

  /* process any macros in the raw command line */
  process_macros_r(mac, raw_command, processed_command, macro_options);
  if (processed_command.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, most)
      << "Processed global service event handler "
         "command line: "
      << processed_command;
  events_logger->debug(
      "Processed global service event handler command line: {}",
      processed_command);

  if (log_event_handlers) {
    std::ostringstream oss;
    oss << "GLOBAL SERVICE EVENT HANDLER: " << svc->get_hostname() << ';'
        << svc->description()
        << ";$SERVICESTATE$;$SERVICESTATETYPE$;$SERVICEATTEMPT$;"
        << global_service_event_handler;
    process_macros_r(mac, oss.str(), processed_logentry, macro_options);
    engine_logger(log_event_handler, basic) << processed_logentry;
    events_logger->debug(processed_logentry);
  }

  static checkable::static_whitelist_last_result cached_cmd;

  if (checkable::command_is_allowed_by_whitelist(processed_command,
                                                 cached_cmd)) {
    /* run the command */
    try {
      my_system_r(mac, processed_command, event_handler_timeout, &early_timeout,
                  &exectime, command_output, 0);
    } catch (std::exception const& e) {
      engine_logger(log_runtime_error, basic)
          << "Error: can't execute global service event handler "
             "command line '"
          << processed_command << "' : " << e.what();
      runtime_logger->error(
          "Error: can't execute global service event handler "
          "command line '{}' : {}",
          processed_command, e.what());
    }
  } else {
    runtime_logger->error(
        "Error: can't execute global service event handler "
        "command line '{}' : it is not allowed by the whitelist",
        processed_command);
  }

  /* check to see if the event handler timed out */
  if (early_timeout) {
    engine_logger(log_event_handler | log_runtime_warning, basic)
        << "Warning: Global service event handler command '"
        << processed_command << "' timed out after " << event_handler_timeout
        << " seconds";
    events_logger->info(
        "Warning: Global service event handler command '{}' timed out after {} "
        "seconds",
        processed_command, event_handler_timeout);
  }
  return OK;
}

/* runs a service event handler command */
int run_service_event_handler(nagios_macros* mac,
                              com::centreon::engine::service* svc) {
  std::string raw_command;
  std::string processed_command;
  std::string processed_logentry;
  std::string command_output;
  bool early_timeout = false;
  double exectime = 0.0;
  struct timeval start_time;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

  bool log_event_handlers;
  uint32_t event_handler_timeout;
  log_event_handlers = pb_indexed_config.state().log_event_handlers();
  event_handler_timeout = pb_indexed_config.state().event_handler_timeout();

  engine_logger(dbg_functions, basic) << "run_service_event_handler()";
  functions_logger->trace("run_service_event_handler()");

  if (svc == nullptr)
    return ERROR;

  /* bail if there's no command */
  if (svc->event_handler().empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, more)
      << "Running event handler for service '" << svc->description()
      << "' on host '" << svc->get_hostname() << "'...";
  events_logger->debug("Running event handler for service '{}' on host '{}'...",
                       svc->description(), svc->get_hostname());

  /* get start time */
  gettimeofday(&start_time, nullptr);

  /* get the raw command line */
  get_raw_command_line_r(mac, svc->get_event_handler_ptr(),
                         svc->event_handler().c_str(), raw_command,
                         macro_options);
  if (raw_command.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, most)
      << "Raw service event handler command line: " << raw_command;
  events_logger->debug("Raw service event handler command line: {}",
                       raw_command);

  /* process any macros in the raw command line */
  process_macros_r(mac, raw_command, processed_command, macro_options);
  if (processed_command.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, most)
      << "Processed service event handler command line: " << processed_command;
  events_logger->debug("Processed service event handler command line: {}",
                       processed_command);

  if (log_event_handlers) {
    std::ostringstream oss;
    oss << "SERVICE EVENT HANDLER: " << svc->get_hostname() << ';'
        << svc->description()
        << ";$SERVICESTATE$;$SERVICESTATETYPE$;$SERVICEATTEMPT$;"
        << svc->event_handler();
    process_macros_r(mac, oss.str(), processed_logentry, macro_options);
    engine_logger(log_event_handler, basic) << processed_logentry;
    events_logger->info(processed_logentry);
  }

  if (svc->command_is_allowed_by_whitelist(processed_command,
                                           checkable::EVH_TYPE)) {
    /* run the command */
    try {
      my_system_r(mac, processed_command, event_handler_timeout, &early_timeout,
                  &exectime, command_output, 0);
    } catch (std::exception const& e) {
      engine_logger(log_runtime_error, basic)
          << "Error: can't execute service event handler command line '"
          << processed_command << "' : " << e.what();
      runtime_logger->error(
          "Error: can't execute service event handler command line '{}' : {}",
          processed_command, e.what());
    }
  } else {
    runtime_logger->error(
        "Error: can't execute service event handler command line '{}' : it is "
        "not allowed by the whitelist",
        processed_command);
  }

  /* check to see if the event handler timed out */
  if (early_timeout) {
    engine_logger(log_event_handler | log_runtime_warning, basic)
        << "Warning: Service event handler command '" << processed_command
        << "' timed out after " << event_handler_timeout << " seconds";
    events_logger->info(
        "Warning: Service event handler command '{}' timed out after {} "
        "seconds",
        processed_command, event_handler_timeout);
  }
  return OK;
}

/******************************************************************/
/****************** HOST EVENT HANDLER FUNCTIONS ******************/
/******************************************************************/

/* handles a change in the status of a host */
int handle_host_event(com::centreon::engine::host* hst) {
  nagios_macros* mac(get_global_macros());

  engine_logger(dbg_functions, basic) << "handle_host_event()";
  functions_logger->trace("handle_host_event()");

  if (hst == nullptr)
    return ERROR;

  bool enable_event_handlers;
  std::string_view global_host_event_handler;
  enable_event_handlers = pb_indexed_config.state().enable_event_handlers();
  global_host_event_handler =
      pb_indexed_config.state().global_host_event_handler();

  /* bail out if we shouldn't be running event handlers */
  if (!enable_event_handlers)
    return OK;
  if (!hst->event_handler_enabled())
    return OK;

  /* update host macros */
  grab_host_macros_r(mac, hst);

  /* run the global host event handler */
  run_global_host_event_handler(mac, hst);

  /* run the event handler command if there is one */
  if (!hst->event_handler().empty())
    run_host_event_handler(mac, hst);

  /* send data to event broker */
  broker_external_command(NEBTYPE_EXTERNALCOMMAND_CHECK, CMD_NONE, nullptr);

  return OK;
}

/* runs the global host event handler */
int run_global_host_event_handler(nagios_macros* mac,
                                  com::centreon::engine::host* hst) {
  std::string raw_command;
  std::string processed_command;
  std::string processed_logentry;
  std::string command_output;
  bool early_timeout = false;
  double exectime = 0.0;
  struct timeval start_time;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

  engine_logger(dbg_functions, basic) << "run_global_host_event_handler()";
  functions_logger->trace("run_global_host_event_handler()");

  bool enable_event_handlers;
  bool log_event_handlers;
  uint32_t event_handler_timeout;
  enable_event_handlers = pb_indexed_config.state().enable_event_handlers();
  const std::string& global_host_event_handler =
      pb_indexed_config.state().global_host_event_handler();
  log_event_handlers = pb_indexed_config.state().log_event_handlers();
  event_handler_timeout = pb_indexed_config.state().event_handler_timeout();

  if (hst == nullptr)
    return ERROR;

  /* bail out if we shouldn't be running event handlers */
  if (!enable_event_handlers)
    return OK;

  /* no global host event handler command is defined */
  if (global_host_event_handler.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, more)
      << "Running global event handler for host '" << hst->name() << "'...";
  events_logger->debug("Running global event handler for host '{}'...",
                       hst->name());

  /* get start time */
  gettimeofday(&start_time, nullptr);

  /* get the raw command line */
  get_raw_command_line_r(mac, global_host_event_handler_ptr,
                         global_host_event_handler.c_str(), raw_command,
                         macro_options);
  if (raw_command.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, most)
      << "Raw global host event handler command line: " << raw_command;
  events_logger->debug("Raw global host event handler command line: {}",
                       raw_command);

  /* process any macros in the raw command line */
  process_macros_r(mac, raw_command, processed_command, macro_options);
  if (processed_command.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, most)
      << "Processed global host event handler "
         "command line: "
      << processed_command;
  events_logger->debug("Processed global host event handler command line: {}",
                       processed_command);

  if (log_event_handlers) {
    std::ostringstream oss;
    oss << "GLOBAL HOST EVENT HANDLER: " << hst->name()
        << "$HOSTSTATE$;$HOSTSTATETYPE$;$HOSTATTEMPT$;"
        << global_host_event_handler;
    process_macros_r(mac, oss.str(), processed_logentry, macro_options);
    engine_logger(log_event_handler, basic) << processed_logentry;
    events_logger->info(processed_logentry);
  }

  static checkable::static_whitelist_last_result cached_cmd;

  if (host::command_is_allowed_by_whitelist(processed_command, cached_cmd)) {
    /* run the command */
    try {
      my_system_r(mac, processed_command, event_handler_timeout, &early_timeout,
                  &exectime, command_output, 0);
    } catch (std::exception const& e) {
      engine_logger(log_runtime_error, basic)
          << "Error: can't execute global host event handler command line '"
          << processed_command << "' : " << e.what();
      runtime_logger->error(
          "Error: can't execute global host event handler command line '{}' : "
          "{}",
          processed_command, e.what());
    }
  } else {
    runtime_logger->error(
        "Error: can't execute global host event handler command line '{}' : it "
        "is not allowed by the whitelist",
        processed_command);
  }

  /* check for a timeout in the execution of the event handler command */
  if (early_timeout) {
    engine_logger(log_event_handler | log_runtime_warning, basic)
        << "Warning: Global host event handler command '" << processed_command
        << "' timed out after " << event_handler_timeout << " seconds";
    events_logger->info(
        "Warning: Global host event handler command '{}' timed out after {} "
        "seconds",
        processed_command, event_handler_timeout);
  }

  return OK;
}

/* runs a host event handler command */
int run_host_event_handler(nagios_macros* mac,
                           com::centreon::engine::host* hst) {
  std::string raw_command;
  std::string processed_command;
  std::string processed_logentry;
  std::string command_output;
  bool early_timeout = false;
  double exectime = 0.0;
  struct timeval start_time;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

  bool log_event_handlers;
  uint32_t event_handler_timeout;
  log_event_handlers = pb_indexed_config.state().log_event_handlers();
  event_handler_timeout = pb_indexed_config.state().event_handler_timeout();

  engine_logger(dbg_functions, basic) << "run_host_event_handler()";
  functions_logger->trace("run_host_event_handler()");

  if (hst == nullptr)
    return ERROR;

  /* bail if there's no command */
  if (hst->event_handler().empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, more)
      << "Running event handler for host '" << hst->name() << "'...";
  events_logger->debug("Running event handler for host '{}'...", hst->name());

  /* get start time */
  gettimeofday(&start_time, nullptr);

  /* get the raw command line */
  get_raw_command_line_r(mac, hst->get_event_handler_ptr(),
                         hst->event_handler().c_str(), raw_command,
                         macro_options);
  if (raw_command.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, most)
      << "Raw host event handler command line: " << raw_command;
  events_logger->debug("Raw host event handler command line: {}", raw_command);

  /* process any macros in the raw command line */
  process_macros_r(mac, raw_command, processed_command, macro_options);
  if (processed_command.empty())
    return ERROR;

  engine_logger(dbg_eventhandlers, most)
      << "Processed host event handler command line: " << processed_command;
  events_logger->debug("Processed host event handler command line: {}",
                       processed_command);

  if (log_event_handlers) {
    std::ostringstream oss;
    oss << "HOST EVENT HANDLER: " << hst->name()
        << ";$HOSTSTATE$;$HOSTSTATETYPE$;$HOSTATTEMPT$;"
        << hst->event_handler();
    process_macros_r(mac, oss.str(), processed_logentry, macro_options);
    engine_logger(log_event_handler, basic) << processed_logentry;
    events_logger->info(processed_logentry);
  }

  if (hst->command_is_allowed_by_whitelist(processed_command,
                                           checkable::EVH_TYPE)) {
    /* run the command */
    try {
      my_system_r(mac, processed_command, event_handler_timeout, &early_timeout,
                  &exectime, command_output, 0);
    } catch (std::exception const& e) {
      engine_logger(log_runtime_error, basic)
          << "Error: can't execute host event handler command line '"
          << processed_command << "' : " << e.what();
      runtime_logger->error(
          "Error: can't execute host event handler command line '{}' : {}",
          processed_command, e.what());
    }
  } else {
    runtime_logger->error(
        "Error: can't execute host event handler command line '{}' : it is not "
        "allowed by the whitelist",
        processed_command);
  }

  /* check to see if the event handler timed out */
  if (early_timeout) {
    engine_logger(log_event_handler | log_runtime_warning, basic)
        << "Warning: Host event handler command '" << processed_command
        << "' timed out after " << event_handler_timeout << " seconds";
    events_logger->info(
        "Warning: Host event handler command '{}' timed out after {} seconds",
        processed_command, event_handler_timeout);
  }
  return OK;
}
