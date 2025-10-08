/**
 * Copyright 2011-2013,2015-2016 Centreon
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

#include "com/centreon/engine/commands/processing.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/retention/applier/state.hh"
#include "com/centreon/engine/retention/dump.hh"
#include "com/centreon/engine/retention/parser.hh"
#include "com/centreon/engine/retention/state.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::commands;
using namespace com::centreon::engine::commands::detail;

// Dummy command.
void dummy_command() {}

const std::unordered_map<std::string, command_info> processing::_lst_command(
    {{"ENTER_STANDBY_MODE",
      command_info(CMD_DISABLE_NOTIFICATIONS,
                   &_redirector<&disable_all_notifications>)},
     {"DISABLE_NOTIFICATIONS",
      command_info(CMD_DISABLE_NOTIFICATIONS,
                   &_redirector<&disable_all_notifications>)},
     // process commands.
     {"ENTER_ACTIVE_MODE",
      command_info(CMD_ENABLE_NOTIFICATIONS,
                   &_redirector<&enable_all_notifications>)},
     {"ENABLE_NOTIFICATIONS",
      command_info(CMD_ENABLE_NOTIFICATIONS,
                   &_redirector<&enable_all_notifications>)},
     {"SHUTDOWN_PROGRAM",
      command_info(CMD_SHUTDOWN_PROCESS, &_redirector<&cmd_signal_process>)},
     {"SHUTDOWN_PROCESS",
      command_info(CMD_SHUTDOWN_PROCESS, &_redirector<&cmd_signal_process>)},
     {"RESTART_PROGRAM",
      command_info(CMD_RESTART_PROCESS, &_redirector<&cmd_signal_process>)},
     {"RESTART_PROCESS",
      command_info(CMD_RESTART_PROCESS, &_redirector<&cmd_signal_process>)},
     {"SAVE_STATE_INFORMATION",
      command_info(CMD_SAVE_STATE_INFORMATION,
                   &_redirector<&_wrapper_save_state_information>)},
     {"READ_STATE_INFORMATION",
      command_info(CMD_READ_STATE_INFORMATION,
                   &_redirector<&_wrapper_read_state_information>)},
     {"ENABLE_EVENT_HANDLERS",
      command_info(CMD_ENABLE_EVENT_HANDLERS,
                   &_redirector<&start_using_event_handlers>)},
     {"DISABLE_EVENT_HANDLERS",
      command_info(CMD_DISABLE_EVENT_HANDLERS,
                   &_redirector<&stop_using_event_handlers>)},
     // _lst_command["FLUSH_PENDING_COMMANDS"] =
     //   command_info(CMD_FLUSH_PENDING_COMMANDS,
     //                &_redirector<&>);
     {"ENABLE_FAILURE_PREDICTION", command_info(CMD_ENABLE_FAILURE_PREDICTION,
                                                &_redirector<&dummy_command>)},
     {"DISABLE_FAILURE_PREDICTION", command_info(CMD_DISABLE_FAILURE_PREDICTION,
                                                 &_redirector<&dummy_command>)},
     {"ENABLE_PERFORMANCE_DATA",
      command_info(CMD_ENABLE_PERFORMANCE_DATA,
                   &_redirector<&enable_performance_data>)},
     {"DISABLE_PERFORMANCE_DATA",
      command_info(CMD_DISABLE_PERFORMANCE_DATA,
                   &_redirector<&disable_performance_data>)},
     {"START_EXECUTING_HOST_CHECKS",
      command_info(CMD_START_EXECUTING_HOST_CHECKS,
                   &_redirector<&start_executing_host_checks>)},
     {"STOP_EXECUTING_HOST_CHECKS",
      command_info(CMD_STOP_EXECUTING_HOST_CHECKS,
                   &_redirector<&stop_executing_host_checks>)},
     {"START_EXECUTING_SVC_CHECKS",
      command_info(CMD_START_EXECUTING_SVC_CHECKS,
                   &_redirector<&start_executing_service_checks>)},
     {"STOP_EXECUTING_SVC_CHECKS",
      command_info(CMD_STOP_EXECUTING_SVC_CHECKS,
                   &_redirector<&stop_executing_service_checks>)},
     {"START_ACCEPTING_PASSIVE_HOST_CHECKS",
      command_info(CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS,
                   &_redirector<&start_accepting_passive_host_checks>)},
     {"STOP_ACCEPTING_PASSIVE_HOST_CHECKS",
      command_info(CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS,
                   &_redirector<&stop_accepting_passive_host_checks>)},
     {"START_ACCEPTING_PASSIVE_SVC_CHECKS",
      command_info(CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS,
                   &_redirector<&start_accepting_passive_service_checks>)},
     {"STOP_ACCEPTING_PASSIVE_SVC_CHECKS",
      command_info(CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS,
                   &_redirector<&stop_accepting_passive_service_checks>)},
     {"START_OBSESSING_OVER_HOST_CHECKS",
      command_info(CMD_START_OBSESSING_OVER_HOST_CHECKS,
                   &_redirector<&start_obsessing_over_host_checks>)},
     {"STOP_OBSESSING_OVER_HOST_CHECKS",
      command_info(CMD_STOP_OBSESSING_OVER_HOST_CHECKS,
                   &_redirector<&stop_obsessing_over_host_checks>)},
     {"START_OBSESSING_OVER_SVC_CHECKS",
      command_info(CMD_START_OBSESSING_OVER_SVC_CHECKS,
                   &_redirector<&start_obsessing_over_service_checks>)},
     {"STOP_OBSESSING_OVER_SVC_CHECKS",
      command_info(CMD_STOP_OBSESSING_OVER_SVC_CHECKS,
                   &_redirector<&stop_obsessing_over_service_checks>)},
     {"ENABLE_FLAP_DETECTION",
      command_info(CMD_ENABLE_FLAP_DETECTION,
                   &_redirector<&enable_flap_detection_routines>)},
     {"DISABLE_FLAP_DETECTION",
      command_info(CMD_DISABLE_FLAP_DETECTION,
                   &_redirector<&disable_flap_detection_routines>)},
     {"CHANGE_GLOBAL_HOST_EVENT_HANDLER",
      command_info(CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_GLOBAL_SVC_EVENT_HANDLER",
      command_info(CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER,
                   &_redirector<&cmd_change_object_char_var>)},
     {"ENABLE_SERVICE_FRESHNESS_CHECKS",
      command_info(CMD_ENABLE_SERVICE_FRESHNESS_CHECKS,
                   &_redirector<&enable_service_freshness_checks>)},
     {"DISABLE_SERVICE_FRESHNESS_CHECKS",
      command_info(CMD_DISABLE_SERVICE_FRESHNESS_CHECKS,
                   &_redirector<&disable_service_freshness_checks>)},
     {"ENABLE_HOST_FRESHNESS_CHECKS",
      command_info(CMD_ENABLE_HOST_FRESHNESS_CHECKS,
                   &_redirector<&enable_host_freshness_checks>)},
     {"DISABLE_HOST_FRESHNESS_CHECKS",
      command_info(CMD_DISABLE_HOST_FRESHNESS_CHECKS,
                   &_redirector<&disable_host_freshness_checks>)},
     // host-related commands.
     {"ADD_HOST_COMMENT",
      command_info(CMD_ADD_HOST_COMMENT, &_redirector<&cmd_add_comment>)},
     {"DEL_HOST_COMMENT",
      command_info(CMD_DEL_HOST_COMMENT, &_redirector<&cmd_delete_comment>)},
     {"DEL_ALL_HOST_COMMENTS",
      command_info(CMD_DEL_ALL_HOST_COMMENTS,
                   &_redirector<&cmd_delete_all_comments>)},
     {"DELAY_HOST_NOTIFICATION",
      command_info(CMD_DELAY_HOST_NOTIFICATION,
                   &_redirector<&cmd_delay_notification>)},
     {"ENABLE_HOST_NOTIFICATIONS",
      command_info(CMD_ENABLE_HOST_NOTIFICATIONS,
                   &_redirector_host<&enable_host_notifications>)},
     {"DISABLE_HOST_NOTIFICATIONS",
      command_info(CMD_DISABLE_HOST_NOTIFICATIONS,
                   &_redirector_host<&disable_host_notifications>)},
     {"ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST",
      command_info(
          CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST,
          &_redirector_host<&_wrapper_enable_all_notifications_beyond_host>)},
     {"DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST",
      command_info(
          CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST,
          &_redirector_host<&_wrapper_disable_all_notifications_beyond_host>)},
     {"ENABLE_HOST_AND_CHILD_NOTIFICATIONS",
      command_info(
          CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS,
          &_redirector_host<&wrapper_enable_host_and_child_notifications>)},
     {"DISABLE_HOST_AND_CHILD_NOTIFICATIONS",
      command_info(
          CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS,
          &_redirector_host<&wrapper_disable_host_and_child_notifications>)},
     {"ENABLE_HOST_SVC_NOTIFICATIONS",
      command_info(CMD_ENABLE_HOST_SVC_NOTIFICATIONS,
                   &_redirector_host<&_wrapper_enable_host_svc_notifications>)},
     {"DISABLE_HOST_SVC_NOTIFICATIONS",
      command_info(
          CMD_DISABLE_HOST_SVC_NOTIFICATIONS,
          &_redirector_host<&_wrapper_disable_host_svc_notifications>)},
     {"ENABLE_HOST_SVC_CHECKS",
      command_info(CMD_ENABLE_HOST_SVC_CHECKS,
                   &_redirector_host<&_wrapper_enable_host_svc_checks>)},
     {"DISABLE_HOST_SVC_CHECKS",
      command_info(CMD_DISABLE_HOST_SVC_CHECKS,
                   &_redirector_host<&_wrapper_disable_host_svc_checks>)},
     {"ENABLE_PASSIVE_HOST_CHECKS",
      command_info(CMD_ENABLE_PASSIVE_HOST_CHECKS,
                   &_redirector_host<&enable_passive_host_checks>)},
     {"DISABLE_PASSIVE_HOST_CHECKS",
      command_info(CMD_DISABLE_PASSIVE_HOST_CHECKS,
                   &_redirector_host<&disable_passive_host_checks>)},
     {"SCHEDULE_HOST_SVC_CHECKS",
      command_info(CMD_SCHEDULE_HOST_SVC_CHECKS,
                   &_redirector<&cmd_schedule_check>)},
     {"SCHEDULE_FORCED_HOST_SVC_CHECKS",
      command_info(CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS,
                   &_redirector<&cmd_schedule_check>)},
     {"ACKNOWLEDGE_HOST_PROBLEM",
      command_info(CMD_ACKNOWLEDGE_HOST_PROBLEM,
                   &_redirector<&cmd_acknowledge_problem>)},
     {"REMOVE_HOST_ACKNOWLEDGEMENT",
      command_info(CMD_REMOVE_HOST_ACKNOWLEDGEMENT,
                   &_redirector<&cmd_remove_acknowledgement>)},
     {"ENABLE_HOST_EVENT_HANDLER",
      command_info(CMD_ENABLE_HOST_EVENT_HANDLER,
                   &_redirector_host<&enable_host_event_handler>)},
     {"DISABLE_HOST_EVENT_HANDLER",
      command_info(CMD_DISABLE_HOST_EVENT_HANDLER,
                   &_redirector_host<&disable_host_event_handler>)},
     {"ENABLE_HOST_CHECK",
      command_info(CMD_ENABLE_HOST_CHECK,
                   &_redirector_host<&enable_host_checks>)},
     {"DISABLE_HOST_CHECK",
      command_info(CMD_DISABLE_HOST_CHECK,
                   &_redirector_host<&disable_host_checks>)},
     {"SCHEDULE_HOST_CHECK",
      command_info(CMD_SCHEDULE_HOST_CHECK, &_redirector<&cmd_schedule_check>)},
     {"SCHEDULE_FORCED_HOST_CHECK",
      command_info(CMD_SCHEDULE_FORCED_HOST_CHECK,
                   &_redirector<&cmd_schedule_check>)},
     {"SCHEDULE_HOST_DOWNTIME",
      command_info(CMD_SCHEDULE_HOST_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     {"SCHEDULE_HOST_SVC_DOWNTIME",
      command_info(CMD_SCHEDULE_HOST_SVC_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     {"DEL_HOST_DOWNTIME",
      command_info(CMD_DEL_HOST_DOWNTIME, &_redirector<&cmd_delete_downtime>)},
     {"DEL_HOST_DOWNTIME_FULL",
      command_info(CMD_DEL_HOST_DOWNTIME_FULL,
                   &_redirector<&cmd_delete_downtime_full>)},
     {"DEL_DOWNTIME_BY_HOST_NAME",
      command_info(CMD_DEL_DOWNTIME_BY_HOST_NAME,
                   &_redirector<&cmd_delete_downtime_by_host_name>)},
     {"DEL_DOWNTIME_BY_HOSTGROUP_NAME",
      command_info(CMD_DEL_DOWNTIME_BY_HOSTGROUP_NAME,
                   &_redirector<&cmd_delete_downtime_by_hostgroup_name>)},
     {"DEL_DOWNTIME_BY_START_TIME_COMMENT",
      command_info(CMD_DEL_DOWNTIME_BY_START_TIME_COMMENT,
                   &_redirector<&cmd_delete_downtime_by_start_time_comment>)},
     {"ENABLE_HOST_FLAP_DETECTION",
      command_info(CMD_ENABLE_HOST_FLAP_DETECTION,
                   &_redirector_host<&enable_host_flap_detection>)},
     {"DISABLE_HOST_FLAP_DETECTION",
      command_info(CMD_DISABLE_HOST_FLAP_DETECTION,
                   &_redirector_host<&disable_host_flap_detection>)},
     {"START_OBSESSING_OVER_HOST",
      command_info(CMD_START_OBSESSING_OVER_HOST,
                   &_redirector_host<&start_obsessing_over_host>)},
     {"STOP_OBSESSING_OVER_HOST",
      command_info(CMD_STOP_OBSESSING_OVER_HOST,
                   &_redirector_host<&stop_obsessing_over_host>)},
     {"CHANGE_HOST_EVENT_HANDLER",
      command_info(CMD_CHANGE_HOST_EVENT_HANDLER,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_HOST_CHECK_COMMAND",
      command_info(CMD_CHANGE_HOST_CHECK_COMMAND,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_NORMAL_HOST_CHECK_INTERVAL",
      command_info(CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL,
                   &_redirector<&cmd_change_object_int_var>)},
     {"CHANGE_RETRY_HOST_CHECK_INTERVAL",
      command_info(CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL,
                   &_redirector<&cmd_change_object_int_var>)},
     {"CHANGE_MAX_HOST_CHECK_ATTEMPTS",
      command_info(CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS,
                   &_redirector<&cmd_change_object_int_var>)},
     {"SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME",
      command_info(CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     {"SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME",
      command_info(CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     {"SET_HOST_NOTIFICATION_NUMBER",
      command_info(CMD_SET_HOST_NOTIFICATION_NUMBER,
                   &_redirector_host<&_wrapper_set_host_notification_number>)},
     {"CHANGE_HOST_CHECK_TIMEPERIOD",
      command_info(CMD_CHANGE_HOST_CHECK_TIMEPERIOD,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_CUSTOM_HOST_VAR",
      command_info(CMD_CHANGE_CUSTOM_HOST_VAR,
                   &_redirector<&cmd_change_object_custom_var>)},
     {"SEND_CUSTOM_HOST_NOTIFICATION",
      command_info(CMD_SEND_CUSTOM_HOST_NOTIFICATION,
                   &_redirector_host<&_wrapper_send_custom_host_notification>)},
     {"CHANGE_HOST_NOTIFICATION_TIMEPERIOD",
      command_info(CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_HOST_MODATTR",
      command_info(CMD_CHANGE_HOST_MODATTR,
                   &_redirector<&cmd_change_object_int_var>)},
     // hostgroup-related commands.
     {"ENABLE_HOSTGROUP_HOST_NOTIFICATIONS",
      command_info(CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS,
                   &_redirector_hostgroup<&enable_host_notifications>)},
     {"DISABLE_HOSTGROUP_HOST_NOTIFICATIONS",
      command_info(CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS,
                   &_redirector_hostgroup<&disable_host_notifications>)},
     {"ENABLE_HOSTGROUP_SVC_NOTIFICATIONS",
      command_info(
          CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS,
          &_redirector_hostgroup<&_wrapper_enable_service_notifications>)},
     {"DISABLE_HOSTGROUP_SVC_NOTIFICATIONS",
      command_info(
          CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS,
          &_redirector_hostgroup<&_wrapper_disable_service_notifications>)},
     {"ENABLE_HOSTGROUP_HOST_CHECKS",
      command_info(CMD_ENABLE_HOSTGROUP_HOST_CHECKS,
                   &_redirector_hostgroup<&enable_host_checks>)},
     {"DISABLE_HOSTGROUP_HOST_CHECKS",
      command_info(CMD_DISABLE_HOSTGROUP_HOST_CHECKS,
                   &_redirector_hostgroup<&disable_host_checks>)},
     {"ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS",
      command_info(CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS,
                   &_redirector_hostgroup<&enable_passive_host_checks>)},
     {"DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS",
      command_info(CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS,
                   &_redirector_hostgroup<&disable_passive_host_checks>)},
     {"ENABLE_HOSTGROUP_SVC_CHECKS",
      command_info(CMD_ENABLE_HOSTGROUP_SVC_CHECKS,
                   &_redirector_hostgroup<&_wrapper_enable_service_checks>)},
     {"DISABLE_HOSTGROUP_SVC_CHECKS",
      command_info(CMD_DISABLE_HOSTGROUP_SVC_CHECKS,
                   &_redirector_hostgroup<&_wrapper_disable_service_checks>)},
     {"ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS",
      command_info(
          CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS,
          &_redirector_hostgroup<&_wrapper_enable_passive_service_checks>)},
     {"DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS",
      command_info(
          CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS,
          &_redirector_hostgroup<&_wrapper_disable_passive_service_checks>)},
     {"SCHEDULE_HOSTGROUP_HOST_DOWNTIME",
      command_info(CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     {"SCHEDULE_HOSTGROUP_SVC_DOWNTIME",
      command_info(CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     // service-related commands.
     {"ADD_SVC_COMMENT",
      command_info(CMD_ADD_SVC_COMMENT, &_redirector<&cmd_add_comment>)},
     {"DEL_SVC_COMMENT",
      command_info(CMD_DEL_SVC_COMMENT, &_redirector<&cmd_delete_comment>)},
     {"DEL_ALL_SVC_COMMENTS",
      command_info(CMD_DEL_ALL_SVC_COMMENTS,
                   &_redirector<&cmd_delete_all_comments>)},
     {"SCHEDULE_SVC_CHECK",
      command_info(CMD_SCHEDULE_SVC_CHECK, &_redirector<&cmd_schedule_check>)},
     {"SCHEDULE_FORCED_SVC_CHECK",
      command_info(CMD_SCHEDULE_FORCED_SVC_CHECK,
                   &_redirector<&cmd_schedule_check>)},
     {"ENABLE_SVC_CHECK",
      command_info(CMD_ENABLE_SVC_CHECK,
                   &_redirector_service<&enable_service_checks>)},
     {"DISABLE_SVC_CHECK",
      command_info(CMD_DISABLE_SVC_CHECK,
                   &_redirector_service<&disable_service_checks>)},
     {"ENABLE_PASSIVE_SVC_CHECKS",
      command_info(CMD_ENABLE_PASSIVE_SVC_CHECKS,
                   &_redirector_service<&enable_passive_service_checks>)},
     {"DISABLE_PASSIVE_SVC_CHECKS",
      command_info(CMD_DISABLE_PASSIVE_SVC_CHECKS,
                   &_redirector_service<&disable_passive_service_checks>)},
     {"DELAY_SVC_NOTIFICATION",
      command_info(CMD_DELAY_SVC_NOTIFICATION,
                   &_redirector<&cmd_delay_notification>)},
     {"ENABLE_SVC_NOTIFICATIONS",
      command_info(CMD_ENABLE_SVC_NOTIFICATIONS,
                   &_redirector_service<&enable_service_notifications>)},
     {"DISABLE_SVC_NOTIFICATIONS",
      command_info(CMD_DISABLE_SVC_NOTIFICATIONS,
                   &_redirector_service<&disable_service_notifications>)},
     {"PROCESS_SERVICE_CHECK_RESULT",
      command_info(CMD_PROCESS_SERVICE_CHECK_RESULT,
                   &_redirector<&cmd_process_service_check_result>,
                   true)},
     {"PROCESS_HOST_CHECK_RESULT",
      command_info(CMD_PROCESS_HOST_CHECK_RESULT,
                   &_redirector<&cmd_process_host_check_result>,
                   true)},
     {"ENABLE_SVC_EVENT_HANDLER",
      command_info(CMD_ENABLE_SVC_EVENT_HANDLER,
                   &_redirector_service<&enable_service_event_handler>)},
     {"DISABLE_SVC_EVENT_HANDLER",
      command_info(CMD_DISABLE_SVC_EVENT_HANDLER,
                   &_redirector_service<&disable_service_event_handler>)},
     {"ENABLE_SVC_FLAP_DETECTION",
      command_info(CMD_ENABLE_SVC_FLAP_DETECTION,
                   &_redirector_service<&enable_service_flap_detection>)},
     {"DISABLE_SVC_FLAP_DETECTION",
      command_info(CMD_DISABLE_SVC_FLAP_DETECTION,
                   &_redirector_service<&disable_service_flap_detection>)},
     {"SCHEDULE_SVC_DOWNTIME",
      command_info(CMD_SCHEDULE_SVC_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     {"DEL_SVC_DOWNTIME",
      command_info(CMD_DEL_SVC_DOWNTIME, &_redirector<&cmd_delete_downtime>)},
     {"DEL_SVC_DOWNTIME_FULL",
      command_info(CMD_DEL_SVC_DOWNTIME_FULL,
                   &_redirector<&cmd_delete_downtime_full>)},
     {"ACKNOWLEDGE_SVC_PROBLEM",
      command_info(CMD_ACKNOWLEDGE_SVC_PROBLEM,
                   &_redirector<&cmd_acknowledge_problem>)},
     {"REMOVE_SVC_ACKNOWLEDGEMENT",
      command_info(CMD_REMOVE_SVC_ACKNOWLEDGEMENT,
                   &_redirector<&cmd_remove_acknowledgement>)},
     {"START_OBSESSING_OVER_SVC",
      command_info(CMD_START_OBSESSING_OVER_SVC,
                   &_redirector_service<&start_obsessing_over_service>)},
     {"STOP_OBSESSING_OVER_SVC",
      command_info(CMD_STOP_OBSESSING_OVER_SVC,
                   &_redirector_service<&stop_obsessing_over_service>)},
     {"CHANGE_SVC_EVENT_HANDLER",
      command_info(CMD_CHANGE_SVC_EVENT_HANDLER,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_SVC_CHECK_COMMAND",
      command_info(CMD_CHANGE_SVC_CHECK_COMMAND,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_NORMAL_SVC_CHECK_INTERVAL",
      command_info(CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL,
                   &_redirector<&cmd_change_object_int_var>)},
     {"CHANGE_RETRY_SVC_CHECK_INTERVAL",
      command_info(CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL,
                   &_redirector<&cmd_change_object_int_var>)},
     {"CHANGE_MAX_SVC_CHECK_ATTEMPTS",
      command_info(CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS,
                   &_redirector<&cmd_change_object_int_var>)},
     {"SET_SVC_NOTIFICATION_NUMBER",
      command_info(
          CMD_SET_SVC_NOTIFICATION_NUMBER,
          &_redirector_service<&_wrapper_set_service_notification_number>)},
     {"CHANGE_SVC_CHECK_TIMEPERIOD",
      command_info(CMD_CHANGE_SVC_CHECK_TIMEPERIOD,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_CUSTOM_SVC_VAR",
      command_info(CMD_CHANGE_CUSTOM_SVC_VAR,
                   &_redirector<&cmd_change_object_custom_var>)},
     {"CHANGE_CUSTOM_CONTACT_VAR",
      command_info(CMD_CHANGE_CUSTOM_CONTACT_VAR,
                   &_redirector<&cmd_change_object_custom_var>)},
     {"SEND_CUSTOM_SVC_NOTIFICATION",
      command_info(
          CMD_SEND_CUSTOM_SVC_NOTIFICATION,
          &_redirector_service<&_wrapper_send_custom_service_notification>)},
     {"CHANGE_SVC_NOTIFICATION_TIMEPERIOD",
      command_info(CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_SVC_MODATTR",
      command_info(CMD_CHANGE_SVC_MODATTR,
                   &_redirector<&cmd_change_object_int_var>)},
     // servicegroup-related commands.
     {"ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS",
      command_info(CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS,
                   &_redirector_servicegroup<&enable_host_notifications>)},
     {"DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS",
      command_info(CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS,
                   &_redirector_servicegroup<&disable_host_notifications>)},
     {"ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS",
      command_info(CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS,
                   &_redirector_servicegroup<&enable_service_notifications>)},
     {"DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS",
      command_info(CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS,
                   &_redirector_servicegroup<&disable_service_notifications>)},
     {"ENABLE_SERVICEGROUP_HOST_CHECKS",
      command_info(CMD_ENABLE_SERVICEGROUP_HOST_CHECKS,
                   &_redirector_servicegroup<&enable_host_checks>)},
     {"DISABLE_SERVICEGROUP_HOST_CHECKS",
      command_info(CMD_DISABLE_SERVICEGROUP_HOST_CHECKS,
                   &_redirector_servicegroup<&disable_host_checks>)},
     {"ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS",
      command_info(CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS,
                   &_redirector_servicegroup<&enable_passive_host_checks>)},
     {"DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS",
      command_info(CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS,
                   &_redirector_servicegroup<&disable_passive_host_checks>)},
     {"ENABLE_SERVICEGROUP_SVC_CHECKS",
      command_info(CMD_ENABLE_SERVICEGROUP_SVC_CHECKS,
                   &_redirector_servicegroup<&enable_service_checks>)},
     {"DISABLE_SERVICEGROUP_SVC_CHECKS",
      command_info(CMD_DISABLE_SERVICEGROUP_SVC_CHECKS,
                   &_redirector_servicegroup<&disable_service_checks>)},
     {"ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS",
      command_info(CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS,
                   &_redirector_servicegroup<&enable_passive_service_checks>)},
     {"DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS",
      command_info(CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS,
                   &_redirector_servicegroup<&disable_passive_service_checks>)},
     {"SCHEDULE_SERVICEGROUP_HOST_DOWNTIME",
      command_info(CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     {"SCHEDULE_SERVICEGROUP_SVC_DOWNTIME",
      command_info(CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME,
                   &_redirector<&cmd_schedule_downtime>)},
     // contact-related commands.
     {"ENABLE_CONTACT_HOST_NOTIFICATIONS",
      command_info(CMD_ENABLE_CONTACT_HOST_NOTIFICATIONS,
                   &_redirector_contact<&enable_contact_host_notifications>)},
     {"DISABLE_CONTACT_HOST_NOTIFICATIONS",
      command_info(CMD_DISABLE_CONTACT_HOST_NOTIFICATIONS,
                   &_redirector_contact<&disable_contact_host_notifications>)},
     {"ENABLE_CONTACT_SVC_NOTIFICATIONS",
      command_info(
          CMD_ENABLE_CONTACT_SVC_NOTIFICATIONS,
          &_redirector_contact<&enable_contact_service_notifications>)},
     {"DISABLE_CONTACT_SVC_NOTIFICATIONS",
      command_info(
          CMD_DISABLE_CONTACT_SVC_NOTIFICATIONS,
          &_redirector_contact<&disable_contact_service_notifications>)},
     {"CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD",
      command_info(CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD",
      command_info(CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD,
                   &_redirector<&cmd_change_object_char_var>)},
     {"CHANGE_CONTACT_MODATTR",
      command_info(CMD_CHANGE_CONTACT_MODATTR,
                   &_redirector<&cmd_change_object_int_var>)},
     {"CHANGE_CONTACT_MODHATTR",
      command_info(CMD_CHANGE_CONTACT_MODHATTR,
                   &_redirector<&cmd_change_object_int_var>)},
     {"CHANGE_CONTACT_MODSATTR",
      command_info(CMD_CHANGE_CONTACT_MODSATTR,
                   &_redirector<&cmd_change_object_int_var>)},
     // contactgroup-related commands.
     {"ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS",
      command_info(
          CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS,
          &_redirector_contactgroup<&enable_contact_host_notifications>)},
     {"DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS",
      command_info(
          CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS,
          &_redirector_contactgroup<&disable_contact_host_notifications>)},
     {"ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS",
      command_info(
          CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS,
          &_redirector_contactgroup<&enable_contact_service_notifications>)},
     {"DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS",
      command_info(
          CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS,
          &_redirector_contactgroup<&disable_contact_service_notifications>)},
     {"NEW_THRESHOLDS_FILE",
      command_info(CMD_NEW_THRESHOLDS_FILE,
                   &_redirector_file<&new_thresholds_file>)},
     {"PROCESS_FILE",
      command_info(CMD_PROCESS_FILE,
                   &_redirector<&cmd_process_external_commands_from_file>)},
     {"CHANGE_ANOMALYDETECTION_SENSITIVITY",
      command_info(CMD_CHANGE_ANOMALYDETECTION_SENSITIVITY,
                   &_redirector_anomalydetection<
                       &change_anomaly_detection_sensitivity>)}});

/********************************************************************
 * redirectors
 ********************************************************************/
