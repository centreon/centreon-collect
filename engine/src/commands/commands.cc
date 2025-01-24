/**
 * Copyright 1999-2008           Ethan Galstad
 * Copyright 2011-2013,2015-2024 Centreon
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

#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/commands/processing.hh"

#include <absl/strings/escaping.h>
#include <sys/time.h>
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/processing.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/downtimes/downtime_finder.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/string.hh"
#include "mmap.h"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::logging;

/******************************************************************/
/****************** EXTERNAL COMMAND PROCESSING *******************/
/******************************************************************/

/* checks for the existence of the external command file and processes all
 * commands found in it */
int check_for_external_commands() {
  functions_logger->error("CHECK_FOR_EXTERNAL_COMMANDS... TEST");
  functions_logger->trace("check_for_external_commands()");

#ifdef LEGACY_CONF
  bool check_external_commands = config->check_external_commands();
#else
  bool check_external_commands = pb_config.check_external_commands();
#endif

  /* bail out if we shouldn't be checking for external commands */
  if (!check_external_commands)
    return ERROR;

  /* update last command check time */
  last_command_check = time(nullptr);

  /* update the status log with new program information */
  /* go easy on the frequency of this if we're checking often - only update
   * program status every 10 seconds.... */
  if (last_command_check >= (last_command_status_update + 10)) {
    last_command_status_update = last_command_check;
    update_program_status(false);
  }

  /* process all commands found in the buffer */
  for (;;) {
    boost::optional<std::string> cmd = external_command_buffer.pop();
    if (!cmd) {
      break;
    }

    /* process the command */
    process_external_command(cmd->c_str());
  }

  return OK;
}

/**
 *  Processes all external commands in a (regular) file.
 *
 *  @param[in] file        File to process.
 *  @param[in] delete_file If non-zero, delete file after all commands
 *                         have been processed.
 *
 *  @return OK on success.
 */
int process_external_commands_from_file(char const* file, int delete_file) {
  engine_logger(dbg_functions, basic)
      << "process_external_commands_from_file()";

  functions_logger->trace("process_external_commands_from_file()");

  if (!file)
    return ERROR;

  engine_logger(dbg_external_command, more)
      << "Processing commands from file '" << file << "'.  File will "
      << (delete_file ? "be" : "NOT be") << " deleted after processing.";

  external_command_logger->debug(
      "Processing commands from file '{}'.  File will {} deleted after "
      "processing.",
      file, delete_file ? "be" : "NOT be");

  /* open the config file for reading */
  mmapfile* thefile(nullptr);
  if ((thefile = mmap_fopen(file)) == nullptr) {
    engine_logger(log_info_message, basic)
        << "Error: Cannot open file '" << file
        << "' to process external commands!";
    config_logger->info(
        "Error: Cannot open file '{}' to process external commands!", file);
    return ERROR;
  }

  /* process all commands in the file */
  char* input(nullptr);
  while (1) {
    /* free memory */
    delete[] input;

    /* read the next line */
    if ((input = mmap_fgets(thefile)) == nullptr)
      break;

    /* process the command */
    process_external_command(input);
  }

  /* close the file */
  mmap_fclose(thefile);

  /* delete the file */
  if (delete_file)
    ::remove(file);

  return OK;
}

/* external command processor */
void process_external_command(const char* cmd) {
  commands::processing::execute(cmd);
}

/******************************************************************/
/*************** EXTERNAL COMMAND IMPLEMENTATIONS  ****************/
/******************************************************************/

