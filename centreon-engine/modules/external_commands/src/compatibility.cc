/*
** Copyright 2002-2006 Ethan Galstad
** Copyright 2011-2019 Centreon
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

#include <cstdlib>
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/modules/external_commands/commands.hh"
#include "com/centreon/engine/modules/external_commands/compatibility.hh"
#include "com/centreon/engine/retention/applier/state.hh"
#include "com/centreon/engine/retention/dump.hh"
#include "com/centreon/engine/retention/parser.hh"
#include "com/centreon/engine/retention/state.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;

int process_external_command1(char* cmd) {
  char* command_id = nullptr;
  char* args = nullptr;
  time_t entry_time = 0L;
  int command_type = CMD_NONE;
  char* temp_ptr = nullptr;

  logger(dbg_functions, basic)
    << "process_external_command1()";

  if (cmd == nullptr)
    return ERROR;

  /* strip the command of newlines and carriage returns */
  strip(cmd);

  logger(dbg_external_command, most)
    << "Raw command entry: " << cmd;

  /* get the command entry time */
  if ((temp_ptr = my_strtok(cmd, "[")) == nullptr)
    return ERROR;
  if ((temp_ptr = my_strtok(nullptr, "]")) == nullptr)
    return ERROR;
  entry_time = (time_t)strtoul(temp_ptr, nullptr, 10);

  /* get the command identifier */
  if ((temp_ptr = my_strtok(nullptr, ";")) == nullptr)
    return ERROR;
  command_id = string::dup(temp_ptr + 1);

  /* get the command arguments */
  if ((temp_ptr = my_strtok(nullptr, "\n")) == nullptr)
    args = string::dup("");
  else
    args = string::dup(temp_ptr);

  /* decide what type of command this is... */

  /**************************/
  /**** PROCESS COMMANDS ****/
  /**************************/

  if (!strcmp(command_id, "ENTER_STANDBY_MODE")
      || !strcmp(command_id, "DISABLE_NOTIFICATIONS"))
    command_type = CMD_DISABLE_NOTIFICATIONS;
  else if (!strcmp(command_id, "ENTER_ACTIVE_MODE")
           || !strcmp(command_id, "ENABLE_NOTIFICATIONS"))
    command_type = CMD_ENABLE_NOTIFICATIONS;

  else if (!strcmp(command_id, "SHUTDOWN_PROGRAM")
           || !strcmp(command_id, "SHUTDOWN_PROCESS"))
    command_type = CMD_SHUTDOWN_PROCESS;
  else if (!strcmp(command_id, "RESTART_PROGRAM")
           || !strcmp(command_id, "RESTART_PROCESS"))
    command_type = CMD_RESTART_PROCESS;

  else if (!strcmp(command_id, "SAVE_STATE_INFORMATION"))
    command_type = CMD_SAVE_STATE_INFORMATION;
  else if (!strcmp(command_id, "READ_STATE_INFORMATION"))
    command_type = CMD_READ_STATE_INFORMATION;

  else if (!strcmp(command_id, "ENABLE_EVENT_HANDLERS"))
    command_type = CMD_ENABLE_EVENT_HANDLERS;
  else if (!strcmp(command_id, "DISABLE_EVENT_HANDLERS"))
    command_type = CMD_DISABLE_EVENT_HANDLERS;

  else if (!strcmp(command_id, "FLUSH_PENDING_COMMANDS"))
    command_type = CMD_FLUSH_PENDING_COMMANDS;

  else if (!strcmp(command_id, "ENABLE_FAILURE_PREDICTION"))
    command_type = CMD_ENABLE_FAILURE_PREDICTION;
  else if (!strcmp(command_id, "DISABLE_FAILURE_PREDICTION"))
    command_type = CMD_DISABLE_FAILURE_PREDICTION;

  else if (!strcmp(command_id, "ENABLE_PERFORMANCE_DATA"))
    command_type = CMD_ENABLE_PERFORMANCE_DATA;
  else if (!strcmp(command_id, "DISABLE_PERFORMANCE_DATA"))
    command_type = CMD_DISABLE_PERFORMANCE_DATA;

  else if (!strcmp(command_id, "START_EXECUTING_HOST_CHECKS"))
    command_type = CMD_START_EXECUTING_HOST_CHECKS;
  else if (!strcmp(command_id, "STOP_EXECUTING_HOST_CHECKS"))
    command_type = CMD_STOP_EXECUTING_HOST_CHECKS;

  else if (!strcmp(command_id, "START_EXECUTING_SVC_CHECKS"))
    command_type = CMD_START_EXECUTING_SVC_CHECKS;
  else if (!strcmp(command_id, "STOP_EXECUTING_SVC_CHECKS"))
    command_type = CMD_STOP_EXECUTING_SVC_CHECKS;

  else if (!strcmp(command_id, "START_ACCEPTING_PASSIVE_HOST_CHECKS"))
    command_type = CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS;
  else if (!strcmp(command_id, "STOP_ACCEPTING_PASSIVE_HOST_CHECKS"))
    command_type = CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS;

  else if (!strcmp(command_id, "START_ACCEPTING_PASSIVE_SVC_CHECKS"))
    command_type = CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS;
  else if (!strcmp(command_id, "STOP_ACCEPTING_PASSIVE_SVC_CHECKS"))
    command_type = CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS;

  else if (!strcmp(command_id, "START_OBSESSING_OVER_HOST_CHECKS"))
    command_type = CMD_START_OBSESSING_OVER_HOST_CHECKS;
  else if (!strcmp(command_id, "STOP_OBSESSING_OVER_HOST_CHECKS"))
    command_type = CMD_STOP_OBSESSING_OVER_HOST_CHECKS;

  else if (!strcmp(command_id, "START_OBSESSING_OVER_SVC_CHECKS"))
    command_type = CMD_START_OBSESSING_OVER_SVC_CHECKS;
  else if (!strcmp(command_id, "STOP_OBSESSING_OVER_SVC_CHECKS"))
    command_type = CMD_STOP_OBSESSING_OVER_SVC_CHECKS;

  else if (!strcmp(command_id, "ENABLE_FLAP_DETECTION"))
    command_type = CMD_ENABLE_FLAP_DETECTION;
  else if (!strcmp(command_id, "DISABLE_FLAP_DETECTION"))
    command_type = CMD_DISABLE_FLAP_DETECTION;

  else if (!strcmp(command_id, "CHANGE_GLOBAL_HOST_EVENT_HANDLER"))
    command_type = CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER;
  else if (!strcmp(command_id, "CHANGE_GLOBAL_SVC_EVENT_HANDLER"))
    command_type = CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER;

  else if (!strcmp(command_id, "ENABLE_SERVICE_FRESHNESS_CHECKS"))
    command_type = CMD_ENABLE_SERVICE_FRESHNESS_CHECKS;
  else if (!strcmp(command_id, "DISABLE_SERVICE_FRESHNESS_CHECKS"))
    command_type = CMD_DISABLE_SERVICE_FRESHNESS_CHECKS;

  else if (!strcmp(command_id, "ENABLE_HOST_FRESHNESS_CHECKS"))
    command_type = CMD_ENABLE_HOST_FRESHNESS_CHECKS;
  else if (!strcmp(command_id, "DISABLE_HOST_FRESHNESS_CHECKS"))
    command_type = CMD_DISABLE_HOST_FRESHNESS_CHECKS;

  /*******************************/
  /**** HOST-RELATED COMMANDS ****/
  /*******************************/

  else if (!strcmp(command_id, "ADD_HOST_COMMENT"))
    command_type = CMD_ADD_HOST_COMMENT;
  else if (!strcmp(command_id, "DEL_HOST_COMMENT"))
    command_type = CMD_DEL_HOST_COMMENT;
  else if (!strcmp(command_id, "DEL_ALL_HOST_COMMENTS"))
    command_type = CMD_DEL_ALL_HOST_COMMENTS;

  else if (!strcmp(command_id, "DELAY_HOST_NOTIFICATION"))
    command_type = CMD_DELAY_HOST_NOTIFICATION;

  else if (!strcmp(command_id, "ENABLE_HOST_NOTIFICATIONS"))
    command_type = CMD_ENABLE_HOST_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_HOST_NOTIFICATIONS"))
    command_type = CMD_DISABLE_HOST_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST"))
    command_type = CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST;
  else if (!strcmp(command_id, "DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST"))
    command_type = CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST;

  else if (!strcmp(command_id, "ENABLE_HOST_AND_CHILD_NOTIFICATIONS"))
    command_type = CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_HOST_AND_CHILD_NOTIFICATIONS"))
    command_type = CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_HOST_SVC_NOTIFICATIONS"))
    command_type = CMD_ENABLE_HOST_SVC_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_HOST_SVC_NOTIFICATIONS"))
    command_type = CMD_DISABLE_HOST_SVC_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_HOST_SVC_CHECKS"))
    command_type = CMD_ENABLE_HOST_SVC_CHECKS;
  else if (!strcmp(command_id, "DISABLE_HOST_SVC_CHECKS"))
    command_type = CMD_DISABLE_HOST_SVC_CHECKS;

  else if (!strcmp(command_id, "ENABLE_PASSIVE_HOST_CHECKS"))
    command_type = CMD_ENABLE_PASSIVE_HOST_CHECKS;
  else if (!strcmp(command_id, "DISABLE_PASSIVE_HOST_CHECKS"))
    command_type = CMD_DISABLE_PASSIVE_HOST_CHECKS;

  else if (!strcmp(command_id, "SCHEDULE_HOST_SVC_CHECKS"))
    command_type = CMD_SCHEDULE_HOST_SVC_CHECKS;
  else if (!strcmp(command_id, "SCHEDULE_FORCED_HOST_SVC_CHECKS"))
    command_type = CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS;

  else if (!strcmp(command_id, "ACKNOWLEDGE_HOST_PROBLEM"))
    command_type = CMD_ACKNOWLEDGE_HOST_PROBLEM;
  else if (!strcmp(command_id, "REMOVE_HOST_ACKNOWLEDGEMENT"))
    command_type = CMD_REMOVE_HOST_ACKNOWLEDGEMENT;

  else if (!strcmp(command_id, "ENABLE_HOST_EVENT_HANDLER"))
    command_type = CMD_ENABLE_HOST_EVENT_HANDLER;
  else if (!strcmp(command_id, "DISABLE_HOST_EVENT_HANDLER"))
    command_type = CMD_DISABLE_HOST_EVENT_HANDLER;

  else if (!strcmp(command_id, "ENABLE_HOST_CHECK"))
    command_type = CMD_ENABLE_HOST_CHECK;
  else if (!strcmp(command_id, "DISABLE_HOST_CHECK"))
    command_type = CMD_DISABLE_HOST_CHECK;

  else if (!strcmp(command_id, "SCHEDULE_HOST_CHECK"))
    command_type = CMD_SCHEDULE_HOST_CHECK;
  else if (!strcmp(command_id, "SCHEDULE_FORCED_HOST_CHECK"))
    command_type = CMD_SCHEDULE_FORCED_HOST_CHECK;

  else if (!strcmp(command_id, "SCHEDULE_HOST_DOWNTIME"))
    command_type = CMD_SCHEDULE_HOST_DOWNTIME;
  else if (!strcmp(command_id, "SCHEDULE_HOST_SVC_DOWNTIME"))
    command_type = CMD_SCHEDULE_HOST_SVC_DOWNTIME;
  else if (!strcmp(command_id, "DEL_HOST_DOWNTIME"))
    command_type = CMD_DEL_HOST_DOWNTIME;
  else if (!strcmp(command_id, "DEL_HOST_DOWNTIME_FULL"))
    command_type = CMD_DEL_HOST_DOWNTIME_FULL;
  else if (!strcmp(command_id, "DEL_DOWNTIME_BY_HOST_NAME"))
    command_type = CMD_DEL_DOWNTIME_BY_HOST_NAME;
  else if (!strcmp(command_id, "DEL_DOWNTIME_BY_HOSTGROUP_NAME"))
    command_type = CMD_DEL_DOWNTIME_BY_HOSTGROUP_NAME;
  else if (!strcmp(command_id, "DEL_DOWNTIME_BY_START_TIME_COMMENT"))
    command_type = CMD_DEL_DOWNTIME_BY_START_TIME_COMMENT;

  else if (!strcmp(command_id, "ENABLE_HOST_FLAP_DETECTION"))
    command_type = CMD_ENABLE_HOST_FLAP_DETECTION;
  else if (!strcmp(command_id, "DISABLE_HOST_FLAP_DETECTION"))
    command_type = CMD_DISABLE_HOST_FLAP_DETECTION;

  else if (!strcmp(command_id, "START_OBSESSING_OVER_HOST"))
    command_type = CMD_START_OBSESSING_OVER_HOST;
  else if (!strcmp(command_id, "STOP_OBSESSING_OVER_HOST"))
    command_type = CMD_STOP_OBSESSING_OVER_HOST;

  else if (!strcmp(command_id, "CHANGE_HOST_EVENT_HANDLER"))
    command_type = CMD_CHANGE_HOST_EVENT_HANDLER;
  else if (!strcmp(command_id, "CHANGE_HOST_CHECK_COMMAND"))
    command_type = CMD_CHANGE_HOST_CHECK_COMMAND;

  else if (!strcmp(command_id, "CHANGE_NORMAL_HOST_CHECK_INTERVAL"))
    command_type = CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL;
  else if (!strcmp(command_id, "CHANGE_RETRY_HOST_CHECK_INTERVAL"))
    command_type = CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL;

  else if (!strcmp(command_id, "CHANGE_MAX_HOST_CHECK_ATTEMPTS"))
    command_type = CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS;

  else if (!strcmp(command_id, "SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME"))
    command_type = CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME;

  else if (!strcmp(command_id, "SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME"))
    command_type = CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME;

  else if (!strcmp(command_id, "SET_HOST_NOTIFICATION_NUMBER"))
    command_type = CMD_SET_HOST_NOTIFICATION_NUMBER;

  else if (!strcmp(command_id, "CHANGE_HOST_CHECK_TIMEPERIOD"))
    command_type = CMD_CHANGE_HOST_CHECK_TIMEPERIOD;

  else if (!strcmp(command_id, "CHANGE_CUSTOM_HOST_VAR"))
    command_type = CMD_CHANGE_CUSTOM_HOST_VAR;

  else if (!strcmp(command_id, "SEND_CUSTOM_HOST_NOTIFICATION"))
    command_type = CMD_SEND_CUSTOM_HOST_NOTIFICATION;

  else if (!strcmp(command_id, "CHANGE_HOST_NOTIFICATION_TIMEPERIOD"))
    command_type = CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD;

  else if (!strcmp(command_id, "CHANGE_HOST_MODATTR"))
    command_type = CMD_CHANGE_HOST_MODATTR;

  /************************************/
  /**** HOSTGROUP-RELATED COMMANDS ****/
  /************************************/

  else if (!strcmp(command_id, "ENABLE_HOSTGROUP_HOST_NOTIFICATIONS"))
    command_type = CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_HOSTGROUP_HOST_NOTIFICATIONS"))
    command_type = CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_HOSTGROUP_SVC_NOTIFICATIONS"))
    command_type = CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_HOSTGROUP_SVC_NOTIFICATIONS"))
    command_type = CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_HOSTGROUP_HOST_CHECKS"))
    command_type = CMD_ENABLE_HOSTGROUP_HOST_CHECKS;
  else if (!strcmp(command_id, "DISABLE_HOSTGROUP_HOST_CHECKS"))
    command_type = CMD_DISABLE_HOSTGROUP_HOST_CHECKS;

  else if (!strcmp(command_id, "ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS"))
    command_type = CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS;
  else if (!strcmp(command_id, "DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS"))
    command_type = CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS;

  else if (!strcmp(command_id, "ENABLE_HOSTGROUP_SVC_CHECKS"))
    command_type = CMD_ENABLE_HOSTGROUP_SVC_CHECKS;
  else if (!strcmp(command_id, "DISABLE_HOSTGROUP_SVC_CHECKS"))
    command_type = CMD_DISABLE_HOSTGROUP_SVC_CHECKS;

  else if (!strcmp(command_id, "ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS"))
    command_type = CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS;
  else if (!strcmp(command_id, "DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS"))
    command_type = CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS;

  else if (!strcmp(command_id, "SCHEDULE_HOSTGROUP_HOST_DOWNTIME"))
    command_type = CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME;
  else if (!strcmp(command_id, "SCHEDULE_HOSTGROUP_SVC_DOWNTIME"))
    command_type = CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME;

  /**********************************/
  /**** SERVICE-RELATED COMMANDS ****/
  /**********************************/

  else if (!strcmp(command_id, "ADD_SVC_COMMENT"))
    command_type = CMD_ADD_SVC_COMMENT;
  else if (!strcmp(command_id, "DEL_SVC_COMMENT"))
    command_type = CMD_DEL_SVC_COMMENT;
  else if (!strcmp(command_id, "DEL_ALL_SVC_COMMENTS"))
    command_type = CMD_DEL_ALL_SVC_COMMENTS;

  else if (!strcmp(command_id, "SCHEDULE_SVC_CHECK"))
    command_type = CMD_SCHEDULE_SVC_CHECK;
  else if (!strcmp(command_id, "SCHEDULE_FORCED_SVC_CHECK"))
    command_type = CMD_SCHEDULE_FORCED_SVC_CHECK;

  else if (!strcmp(command_id, "ENABLE_SVC_CHECK"))
    command_type = CMD_ENABLE_SVC_CHECK;
  else if (!strcmp(command_id, "DISABLE_SVC_CHECK"))
    command_type = CMD_DISABLE_SVC_CHECK;

  else if (!strcmp(command_id, "ENABLE_PASSIVE_SVC_CHECKS"))
    command_type = CMD_ENABLE_PASSIVE_SVC_CHECKS;
  else if (!strcmp(command_id, "DISABLE_PASSIVE_SVC_CHECKS"))
    command_type = CMD_DISABLE_PASSIVE_SVC_CHECKS;

  else if (!strcmp(command_id, "DELAY_SVC_NOTIFICATION"))
    command_type = CMD_DELAY_SVC_NOTIFICATION;
  else if (!strcmp(command_id, "ENABLE_SVC_NOTIFICATIONS"))
    command_type = CMD_ENABLE_SVC_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_SVC_NOTIFICATIONS"))
    command_type = CMD_DISABLE_SVC_NOTIFICATIONS;

  else if (!strcmp(command_id, "PROCESS_SERVICE_CHECK_RESULT"))
    command_type = CMD_PROCESS_SERVICE_CHECK_RESULT;
  else if (!strcmp(command_id, "PROCESS_HOST_CHECK_RESULT"))
    command_type = CMD_PROCESS_HOST_CHECK_RESULT;

  else if (!strcmp(command_id, "ENABLE_SVC_EVENT_HANDLER"))
    command_type = CMD_ENABLE_SVC_EVENT_HANDLER;
  else if (!strcmp(command_id, "DISABLE_SVC_EVENT_HANDLER"))
    command_type = CMD_DISABLE_SVC_EVENT_HANDLER;

  else if (!strcmp(command_id, "ENABLE_SVC_FLAP_DETECTION"))
    command_type = CMD_ENABLE_SVC_FLAP_DETECTION;
  else if (!strcmp(command_id, "DISABLE_SVC_FLAP_DETECTION"))
    command_type = CMD_DISABLE_SVC_FLAP_DETECTION;

  else if (!strcmp(command_id, "SCHEDULE_SVC_DOWNTIME"))
    command_type = CMD_SCHEDULE_SVC_DOWNTIME;
  else if (!strcmp(command_id, "DEL_SVC_DOWNTIME"))
    command_type = CMD_DEL_SVC_DOWNTIME;
  else if (!strcmp(command_id, "DEL_SVC_DOWNTIME_FULL"))
    command_type = CMD_DEL_SVC_DOWNTIME_FULL;

  else if (!strcmp(command_id, "ACKNOWLEDGE_SVC_PROBLEM"))
    command_type = CMD_ACKNOWLEDGE_SVC_PROBLEM;
  else if (!strcmp(command_id, "REMOVE_SVC_ACKNOWLEDGEMENT"))
    command_type = CMD_REMOVE_SVC_ACKNOWLEDGEMENT;

  else if (!strcmp(command_id, "START_OBSESSING_OVER_SVC"))
    command_type = CMD_START_OBSESSING_OVER_SVC;
  else if (!strcmp(command_id, "STOP_OBSESSING_OVER_SVC"))
    command_type = CMD_STOP_OBSESSING_OVER_SVC;

  else if (!strcmp(command_id, "CHANGE_SVC_EVENT_HANDLER"))
    command_type = CMD_CHANGE_SVC_EVENT_HANDLER;
  else if (!strcmp(command_id, "CHANGE_SVC_CHECK_COMMAND"))
    command_type = CMD_CHANGE_SVC_CHECK_COMMAND;

  else if (!strcmp(command_id, "CHANGE_NORMAL_SVC_CHECK_INTERVAL"))
    command_type = CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL;
  else if (!strcmp(command_id, "CHANGE_RETRY_SVC_CHECK_INTERVAL"))
    command_type = CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL;

  else if (!strcmp(command_id, "CHANGE_MAX_SVC_CHECK_ATTEMPTS"))
    command_type = CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS;

  else if (!strcmp(command_id, "SET_SVC_NOTIFICATION_NUMBER"))
    command_type = CMD_SET_SVC_NOTIFICATION_NUMBER;

  else if (!strcmp(command_id, "CHANGE_SVC_CHECK_TIMEPERIOD"))
    command_type = CMD_CHANGE_SVC_CHECK_TIMEPERIOD;

  else if (!strcmp(command_id, "CHANGE_CUSTOM_SVC_VAR"))
    command_type = CMD_CHANGE_CUSTOM_SVC_VAR;

  else if (!strcmp(command_id, "CHANGE_CUSTOM_CONTACT_VAR"))
    command_type = CMD_CHANGE_CUSTOM_CONTACT_VAR;

  else if (!strcmp(command_id, "SEND_CUSTOM_SVC_NOTIFICATION"))
    command_type = CMD_SEND_CUSTOM_SVC_NOTIFICATION;

  else if (!strcmp(command_id, "CHANGE_SVC_NOTIFICATION_TIMEPERIOD"))
    command_type = CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD;

  else if (!strcmp(command_id, "CHANGE_SVC_MODATTR"))
    command_type = CMD_CHANGE_SVC_MODATTR;

  /***************************************/
  /**** SERVICEGROUP-RELATED COMMANDS ****/
  /***************************************/

  else if (!strcmp(command_id, "ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS"))
    command_type = CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS"))
    command_type = CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS"))
    command_type = CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS"))
    command_type = CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_SERVICEGROUP_HOST_CHECKS"))
    command_type = CMD_ENABLE_SERVICEGROUP_HOST_CHECKS;
  else if (!strcmp(command_id, "DISABLE_SERVICEGROUP_HOST_CHECKS"))
    command_type = CMD_DISABLE_SERVICEGROUP_HOST_CHECKS;

  else if (!strcmp(command_id, "ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS"))
    command_type = CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS;
  else if (!strcmp(command_id, "DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS"))
    command_type = CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS;

  else if (!strcmp(command_id, "ENABLE_SERVICEGROUP_SVC_CHECKS"))
    command_type = CMD_ENABLE_SERVICEGROUP_SVC_CHECKS;
  else if (!strcmp(command_id, "DISABLE_SERVICEGROUP_SVC_CHECKS"))
    command_type = CMD_DISABLE_SERVICEGROUP_SVC_CHECKS;

  else if (!strcmp(command_id, "ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS"))
    command_type = CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS;
  else if (!strcmp(command_id, "DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS"))
    command_type = CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS;

  else if (!strcmp(command_id, "SCHEDULE_SERVICEGROUP_HOST_DOWNTIME"))
    command_type = CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME;
  else if (!strcmp(command_id, "SCHEDULE_SERVICEGROUP_SVC_DOWNTIME"))
    command_type = CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME;

  /**********************************/
  /**** CONTACT-RELATED COMMANDS ****/
  /**********************************/

  else if (!strcmp(command_id, "ENABLE_CONTACT_HOST_NOTIFICATIONS"))
    command_type = CMD_ENABLE_CONTACT_HOST_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_CONTACT_HOST_NOTIFICATIONS"))
    command_type = CMD_DISABLE_CONTACT_HOST_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_CONTACT_SVC_NOTIFICATIONS"))
    command_type = CMD_ENABLE_CONTACT_SVC_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_CONTACT_SVC_NOTIFICATIONS"))
    command_type = CMD_DISABLE_CONTACT_SVC_NOTIFICATIONS;

  else if (!strcmp(command_id, "CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD"))
    command_type = CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD;

  else if (!strcmp(command_id, "CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD"))
    command_type = CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD;

  else if (!strcmp(command_id, "CHANGE_CONTACT_MODATTR"))
    command_type = CMD_CHANGE_CONTACT_MODATTR;
  else if (!strcmp(command_id, "CHANGE_CONTACT_MODHATTR"))
    command_type = CMD_CHANGE_CONTACT_MODHATTR;
  else if (!strcmp(command_id, "CHANGE_CONTACT_MODSATTR"))
    command_type = CMD_CHANGE_CONTACT_MODSATTR;

  /***************************************/
  /**** CONTACTGROUP-RELATED COMMANDS ****/
  /***************************************/

  else if (!strcmp(command_id, "ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS"))
    command_type = CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS"))
    command_type = CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS;

  else if (!strcmp(command_id, "ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS"))
    command_type = CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS;
  else if (!strcmp(command_id, "DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS"))
    command_type = CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS;

  /**************************/
  /****** MISC COMMANDS *****/
  /**************************/

  else if (!strcmp(command_id, "PROCESS_FILE"))
    command_type = CMD_PROCESS_FILE;

  /****************************/
  /****** CUSTOM COMMANDS *****/
  /****************************/

  else if (command_id[0] == '_')
    command_type = CMD_CUSTOM_COMMAND;

  /**** UNKNOWN COMMAND* ***/
  else {
    /* log the bad external command */
    logger(log_external_command | log_runtime_warning, basic)
      << "Warning: Unrecognized external command -> "
      << command_id << ";" << args;

    /* free memory */
    delete[] command_id;
    delete[] args;

    return ERROR;
  }

  /* update statistics for external commands */
  update_check_stats(EXTERNAL_COMMAND_STATS, time(nullptr));

  /* log the external command */
  std::ostringstream oss;
  oss << "EXTERNAL COMMAND: " << command_id << ';' << args << std::endl;

  if (command_type == CMD_PROCESS_SERVICE_CHECK_RESULT
      || command_type == CMD_PROCESS_HOST_CHECK_RESULT) {
    /* passive checks are logged in checks.c as well, as some my bypass external commands by getting dropped in checkresults dir */
    if (config->log_passive_checks())
      logger(log_passive_check, basic) << oss.str();
  }
  else {
    if (config->log_external_commands())
      logger(log_external_command, basic) << oss.str();
  }

  /* send data to event broker */
  broker_external_command(NEBTYPE_EXTERNALCOMMAND_START,
                          NEBFLAG_NONE,
                          NEBATTR_NONE,
                          command_type,
                          entry_time,
                          command_id,
                          args,
                          nullptr);

  /* process the command */
  process_external_command2(command_type, entry_time, args);

  /* send data to event broker */
  broker_external_command(NEBTYPE_EXTERNALCOMMAND_END,
                          NEBFLAG_NONE,
                          NEBATTR_NONE,
                          command_type,
                          entry_time,
                          command_id,
                          args,
                          nullptr);

  /* free memory */
  delete[] command_id;
  delete[] args;

  return OK;
}