template <void (*fptr)(host*)>
void processing::_redirector_host(int id, time_t entry_time, char* args) {
  (void)id;
  (void)entry_time;

  char* name(my_strtok(args, ";"));

  host* hst{nullptr};
  host_map::const_iterator it(host::hosts.find(name));
  if (it != host::hosts.end())
    hst = it->second.get();

  if (!hst) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown host: {}", name);
    return;
  }
  (*fptr)(hst);
}

template <void (*fptr)(host*, char*)>
void processing::_redirector_host(int id, time_t entry_time, char* args) {
  (void)id;
  (void)entry_time;

  std::string name(my_strtok(args, ";"));

  host* hst{nullptr};
  host_map::const_iterator it(host::hosts.find(name));
  if (it != host::hosts.end())
    hst = it->second.get();

  if (!hst) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown host: {}", name);
    return;
  }
  (*fptr)(hst, args + name.length() + 1);
}

template <void (*fptr)(host*)>
void processing::_redirector_hostgroup(int id, time_t entry_time, char* args) {
  (void)id;
  (void)entry_time;

  char* group_name(my_strtok(args, ";"));

  hostgroup* group(nullptr);
  hostgroup_map::const_iterator it{hostgroup::hostgroups.find(group_name)};
  if (it != hostgroup::hostgroups.end())
    group = it->second.get();
  if (!group) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown group: {}",
                        group_name);
    return;
  }

  for (host_map_unsafe::iterator it(group->members.begin()),
       end(group->members.begin());
       it != end; ++it)
    if (it->second)
      (*fptr)(it->second);
}