/* adds a host or service comment to the status log */
int cmd_add_comment(int cmd, time_t entry_time, char* args) {
  char* temp_ptr(nullptr);
  host* temp_host(nullptr);
  char* host_name;
  char* svc_description(nullptr);
  char* user(nullptr);
  char* comment_data(nullptr);
  bool persistent{false};
  uint64_t service_id = 0;
  const char* command_name;

  /* get the host name */
  if ((host_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* if we're adding a service comment...  */
  if (cmd == CMD_ADD_SVC_COMMENT) {
    command_name = "ADD_SVC_COMMENT";

    /* get the service description */
    if ((svc_description = my_strtok(nullptr, ";")) == nullptr)
      return ERROR;

    /* verify that the service is valid */
    service_map::const_iterator found(
        service::services.find({host_name, svc_description}));
    if (found == service::services.end() || !found->second)
      return ERROR;
    service_id = found->second->service_id();
  } else {
    command_name = "ADD_HOST_COMMENT";
  }

  /* else verify that the host is valid */
  temp_host = nullptr;
  host_map::const_iterator it(host::hosts.find(host_name));
  if (it != host::hosts.end())
    temp_host = it->second.get();
  if (temp_host == nullptr)
    return ERROR;

  /* get the persistent flag */
  if ((temp_ptr = my_strtok(nullptr, ";")) == nullptr)
    return ERROR;

  if (!absl::SimpleAtob(temp_ptr, &persistent)) {
    external_command_logger->error(
        "Error: could not {} : persistent '{}' must be 1 or 0", command_name,
        temp_ptr);
    return ERROR;
  }

  /* get the name of the user who entered the comment */
  if ((user = my_strtok(nullptr, ";")) == nullptr)
    return ERROR;

  /* get the comment */
  if ((comment_data = my_strtok(nullptr, "\n")) == nullptr)
    return ERROR;

  /* add the comment */
  auto com = std::make_shared<comment>(
      (cmd == CMD_ADD_HOST_COMMENT) ? comment::host : comment::service,
      comment::user, temp_host->host_id(), service_id, entry_time, user,
      comment_data, persistent, comment::external, false, (time_t)0);
  uint64_t comment_id = com->get_comment_id();
  comment::comments.insert({comment_id, com});
  external_command_logger->trace("{}, comment_id: {}, data: {}", command_name,
                                 comment_id, com->get_comment_data());
  return OK;
}

/* removes a host or service comment from the status log */
int cmd_delete_comment(int cmd [[maybe_unused]], char* args) {
  uint64_t comment_id{0};
  /* get the comment id we should delete */
  if (!absl::SimpleAtoi(args, &comment_id)) {
    external_command_logger->error(
        "Error: could not delete comment : comment_id '{}' must be an "
        "integer >= 0",
        args);
    return ERROR;
  }
  /* delete the specified comment */
  comment::delete_comment(comment_id);
  return OK;
}

/* removes all comments associated with a host or service from the status log */
int cmd_delete_all_comments(int cmd, char* args) {
  char* host_name(nullptr);
  char* svc_description(nullptr);

  /* get the host name */
  if ((host_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  host* temp_host = nullptr;
  service* temp_service = nullptr;
  /* if we're deleting service comments...  */
  if (cmd == CMD_DEL_ALL_SVC_COMMENTS) {
    /* get the service description */
    if ((svc_description = my_strtok(nullptr, ";")) == nullptr)
      return ERROR;

    /* verify that the service is valid */
    service_map::const_iterator found(
        service::services.find({host_name, svc_description}));
    if (found != service::services.end())
      temp_service = found->second.get();
    if (temp_service == nullptr)
      return ERROR;
    /* delete comments */
    comment::delete_service_comments(temp_service->host_id(),
                                     temp_service->service_id());
  } else {
    /* else verify that the host is valid */
    host_map::const_iterator it(host::hosts.find(host_name));
    if (it != host::hosts.end())
      temp_host = it->second.get();
    if (temp_host == nullptr)
      return ERROR;
    /* delete comments */
    comment::delete_host_comments(temp_host->host_id());
  }
  return OK;
}

/* delays a host or service notification for given number of minutes */
int cmd_delay_notification(int cmd, char* args) {
  char* temp_ptr(nullptr);
  host* temp_host(nullptr);
  char* host_name(nullptr);
  char* svc_description(nullptr);
  time_t delay_time(0);
  service_map::const_iterator found;

  /* get the host name */
  if ((host_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* if this is a service notification delay...  */
  if (cmd == CMD_DELAY_SVC_NOTIFICATION) {
    /* get the service description */
    if ((svc_description = my_strtok(nullptr, ";")) == nullptr)
      return ERROR;

    /* verify that the service is valid */
    found = service::services.find({host_name, svc_description});

    if (found == service::services.end() || !found->second)
      return ERROR;
  }

  /* else verify that the host is valid */
  else {
    temp_host = nullptr;
    host_map::const_iterator it(host::hosts.find(host_name));
    if (it != host::hosts.end())
      temp_host = it->second.get();
    if (temp_host == nullptr)
      return ERROR;
  }

  /* get the time that we should delay until... */
  if ((temp_ptr = my_strtok(nullptr, "\n")) == nullptr)
    return ERROR;
  if (!absl::SimpleAtoi(temp_ptr, &delay_time)) {
    external_command_logger->error(
        "Error: could not delay notification : delay_time '{}' must be "
        "an integer",
        temp_ptr);
    return ERROR;
  }

  /* delay the next notification... */
  if (cmd == CMD_DELAY_HOST_NOTIFICATION)
    temp_host->set_next_notification(delay_time);
  else
    found->second->set_next_notification(delay_time);

  return OK;
}

/* schedules a host check at a particular time */
int cmd_schedule_check(int cmd, char* args) {
  char* temp_ptr(nullptr);
  host* temp_host(nullptr);
  char* host_name(nullptr);
  char* svc_description(nullptr);
  time_t delay_time(0);
  service_map::const_iterator found;

  /* get the host name */
  if ((host_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  if (cmd == CMD_SCHEDULE_HOST_CHECK || cmd == CMD_SCHEDULE_FORCED_HOST_CHECK ||
      cmd == CMD_SCHEDULE_HOST_SVC_CHECKS ||
      cmd == CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS) {
    /* verify that the host is valid */
    temp_host = nullptr;
    host_map::const_iterator it(host::hosts.find(host_name));
    if (it != host::hosts.end())
      temp_host = it->second.get();
    if (temp_host == nullptr)
      return ERROR;
  } else {
    /* get the service description */
    if ((svc_description = my_strtok(nullptr, ";")) == nullptr)
      return ERROR;

    /* verify that the service is valid */
    found = service::services.find({host_name, svc_description});

    if (found == service::services.end() || !found->second)
      return ERROR;
  }

  /* get the next check time */
  if ((temp_ptr = my_strtok(nullptr, "\n")) == nullptr)
    return ERROR;
  if (!absl::SimpleAtoi(temp_ptr, &delay_time)) {
    external_command_logger->error(
        "Error: could not schedule check : delay_time '{}' must be "
        "an integer",
        temp_ptr);
    return ERROR;
  }

  /* schedule the host check */
  if (cmd == CMD_SCHEDULE_HOST_CHECK || cmd == CMD_SCHEDULE_FORCED_HOST_CHECK)
    temp_host->schedule_check(delay_time,
                              cmd == CMD_SCHEDULE_FORCED_HOST_CHECK
                                  ? CHECK_OPTION_FORCE_EXECUTION
                                  : CHECK_OPTION_NONE);

  /* schedule service checks */
  else if (cmd == CMD_SCHEDULE_HOST_SVC_CHECKS ||
           cmd == CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS) {
    for (service_map_unsafe::iterator it(temp_host->services.begin()),
         end(temp_host->services.end());
         it != end; ++it) {
      if (!it->second)
        continue;
      it->second->schedule_check(delay_time,
                                 cmd == CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS
                                     ? CHECK_OPTION_FORCE_EXECUTION
                                     : CHECK_OPTION_NONE);
    }
  } else
    found->second->schedule_check(delay_time,
                                  cmd == CMD_SCHEDULE_FORCED_SVC_CHECK
                                      ? CHECK_OPTION_FORCE_EXECUTION
                                      : CHECK_OPTION_NONE);

  return OK;
}

/* schedules all service checks on a host for a particular time */
int cmd_schedule_host_service_checks(int cmd, char* args, int force) {
  char* temp_ptr(nullptr);
  host* temp_host(nullptr);
  char* host_name(nullptr);
  time_t delay_time(0);

  (void)cmd;

  /* get the host name */
  if ((host_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* verify that the host is valid */
  temp_host = nullptr;
  host_map::const_iterator it(host::hosts.find(host_name));
  if (it != host::hosts.end())
    temp_host = it->second.get();
  if (temp_host == nullptr)
    return ERROR;

  /* get the next check time */
  if ((temp_ptr = my_strtok(nullptr, "\n")) == nullptr)
    return ERROR;
  if (!absl::SimpleAtoi(temp_ptr, &delay_time)) {
    external_command_logger->error(
        "Error: could not schedule host service checks : delay_time '{}' "
        "must be an integer",
        temp_ptr);
    return ERROR;
  }

  /* reschedule all services on the specified host */
  for (service_map_unsafe::iterator it(temp_host->services.begin()),
       end(temp_host->services.end());
       it != end; ++it) {
    if (!it->second)
      continue;
    it->second->schedule_check(
        delay_time, (force) ? CHECK_OPTION_FORCE_EXECUTION : CHECK_OPTION_NONE);
  }

  return OK;
}

/* schedules a program shutdown or restart */
void cmd_signal_process(int cmd, char* args) {
  time_t scheduled_time(0);
  char* temp_ptr(nullptr);

  /* get the time to schedule the event */
  if ((temp_ptr = my_strtok(args, "\n")) == nullptr)
    scheduled_time = 0L;
  else if (!absl::SimpleAtoi(temp_ptr, &scheduled_time)) {
    external_command_logger->error(
        "Error: could not signal process : scheduled_time '{}' "
        "must be an integer",
        temp_ptr);
    return;
  }

  /* add a scheduled program shutdown or restart to the event list */
  events::loop::instance().schedule(
      std::make_unique<timed_event>(
          cmd == CMD_SHUTDOWN_PROCESS ? timed_event::EVENT_PROGRAM_SHUTDOWN
                                      : timed_event::EVENT_PROGRAM_RESTART,
          scheduled_time, false, 0, nullptr, false, nullptr, nullptr, 0),
      true);
}

/**
 *  Processes results of an external service check.
 *
 *  @param[in]     cmd         Command ID.
 *  @param[in]     check_time  Check time.
 *  @param[in,out] args        Command arguments.
 *
 *  @return OK on success.
 */
int cmd_process_service_check_result(int cmd [[maybe_unused]],
                                     time_t check_time,
                                     char* args) {
#ifdef LEGACY_CONF
  bool accept_passive_service_checks = config->accept_passive_service_checks();
#else
  bool accept_passive_service_checks =
      pb_config.accept_passive_service_checks();
#endif

  /* skip this service check result if we aren't accepting passive service
   * checks */
  if (!accept_passive_service_checks)
    return ERROR;

  auto a{absl::StrSplit(args, absl::MaxSplits(';', 3))};
  auto ait = a.begin();
  if (ait == a.end())
    return ERROR;

  std::string real_host_name;
  auto host_name = *ait;
  ++ait;

  if (ait == a.end())
    return ERROR;
  std::string svc_description{ait->data(), ait->size()};
  ++ait;

  /* find the host by its name or address */
  host_map::const_iterator it(host::hosts.find(host_name));
  if (it != host::hosts.end() && it->second)
    real_host_name = std::string(host_name.data(), host_name.size());
  else {
    for (host_map::iterator itt = host::hosts.begin(), end = host::hosts.end();
         itt != end; ++itt) {
      if (itt->second && itt->second->get_address() == host_name) {
        real_host_name = itt->first;
        it = itt;
        break;
      }
    }
  }

  /* we couldn't find the host */
  if (real_host_name.empty()) {
    engine_logger(log_runtime_warning, basic)
        << "Warning:  Passive check result was received for service '"
        << svc_description << "' on host '" << real_host_name
        << "', but the host could not be found!";
    runtime_logger->warn(
        "Warning:  Passive check result was received for service '{}' on host "
        "'{}', but the host could not be found!",
        svc_description, host_name);
    return ERROR;
  }

  /* make sure the service exists */
  service_map::const_iterator found(
      service::services.find({real_host_name, svc_description}));
  if (found == service::services.end() || !found->second) {
    engine_logger(log_runtime_warning, basic)
        << "Warning:  Passive check result was received for service '"
        << svc_description << "' on host '" << real_host_name
        << "', but the service could not be found!";
    runtime_logger->warn(
        "Warning:  Passive check result was received for service '{}' on "
        "host "
        "'{}', but the service could not be found!",
        svc_description, host_name);
    return ERROR;
  }

  /* skip this is we aren't accepting passive checks for this service */
  if (!found->second->passive_checks_enabled())
    return ERROR;

  int32_t return_code;
  if (!absl::SimpleAtoi(*ait, &return_code))
    return ERROR;
  ++ait;

  // replace \\n with \n
  std::string output(ait->data(), ait->size());
  string::unescape(output);

  timeval tv;
  gettimeofday(&tv, nullptr);

  timeval set_tv = {.tv_sec = check_time, .tv_usec = 0};

  check_result::pointer result = std::make_shared<check_result>(
      service_check, found->second.get(), checkable::check_passive,
      CHECK_OPTION_NONE, false,
      static_cast<double>(tv.tv_sec - check_time) +
          static_cast<double>(tv.tv_usec / 1000000.0),
      set_tv, set_tv, false, true, return_code, std::move(output));

  /* make sure the return code is within bounds */
  if (result->get_return_code() < 0 || result->get_return_code() > 3) {
    result->set_return_code(service::state_unknown);
  }

  if (result->get_latency() < 0.0) {
    result->set_latency(0.0);
  }

  checks::checker::instance().add_check_result_to_reap(result);

  return OK;
}

/* submits a passive service check result for later processing */
int process_passive_service_check(time_t check_time,
                                  char const* host_name,
                                  char const* svc_description,
                                  int return_code,
                                  char const* output) {
  char const* real_host_name(nullptr);

#ifdef LEGACY_CONF
  bool accept_passive_service_checks = config->accept_passive_service_checks();
#else
  bool accept_passive_service_checks =
      pb_config.accept_passive_service_checks();
#endif

  /* skip this service check result if we aren't accepting passive service
   * checks */
  if (!accept_passive_service_checks)
    return ERROR;

  /* make sure we have all required data */
  if (host_name == nullptr || svc_description == nullptr || output == nullptr)
    return ERROR;

  /* find the host by its name or address */
  host_map::const_iterator it(host::hosts.find(host_name));
  if (it != host::hosts.end() && it->second)
    real_host_name = host_name;
  else {
    for (host_map::iterator itt(host::hosts.begin()), end(host::hosts.end());
         itt != end; ++itt) {
      if (itt->second && itt->second->get_address() == host_name) {
        real_host_name = itt->first.c_str();
        it = itt;
        break;
      }
    }
  }

  /* we couldn't find the host */
  if (real_host_name == nullptr) {
    engine_logger(log_runtime_warning, basic)
        << "Warning:  Passive check result was received for service '"
        << svc_description << "' on host '" << host_name
        << "', but the host could not be found!";
    runtime_logger->warn(
        "Warning:  Passive check result was received for service '{}' on "
        "host "
        "'{}', but the host could not be found!",
        svc_description, host_name);
    return ERROR;
  }

  /* make sure the service exists */
  service_map::const_iterator found(
      service::services.find({real_host_name, svc_description}));
  if (found == service::services.end() || !found->second) {
    engine_logger(log_runtime_warning, basic)
        << "Warning:  Passive check result was received for service '"
        << svc_description << "' on host '" << host_name
        << "', but the service could not be found!";
    runtime_logger->warn(
        "Warning:  Passive check result was received for service '{}' on "
        "host "
        "'{}', but the service could not be found!",
        svc_description, host_name);
    return ERROR;
  }

  /* skip this is we aren't accepting passive checks for this service */
  if (!found->second->passive_checks_enabled())
    return ERROR;

  timeval tv;
  gettimeofday(&tv, nullptr);

  timeval set_tv = {.tv_sec = check_time, .tv_usec = 0};

  check_result::pointer result = std::make_shared<check_result>(
      service_check, found->second.get(), checkable::check_passive,
      CHECK_OPTION_NONE, false,
      static_cast<double>(tv.tv_sec - check_time) +
          static_cast<double>(tv.tv_usec / 1000000.0),
      set_tv, set_tv, false, true, return_code, output);

  /* make sure the return code is within bounds */
  if (result->get_return_code() < 0 || result->get_return_code() > 3) {
    result->set_return_code(service::state_unknown);
  }

  if (result->get_latency() < 0.0) {
    result->set_latency(0.0);
  }

  checks::checker::instance().add_check_result_to_reap(result);

  return OK;
}

/**
 *  Processes results of an external host check.
 *
 *  @param[in]     cmd         Command ID.
 *  @param[in]     check_time  Check time.
 *  @param[in,out] args        Command arguments.
 *
 *  @return OK on success.
 */
int cmd_process_host_check_result(int cmd, time_t check_time, char* args) {
  (void)cmd;

  if (!args)
    return ERROR;

  // Get the host name.
  auto split = absl::StrSplit(args, ';');
  auto split_it = split.begin();

  if (split_it == split.end())
    return ERROR;

  // Get the host check return code and output.
  std::string host_name = std::string(*split_it);

  int return_code;
  ++split_it;

  if (split_it == split.end())
    return ERROR;

  if (!absl::SimpleAtoi(*split_it, &return_code))
    return ERROR;

  ++split_it;

  std::string output = "";
  if (split_it != split.end()) {
    output = split_it->data();
    // replace \\n with \n
    string::unescape(output);
  }

  // Submit the check result.
  return process_passive_host_check(check_time, host_name.c_str(), return_code,
                                    output.c_str());
}

/* process passive host check result */
int process_passive_host_check(time_t check_time,
                               char const* host_name,
                               int return_code,
                               char const* output) {
  char const* real_host_name(nullptr);

#ifdef LEGACY_CONF
  bool accept_passive_service_checks = config->accept_passive_service_checks();
#else
  bool accept_passive_service_checks =
      pb_config.accept_passive_service_checks();
#endif

  /* skip this host check result if we aren't accepting passive host checks */
  if (!accept_passive_service_checks)
    return ERROR;

  /* make sure we have all required data */
  if (host_name == nullptr || output == nullptr)
    return ERROR;

  /* make sure we have a reasonable return code */
  if (return_code < 0 || return_code > 2)
    return ERROR;

  /* find the host by its name or address */
  host_map::const_iterator it(host::hosts.find(host_name));
  if (it != host::hosts.end() && it->second)
    real_host_name = host_name;
  else {
    for (host_map::iterator itt(host::hosts.begin()), end(host::hosts.end());
         itt != end; ++itt) {
      if (itt->second && itt->second->get_address() == host_name) {
        real_host_name = itt->first.c_str();
        it = itt;
        break;
      }
    }
  }

  /* we couldn't find the host */
  if (real_host_name == nullptr) {
    engine_logger(log_runtime_warning, basic)
        << "Warning:  Passive check result was received for host '" << host_name
        << "', but the host could not be found!";
    runtime_logger->warn(
        "Warning:  Passive check result was received for host '{}', but the "
        "host could not be found!",
        host_name);
    return ERROR;
  }

  /* skip this is we aren't accepting passive checks for this host */
  if (!it->second->passive_checks_enabled())
    return ERROR;

  timeval tv;
  gettimeofday(&tv, nullptr);
  timeval tv_start = {.tv_sec = check_time, .tv_usec = 0};

  check_result::pointer result = std::make_shared<check_result>(
      host_check, it->second.get(), checkable::check_passive, CHECK_OPTION_NONE,
      false,
      static_cast<double>(tv.tv_sec - check_time) +
          static_cast<double>(tv.tv_usec / 1000000.0),
      tv_start, tv_start, false, true, return_code, output);

  /* make sure the return code is within bounds */
  if (result->get_return_code() < 0 || result->get_return_code() > 3)
    result->set_return_code(service::state_unknown);

  if (result->get_latency() < 0.0)
    result->set_latency(0.0);

  checks::checker::instance().add_check_result_to_reap(result);

  return OK;
}

/* acknowledges a host or service problem */
int cmd_acknowledge_problem(int cmd, char* args) {
  std::string host_name;
  std::string svc_description;
  std::string ack_author;
  std::string ack_data;
  int type(AckType::NORMAL);
  int notify(true);
  int persistent(true);
  service_map::const_iterator found;

  string::c_strtok arg(args);

  /* get the host name */
  if (!arg.extract(';', host_name))
    return ERROR;

  /* verify that the host is valid */
  host_map::const_iterator it(host::hosts.find(host_name));
  if (it == host::hosts.end() || !it->second)
    return ERROR;

  /* this is a service acknowledgement */
  if (cmd == CMD_ACKNOWLEDGE_SVC_PROBLEM) {
    /* get the service name */
    if (!arg.extract(';', svc_description))
      return ERROR;

    /* verify that the service is valid */
    found = service::services.find({it->second->name(), svc_description});

    if (found == service::services.end() || !found->second)
      return ERROR;
  }

  /* get the type */
  if (!arg.extract(';', type))
    return ERROR;
  external_command_logger->trace("New acknowledgement with type {}", type);

  /* get the notification option */
  int ival;
  if (!arg.extract(';', ival))
    return ERROR;

  notify = (ival > 0) ? true : false;

  /* get the persistent option */
  if (!arg.extract(';', ival))
    return ERROR;
  persistent = (ival > 0) ? true : false;

  /* get the acknowledgement author */
  if (!arg.extract(';', ack_author))
    return ERROR;

  /* get the acknowledgement data */
  if (!arg.extract('\n', ack_data)) {
    return ERROR;
  }

  /* acknowledge the host problem */
  if (cmd == CMD_ACKNOWLEDGE_HOST_PROBLEM)
    acknowledge_host_problem(it->second.get(), ack_author, ack_data, type,
                             notify, persistent);
  /* acknowledge the service problem */
  else
    acknowledge_service_problem(found->second.get(), ack_author, ack_data, type,
                                notify, persistent);

  return OK;
}

/* removes a host or service acknowledgement */
int cmd_remove_acknowledgement(int cmd, char* args) {
  auto a{absl::StrSplit(args, ';')};
  auto ait = a.begin();
  if (ait == a.end())
    return ERROR;

  /* verify that the host is valid */
  auto hostname = *ait;
  ++ait;

  switch (cmd) {
    case CMD_REMOVE_HOST_ACKNOWLEDGEMENT: {
      host_map::const_iterator hit = host::hosts.find(hostname);
      if (hit == host::hosts.end() || !hit->second)
        return ERROR;
      remove_host_acknowledgement(hit->second.get());
    } break;
    case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
      /* we are removing a service acknowledgement */
      {
        /* get the service name */
        if (ait == a.end())
          return ERROR;

        /* verify that the service is valid */
        service_map::const_iterator sit = service::services.find(
            std::make_pair(std::string(hostname.data(), hostname.size()),
                           std::string(ait->data(), ait->size())));

        if (sit == service::services.end() || !sit->second)
          return ERROR;
        remove_service_acknowledgement(sit->second.get());
      }
      break;
  }
  return OK;
}

/* schedules downtime for a specific host or service */
int cmd_schedule_downtime(int cmd, time_t entry_time, char* args) {
  host* temp_host{nullptr};
  service* temp_service{nullptr};
  host* last_host{nullptr};
  hostgroup* hg{nullptr};
  time_t start_time{0};
  time_t end_time{0};
  bool fixed{false};
  uint64_t triggered_by{0};
  unsigned long duration{0};
  uint64_t downtime_id{0};
  servicegroup_map::const_iterator sg_it;

  auto a{absl::StrSplit(args, ';')};
  auto ait = a.begin();

  if (ait == a.end())
    return ERROR;

  if (cmd == CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME ||
      cmd == CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME) {
    /* get the hostgroup name */
    hostgroup_map::const_iterator it = hostgroup::hostgroups.find(*ait);
    if (it == hostgroup::hostgroups.end() || !it->second)
      return ERROR;
    hg = it->second.get();
    ++ait;
  } else if (cmd == CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME ||
             cmd == CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME) {
    /* get the servicegroup name */
    sg_it = servicegroup::servicegroups.find(*ait);
    if (sg_it == servicegroup::servicegroups.end() || !sg_it->second)
      return ERROR;
    ++ait;
  } else {
    /* get the host name */
    host_map::const_iterator it{host::hosts.find(*ait)};
    if (it == host::hosts.end() || !it->second)
      return ERROR;
    ++ait;
    temp_host = it->second.get();

    /* this is a service downtime */
    if (cmd == CMD_SCHEDULE_SVC_DOWNTIME) {
      /* get the service name */
      if (ait == a.end())
        return ERROR;
      service_map::const_iterator found = service::services.find(
          {temp_host->name(), std::string(ait->data(), ait->size())});

      if (found == service::services.end() || !found->second)
        return ERROR;
      ++ait;
      temp_service = found->second.get();
    }
  }

  /* get the start time */
  if (ait == a.end())
    return ERROR;
  if (!absl::SimpleAtoi(*ait, &start_time)) {
    external_command_logger->error(
        "Error: could not schedule downtime : start_time '{}' must be "
        "an integer",
        *ait);
    return ERROR;
  }
  ++ait;

  /* get the end time */
  if (ait == a.end())
    return ERROR;
  if (!absl::SimpleAtoi(*ait, &end_time)) {
    external_command_logger->error(
        "Error: could not schedule downtime : end_time '{}' must be "
        "an integer",
        *ait);
    return ERROR;
  }
  ++ait;

  /* get the fixed flag */
  if (ait == a.end())
    return ERROR;
  if (!absl::SimpleAtob(*ait, &fixed)) {
    external_command_logger->error(
        "Error: could not schedule downtime : fixed '{}' must be 1 or 0", *ait);
    return ERROR;
  }
  ++ait;

  /* get the trigger id */
  if (ait == a.end())
    return ERROR;
  if (!absl::SimpleAtoi(*ait, &triggered_by)) {
    external_command_logger->error(
        "Error: could not schedule downtime : triggered_by '{}' must be an "
        "integer >= 0",
        *ait);
    return ERROR;
  }
  ++ait;

  /* get the duration */
  if (ait == a.end())
    return ERROR;
  if (!ait->empty()) {
    if (!absl::SimpleAtoi(*ait, &duration)) {
      external_command_logger->error(
          "Error: could not schedule downtime : duration '{}' must be an "
          "integer "
          ">= 0",
          *ait);
      return ERROR;
    }
  }
  ++ait;

  /* get the author */
  if (ait == a.end())
    return ERROR;
  std::string author(ait->data(), ait->size());
  ++ait;

  /* get the comment */
  if (ait == a.end())
    return ERROR;
  std::string comment_data(ait->data(), ait->size());

  /* check if flexible downtime demanded and duration set to non-zero.
  ** according to the documentation, a flexible downtime is started
  ** between start and end time and will last for "duration" seconds.
  ** strtoul converts a nullptr value to 0 so if set to 0, bail out as a
  ** duration>0 is needed.
  */
  if (!fixed && !duration) {
    SPDLOG_LOGGER_ERROR(external_command_logger,
                        "no duration defined for a fixed downtime");
    return ERROR;
  }

  /* duration should be auto-calculated, not user-specified */
  if (fixed)
    duration = (unsigned long)(end_time - start_time);

  /* schedule downtime */
  switch (cmd) {
    case CMD_SCHEDULE_HOST_DOWNTIME:
      downtime_manager::instance().schedule_downtime(
          downtime::host_downtime, temp_host->host_id(), 0, entry_time,
          author.c_str(), comment_data.c_str(), start_time, end_time, fixed,
          triggered_by, duration, &downtime_id);
      break;

    case CMD_SCHEDULE_SVC_DOWNTIME:
      downtime_manager::instance().schedule_downtime(
          downtime::service_downtime, temp_service->host_id(),
          temp_service->service_id(), entry_time, author.c_str(),
          comment_data.c_str(), start_time, end_time, fixed, triggered_by,
          duration, &downtime_id);
      break;

    case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
      for (service_map_unsafe::iterator it = temp_host->services.begin(),
                                        end = temp_host->services.end();
           it != end; ++it) {
        if (!it->second)
          continue;
        downtime_manager::instance().schedule_downtime(
            downtime::service_downtime, temp_host->host_id(),
            it->second->service_id(), entry_time, author.c_str(),
            comment_data.c_str(), start_time, end_time, fixed, triggered_by,
            duration, &downtime_id);
      }
      break;

    case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
      for (host_map_unsafe::iterator it = hg->members.begin(),
                                     end = hg->members.end();
           it != end; ++it)
        downtime_manager::instance().schedule_downtime(
            downtime::host_downtime, it->second->host_id(), 0, entry_time,
            author.c_str(), comment_data.c_str(), start_time, end_time, fixed,
            triggered_by, duration, &downtime_id);
      break;

    case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
      for (host_map_unsafe::iterator it(hg->members.begin()),
           end(hg->members.end());
           it != end; ++it) {
        if (!it->second)
          continue;
        for (service_map_unsafe::iterator it2 = it->second->services.begin(),
                                          end2 = it->second->services.end();
             it2 != end2; ++it2) {
          if (!it2->second)
            continue;
          downtime_manager::instance().schedule_downtime(
              downtime::service_downtime, it2->second->host_id(),
              it2->second->service_id(), entry_time, author.c_str(),
              comment_data.c_str(), start_time, end_time, fixed, triggered_by,
              duration, &downtime_id);
        }
      }
      break;

    case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
      last_host = nullptr;
      for (service_map_unsafe::iterator it(sg_it->second->members.begin()),
           end(sg_it->second->members.end());
           it != end; ++it) {
        temp_host = nullptr;
        host_map::const_iterator found = host::hosts.find(it->first.first);
        if (found == host::hosts.end() || !found->second)
          continue;
        temp_host = found->second.get();
        if (last_host == temp_host)
          continue;
        downtime_manager::instance().schedule_downtime(
            downtime::host_downtime, it->second->host_id(), 0, entry_time,
            author.c_str(), comment_data.c_str(), start_time, end_time, fixed,
            triggered_by, duration, &downtime_id);
        last_host = temp_host;
      }
      break;

    case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
      for (service_map_unsafe::iterator it(sg_it->second->members.begin()),
           end(sg_it->second->members.end());
           it != end; ++it)
        downtime_manager::instance().schedule_downtime(
            downtime::service_downtime, it->second->host_id(),
            it->second->service_id(), entry_time, author.c_str(),
            comment_data.c_str(), start_time, end_time, fixed, triggered_by,
            duration, &downtime_id);
      break;

    case CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME:
      /* schedule downtime for "parent" host */
      downtime_manager::instance().schedule_downtime(
          downtime::host_downtime, temp_host->host_id(), 0, entry_time,
          author.c_str(), comment_data.c_str(), start_time, end_time, fixed,
          triggered_by, duration, &downtime_id);

      /* schedule (non-triggered) downtime for all child hosts */
      schedule_and_propagate_downtime(temp_host, entry_time, author.c_str(),
                                      comment_data.c_str(), start_time,
                                      end_time, fixed, 0, duration);
      break;

    case CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME:
      /* schedule downtime for "parent" host */
      downtime_manager::instance().schedule_downtime(
          downtime::host_downtime, temp_host->host_id(), 0, entry_time,
          author.c_str(), comment_data.c_str(), start_time, end_time, fixed,
          triggered_by, duration, &downtime_id);

      /* schedule triggered downtime for all child hosts */
      schedule_and_propagate_downtime(temp_host, entry_time, author.c_str(),
                                      comment_data.c_str(), start_time,
                                      end_time, fixed, downtime_id, duration);
      break;

    default:
      break;
  }
  return OK;
}

/* deletes scheduled host or service downtime */
int cmd_delete_downtime(int cmd, char* args) {
  uint64_t downtime_id(0);
  char* temp_ptr(nullptr);

  /* Get the id of the downtime to delete. */
  if (nullptr == (temp_ptr = my_strtok(args, "\n")))
    return ERROR;

  if (!absl::SimpleAtoi(temp_ptr, &downtime_id)) {
    external_command_logger->error(
        "Error: could not delete downtime : downtime_id '{}' must be an "
        "integer >= 0",
        temp_ptr);
    return ERROR;
  }

  if (CMD_DEL_HOST_DOWNTIME == cmd || CMD_DEL_SVC_DOWNTIME == cmd)
    downtime_manager::instance().unschedule_downtime(downtime_id);

  return OK;
}

/**
 *  Delete scheduled host or service downtime, according to some criterias.
 *
 *  @param[in] cmd   Command ID.
 *  @param[in] args  Command arguments.
 */
int cmd_delete_downtime_full(int cmd, char* args) {
  functions_logger->trace("cmd_delete_downtime_full() args = {}", args);
  downtime_finder::criteria_set criterias;

  auto a{absl::StrSplit(args, ';')};
  auto it = a.begin();

  // Host name.
  if (it == a.end())
    return ERROR;
  if (!it->empty())
    criterias.emplace_back("host", std::string(it->data(), it->size()));

  ++it;

  // Service description and downtime type.
  if (cmd == CMD_DEL_SVC_DOWNTIME_FULL) {
    if (!it->empty())
      criterias.emplace_back("service", std::string(it->data(), it->size()));
    ++it;
  }

  // Start time.
  if (it == a.end())
    return ERROR;
  if (!it->empty())
    criterias.emplace_back("start", std::string(it->data(), it->size()));
  ++it;

  // End time.
  if (it == a.end())
    return ERROR;
  if (!it->empty())
    criterias.emplace_back("end", std::string(it->data(), it->size()));
  ++it;

  // Fixed.
  if (it == a.end())
    return ERROR;
  if (!it->empty())
    criterias.emplace_back("fixed", std::string(it->data(), it->size()));
  ++it;

  // Trigger ID.
  if (it == a.end())
    return ERROR;
  if (!it->empty())
    criterias.emplace_back("triggered_by", std::string(it->data(), it->size()));
  ++it;

  // Duration.
  if (it == a.end())
    return ERROR;
  if (!it->empty())
    criterias.emplace_back("duration", std::string(it->data(), it->size()));
  ++it;

  // Author.
  if (it == a.end())
    return ERROR;
  if (!it->empty())
    criterias.emplace_back("author", std::string(it->data(), it->size()));
  ++it;

  // Comment.
  if (it == a.end())
    return ERROR;
  if (!it->empty())
    criterias.emplace_back("comment", std::string(it->data(), it->size()));
  ++it;

  // Find downtimes.
  downtime_finder dtf(
      downtimes::downtime_manager::instance().get_scheduled_downtimes());
  downtime_finder::result_set result(dtf.find_matching_all(criterias));
  for (downtime_finder::result_set::const_iterator it = result.begin(),
                                                   end = result.end();
       it != end; ++it) {
    downtime_manager::instance().unschedule_downtime(*it);
  }

  return OK;
}

/*
** Some of these commands are now "distributable" as no downtime ids are
** used. Deletes scheduled host and service downtime based on hostname
** and optionally other filter arguments.
*/
int cmd_delete_downtime_by_host_name(int cmd, char* args) {
  char* temp_ptr(nullptr);
  char* end_ptr(nullptr);
  char* hostname(nullptr);
  char* service_description(nullptr);
  char* downtime_comment(nullptr);
  std::pair<bool, time_t> start_time = {false, 0};
  int deleted(0);

  (void)cmd;

  /* Get the host name of the downtime to delete. */
  temp_ptr = my_strtok(args, ";");
  if (nullptr == temp_ptr)
    return ERROR;
  hostname = temp_ptr;

  /* Get the optional service name. */
  temp_ptr = my_strtok(nullptr, ";");
  if (temp_ptr != nullptr) {
    if (*temp_ptr != '\0')
      service_description = temp_ptr;

    /* Get the optional start time. */
    temp_ptr = my_strtok(nullptr, ";");
    if (temp_ptr != nullptr) {
      start_time.second = strtoul(temp_ptr, &end_ptr, 10);
      if (temp_ptr != end_ptr)
        start_time.first = true;
      /* Get the optional comment. */
      temp_ptr = my_strtok(nullptr, ";");
      if (temp_ptr != nullptr) {
        if (*temp_ptr != '\0')
          downtime_comment = temp_ptr;
      }
    }
  }

  deleted =
      downtime_manager::instance()
          .delete_downtime_by_hostname_service_description_start_time_comment(
              hostname, service_description, start_time, downtime_comment);
  if (0 == deleted)
    return ERROR;
  return OK;
}

/* Deletes scheduled host and service downtime based on hostgroup and
 * optionally other filter arguments. */
int cmd_delete_downtime_by_hostgroup_name(int cmd, char* args) {
  char* temp_ptr(nullptr);
  char* end_ptr(nullptr);
  char* service_description(nullptr);
  char* downtime_comment(nullptr);
  char* host_name(nullptr);
  int deleted(0);
  std::pair<bool, time_t> start_time = {false, 0};

  (void)cmd;

  /* Get the host group name of the downtime to delete. */
  temp_ptr = my_strtok(args, ";");
  if (nullptr == temp_ptr)
    return ERROR;

  hostgroup_map::const_iterator it{hostgroup::hostgroups.find(temp_ptr)};
  if (it == hostgroup::hostgroups.end() || !it->second)
    return ERROR;

  /* Get the optional host name. */
  temp_ptr = my_strtok(nullptr, ";");
  if (temp_ptr != nullptr) {
    if (*temp_ptr != '\0')
      host_name = temp_ptr;

    /* Get the optional service name. */
    temp_ptr = my_strtok(nullptr, ";");
    if (temp_ptr != nullptr) {
      if (*temp_ptr != '\0')
        service_description = temp_ptr;

      /* Get the optional start time. */
      temp_ptr = my_strtok(nullptr, ";");
      if (temp_ptr != nullptr) {
        start_time.second = strtoul(temp_ptr, &end_ptr, 10);
        if (temp_ptr != end_ptr)
          start_time.first = true;
        /* Get the optional comment. */
        temp_ptr = my_strtok(nullptr, ";");
        if (temp_ptr != nullptr) {
          if (*temp_ptr != '\0')
            downtime_comment = temp_ptr;
        }
      }
    }

    /* Get the optional service name. */
    temp_ptr = my_strtok(nullptr, ";");
    if (temp_ptr != nullptr) {
      if (*temp_ptr != '\0')
        service_description = temp_ptr;

      /* Get the optional start time. */
      temp_ptr = my_strtok(nullptr, ";");
      if (temp_ptr != nullptr) {
        start_time.second = strtoul(temp_ptr, &end_ptr, 10);
        if (temp_ptr != end_ptr)
          start_time.first = true;
        /* Get the optional comment. */
        temp_ptr = my_strtok(nullptr, ";");
        if (temp_ptr != nullptr) {
          if (*temp_ptr != '\0')
            downtime_comment = temp_ptr;
        }
      }
    }
  }

  for (host_map_unsafe::iterator it_h(it->second->members.begin()),
       end_h(it->second->members.end());
       it_h != end_h; ++it_h) {
    if (!it_h->second)
      continue;
    if (host_name != nullptr && it_h->first != host_name)
      continue;
    deleted =
        downtime_manager::instance()
            .delete_downtime_by_hostname_service_description_start_time_comment(
                host_name, service_description, start_time, downtime_comment);
  }

  if (0 == deleted)
    return ERROR;

  return OK;
}

/* Delete downtimes based on start time and/or comment. */
int cmd_delete_downtime_by_start_time_comment(int cmd, char* args) {
  char* downtime_comment(nullptr);
  char* temp_ptr(nullptr);
  char* end_ptr(nullptr);
  int deleted(0);
  std::pair<bool, time_t> start_time = {false, 0};

  (void)cmd;

  /* Get start time if set. */
  temp_ptr = my_strtok(args, ";");
  if (temp_ptr != nullptr)
    /* This will be set to 0 if no start_time is entered or data is bad. */
    start_time.second = strtoul(temp_ptr, &end_ptr, 10);
  if (temp_ptr != end_ptr)
    start_time.first = true;

  /* Get comment - not sure if this should be also tokenised by ; */
  temp_ptr = my_strtok(nullptr, "\n");
  if ((temp_ptr != nullptr) && (*temp_ptr != '\0'))
    downtime_comment = temp_ptr;

  deleted =
      downtime_manager::instance()
          .delete_downtime_by_hostname_service_description_start_time_comment(
              "", "", start_time, downtime_comment);

  if (0 == deleted)
    return ERROR;

  return OK;
}

/* changes a host or service (integer) variable */
int cmd_change_object_int_var(int cmd, char* args) {
  host* temp_host(nullptr);
  char* host_name(nullptr);
  char* svc_description(nullptr);
  char* contact_name(nullptr);
  char const* temp_ptr(nullptr);
  int intval(0);
  double dval(0.0);
  double old_dval(0.0);
  time_t preferred_time(0);
  time_t next_valid_time(0);
  uint32_t attr = MODATTR_NONE;
  unsigned long hattr(MODATTR_NONE);
  unsigned long sattr(MODATTR_NONE);
  host_map::const_iterator it;
  service_map::const_iterator found_svc;
  contact_map::iterator cnct;

  switch (cmd) {
    case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:
    case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:
    case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:
    case CMD_CHANGE_SVC_MODATTR:

      /* get the host name */
      if ((host_name = my_strtok(args, ";")) == nullptr)
        return ERROR;

      /* get the service name */
      if ((svc_description = my_strtok(nullptr, ";")) == nullptr)
        return ERROR;

      /* verify that the service is valid */
      found_svc = service::services.find({host_name, svc_description});

      if (found_svc == service::services.end() || !found_svc->second)
        return ERROR;
      break;

    case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:
    case CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL:
    case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:
    case CMD_CHANGE_HOST_MODATTR:
      /* get the host name */
      if ((host_name = my_strtok(args, ";")) == nullptr)
        return ERROR;

      /* verify that the host is valid */
      temp_host = nullptr;
      it = host::hosts.find(host_name);
      if (it != host::hosts.end())
        temp_host = it->second.get();
      if (temp_host == nullptr)
        return ERROR;
      break;

    case CMD_CHANGE_CONTACT_MODATTR:
    case CMD_CHANGE_CONTACT_MODHATTR:
    case CMD_CHANGE_CONTACT_MODSATTR:
      /* get the contact name */
      if ((contact_name = my_strtok(args, ";")) == nullptr)
        return ERROR;

      cnct = contact::contacts.find(contact_name);
      /* verify that the contact is valid */
      if (cnct == contact::contacts.end() || !cnct->second)
        return ERROR;
      break;

    default:
      /* unknown command */
      return ERROR;
      break;
  }

  /* get the value */
  if ((temp_ptr = my_strtok(nullptr, ";")) == nullptr)
    return ERROR;
  intval = (int)strtol(temp_ptr, nullptr, 0);
  if (intval < 0 || (intval == 0 && errno == EINVAL))
    return ERROR;
  dval = (int)strtod(temp_ptr, nullptr);

  switch (cmd) {
    case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:
      /* save the old check interval */
      old_dval = temp_host->check_interval();

      /* modify the check interval */
      temp_host->set_check_interval(dval);
      attr = MODATTR_NORMAL_CHECK_INTERVAL;
      temp_host->set_modified_attributes(temp_host->get_modified_attributes() |
                                         attr);

      /* schedule a host check if previous interval was 0 (checks were not
       * regularly scheduled) */
      if (old_dval == 0 && temp_host->active_checks_enabled()) {
        /* set the host check flag */
        temp_host->set_should_be_scheduled(true);

        /* schedule a check for right now (or as soon as possible) */
        time(&preferred_time);
        if (!check_time_against_period(preferred_time,
                                       temp_host->check_period_ptr)) {
          get_next_valid_time(preferred_time, &next_valid_time,
                              temp_host->check_period_ptr);
          temp_host->set_next_check(next_valid_time);
        } else
          temp_host->set_next_check(preferred_time);

        /* schedule a check if we should */
        if (temp_host->get_should_be_scheduled())
          temp_host->schedule_check(temp_host->get_next_check(),
                                    CHECK_OPTION_NONE);
      }
      broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                temp_host, attr);

      /* We need check result to handle next check */
      temp_host->update_status();
      break;

    case CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL:
      temp_host->set_retry_interval(dval);
      attr = MODATTR_RETRY_CHECK_INTERVAL;
      temp_host->set_modified_attributes(temp_host->get_modified_attributes() |
                                         attr);
      broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                temp_host, attr);
      break;

    case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:
      temp_host->set_max_attempts(intval);
      attr = MODATTR_MAX_CHECK_ATTEMPTS;
      temp_host->set_modified_attributes(temp_host->get_modified_attributes() |
                                         attr);

      broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                temp_host, attr);

      /* adjust current attempt number if in a hard state */
      if (temp_host->get_state_type() == notifier::hard &&
          temp_host->get_current_state() != host::state_up &&
          temp_host->get_current_attempt() > 1) {
        temp_host->set_current_attempt(temp_host->max_check_attempts());
        /* We need check result to handle next check */
        temp_host->update_status();
      }
      break;

    case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:
      /* save the old check interval */
      old_dval = found_svc->second->check_interval();

      /* modify the check interval */
      found_svc->second->set_check_interval(dval);
      attr = MODATTR_NORMAL_CHECK_INTERVAL;

      /* schedule a service check if previous interval was 0 (checks were not
       * regularly scheduled) */
      if (old_dval == 0 && found_svc->second->active_checks_enabled() &&
          found_svc->second->check_interval() != 0) {
        /* set the service check flag */
        found_svc->second->set_should_be_scheduled(true);

        /* schedule a check for right now (or as soon as possible) */
        time(&preferred_time);
        if (!check_time_against_period(preferred_time,
                                       found_svc->second->check_period_ptr)) {
          get_next_valid_time(preferred_time, &next_valid_time,
                              found_svc->second->check_period_ptr);
          found_svc->second->set_next_check(next_valid_time);
        } else
          found_svc->second->set_next_check(preferred_time);

        /* schedule a check if we should */
        if (found_svc->second->get_should_be_scheduled())
          found_svc->second->schedule_check(found_svc->second->get_next_check(),
                                            CHECK_OPTION_NONE);
      }
      found_svc->second->set_modified_attributes(
          found_svc->second->get_modified_attributes() | attr);
      broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                                   found_svc->second.get(), attr);

      /* We need check result to handle next check */
      found_svc->second->update_status();
      break;

    case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:
      found_svc->second->set_retry_interval(dval);
      attr = MODATTR_RETRY_CHECK_INTERVAL;
      found_svc->second->set_modified_attributes(
          found_svc->second->get_modified_attributes() | attr);
      broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                                   found_svc->second.get(), attr);
      break;

    case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:
      found_svc->second->set_max_attempts(intval);
      attr = MODATTR_MAX_CHECK_ATTEMPTS;
      found_svc->second->set_modified_attributes(
          found_svc->second->get_modified_attributes() | attr);
      /* send data to event broker */
      broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                                   found_svc->second.get(), attr);

      /* adjust current attempt number if in a hard state */
      if (found_svc->second->get_state_type() == notifier::hard &&
          found_svc->second->get_current_state() != service::state_ok &&
          found_svc->second->get_current_attempt() > 1) {
        found_svc->second->set_current_attempt(
            found_svc->second->max_check_attempts());
        /* We need check result to handle next check */
        found_svc->second->update_status();
      }
      break;

    case CMD_CHANGE_HOST_MODATTR:
      attr = intval;
      temp_host->set_modified_attributes(attr);
      /* send data to event broker */
      broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                temp_host, attr);
      break;

    case CMD_CHANGE_SVC_MODATTR:
      attr = intval;
      found_svc->second->set_modified_attributes(attr);

      /* send data to event broker */
      broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                                   found_svc->second.get(), attr);
      break;

    case CMD_CHANGE_CONTACT_MODATTR:
      attr = intval;
      break;

    case CMD_CHANGE_CONTACT_MODHATTR:
      hattr = intval;
      break;

    case CMD_CHANGE_CONTACT_MODSATTR:
      sattr = intval;
      break;

    default:
      break;
  }

  /* send data to event broker and update status file */
  switch (cmd) {
    case CMD_CHANGE_CONTACT_MODATTR:
    case CMD_CHANGE_CONTACT_MODHATTR:
    case CMD_CHANGE_CONTACT_MODSATTR:
      /* set the modified attribute */
      switch (cmd) {
        case CMD_CHANGE_CONTACT_MODATTR:
          cnct->second->set_modified_attributes(attr);
          break;

        case CMD_CHANGE_CONTACT_MODHATTR:
          cnct->second->set_modified_host_attributes(hattr);
          break;

        case CMD_CHANGE_CONTACT_MODSATTR:
          cnct->second->set_modified_service_attributes(sattr);
          break;

        default:
          break;
      }
      break;

    default:
      break;
  }

  return OK;
}

/* changes a host or service (char) variable */
int cmd_change_object_char_var(int cmd, char* args) {
  host* temp_host{nullptr};
  timeperiod* temp_timeperiod{nullptr};
  char* host_name{nullptr};
  char* svc_description{nullptr};
  char* contact_name{nullptr};
  char* charval{nullptr};
  std::string temp_ptr;
  char* temp_ptr2{nullptr};
  unsigned long attr{MODATTR_NONE};
  unsigned long hattr{MODATTR_NONE};
  unsigned long sattr{MODATTR_NONE};
  host_map::const_iterator it;
  service_map::const_iterator found_svc;
  contact_map::iterator cnct;

  /* SECURITY PATCH - disable these for the time being */
  switch (cmd) {
    case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
    case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
    case CMD_CHANGE_HOST_EVENT_HANDLER:
    case CMD_CHANGE_SVC_EVENT_HANDLER:
    case CMD_CHANGE_HOST_CHECK_COMMAND:
    case CMD_CHANGE_SVC_CHECK_COMMAND:
      return ERROR;
  }

  /* get the command arguments */
  switch (cmd) {
    case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
    case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
      if ((charval = my_strtok(args, "\n")) == nullptr)
        return ERROR;
      break;

    case CMD_CHANGE_HOST_EVENT_HANDLER:
    case CMD_CHANGE_HOST_CHECK_COMMAND:
    case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
    case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:
      /* get the host name */
      if ((host_name = my_strtok(args, ";")) == nullptr)
        return ERROR;

      /* verify that the host is valid */
      temp_host = nullptr;
      it = host::hosts.find(host_name);
      if (it != host::hosts.end())
        temp_host = it->second.get();
      if (temp_host == nullptr)
        return ERROR;

      if ((charval = my_strtok(nullptr, "\n")) == nullptr)
        return ERROR;
      break;

    case CMD_CHANGE_SVC_EVENT_HANDLER:
    case CMD_CHANGE_SVC_CHECK_COMMAND:
    case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
    case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:
      /* get the host name */
      if ((host_name = my_strtok(args, ";")) == nullptr)
        return ERROR;

      /* get the service name */
      if ((svc_description = my_strtok(nullptr, ";")) == nullptr)
        return ERROR;

      /* verify that the service is valid */
      found_svc = service::services.find({host_name, svc_description});

      if (found_svc == service::services.end() || !found_svc->second)
        return ERROR;

      if ((charval = my_strtok(nullptr, "\n")) == nullptr)
        return ERROR;
      break;

    case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
    case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:
      /* get the contact name */
      if ((contact_name = my_strtok(args, ";")) == nullptr)
        return ERROR;

      /* verify that the contact is valid */
      cnct = contact::contacts.find(contact_name);
      if (cnct == contact::contacts.end() || !cnct->second)
        return ERROR;

      if ((charval = my_strtok(nullptr, "\n")) == nullptr)
        return ERROR;
      break;

    default:
      /* invalid command */
      return ERROR;
      break;
  }

  temp_ptr = charval;

  timeperiod_map::const_iterator found;
  command_map::iterator cmd_found;

  /* do some validation */
  switch (cmd) {
    case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
    case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
    case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:
    case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:
    case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
    case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:
      /* make sure the timeperiod is valid */

      temp_timeperiod = nullptr;
      found = timeperiod::timeperiods.find(temp_ptr);

      if (found != timeperiod::timeperiods.end())
        temp_timeperiod = found->second.get();

      if (temp_timeperiod == nullptr) {
        return ERROR;
      }
      break;

    case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
    case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
    case CMD_CHANGE_HOST_EVENT_HANDLER:
    case CMD_CHANGE_SVC_EVENT_HANDLER:
    case CMD_CHANGE_HOST_CHECK_COMMAND:
    case CMD_CHANGE_SVC_CHECK_COMMAND:
      /* make sure the command exists */
      temp_ptr2 = my_strtok(temp_ptr.c_str(), "!");
      cmd_found = commands::command::commands.find(temp_ptr2);
      if (cmd_found == commands::command::commands.end() ||
          !cmd_found->second) {
        return ERROR;
      }

      temp_ptr = charval;
      break;

    default:
      break;
  }

  /* update the variable */
  switch (cmd) {
    case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
#ifdef LEGACY_CONF
      config->global_host_event_handler(temp_ptr);
#else
      pb_config.set_global_host_event_handler(temp_ptr);
#endif
      global_host_event_handler_ptr = cmd_found->second.get();
      attr = MODATTR_EVENT_HANDLER_COMMAND;
      break;

    case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
#ifdef LEGACY_CONF
      config->global_service_event_handler(temp_ptr);
#else
      pb_config.set_global_service_event_handler(temp_ptr);
#endif
      global_service_event_handler_ptr = cmd_found->second.get();
      attr = MODATTR_EVENT_HANDLER_COMMAND;
      break;

    case CMD_CHANGE_HOST_EVENT_HANDLER:
      temp_host->set_event_handler(temp_ptr);
      temp_host->set_event_handler_ptr(cmd_found->second.get());
      attr = MODATTR_EVENT_HANDLER_COMMAND;
      break;

    case CMD_CHANGE_HOST_CHECK_COMMAND:
      temp_host->set_check_command(temp_ptr);
      temp_host->set_check_command_ptr(cmd_found->second);
      attr = MODATTR_CHECK_COMMAND;
      break;

    case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
      temp_host->set_check_period(temp_ptr);
      temp_host->check_period_ptr = temp_timeperiod;
      attr = MODATTR_CHECK_TIMEPERIOD;
      break;

    case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:
      temp_host->set_notification_period(temp_ptr);
      temp_host->set_notification_period_ptr(temp_timeperiod);
      attr = MODATTR_NOTIFICATION_TIMEPERIOD;
      break;

    case CMD_CHANGE_SVC_EVENT_HANDLER:
      found_svc->second->set_event_handler(temp_ptr);
      found_svc->second->set_event_handler_ptr(cmd_found->second.get());
      attr = MODATTR_EVENT_HANDLER_COMMAND;
      break;

    case CMD_CHANGE_SVC_CHECK_COMMAND:
      found_svc->second->set_check_command(temp_ptr);
      found_svc->second->set_check_command_ptr(cmd_found->second);
      attr = MODATTR_CHECK_COMMAND;
      break;

    case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
      found_svc->second->set_check_period(temp_ptr);
      found_svc->second->check_period_ptr = temp_timeperiod;
      attr = MODATTR_CHECK_TIMEPERIOD;
      break;

    case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:
      found_svc->second->set_notification_period(temp_ptr);
      found_svc->second->set_notification_period_ptr(temp_timeperiod);
      attr = MODATTR_NOTIFICATION_TIMEPERIOD;
      break;

    case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
      cnct->second->set_host_notification_period(temp_ptr);
      cnct->second->set_host_notification_period_ptr(temp_timeperiod);
      hattr = MODATTR_NOTIFICATION_TIMEPERIOD;
      break;

    case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:
      cnct->second->set_service_notification_period(temp_ptr);
      cnct->second->set_service_notification_period_ptr(temp_timeperiod);
      sattr = MODATTR_NOTIFICATION_TIMEPERIOD;
      break;

    default:
      break;
  }

  /* send data to event broker and update status file */
  switch (cmd) {
    case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
      /* set the modified host attribute */
      modified_host_process_attributes |= attr;

      /* update program status */
      update_program_status(false);
      break;

    case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
      /* set the modified service attribute */
      modified_service_process_attributes |= attr;

      /* update program status */
      update_program_status(false);
      break;

    case CMD_CHANGE_SVC_EVENT_HANDLER:
    case CMD_CHANGE_SVC_CHECK_COMMAND:
    case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
    case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:

      /* set the modified service attribute */
      found_svc->second->add_modified_attributes(attr);

      /* send data to event broker */
      broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                                   found_svc->second.get(), attr);
      break;

    case CMD_CHANGE_HOST_EVENT_HANDLER:
    case CMD_CHANGE_HOST_CHECK_COMMAND:
    case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
    case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:
      /* set the modified host attribute */
      temp_host->add_modified_attributes(attr);

      /* send data to event broker */
      broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                temp_host, attr);
      break;

    case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
    case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:
      /* set the modified attributes */
      cnct->second->set_modified_host_attributes(
          cnct->second->get_modified_host_attributes() | hattr);
      cnct->second->set_modified_service_attributes(
          cnct->second->get_modified_service_attributes() | sattr);
      break;

    default:
      break;
  }

  return OK;
}