int process_external_command2(int cmd,
                              time_t entry_time,
                              char* args) {
  logger(dbg_functions, basic)
    << "process_external_command2()";
  logger(dbg_external_command, more)
    << "External Command Type: " << cmd;
  logger(dbg_external_command, more)
    << "Command Entry Time: " << (unsigned long)entry_time;
  logger(dbg_external_command, more)
    << "Command Arguments: " << (args == nullptr ? "" : args);

  /* how shall we execute the command? */
  switch (cmd) {
    /***************************/
    /***** SYSTEM COMMANDS *****/
    /***************************/

  case CMD_SHUTDOWN_PROCESS:
  case CMD_RESTART_PROCESS:
    cmd_signal_process(cmd, args);
    break;

  case CMD_SAVE_STATE_INFORMATION:
    retention::dump::save(config->state_retention_file());
    break;

  case CMD_READ_STATE_INFORMATION:
    {
      retention::state state;
      retention::parser p;
      p.parse(config->state_retention_file(), state);
      retention::applier::state app_state;
      app_state.apply(*config, state);
    }
    break;

  case CMD_ENABLE_NOTIFICATIONS:
    enable_all_notifications();
    break;

  case CMD_DISABLE_NOTIFICATIONS:
    disable_all_notifications();
    break;

  case CMD_START_EXECUTING_SVC_CHECKS:
    start_executing_service_checks();
    break;

  case CMD_STOP_EXECUTING_SVC_CHECKS:
    stop_executing_service_checks();
    break;

  case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
    start_accepting_passive_service_checks();
    break;

  case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
    stop_accepting_passive_service_checks();
    break;

  case CMD_START_OBSESSING_OVER_SVC_CHECKS:
    start_obsessing_over_service_checks();
    break;

  case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
    stop_obsessing_over_service_checks();
    break;

  case CMD_START_EXECUTING_HOST_CHECKS:
    start_executing_host_checks();
    break;

  case CMD_STOP_EXECUTING_HOST_CHECKS:
    stop_executing_host_checks();
    break;

  case CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS:
    start_accepting_passive_host_checks();
    break;

  case CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS:
    stop_accepting_passive_host_checks();
    break;

  case CMD_START_OBSESSING_OVER_HOST_CHECKS:
    start_obsessing_over_host_checks();
    break;

  case CMD_STOP_OBSESSING_OVER_HOST_CHECKS:
    stop_obsessing_over_host_checks();
    break;

  case CMD_ENABLE_EVENT_HANDLERS:
    start_using_event_handlers();
    break;

  case CMD_DISABLE_EVENT_HANDLERS:
    stop_using_event_handlers();
    break;

  case CMD_ENABLE_FLAP_DETECTION:
    enable_flap_detection_routines();
    break;

  case CMD_DISABLE_FLAP_DETECTION:
    disable_flap_detection_routines();
    break;

  case CMD_ENABLE_SERVICE_FRESHNESS_CHECKS:
    enable_service_freshness_checks();
    break;

  case CMD_DISABLE_SERVICE_FRESHNESS_CHECKS:
    disable_service_freshness_checks();
    break;

  case CMD_ENABLE_HOST_FRESHNESS_CHECKS:
    enable_host_freshness_checks();
    break;

  case CMD_DISABLE_HOST_FRESHNESS_CHECKS:
    disable_host_freshness_checks();
    break;

  case CMD_ENABLE_FAILURE_PREDICTION:
    break;

  case CMD_DISABLE_FAILURE_PREDICTION:
    break;

  case CMD_ENABLE_PERFORMANCE_DATA:
    enable_performance_data();
    break;

  case CMD_DISABLE_PERFORMANCE_DATA:
    disable_performance_data();
    break;

    /***************************/
    /*****  HOST COMMANDS  *****/
    /***************************/

  case CMD_ENABLE_HOST_CHECK:
  case CMD_DISABLE_HOST_CHECK:
  case CMD_ENABLE_PASSIVE_HOST_CHECKS:
  case CMD_DISABLE_PASSIVE_HOST_CHECKS:
  case CMD_ENABLE_HOST_SVC_CHECKS:
  case CMD_DISABLE_HOST_SVC_CHECKS:
  case CMD_ENABLE_HOST_NOTIFICATIONS:
  case CMD_DISABLE_HOST_NOTIFICATIONS:
  case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
  case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
  case CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS:
  case CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS:
  case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
  case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
  case CMD_ENABLE_HOST_FLAP_DETECTION:
  case CMD_DISABLE_HOST_FLAP_DETECTION:
  case CMD_ENABLE_HOST_EVENT_HANDLER:
  case CMD_DISABLE_HOST_EVENT_HANDLER:
  case CMD_START_OBSESSING_OVER_HOST:
  case CMD_STOP_OBSESSING_OVER_HOST:
  case CMD_SET_HOST_NOTIFICATION_NUMBER:
  case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
    process_host_command(cmd, entry_time, args);
    break;

    /*****************************/
    /***** HOSTGROUP COMMANDS ****/
    /*****************************/

  case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
  case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
  case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
  case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
  case CMD_ENABLE_HOSTGROUP_HOST_CHECKS:
  case CMD_DISABLE_HOSTGROUP_HOST_CHECKS:
  case CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
  case CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
  case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
  case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
  case CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
  case CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
    process_hostgroup_command(cmd, entry_time, args);
    break;

    /***************************/
    /***** SERVICE COMMANDS ****/
    /***************************/

  case CMD_ENABLE_SVC_CHECK:
  case CMD_DISABLE_SVC_CHECK:
  case CMD_ENABLE_PASSIVE_SVC_CHECKS:
  case CMD_DISABLE_PASSIVE_SVC_CHECKS:
  case CMD_ENABLE_SVC_NOTIFICATIONS:
  case CMD_DISABLE_SVC_NOTIFICATIONS:
  case CMD_ENABLE_SVC_FLAP_DETECTION:
  case CMD_DISABLE_SVC_FLAP_DETECTION:
  case CMD_ENABLE_SVC_EVENT_HANDLER:
  case CMD_DISABLE_SVC_EVENT_HANDLER:
  case CMD_START_OBSESSING_OVER_SVC:
  case CMD_STOP_OBSESSING_OVER_SVC:
  case CMD_SET_SVC_NOTIFICATION_NUMBER:
  case CMD_SEND_CUSTOM_SVC_NOTIFICATION:
    process_service_command(cmd, entry_time, args);
    break;

    /********************************/
    /***** SERVICEGROUP COMMANDS ****/
    /********************************/

  case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
  case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
  case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
  case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
  case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
  case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
  case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
  case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
  case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
  case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
  case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
  case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
    process_servicegroup_command(cmd, entry_time, args);
    break;

    /**********************************/
    /**** CONTACT-RELATED COMMANDS ****/
    /**********************************/

  case CMD_ENABLE_CONTACT_HOST_NOTIFICATIONS:
  case CMD_DISABLE_CONTACT_HOST_NOTIFICATIONS:
  case CMD_ENABLE_CONTACT_SVC_NOTIFICATIONS:
  case CMD_DISABLE_CONTACT_SVC_NOTIFICATIONS:
    process_contact_command(cmd, entry_time, args);
    break;

    /***************************************/
    /**** CONTACTGROUP-RELATED COMMANDS ****/
    /***************************************/

  case CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
  case CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
  case CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
  case CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
    process_contactgroup_command(cmd, entry_time, args);
    break;

    /***************************/
    /**** UNSORTED COMMANDS ****/
    /***************************/

  case CMD_ADD_HOST_COMMENT:
  case CMD_ADD_SVC_COMMENT:
    cmd_add_comment(cmd, entry_time, args);
    break;

  case CMD_DEL_HOST_COMMENT:
  case CMD_DEL_SVC_COMMENT:
    cmd_delete_comment(cmd, args);
    break;

  case CMD_DELAY_HOST_NOTIFICATION:
  case CMD_DELAY_SVC_NOTIFICATION:
    cmd_delay_notification(cmd, args);
    break;

  case CMD_SCHEDULE_SVC_CHECK:
  case CMD_SCHEDULE_FORCED_SVC_CHECK:
    cmd_schedule_check(cmd, args);
    break;

  case CMD_SCHEDULE_HOST_SVC_CHECKS:
  case CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS:
    cmd_schedule_check(cmd, args);
    break;

  case CMD_DEL_ALL_HOST_COMMENTS:
  case CMD_DEL_ALL_SVC_COMMENTS:
    cmd_delete_all_comments(cmd, args);
    break;

  case CMD_PROCESS_SERVICE_CHECK_RESULT:
    cmd_process_service_check_result(cmd, entry_time, args);
    break;

  case CMD_PROCESS_HOST_CHECK_RESULT:
    cmd_process_host_check_result(cmd, entry_time, args);
    break;

  case CMD_ACKNOWLEDGE_HOST_PROBLEM:
  case CMD_ACKNOWLEDGE_SVC_PROBLEM:
    cmd_acknowledge_problem(cmd, args);
    break;

  case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
  case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
    cmd_remove_acknowledgement(cmd, args);
    break;

  case CMD_SCHEDULE_HOST_DOWNTIME:
  case CMD_SCHEDULE_SVC_DOWNTIME:
  case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
  case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
  case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
  case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
  case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
  case CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME:
  case CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME:
    cmd_schedule_downtime(cmd, entry_time, args);
    break;

  case CMD_DEL_HOST_DOWNTIME:
  case CMD_DEL_SVC_DOWNTIME:
    cmd_delete_downtime(cmd, args);
    break;

  case CMD_DEL_HOST_DOWNTIME_FULL:
  case CMD_DEL_SVC_DOWNTIME_FULL:
    cmd_delete_downtime_full(cmd, args);
    break ;

  case CMD_DEL_DOWNTIME_BY_HOST_NAME:
    cmd_delete_downtime_by_host_name(cmd, args);
    break ;

  case CMD_DEL_DOWNTIME_BY_HOSTGROUP_NAME:
    cmd_delete_downtime_by_hostgroup_name(cmd, args);
    break ;

  case CMD_DEL_DOWNTIME_BY_START_TIME_COMMENT:
    cmd_delete_downtime_by_start_time_comment(cmd, args);
    break ;

  case CMD_CANCEL_ACTIVE_HOST_SVC_DOWNTIME:
  case CMD_CANCEL_PENDING_HOST_SVC_DOWNTIME:
    break;

  case CMD_SCHEDULE_HOST_CHECK:
  case CMD_SCHEDULE_FORCED_HOST_CHECK:
    cmd_schedule_check(cmd, args);
    break;

  case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
  case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
  case CMD_CHANGE_HOST_EVENT_HANDLER:
  case CMD_CHANGE_SVC_EVENT_HANDLER:
  case CMD_CHANGE_HOST_CHECK_COMMAND:
  case CMD_CHANGE_SVC_CHECK_COMMAND:
  case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
  case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
  case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:
  case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:
  case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
  case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:
    cmd_change_object_char_var(cmd, args);
    break;

  case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:
  case CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL:
  case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:
  case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:
  case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:
  case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:
  case CMD_CHANGE_HOST_MODATTR:
  case CMD_CHANGE_SVC_MODATTR:
  case CMD_CHANGE_CONTACT_MODATTR:
  case CMD_CHANGE_CONTACT_MODHATTR:
  case CMD_CHANGE_CONTACT_MODSATTR:
    cmd_change_object_int_var(cmd, args);
    break;

  case CMD_CHANGE_CUSTOM_HOST_VAR:
  case CMD_CHANGE_CUSTOM_SVC_VAR:
  case CMD_CHANGE_CUSTOM_CONTACT_VAR:
    cmd_change_object_custom_var(cmd, args);
    break;

    /***********************/
    /**** MISC COMMANDS ****/
    /***********************/

  case CMD_PROCESS_FILE:
    cmd_process_external_commands_from_file(cmd, args);
    break;

    /*************************/
    /**** CUSTOM COMMANDS ****/
    /*************************/

  case CMD_CUSTOM_COMMAND:
    /* custom commands aren't handled internally by Centreon Engine, but may be by NEB modules */
    break;

  default:
    return ERROR;
  }

  return OK;
}