template <void (*fptr)(service*)>
void processing::_redirector_service(int id, time_t entry_time, char* args) {
  (void)id;
  (void)entry_time;

  char* name(my_strtok(args, ";"));
  char* description(my_strtok(NULL, ";"));

  service_map::const_iterator found(
      service::services.find({name, description}));

  if (found == service::services.end() || !found->second) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown service: {}@{}",
                        description, name);
    return;
  }
  (*fptr)(found->second.get());
}

template <void (*fptr)(service*, char*)>
void processing::_redirector_service(int id, time_t entry_time, char* args) {
  (void)id;
  (void)entry_time;

  std::string name{my_strtok(args, ";")};
  std::string description{my_strtok(NULL, ";")};
  service_map::const_iterator found{
      service::services.find({name, description})};

  if (found == service::services.end() || !found->second) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown service: {}@{}",
                        description, name);
    return;
  }
  (*fptr)(found->second.get(), args + name.length() + description.length() + 2);
}

template <void (*fptr)(service*)>
void processing::_redirector_servicegroup(int id,
                                          time_t entry_time,
                                          char* args) {
  (void)id;
  (void)entry_time;

  char* group_name(my_strtok(args, ";"));
  servicegroup_map::const_iterator sg_it{
      servicegroup::servicegroups.find(group_name)};
  if (sg_it == servicegroup::servicegroups.end() || !sg_it->second) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown service group: {}",
                        group_name);
    return;
  }

  for (service_map_unsafe::iterator it2(sg_it->second->members.begin()),
       end2(sg_it->second->members.end());
       it2 != end2; ++it2)
    if (it2->second)
      (*fptr)(it2->second);
}