/* changes a custom host or service variable */
int cmd_change_object_custom_var(int cmd, char* args) {
  /* get the host or contact name */
  char* temp_ptr(index(args, ';'));
  if (!temp_ptr)
    return ERROR;
  int pos(temp_ptr - args);
  std::string name1(args, pos);
  args += pos + 1;

  /* get the service name */
  std::string name2;
  if (cmd == CMD_CHANGE_CUSTOM_SVC_VAR) {
    temp_ptr = index(args, ';');
    if (!temp_ptr)
      return ERROR;
    pos = temp_ptr - args;
    name2 = std::string(args, pos);
    args += pos + 1;
  }

  /* get the custom variable name */
  temp_ptr = index(args, ';');
  if (!temp_ptr)
    return ERROR;
  pos = temp_ptr - args;
  std::string varname(args, pos);
  args += pos + 1;

  /* get the custom variable value */
  std::string varvalue{args};

  std::transform(varname.begin(), varname.end(), varname.begin(), ::toupper);

  /* find the object */
  switch (cmd) {
    case CMD_CHANGE_CUSTOM_HOST_VAR: {
      host* temp_host{nullptr};
      host_map::const_iterator it_h(host::hosts.find(name1));
      if (it_h != host::hosts.end())
        temp_host = it_h->second.get();
      if (temp_host == nullptr)
        return ERROR;
      map_customvar::iterator it(temp_host->custom_variables.find(varname));
      if (it == temp_host->custom_variables.end())
        temp_host->custom_variables[varname] = customvariable(varvalue);
      else
        it->second.update(varvalue);

      /* set the modified attributes and update the status of the object */
      temp_host->add_modified_attributes(MODATTR_CUSTOM_VARIABLE);
    } break;
    case CMD_CHANGE_CUSTOM_SVC_VAR: {
      service_map::const_iterator found(service::services.find({name1, name2}));

      if (found == service::services.end() || !found->second)
        return ERROR;
      map_customvar::iterator it(found->second->custom_variables.find(varname));
      if (it == found->second->custom_variables.end())
        found->second->custom_variables[varname] = customvariable(varvalue);
      else
        it->second.update(varvalue);

      found->second->add_modified_attributes(MODATTR_CUSTOM_VARIABLE);
    } break;
    case CMD_CHANGE_CUSTOM_CONTACT_VAR: {
      contact_map::iterator cnct_it = contact::contacts.find(name1);
      if (cnct_it == contact::contacts.end() || !cnct_it->second)
        return ERROR;

      map_customvar::iterator it(
          cnct_it->second->get_custom_variables().find(varname));
      if (it == cnct_it->second->get_custom_variables().end())
        cnct_it->second->get_custom_variables()[varname] =
            customvariable(varvalue);
      else
        it->second.update(varvalue);

      cnct_it->second->add_modified_attributes(MODATTR_CUSTOM_VARIABLE);
    } break;
    default:
      break;
  }

  return OK;
}