int process_hostgroup_command(int cmd,
                              time_t entry_time,
                              char* args) {
  char* hostgroup_name = nullptr;
  hostgroup* temp_hostgroup = nullptr;

  (void)entry_time;

  /* get the hostgroup name */
  if ((hostgroup_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* find the hostgroup */
  temp_hostgroup = nullptr;
  hostgroup_map::const_iterator
    it(hostgroup::hostgroups.find(hostgroup_name));
  if (it != hostgroup::hostgroups.end())
    temp_hostgroup = it->second.get();
  if (temp_hostgroup == nullptr)
    return ERROR;

  /* loop through all hosts in the hostgroup */
  for (host_map::iterator
         it(temp_hostgroup->members.begin()),
         end(temp_hostgroup->members.end());
       it != end;
       ++it) {

    if (it->second == nullptr)
      continue;

    switch (cmd) {

    case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
      enable_host_notifications(it->second.get());
      break;

    case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
      disable_host_notifications(it->second.get());
      break;

    case CMD_ENABLE_HOSTGROUP_HOST_CHECKS:
      enable_host_checks(it->second.get());
      break;

    case CMD_DISABLE_HOSTGROUP_HOST_CHECKS:
      disable_host_checks(it->second.get());
      break;

    case CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
      enable_passive_host_checks(it->second.get());
      break;

    case CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
      disable_passive_host_checks(it->second.get());
      break;

    default:

      /* loop through all services on the host */
      for (service_map::iterator
             it2(it->second->services.begin()),
             end2(it->second->services.end());
           it2 != end2;
           ++it2) {
        if (!it2->second)
          continue;

        switch (cmd) {

        case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
          enable_service_notifications(it2->second);
          break;

        case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
          disable_service_notifications(it2->second);
          break;

        case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
          enable_service_checks(it2->second);
          break;

        case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
          disable_service_checks(it2->second);
          break;

        case CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
          enable_passive_service_checks(it2->second);
          break;

        case CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
          disable_passive_service_checks(it2->second);
          break;

        default:
          break;
        }
      }
      break;
    }
  }
  return OK;
}

int process_host_command(int cmd,
                         time_t entry_time,
                         char* args) {
  char* host_name = nullptr;
  host* temp_host = nullptr;
  char* str = nullptr;
  char* buf[2] = { nullptr, nullptr };
  int intval = 0;

  (void)entry_time;

  /* get the host name */
  if ((host_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* find the host */
  temp_host = nullptr;
  umap<uint64_t, std::shared_ptr<com::centreon::engine::host>>::const_iterator
    it = configuration::applier::state::instance().hosts().find(get_host_id(host_name));
  if (it != configuration::applier::state::instance().hosts().end())
    temp_host = it->second.get();
  if (temp_host == nullptr)
    return ERROR;

  switch (cmd) {
  case CMD_ENABLE_HOST_NOTIFICATIONS:
    enable_host_notifications(temp_host);
    break;

  case CMD_DISABLE_HOST_NOTIFICATIONS:
    disable_host_notifications(temp_host);
    break;

  case CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS:
    enable_and_propagate_notifications(temp_host, 0, true, true, false);
    break;

  case CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS:
    disable_and_propagate_notifications(temp_host, 0, true, true, false);
    break;

  case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
    enable_and_propagate_notifications(temp_host, 0, false, true, true);
    break;

  case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
    disable_and_propagate_notifications(temp_host, 0, false, true, true);
    break;

  case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
  case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
    for (service_map::iterator
           it(temp_host->services.begin()),
           end(temp_host->services.end());
         it != end;
         ++it) {
      if (!it->second)
        continue;
      if (cmd == CMD_ENABLE_HOST_SVC_NOTIFICATIONS)
        enable_service_notifications(it->second);
      else
        disable_service_notifications(it->second);
    }
    break;

  case CMD_ENABLE_HOST_SVC_CHECKS:
  case CMD_DISABLE_HOST_SVC_CHECKS:
    for (service_map::iterator
           it(temp_host->services.begin()),
           end(temp_host->services.end());
         it != end;
         ++it) {
      if (!it->second)
        continue;
      if (cmd == CMD_ENABLE_HOST_SVC_CHECKS)
        enable_service_checks(it->second);
      else
        disable_service_checks(it->second);
    }
    break;

  case CMD_ENABLE_HOST_CHECK:
    enable_host_checks(temp_host);
    break;

  case CMD_DISABLE_HOST_CHECK:
    disable_host_checks(temp_host);
    break;

  case CMD_ENABLE_HOST_EVENT_HANDLER:
    enable_host_event_handler(temp_host);
    break;

  case CMD_DISABLE_HOST_EVENT_HANDLER:
    disable_host_event_handler(temp_host);
    break;

  case CMD_ENABLE_HOST_FLAP_DETECTION:
    enable_host_flap_detection(temp_host);
    break;

  case CMD_DISABLE_HOST_FLAP_DETECTION:
    disable_host_flap_detection(temp_host);
    break;

  case CMD_ENABLE_PASSIVE_HOST_CHECKS:
    enable_passive_host_checks(temp_host);
    break;

  case CMD_DISABLE_PASSIVE_HOST_CHECKS:
    disable_passive_host_checks(temp_host);
    break;

  case CMD_START_OBSESSING_OVER_HOST:
    start_obsessing_over_host(temp_host);
    break;

  case CMD_STOP_OBSESSING_OVER_HOST:
    stop_obsessing_over_host(temp_host);
    break;

  case CMD_SET_HOST_NOTIFICATION_NUMBER:
    if ((str = my_strtok(nullptr, ";"))) {
      intval = atoi(str);
      set_host_notification_number(temp_host, intval);
    }
    break;

  case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
    if ((str = my_strtok(nullptr, ";")))
      intval = atoi(str);
    str = my_strtok(nullptr, ";");
    if (str)
      buf[0] = string::dup(str);
    str = my_strtok(nullptr, ";");
    if (str)
      buf[1] = string::dup(str);
    if (buf[0] && buf[1])
      temp_host->notify(notifier::notification_custom, buf[0], buf[1], intval);
    break;

  default:
    break;
  }

  return OK;
}

int process_service_command(int cmd,
                            time_t entry_time,
                            char* args) {
  char* host_name = nullptr;
  char* svc_description = nullptr;
  char* str = nullptr;
  char* buf[2] = { nullptr, nullptr };
  int intval = 0;

  (void)entry_time;

  /* get the host name */
  if ((host_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* get the service description */
  if ((svc_description = my_strtok(nullptr, ";")) == nullptr)
    return ERROR;

  /* find the service */
  std::pair<uint64_t, uint64_t> id(get_host_and_service_id(
    host_name, svc_description));
  std::unordered_map<std::pair<uint64_t, uint64_t>,
                     std::shared_ptr<service> >::const_iterator
    found(state::instance().services().find(id));
  if (found == state::instance().services().end() || !found->second.get())
    return ERROR;

  switch (cmd) {
  case CMD_ENABLE_SVC_NOTIFICATIONS:
    enable_service_notifications(found->second);
    break;

  case CMD_DISABLE_SVC_NOTIFICATIONS:
    disable_service_notifications(found->second);
    break;

  case CMD_ENABLE_SVC_CHECK:
    enable_service_checks(found->second);
    break;

  case CMD_DISABLE_SVC_CHECK:
    disable_service_checks(found->second);
    break;

  case CMD_ENABLE_SVC_EVENT_HANDLER:
    enable_service_event_handler(found->second);
    break;

  case CMD_DISABLE_SVC_EVENT_HANDLER:
    disable_service_event_handler(found->second);
    break;

  case CMD_ENABLE_SVC_FLAP_DETECTION:
    enable_service_flap_detection(found->second.get());
    break;

  case CMD_DISABLE_SVC_FLAP_DETECTION:
    disable_service_flap_detection(found->second.get());
    break;

  case CMD_ENABLE_PASSIVE_SVC_CHECKS:
    enable_passive_service_checks(found->second);
    break;

  case CMD_DISABLE_PASSIVE_SVC_CHECKS:
    disable_passive_service_checks(found->second);
    break;

  case CMD_START_OBSESSING_OVER_SVC:
    start_obsessing_over_service(found->second);
    break;

  case CMD_STOP_OBSESSING_OVER_SVC:
    stop_obsessing_over_service(found->second);
    break;

  case CMD_SET_SVC_NOTIFICATION_NUMBER:
    if ((str = my_strtok(nullptr, ";"))) {
      intval = atoi(str);
      set_service_notification_number(found->second, intval);
    }
    break;

  case CMD_SEND_CUSTOM_SVC_NOTIFICATION:
    if ((str = my_strtok(nullptr, ";")))
      intval = atoi(str);
    str = my_strtok(nullptr, ";");
    if (str)
      buf[0] = string::dup(str);
    str = my_strtok(nullptr, ";");
    if (str)
      buf[1] = string::dup(str);
    if (buf[0] && buf[1])
      found->second->notify(
                           notifier::notification_custom,
                           buf[0],
                           buf[1],
                           intval);
    break;

  default:
    break;
  }

  return OK;
}

int process_servicegroup_command(int cmd,
                                 time_t entry_time,
                                 char* args) {
  (void)entry_time;
  char* servicegroup_name{nullptr};
  std::shared_ptr<servicegroup> temp_servicegroup;
  host* temp_host{nullptr};
  host* last_host{nullptr};

  /* get the servicegroup name */
  if ((servicegroup_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* find the servicegroup */
  servicegroup_map::const_iterator
    sg_it{servicegroup::servicegroups.find(servicegroup_name)};
  if (sg_it == servicegroup::servicegroups.end() || !sg_it->second)
    return ERROR;
  temp_servicegroup = sg_it->second;

  switch (cmd) {

  case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
  case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
  case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
  case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
  case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
  case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:

    /* loop through all servicegroup members */
    for (service_map::iterator
           it(sg_it->second->members.begin()),
           end(sg_it->second->members.end());
         it != end;
         ++it) {

      std::pair<uint64_t, uint64_t> id(get_host_and_service_id(
        it->first.first, it->first.second));
      std::unordered_map<std::pair<uint64_t, uint64_t>,
                         std::shared_ptr<service> >::const_iterator
        found(state::instance().services().find(id));
      if (found == state::instance().services().end() || !found->second)
        continue;

      switch (cmd) {

      case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
        enable_service_notifications(found->second);
        break;

      case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
        disable_service_notifications(found->second);
        break;

      case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
        enable_service_checks(found->second);
        break;

      case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
        disable_service_checks(found->second);
        break;

      case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
        enable_passive_service_checks(found->second);
        break;

      case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
        disable_passive_service_checks(found->second);
        break;

      default:
        break;
      }
    }
    break;

  case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
  case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
  case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
  case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
  case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
  case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
    /* loop through all hosts that have services belonging to the servicegroup */
    last_host = nullptr;
    for (service_map::iterator
           it(sg_it->second->members.begin()),
           end(sg_it->second->members.end());
         it != end;
         ++it) {
      temp_host = nullptr;
      umap<uint64_t,
           std::shared_ptr<com::centreon::engine::host>>::const_iterator found =
          configuration::applier::state::instance().hosts().find(
              get_host_id(it->first.first));
      if (found != configuration::applier::state::instance().hosts().end())
        temp_host = found->second.get();
      if (temp_host == nullptr)
        continue;

      if (temp_host == last_host)
        continue;

      switch (cmd) {
        case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
          enable_host_notifications(temp_host);
          break;

        case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
          disable_host_notifications(temp_host);
          break;

        case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
          enable_host_checks(temp_host);
          break;

        case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
          disable_host_checks(temp_host);
          break;

        case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
          enable_passive_host_checks(temp_host);
          break;

        case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
          disable_passive_host_checks(temp_host);
          break;

        default:
          break;
      }

      last_host = temp_host;
    }
    break;

  default:
    break;
  }
  return OK;
}

int process_contact_command(int cmd,
                            time_t entry_time,
                            char* args) {
  char* contact_name = nullptr;
  contact* temp_contact = nullptr;

  (void)entry_time;

  /* get the contact name */
  if ((contact_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* find the contact */
  if ((temp_contact = configuration::applier::state::instance().find_contact(contact_name)) == nullptr)
    return ERROR;

  switch (cmd) {

  case CMD_ENABLE_CONTACT_HOST_NOTIFICATIONS:
    enable_contact_host_notifications(temp_contact);
    break;

  case CMD_DISABLE_CONTACT_HOST_NOTIFICATIONS:
    disable_contact_host_notifications(temp_contact);
    break;

  case CMD_ENABLE_CONTACT_SVC_NOTIFICATIONS:
    enable_contact_service_notifications(temp_contact);
    break;

  case CMD_DISABLE_CONTACT_SVC_NOTIFICATIONS:
    disable_contact_service_notifications(temp_contact);
    break;

  default:
    break;
  }
  return OK;
}

int process_contactgroup_command(int cmd,
                                 time_t entry_time,
                                 char* args) {
  char* contactgroup_name = nullptr;
  contactgroup* temp_contactgroup = nullptr;

  (void)entry_time;

  /* get the contactgroup name */
  if ((contactgroup_name = my_strtok(args, ";")) == nullptr)
    return ERROR;

  /* find the contactgroup */
  if ((temp_contactgroup = configuration::applier::state::instance().find_contactgroup(contactgroup_name)) == nullptr)
    return ERROR;

  switch (cmd) {

  case CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
  case CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
  case CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
  case CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS:

    /* loop through all contactgroup members */
    for (contact_map::const_iterator
           it{temp_contactgroup->get_members().begin()},
           end{temp_contactgroup->get_members().end()};
         it != end; ++it) {

      if (!it->second)
        continue;

      switch (cmd) {

      case CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
        enable_contact_host_notifications(it->second.get());
        break;

      case CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
        disable_contact_host_notifications(it->second.get());
        break;

      case CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
        enable_contact_service_notifications(it->second.get());
        break;

      case CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
        disable_contact_service_notifications(it->second.get());
        break;

      default:
        break;
      }
    }
    break;

  default:
    break;
  }
  return OK;
}