template <void (*fptr)(host*)>
void processing::_redirector_servicegroup(int id,
                                          time_t entry_time,
                                          char* args) {
  (void)id;
  (void)entry_time;

  char* group_name(my_strtok(args, ";"));
  servicegroup_map::const_iterator sg_it{
      servicegroup::servicegroups.find(group_name)};
  if (sg_it == servicegroup::servicegroups.end() || !sg_it->second) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown service group: {}",
                        group_name);
    return;
  }

  host* last_host{nullptr};
  for (service_map_unsafe::iterator it2(sg_it->second->members.begin()),
       end2(sg_it->second->members.end());
       it2 != end2; ++it2) {
    host* hst{nullptr};
    host_map::const_iterator found(host::hosts.find(it2->first.first));
    if (found != host::hosts.end())
      hst = found->second.get();

    if (!hst || hst == last_host)
      continue;
    (*fptr)(hst);
    last_host = hst;
  }
}

template <void (*fptr)(contact*)>
void processing::_redirector_contact(int id, time_t entry_time, char* args) {
  (void)id;
  (void)entry_time;

  char* name(my_strtok(args, ";"));
  contact_map::const_iterator ct_it{contact::contacts.find(name)};
  if (ct_it == contact::contacts.end()) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown contact: {}", name);
    return;
  }
  (*fptr)(ct_it->second.get());
}