/* processes an external host command */
int cmd_process_external_commands_from_file(int, char* args) {
  std::string fname;
  int delete_file;

  string::c_strtok arg(args);

  /* get the file name */
  if (!arg.extract(';', fname))
    return ERROR;

  /* find the deletion option */
  if (!arg.extract(';', delete_file)) {
    return ERROR;
  }

  /* process the file */
  process_external_commands_from_file(fname.c_str(),
                                      delete_file ? true : false);

  return OK;
}

/******************************************************************/
/*************** INTERNAL COMMAND IMPLEMENTATIONS  ****************/
/******************************************************************/

/* temporarily disables a service check */
void disable_service_checks(service* svc) {
  constexpr uint32_t attr = MODATTR_ACTIVE_CHECKS_ENABLED;

  /* checks are already disabled */
  if (!svc->active_checks_enabled())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* disable the service check... */
  svc->set_checks_enabled(false);
  svc->set_should_be_scheduled(false);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* enables a service check */
void enable_service_checks(service* svc) {
  time_t preferred_time(0);
  time_t next_valid_time(0);
  constexpr uint32_t attr = MODATTR_ACTIVE_CHECKS_ENABLED;

  /* checks are already enabled */
  if (svc->active_checks_enabled())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* enable the service check... */
  svc->set_checks_enabled(true);
  svc->set_should_be_scheduled(true);

  /* services with no check intervals don't get checked */
  if (svc->check_interval() == 0)
    svc->set_should_be_scheduled(false);

  /* schedule a check for right now (or as soon as possible) */
  time(&preferred_time);
  if (!check_time_against_period(preferred_time, svc->check_period_ptr)) {
    get_next_valid_time(preferred_time, &next_valid_time,
                        svc->check_period_ptr);
    svc->set_next_check(next_valid_time);
  } else
    svc->set_next_check(preferred_time);

  /* schedule a check if we should */
  if (svc->get_should_be_scheduled())
    svc->schedule_check(svc->get_next_check(), CHECK_OPTION_NONE);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);

  /* update the status log with the host info */
  svc->update_status();
}

/* enable notifications on a program-wide basis */
void enable_all_notifications(void) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

#ifdef LEGACY_CONF
  bool enable_notifications = config->enable_notifications();
#else
  bool enable_notifications = pb_config.enable_notifications();
#endif

  /* bail out if we're already set... */
  if (enable_notifications)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

  /* update notification status */
#ifdef LEGACY_CONF
  config->enable_notifications(true);
#else
  pb_config.set_enable_notifications(true);
#endif

  /* update the status log */
  update_program_status(false);
}

/* disable notifications on a program-wide basis */
void disable_all_notifications(void) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

#ifdef LEGACY_CONF
  bool enable_notifications = config->enable_notifications();
#else
  bool enable_notifications = pb_config.enable_notifications();
#endif

  /* bail out if we're already set... */
  if (!enable_notifications)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

  /* update notification status */
#ifdef LEGACY_CONF
  config->enable_notifications(false);
#else
  pb_config.set_enable_notifications(false);
#endif

  /* update the status log */
  update_program_status(false);
}

/* enables notifications for a service */
void enable_service_notifications(service* svc) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

  /* no change */
  if (svc->get_notifications_enabled())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* enable the service notifications... */
  svc->set_notifications_enabled(true);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* disables notifications for a service */
void disable_service_notifications(service* svc) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

  /* no change */
  if (!svc->get_notifications_enabled())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* disable the service notifications... */
  svc->set_notifications_enabled(false);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* enables notifications for a host */