template <void (*fptr)(char*)>
void processing::_redirector_file(int id __attribute__((unused)),
                                  time_t entry_time __attribute__((unused)),
                                  char* args) {
  char* filename(my_strtok(args, ";"));
  (*fptr)(filename);
}

template <void (*fptr)(contact*)>
void processing::_redirector_contactgroup(int id,
                                          time_t entry_time,
                                          char* args) {
  (void)id;
  (void)entry_time;

  char* group_name(my_strtok(args, ";"));
  contactgroup_map::iterator it_cg{
      contactgroup::contactgroups.find(group_name)};
  if (it_cg == contactgroup::contactgroups.end() || !it_cg->second) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "unknown contact group: {}",
                        group_name);
    return;
  }

  for (contact_map::const_iterator it = it_cg->second->get_members().begin(),
                                   end = it_cg->second->get_members().end();
       it != end; ++it)
    if (it->second)
      (*fptr)(it->second.get());
}

template <void (*fptr)(anomalydetection*, char*)>
void processing::_redirector_anomalydetection(int id,
                                              time_t entry_time,
                                              char* args) {
  (void)id;
  (void)entry_time;

  std::string name{my_strtok(args, ";")};
  std::string description{my_strtok(NULL, ";")};
  service_map::const_iterator found{
      service::services.find({name, description})};

  if (found == service::services.end() || !found->second) {
    SPDLOG_LOGGER_ERROR(external_command_logger,
                        "unknown anomaly detection {}@{}", description, name);
    return;
  }

  if (found->second->get_service_type() != service_type::ANOMALY_DETECTION) {
    SPDLOG_LOGGER_ERROR(external_command_logger,
                        "{}@{} is not an anomalydetection", description, name);
    return;
  }

  std::shared_ptr<anomalydetection> ano =
      std::static_pointer_cast<anomalydetection>(found->second);
  (*fptr)(ano.get(), args + name.length() + description.length() + 2);
}