void enable_host_notifications(host* hst) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

  /* no change */
  if (hst->get_notifications_enabled())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* enable the host notifications... */
  hst->set_notifications_enabled(true);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

/* disables notifications for a host */
void disable_host_notifications(host* hst) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

  /* no change */
  if (!hst->get_notifications_enabled())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* disable the host notifications... */
  hst->set_notifications_enabled(false);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

/* enables notifications for all hosts and services "beyond" a given host */
void enable_and_propagate_notifications(host* hst,
                                        int level,
                                        int affect_top_host,
                                        int affect_hosts,
                                        int affect_services) {
  /* enable notification for top level host */
  if (affect_top_host && level == 0)
    enable_host_notifications(hst);

  /* check all child hosts... */
  for (const auto& [_, ptr_host] : hst->child_hosts) {
    if (ptr_host == nullptr)
      continue;

    /* recurse... */
    enable_and_propagate_notifications(ptr_host, level + 1, affect_top_host,
                                       affect_hosts, affect_services);

    /* enable notifications for this host */
    if (affect_hosts)
      enable_host_notifications(ptr_host);

    /* enable notifications for all services on this host... */
    if (affect_services) {
      for (const auto& [_, ptr_srv] : ptr_host->services) {
        if (!ptr_srv)
          continue;
        enable_service_notifications(ptr_srv);
      }
    }
  }
}

/* disables notifications for all hosts and services "beyond" a given host */
void disable_and_propagate_notifications(host* hst,
                                         int level,
                                         int affect_top_host,
                                         int affect_hosts,
                                         int affect_services) {
  if (!hst)
    return;

  /* disable notifications for top host */
  if (affect_top_host && level == 0)
    disable_host_notifications(hst);

  /* check all child hosts... */
  for (const auto& [_, ptr_host] : hst->child_hosts) {
    if (!ptr_host)
      continue;

    /* recurse... */
    disable_and_propagate_notifications(ptr_host, level + 1, affect_top_host,
                                        affect_hosts, affect_services);

    /* disable notifications for this host */
    if (affect_hosts)
      disable_host_notifications(ptr_host);

    /* disable notifications for all services on this host... */
    if (affect_services) {
      for (const auto& [_, ptr_srv] : ptr_host->services) {
        if (!ptr_srv)
          continue;
        disable_service_notifications(ptr_srv);
      }
    }
  }
}

/* enables host notifications for a contact */
void enable_contact_host_notifications(contact* cntct) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

  /* no change */
  if (cntct->get_host_notifications_enabled())
    return;

  /* set the attribute modified flag */
  cntct->set_modified_host_attributes(cntct->get_modified_host_attributes() |
                                      attr);

  /* enable the host notifications... */
  cntct->set_host_notifications_enabled(true);
}

/* disables host notifications for a contact */
void disable_contact_host_notifications(contact* cntct) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

  /* no change */
  if (!cntct->get_host_notifications_enabled())
    return;

  /* set the attribute modified flag */
  cntct->set_modified_host_attributes(cntct->get_modified_host_attributes() |
                                      attr);

  /* enable the host notifications... */
  cntct->set_host_notifications_enabled(false);
}

/* enables service notifications for a contact */
void enable_contact_service_notifications(contact* cntct) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

  /* no change */
  if (cntct->get_service_notifications_enabled())
    return;

  /* set the attribute modified flag */
  cntct->set_modified_service_attributes(
      cntct->get_modified_service_attributes() | attr);

  /* enable the host notifications... */
  cntct->set_service_notifications_enabled(true);
}

/* disables service notifications for a contact */
void disable_contact_service_notifications(contact* cntct) {
  constexpr uint32_t attr = MODATTR_NOTIFICATIONS_ENABLED;

  /* no change */
  if (!cntct->get_service_notifications_enabled())
    return;

  /* set the attribute modified flag */
  cntct->set_modified_service_attributes(
      cntct->get_modified_service_attributes() | attr);

  /* enable the host notifications... */
  cntct->set_service_notifications_enabled(false);
}

/* schedules downtime for all hosts "beyond" a given host */
void schedule_and_propagate_downtime(host* temp_host,
                                     time_t entry_time,
                                     char const* author,
                                     char const* comment_data,
                                     time_t start_time,
                                     time_t end_time,
                                     bool fixed,
                                     unsigned long triggered_by,
                                     unsigned long duration) {
  /* check all child hosts... */
  for (const auto& [_, ptr_host] : temp_host->child_hosts) {
    if (ptr_host == nullptr)
      continue;

    /* recurse... */
    schedule_and_propagate_downtime(ptr_host, entry_time, author, comment_data,
                                    start_time, end_time, fixed, triggered_by,
                                    duration);

    /* schedule downtime for this host */
    downtime_manager::instance().schedule_downtime(
        downtime::host_downtime, ptr_host->host_id(), 0, entry_time, author,
        comment_data, start_time, end_time, fixed, triggered_by, duration,
        nullptr);
  }
}

/* acknowledges a host problem */
void acknowledge_host_problem(host* hst,
                              const std::string& ack_author,
                              const std::string& ack_data,
                              int type,
                              int notify,
                              int persistent) {
  /* cannot acknowledge a non-existent problem */
  if (hst->get_current_state() == host::state_up)
    return;

  /* set the acknowledgement flag */
  hst->set_acknowledgement(type == AckType::STICKY ? AckType::STICKY
                                                   : AckType::NORMAL);

  /* schedule acknowledgement expiration */
  time_t current_time = time(nullptr);
  hst->set_last_acknowledgement(current_time);
  hst->schedule_acknowledgement_expiration();

  /* send data to event broker */
  broker_acknowledgement_data(hst, ack_author.c_str(), ack_data.c_str(), type,
                              notify, persistent);

  /* send out an acknowledgement notification */
  if (notify)
    hst->notify(notifier::reason_acknowledgement, ack_author, ack_data,
                notifier::notification_option_none);

  /* update the status log with the host info */
  hst->update_status(host::STATUS_ACKNOWLEDGEMENT);

  /* add a comment for the acknowledgement */
  auto com{std::make_shared<comment>(
      comment::host, comment::acknowledgment, hst->host_id(), 0, current_time,
      ack_author, ack_data, persistent, comment::internal, false, (time_t)0)};
  comment::comments.insert({com->get_comment_id(), com});
}

/* acknowledges a service problem */
void acknowledge_service_problem(service* svc,
                                 const std::string& ack_author,
                                 const std::string& ack_data,
                                 int type,
                                 int notify,
                                 int persistent) {
  /* cannot acknowledge a non-existent problem */
  if (svc->get_current_state() == service::state_ok)
    return;

  /* set the acknowledgement flag */
  svc->set_acknowledgement(type == AckType::STICKY ? AckType::STICKY
                                                   : AckType::NORMAL);

  /* schedule acknowledgement expiration */
  time_t current_time = time(nullptr);
  svc->set_last_acknowledgement(current_time);
  svc->schedule_acknowledgement_expiration();

  /* send data to event broker */
  broker_acknowledgement_data(svc, ack_author.c_str(), ack_data.c_str(), type,
                              notify, persistent);

  /* send out an acknowledgement notification */
  if (notify)
    svc->notify(notifier::reason_acknowledgement, ack_author, ack_data,
                notifier::notification_option_none);

  /* update the status log with the service info */
  svc->update_status(service::STATUS_ACKNOWLEDGEMENT);

  /* add a comment for the acknowledgement */
  auto com{std::make_shared<comment>(
      comment::service, comment::acknowledgment, svc->host_id(),
      svc->service_id(), current_time, ack_author, ack_data, persistent,
      comment::internal, false, (time_t)0)};
  comment::comments.insert({com->get_comment_id(), com});
}

/* removes a host acknowledgement */
void remove_host_acknowledgement(host* hst) {
  /* set the acknowledgement flag */
  hst->set_acknowledgement(AckType::NONE);

  /* update the status log with the host info */
  hst->update_status(host::STATUS_ACKNOWLEDGEMENT);

  /* remove any non-persistant comments associated with the ack */
  comment::delete_host_acknowledgement_comments(hst);
}

/* removes a service acknowledgement */
void remove_service_acknowledgement(service* svc) {
  /* set the acknowledgement flag */
  svc->set_acknowledgement(AckType::NONE);

  /* update the status log with the service info */
  svc->update_status(host::STATUS_ACKNOWLEDGEMENT);

  /* remove any non-persistant comments associated with the ack */
  comment::delete_service_acknowledgement_comments(svc);
}