bool processing::execute(const std::string& cmdstr) {
  engine_logger(dbg_functions, basic) << "processing external command";
  functions_logger->trace("processing external command {}", cmdstr);

  char const* cmd{cmdstr.c_str()};
  size_t len{cmdstr.size()};

  // Left trim command
  while (*cmd && isspace(*cmd))
    ++cmd;
  if (*cmd != '[')
    return false;

  // Right trim just by recomputing the optimal length value.
  char const* end{cmd + len - 1};
  while (end != cmd && isspace(*end))
    --end;

  cmd++;
  char* tmp;
  time_t entry_time{static_cast<time_t>(strtoul(cmd, &tmp, 10))};

  while (*tmp && isspace(*tmp))
    ++tmp;

  if (*tmp != ']' || tmp[1] != ' ')
    return false;

  cmd = tmp + 2;
  char const* a;
  for (a = cmd; *a && *a != ';'; ++a)
    ;

  std::string command_name(cmd, a - cmd);
  std::string args;
  if (*a == ';') {
    a++;
    args = std::string(a, end - a + 1);
  }

  int command_id(CMD_CUSTOM_COMMAND);

  std::unordered_map<std::string, command_info>::const_iterator it =
      _lst_command.find(command_name);
  if (it != _lst_command.end())
    command_id = it->second.id;
  else if (command_name[0] != '_') {
    engine_logger(log_external_command | log_runtime_warning, basic)
        << "Warning: Unrecognized external command -> " << command_name;
    external_command_logger->warn(
        "Warning: Unrecognized external command -> {}", command_name);
    return false;
  }

  // Update statistics for external commands.
  update_check_stats(EXTERNAL_COMMAND_STATS, std::time(nullptr));

  // Log the external command.
  if (command_id == CMD_PROCESS_SERVICE_CHECK_RESULT ||
      command_id == CMD_PROCESS_HOST_CHECK_RESULT) {
#ifdef LEGACY_CONF
    bool log_passive_check = config->log_passive_checks();
#else
    bool log_passive_check = pb_config.log_passive_checks();
#endif
    // Passive checks are logged in checks.c.
    if (log_passive_checks) {
      engine_logger(log_passive_check, basic)
          << "EXTERNAL COMMAND: " << command_name << ';' << args;
      checks_logger->info("EXTERNAL COMMAND: {};{}", command_name, args);
    }
#ifdef LEGACY_CONF
  } else if (config->log_external_commands()) {
#else
  } else if (pb_config.log_external_commands()) {
#endif
    engine_logger(log_external_command, basic)
        << "EXTERNAL COMMAND: " << command_name << ';' << args;
    SPDLOG_LOGGER_INFO(external_command_logger, "EXTERNAL COMMAND: {};{}",
                       command_name, args);
  }

  engine_logger(dbg_external_command, more)
      << "External command id: " << command_id
      << "\nCommand entry time: " << entry_time
      << "\nCommand arguments: " << args;
  SPDLOG_LOGGER_DEBUG(external_command_logger, "External command id: {}",
                      command_id);
  SPDLOG_LOGGER_DEBUG(external_command_logger, "Command entry time: {}",
                      entry_time);
  SPDLOG_LOGGER_DEBUG(external_command_logger, "Command arguments: {}", args);

  // Send data to event broker.
  broker_external_command(NEBTYPE_EXTERNALCOMMAND_START, command_id,
                          const_cast<char*>(args.c_str()), nullptr);

  if (it != _lst_command.end())
    (*it->second.func)(command_id, entry_time, const_cast<char*>(args.c_str()));

  // Send data to event broker.
  broker_external_command(NEBTYPE_EXTERNALCOMMAND_END, command_id,
                          const_cast<char*>(args.c_str()), nullptr);
  return true;
}

/**
 *  Check if a command is thread-safe.
 *
 *  @param[in] cmd  Command to check.
 *
 *  @return True if command is thread-safe.
 */
bool processing::is_thread_safe(char const* cmd) {
  char const* ptr = cmd + strspn(cmd, "[]0123456789 ");
  std::string short_cmd(ptr, strcspn(ptr, ";"));
  std::unordered_map<std::string, command_info>::const_iterator it =
      _lst_command.find(short_cmd);
  return it != _lst_command.end() && it->second.thread_safe;
}

void processing::_wrapper_read_state_information() {
  try {
    retention::state state;
    retention::parser p;
#ifdef LEGACY_CONF
    const std::string& retention_file = config->state_retention_file();
#else
    const std::string& retention_file = pb_config.state_retention_file();
#endif
    p.parse(retention_file, state);
    retention::applier::state app_state;
#ifdef LEGACY_CONF
    app_state.apply(*config, state);
#else
    app_state.apply(pb_config, state);
#endif
  } catch (std::exception const& e) {
    engine_logger(log_runtime_error, basic)
        << "Error: could not load retention file: " << e.what();
    SPDLOG_LOGGER_ERROR(runtime_logger,
                        "Error: could not load retention file: {}", e.what());
  }
}

void processing::_wrapper_save_state_information() {
#ifdef LEGACY_CONF
  retention::dump::save(config->state_retention_file());
#else
  retention::dump::save(pb_config.state_retention_file());
#endif
}