/* starts executing service checks */
void start_executing_service_checks(void) {
  constexpr uint32_t attr = MODATTR_ACTIVE_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool execute_service_checks = config->execute_service_checks();
#else
  bool execute_service_checks = pb_config.execute_service_checks();
#endif

  /* bail out if we're already executing services */
  if (execute_service_checks)
    return;

  /* set the attribute modified flag */
  modified_service_process_attributes |= attr;

  /* set the service check execution flag */
#ifdef LEGACY_CONF
  config->execute_service_checks(true);
#else
  pb_config.set_execute_service_checks(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* stops executing service checks */
void stop_executing_service_checks(void) {
  unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool execute_service_checks = config->execute_service_checks();
#else
  bool execute_service_checks = pb_config.execute_service_checks();
#endif

  /* bail out if we're already not executing services */
  if (!execute_service_checks)
    return;

  /* set the attribute modified flag */
  modified_service_process_attributes |= attr;

  /* set the service check execution flag */
#ifdef LEGACY_CONF
  config->execute_service_checks(false);
#else
  pb_config.set_execute_service_checks(false);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* starts accepting passive service checks */
void start_accepting_passive_service_checks(void) {
  constexpr uint32_t attr = MODATTR_PASSIVE_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool accept_passive_service_checks = config->accept_passive_service_checks();
#else
  bool accept_passive_service_checks =
      pb_config.accept_passive_service_checks();
#endif

  /* bail out if we're already accepting passive services */
  if (accept_passive_service_checks)
    return;

  /* set the attribute modified flag */
  modified_service_process_attributes |= attr;

  /* set the service check flag */
#ifdef LEGACY_CONF
  config->accept_passive_service_checks(true);
#else
  pb_config.set_accept_passive_service_checks(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* stops accepting passive service checks */
void stop_accepting_passive_service_checks(void) {
  constexpr uint32_t attr = MODATTR_PASSIVE_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool accept_passive_service_checks = config->accept_passive_service_checks();
#else
  bool accept_passive_service_checks =
      pb_config.accept_passive_service_checks();
#endif

  /* bail out if we're already not accepting passive services */
  if (!accept_passive_service_checks)
    return;

  /* set the attribute modified flag */
  modified_service_process_attributes |= attr;

  /* set the service check flag */
#ifdef LEGACY_CONF
  config->accept_passive_service_checks(false);
#else
  pb_config.set_accept_passive_service_checks(false);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* enables passive service checks for a particular service */
void enable_passive_service_checks(service* svc) {
  constexpr const unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

  /* no change */
  if (svc->passive_checks_enabled())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* set the passive check flag */
  svc->set_accept_passive_checks(true);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* disables passive service checks for a particular service */
void disable_passive_service_checks(service* svc) {
  constexpr uint32_t attr = MODATTR_PASSIVE_CHECKS_ENABLED;

  /* no change */
  if (!svc->passive_checks_enabled())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* set the passive check flag */
  svc->set_accept_passive_checks(false);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* starts executing host checks */
void start_executing_host_checks(void) {
  constexpr uint32_t attr = MODATTR_ACTIVE_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool execute_host_checks = config->execute_host_checks();
#else
  bool execute_host_checks = pb_config.execute_host_checks();
#endif

  /* bail out if we're already executing hosts */
  if (execute_host_checks)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;

  /* set the host check execution flag */
#ifdef LEGACY_CONF
  config->execute_host_checks(true);
#else
  pb_config.set_execute_host_checks(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* stops executing host checks */
void stop_executing_host_checks(void) {
  constexpr uint32_t attr = MODATTR_ACTIVE_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool execute_host_checks = config->execute_host_checks();
#else
  bool execute_host_checks = pb_config.execute_host_checks();
#endif
  /* bail out if we're already not executing hosts */
  if (!execute_host_checks)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;

  /* set the host check execution flag */
#ifdef LEGACY_CONF
  config->execute_host_checks(true);
#else
  pb_config.set_execute_host_checks(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* starts accepting passive host checks */
void start_accepting_passive_host_checks(void) {
  constexpr uint32_t attr = MODATTR_PASSIVE_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool accept_passive_host_checks = config->accept_passive_host_checks();
#else
  bool accept_passive_host_checks = pb_config.accept_passive_host_checks();
#endif

  /* bail out if we're already accepting passive hosts */
  if (accept_passive_host_checks)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;

  /* set the host check flag */
#ifdef LEGACY_CONF
  config->accept_passive_host_checks(true);
#else
  pb_config.set_accept_passive_host_checks(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* stops accepting passive host checks */
void stop_accepting_passive_host_checks(void) {
  constexpr uint32_t attr = MODATTR_PASSIVE_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool accept_passive_host_checks = config->accept_passive_host_checks();
#else
  bool accept_passive_host_checks = pb_config.accept_passive_host_checks();
#endif

  /* bail out if we're already not accepting passive hosts */
  if (!accept_passive_host_checks)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;

  /* set the host check flag */
#ifdef LEGACY_CONF
  config->accept_passive_host_checks(false);
#else
  pb_config.set_accept_passive_host_checks(false);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* enables passive host checks for a particular host */
void enable_passive_host_checks(host* hst) {
  constexpr const unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

  /* no change */
  if (hst->passive_checks_enabled())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* set the passive check flag */
  hst->set_accept_passive_checks(true);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

/* disables passive host checks for a particular host */
void disable_passive_host_checks(host* hst) {
  constexpr uint32_t attr = MODATTR_PASSIVE_CHECKS_ENABLED;

  /* no change */
  if (!hst->passive_checks_enabled())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* set the passive check flag */
  hst->set_accept_passive_checks(false);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

/* enables event handlers on a program-wide basis */
void start_using_event_handlers(void) {
  constexpr uint32_t attr = MODATTR_EVENT_HANDLER_ENABLED;

#ifdef LEGACY_CONF
  bool enable_event_handlers = config->enable_event_handlers();
#else
  bool enable_event_handlers = pb_config.enable_event_handlers();
#endif

  /* no change */
  if (enable_event_handlers)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

  /* set the event handler flag */
#ifdef LEGACY_CONF
  config->enable_event_handlers(true);
#else
  pb_config.set_enable_event_handlers(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* disables event handlers on a program-wide basis */
void stop_using_event_handlers(void) {
  constexpr uint32_t attr = MODATTR_EVENT_HANDLER_ENABLED;

#ifdef LEGACY_CONF
  bool enable_event_handlers = config->enable_event_handlers();
#else
  bool enable_event_handlers = pb_config.enable_event_handlers();
#endif

  /* no change */
  if (!enable_event_handlers)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

  /* set the event handler flag */
#ifdef LEGACY_CONF
  config->enable_event_handlers(false);
#else
  pb_config.set_enable_event_handlers(false);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* enables the event handler for a particular service */
void enable_service_event_handler(service* svc) {
  constexpr uint32_t attr = MODATTR_EVENT_HANDLER_ENABLED;

  /* no change */
  if (svc->event_handler_enabled())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* set the event handler flag */
  svc->set_event_handler_enabled(true);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* disables the event handler for a particular service */
void disable_service_event_handler(service* svc) {
  constexpr uint32_t attr = MODATTR_EVENT_HANDLER_ENABLED;

  /* no change */
  if (!svc->event_handler_enabled())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* set the event handler flag */
  svc->set_event_handler_enabled(false);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* enables the event handler for a particular host */
void enable_host_event_handler(host* hst) {
  constexpr uint32_t attr = MODATTR_EVENT_HANDLER_ENABLED;

  /* no change */
  if (hst->event_handler_enabled())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* set the event handler flag */
  hst->set_event_handler_enabled(true);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

/* disables the event handler for a particular host */
void disable_host_event_handler(host* hst) {
  constexpr uint32_t attr = MODATTR_EVENT_HANDLER_ENABLED;

  /* no change */
  if (!hst->event_handler_enabled())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* set the event handler flag */
  hst->set_event_handler_enabled(false);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

/* disables checks of a particular host */
void disable_host_checks(host* hst) {
  constexpr uint32_t attr = MODATTR_ACTIVE_CHECKS_ENABLED;

  /* checks are already disabled */
  if (!hst->active_checks_enabled())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* set the host check flag */
  hst->set_checks_enabled(false);
  hst->set_should_be_scheduled(false);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

/* enables checks of a particular host */
void enable_host_checks(host* hst) {
  time_t preferred_time(0);
  time_t next_valid_time(0);
  constexpr uint32_t attr = MODATTR_ACTIVE_CHECKS_ENABLED;

  /* checks are already enabled */
  if (hst->active_checks_enabled())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* set the host check flag */
  hst->set_checks_enabled(true);
  hst->set_should_be_scheduled(true);

  /* hosts with no check intervals don't get checked */
  if (hst->check_interval() == 0)
    hst->set_should_be_scheduled(false);

  /* schedule a check for right now (or as soon as possible) */
  time(&preferred_time);
  if (!check_time_against_period(preferred_time, hst->check_period_ptr)) {
    get_next_valid_time(preferred_time, &next_valid_time,
                        hst->check_period_ptr);
    hst->set_next_check(next_valid_time);
  } else
    hst->set_next_check(preferred_time);

  /* schedule a check if we should */
  if (hst->get_should_be_scheduled())
    hst->schedule_check(hst->get_next_check(), CHECK_OPTION_NONE);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);

  /* update the status log with the host info */
  hst->update_status();
}

/* start obsessing over service check results */
void start_obsessing_over_service_checks(void) {
  constexpr uint32_t attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

#ifdef LEGACY_CONF
  bool obsess_over_services = config->obsess_over_services();
#else
  bool obsess_over_services = pb_config.obsess_over_services();
#endif

  /* no change */
  if (obsess_over_services)
    return;

  /* set the attribute modified flag */
  modified_service_process_attributes |= attr;

  /* set the service obsession flag */
#ifdef LEGACY_CONF
  config->obsess_over_services(true);
#else
  pb_config.set_obsess_over_services(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* stop obsessing over service check results */
void stop_obsessing_over_service_checks(void) {
  constexpr uint32_t attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

#ifdef LEGACY_CONF
  bool obsess_over_services = config->obsess_over_services();
#else
  bool obsess_over_services = pb_config.obsess_over_services();
#endif

  /* no change */
  if (!obsess_over_services)
    return;

  /* set the attribute modified flag */
  modified_service_process_attributes |= attr;

  /* set the service obsession flag */
#ifdef LEGACY_CONF
  config->obsess_over_services(false);
#else
  pb_config.set_obsess_over_services(false);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* start obsessing over host check results */
void start_obsessing_over_host_checks(void) {
  unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

#ifdef LEGACY_CONF
  bool obsess_over_hosts = config->obsess_over_hosts();
#else
  bool obsess_over_hosts = pb_config.obsess_over_hosts();
#endif

  /* no change */
  if (obsess_over_hosts)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;

  /* set the host obsession flag */
#ifdef LEGACY_CONF
  config->obsess_over_hosts(true);
#else
  pb_config.set_obsess_over_hosts(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* stop obsessing over host check results */
void stop_obsessing_over_host_checks(void) {
  constexpr uint32_t attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

#ifdef LEGACY_CONF
  bool obsess_over_hosts = config->obsess_over_hosts();
#else
  bool obsess_over_hosts = pb_config.obsess_over_hosts();
#endif

  /* no change */
  if (!obsess_over_hosts)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;

  /* set the host obsession flag */
#ifdef LEGACY_CONF
  config->obsess_over_hosts(false);
#else
  pb_config.set_obsess_over_hosts(false);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* enables service freshness checking */
void enable_service_freshness_checks(void) {
  constexpr uint32_t attr = MODATTR_FRESHNESS_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool check_service_freshness = config->check_service_freshness();
#else
  bool check_service_freshness = pb_config.check_service_freshness();
#endif

  /* no change */
  if (check_service_freshness)
    return;

  /* set the attribute modified flag */
  modified_service_process_attributes |= attr;

  /* set the freshness check flag */
#ifdef LEGACY_CONF
  config->check_service_freshness(true);
#else
  pb_config.set_check_service_freshness(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* disables service freshness checking */
void disable_service_freshness_checks(void) {
  constexpr uint32_t attr = MODATTR_FRESHNESS_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool check_service_freshness = config->check_service_freshness();
#else
  bool check_service_freshness = pb_config.check_service_freshness();
#endif

  /* no change */
  if (!check_service_freshness)
    return;

  /* set the attribute modified flag */
  modified_service_process_attributes |= attr;

  /* set the freshness check flag */
#ifdef LEGACY_CONF
  config->check_service_freshness(false);
#else
  pb_config.set_check_service_freshness(false);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* enables host freshness checking */
void enable_host_freshness_checks(void) {
  constexpr uint32_t attr = MODATTR_FRESHNESS_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool check_host_freshness = config->check_host_freshness();
#else
  bool check_host_freshness = pb_config.check_host_freshness();
#endif

  /* no change */
  if (check_host_freshness)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;

  /* set the freshness check flag */
#ifdef LEGACY_CONF
  config->check_host_freshness(true);
#else
  pb_config.set_check_host_freshness(true);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* disables host freshness checking */
void disable_host_freshness_checks(void) {
  constexpr uint32_t attr = MODATTR_FRESHNESS_CHECKS_ENABLED;

#ifdef LEGACY_CONF
  bool check_host_freshness = config->check_host_freshness();
#else
  bool check_host_freshness = pb_config.check_host_freshness();
#endif

  /* no change */
  if (!check_host_freshness)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;

  /* set the freshness check flag */
#ifdef LEGACY_CONF
  config->check_host_freshness(false);
#else
  pb_config.set_check_host_freshness(false);
#endif

  /* update the status log with the program info */
  update_program_status(false);
}

/* enable performance data on a program-wide basis */
void enable_performance_data(void) {
  constexpr uint32_t attr = MODATTR_PERFORMANCE_DATA_ENABLED;

#ifdef LEGACY_CONF
  bool process_performance_data = config->process_performance_data();
#else
  bool process_performance_data = pb_config.process_performance_data();
#endif

  /* bail out if we're already set... */
  if (process_performance_data)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

#ifdef LEGACY_CONF
  config->process_performance_data(true);
#else
  pb_config.set_process_performance_data(true);
#endif

  /* update the status log */
  update_program_status(false);
}

/* disable performance data on a program-wide basis */
void disable_performance_data(void) {
  constexpr uint32_t attr = MODATTR_PERFORMANCE_DATA_ENABLED;

#ifdef LEGACY_CONF
  bool process_performance_data = config->process_performance_data();
#else
  bool process_performance_data = pb_config.process_performance_data();
#endif

  /* bail out if we're already set... */
  if (!process_performance_data)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

#ifdef LEGACY_CONF
  config->process_performance_data(false);
#else
  pb_config.set_process_performance_data(false);
#endif

  /* update the status log */
  update_program_status(false);
}

/* start obsessing over a particular service */
void start_obsessing_over_service(service* svc) {
  constexpr uint32_t attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

  /* no change */
  if (svc->obsess_over())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* set the obsess over service flag */
  svc->set_obsess_over(true);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* stop obsessing over a particular service */
void stop_obsessing_over_service(service* svc) {
  constexpr uint32_t attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

  /* no change */
  if (!svc->obsess_over())
    return;

  /* set the attribute modified flag */
  svc->add_modified_attributes(attr);

  /* set the obsess over service flag */
  svc->set_obsess_over(false);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               svc, attr);
}

/* start obsessing over a particular host */
void start_obsessing_over_host(host* hst) {
  constexpr uint32_t attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

  /* no change */
  if (hst->obsess_over())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* set the obsess over host flag */
  hst->set_obsess_over(true);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

/* stop obsessing over a particular host */
void stop_obsessing_over_host(host* hst) {
  constexpr uint32_t attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

  /* no change */
  if (!hst->obsess_over())
    return;

  /* set the attribute modified flag */
  hst->add_modified_attributes(attr);

  /* set the obsess over host flag */
  hst->set_obsess_over(false);

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, hst,
                            attr);
}

void new_thresholds_file(char* filename) {
  anomalydetection::update_thresholds(filename);
}