void processing::wrapper_enable_host_and_child_notifications(host* hst) {
  enable_and_propagate_notifications(hst, 0, true, true, false);
}

void processing::wrapper_disable_host_and_child_notifications(host* hst) {
  disable_and_propagate_notifications(hst, 0, true, true, false);
}

void processing::_wrapper_enable_all_notifications_beyond_host(host* hst) {
  enable_and_propagate_notifications(hst, 0, false, true, true);
}

void processing::_wrapper_disable_all_notifications_beyond_host(host* hst) {
  disable_and_propagate_notifications(hst, 0, false, true, true);
}

void processing::_wrapper_enable_host_svc_notifications(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      enable_service_notifications(it->second);
}

void processing::_wrapper_disable_host_svc_notifications(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      disable_service_notifications(it->second);
}

void processing::_wrapper_disable_host_svc_checks(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      disable_service_checks(it->second);
}

void processing::_wrapper_enable_host_svc_checks(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      enable_service_checks(it->second);
}

void processing::_wrapper_set_host_notification_number(host* hst, char* args) {
  if (hst && args) {
    int notification_number;
    if (!absl::SimpleAtoi(args, &notification_number)) {
      SPDLOG_LOGGER_ERROR(
          runtime_logger,
          "Error: could not set host notification number: '{}' must be a "
          "positive integer",
          args);
      return;
    }
    hst->set_notification_number(notification_number);
  }
}

void processing::_wrapper_send_custom_host_notification(host* hst, char* args) {
  char* buf[3] = {NULL, NULL, NULL};
  int option;
  if ((buf[0] = my_strtok(args, ";")) && (buf[1] = my_strtok(NULL, ";")) &&
      (buf[2] = my_strtok(NULL, ";"))) {
    if (!absl::SimpleAtoi(buf[0], &option)) {
      SPDLOG_LOGGER_ERROR(
          runtime_logger,
          "Error: could not send custom host notification: '{}' must be an "
          "integer between 0 and 7",
          buf[0]);
    } else if (option >= 0 && option <= 7) {
      hst->notify(notifier::reason_custom, buf[1], buf[2],
                  static_cast<notifier::notification_option>(option));
    } else {
      SPDLOG_LOGGER_ERROR(
          runtime_logger,
          "Error: could not send custom host notification: '{}' must be an "
          "integer between 0 and 7",
          buf[0]);
    }
  }
}

void processing::_wrapper_enable_service_notifications(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      enable_service_notifications(it->second);
}

void processing::_wrapper_disable_service_notifications(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      disable_service_notifications(it->second);
}

void processing::_wrapper_enable_service_checks(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      enable_service_checks(it->second);
}

void processing::_wrapper_disable_service_checks(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      disable_service_checks(it->second);
}

void processing::_wrapper_enable_passive_service_checks(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      enable_passive_service_checks(it->second);
}

void processing::_wrapper_disable_passive_service_checks(host* hst) {
  for (service_map_unsafe::iterator it(hst->services.begin()),
       end(hst->services.end());
       it != end; ++it)
    if (it->second)
      disable_passive_service_checks(it->second);
}

void processing::_wrapper_set_service_notification_number(service* svc,
                                                          char* args) {
  char* str(my_strtok(args, ";"));
  int notification_number;
  if (svc && str) {
    if (!absl::SimpleAtoi(str, &notification_number)) {
      SPDLOG_LOGGER_ERROR(
          runtime_logger,
          "Error: could not set service notification number: '{}' must be a "
          "positive integer",
          str);
      return;
    }
    svc->set_notification_number(notification_number);
  }
}

void processing::_wrapper_send_custom_service_notification(service* svc,
                                                           char* args) {
  char* buf[3] = {NULL, NULL, NULL};
  int notification_number;
  if ((buf[0] = my_strtok(args, ";")) && (buf[1] = my_strtok(NULL, ";")) &&
      (buf[2] = my_strtok(NULL, ";"))) {
    if (!absl::SimpleAtoi(buf[0], &notification_number)) {
      SPDLOG_LOGGER_ERROR(
          runtime_logger,
          "Error: could not send custom service notification: '{}' must be an "
          "integer between 0 and 7",
          buf[0]);
    } else if (notification_number >= 0 && notification_number <= 7) {
      svc->notify(
          notifier::reason_custom, buf[1], buf[2],
          static_cast<notifier::notification_option>(notification_number));
    } else {
      SPDLOG_LOGGER_ERROR(
          runtime_logger,
          "Error: could not send custom service notification: '{}' must be an "
          "integer between 0 and 7",
          buf[0]);
    }
  }
}

void processing::change_anomaly_detection_sensitivity(anomalydetection* ano,
                                                      char* args) {
  double new_sensitivity;
  if (absl::SimpleAtod(args, &new_sensitivity)) {
    ano->set_sensitivity(new_sensitivity);
  } else {
    SPDLOG_LOGGER_ERROR(
        external_command_logger,
        "change_anomaly_detection_sensitivity unable to parse args: {}", args);
  }
}
