/**
 * Copyright 2026 Centreon
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

#include "common/external_commands/command_table.hh"

#include <absl/container/flat_hash_map.h>

namespace com::centreon::common::external_commands {

namespace {

/* One row per command name known to Engine's commands::processing table. A
 * unit test on the Engine side checks the two tables list the same names. */
const absl::flat_hash_map<std::string_view, command_info> table = {
    {"ENTER_STANDBY_MODE", {target::process, ""}},
    {"DISABLE_NOTIFICATIONS", {target::global, "SetPollerNotifications"}},
    {"ENTER_ACTIVE_MODE", {target::process, ""}},
    {"ENABLE_NOTIFICATIONS", {target::global, "SetPollerNotifications"}},
    {"SHUTDOWN_PROGRAM", {target::process, ""}},
    {"SHUTDOWN_PROCESS", {target::process, ""}},
    {"RESTART_PROGRAM", {target::process, ""}},
    {"RESTART_PROCESS", {target::process, ""}},
    {"SAVE_STATE_INFORMATION", {target::process, ""}},
    {"READ_STATE_INFORMATION", {target::process, ""}},
    {"ENABLE_EVENT_HANDLERS", {target::global, ""}},
    {"DISABLE_EVENT_HANDLERS", {target::global, ""}},
    {"ENABLE_FAILURE_PREDICTION", {target::global, ""}},
    {"DISABLE_FAILURE_PREDICTION", {target::global, ""}},
    {"ENABLE_PERFORMANCE_DATA", {target::global, ""}},
    {"DISABLE_PERFORMANCE_DATA", {target::global, ""}},
    {"START_EXECUTING_HOST_CHECKS", {target::global, ""}},
    {"STOP_EXECUTING_HOST_CHECKS", {target::global, ""}},
    {"START_EXECUTING_SVC_CHECKS", {target::global, ""}},
    {"STOP_EXECUTING_SVC_CHECKS", {target::global, ""}},
    {"START_ACCEPTING_PASSIVE_HOST_CHECKS", {target::global, ""}},
    {"STOP_ACCEPTING_PASSIVE_HOST_CHECKS", {target::global, ""}},
    {"START_ACCEPTING_PASSIVE_SVC_CHECKS", {target::global, ""}},
    {"STOP_ACCEPTING_PASSIVE_SVC_CHECKS", {target::global, ""}},
    {"START_OBSESSING_OVER_HOST_CHECKS", {target::global, ""}},
    {"STOP_OBSESSING_OVER_HOST_CHECKS", {target::global, ""}},
    {"START_OBSESSING_OVER_SVC_CHECKS", {target::global, ""}},
    {"STOP_OBSESSING_OVER_SVC_CHECKS", {target::global, ""}},
    {"ENABLE_FLAP_DETECTION", {target::global, ""}},
    {"DISABLE_FLAP_DETECTION", {target::global, ""}},
    {"CHANGE_GLOBAL_HOST_EVENT_HANDLER", {target::global, ""}},
    {"CHANGE_GLOBAL_SVC_EVENT_HANDLER", {target::global, ""}},
    {"ENABLE_SERVICE_FRESHNESS_CHECKS", {target::global, ""}},
    {"DISABLE_SERVICE_FRESHNESS_CHECKS", {target::global, ""}},
    {"ENABLE_HOST_FRESHNESS_CHECKS", {target::global, ""}},
    {"DISABLE_HOST_FRESHNESS_CHECKS", {target::global, ""}},
    {"ADD_HOST_COMMENT", {target::host, "AddHostComment"}},
    {"DEL_HOST_COMMENT", {target::comment, "DeleteComment"}},
    {"DEL_ALL_HOST_COMMENTS", {target::host, "DeleteAllHostComments"}},
    {"DELAY_HOST_NOTIFICATION", {target::host, ""}},
    {"ENABLE_HOST_NOTIFICATIONS", {target::host, "SetHostNotifications"}},
    {"DISABLE_HOST_NOTIFICATIONS", {target::host, "SetHostNotifications"}},
    {"ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST",
     {target::host, "SetHostNotifications"}},
    {"DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST",
     {target::host, "SetHostNotifications"}},
    {"ENABLE_HOST_AND_CHILD_NOTIFICATIONS",
     {target::host, "SetHostNotifications"}},
    {"DISABLE_HOST_AND_CHILD_NOTIFICATIONS",
     {target::host, "SetHostNotifications"}},
    {"ENABLE_HOST_SVC_NOTIFICATIONS", {target::host, "SetHostNotifications"}},
    {"DISABLE_HOST_SVC_NOTIFICATIONS", {target::host, "SetHostNotifications"}},
    {"ENABLE_HOST_SVC_CHECKS", {target::host, ""}},
    {"DISABLE_HOST_SVC_CHECKS", {target::host, ""}},
    {"ENABLE_PASSIVE_HOST_CHECKS", {target::host, ""}},
    {"DISABLE_PASSIVE_HOST_CHECKS", {target::host, ""}},
    {"SCHEDULE_HOST_SVC_CHECKS", {target::host, ""}},
    {"SCHEDULE_FORCED_HOST_SVC_CHECKS", {target::host, ""}},
    {"ACKNOWLEDGE_HOST_PROBLEM", {target::host, "AcknowledgeHostProblem"}},
    {"REMOVE_HOST_ACKNOWLEDGEMENT",
     {target::host, "RemoveHostAcknowledgement"}},
    {"ENABLE_HOST_EVENT_HANDLER", {target::host, ""}},
    {"DISABLE_HOST_EVENT_HANDLER", {target::host, ""}},
    {"ENABLE_HOST_CHECK", {target::host, ""}},
    {"DISABLE_HOST_CHECK", {target::host, ""}},
    {"SCHEDULE_HOST_CHECK", {target::host, ""}},
    {"SCHEDULE_FORCED_HOST_CHECK", {target::host, ""}},
    {"SCHEDULE_HOST_DOWNTIME", {target::host, "ScheduleDowntime"}},
    {"SCHEDULE_HOST_SVC_DOWNTIME", {target::host, "ScheduleDowntime"}},
    {"DEL_HOST_DOWNTIME", {target::downtime, "DeleteDowntime"}},
    {"DEL_HOST_DOWNTIME_FULL", {target::downtime, "DeleteDowntime"}},
    {"DEL_DOWNTIME_BY_HOST_NAME", {target::host, "DeleteDowntime"}},
    {"DEL_DOWNTIME_BY_HOSTGROUP_NAME", {target::hostgroup, "DeleteDowntime"}},
    {"DEL_DOWNTIME_BY_START_TIME_COMMENT",
     {target::downtime, "DeleteDowntime"}},
    {"ENABLE_HOST_FLAP_DETECTION", {target::host, ""}},
    {"DISABLE_HOST_FLAP_DETECTION", {target::host, ""}},
    {"START_OBSESSING_OVER_HOST", {target::host, ""}},
    {"STOP_OBSESSING_OVER_HOST", {target::host, ""}},
    {"CHANGE_HOST_EVENT_HANDLER", {target::host, ""}},
    {"CHANGE_HOST_CHECK_COMMAND", {target::host, ""}},
    {"CHANGE_NORMAL_HOST_CHECK_INTERVAL", {target::host, ""}},
    {"CHANGE_RETRY_HOST_CHECK_INTERVAL", {target::host, ""}},
    {"CHANGE_MAX_HOST_CHECK_ATTEMPTS", {target::host, ""}},
    {"SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME",
     {target::host, "ScheduleDowntime"}},
    {"SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME",
     {target::host, "ScheduleDowntime"}},
    {"SET_HOST_NOTIFICATION_NUMBER",
     {target::host, "SetHostNotificationNumber"}},
    {"CHANGE_HOST_CHECK_TIMEPERIOD", {target::host, ""}},
    {"CHANGE_CUSTOM_HOST_VAR", {target::host, ""}},
    {"SEND_CUSTOM_HOST_NOTIFICATION",
     {target::host, "SendCustomHostNotification"}},
    {"CHANGE_HOST_NOTIFICATION_TIMEPERIOD",
     {target::host, "SetHostNotificationPeriod"}},
    {"CHANGE_HOST_MODATTR", {target::host, ""}},
    {"ENABLE_HOSTGROUP_HOST_NOTIFICATIONS",
     {target::hostgroup, "SetHostNotifications"}},
    {"DISABLE_HOSTGROUP_HOST_NOTIFICATIONS",
     {target::hostgroup, "SetHostNotifications"}},
    {"ENABLE_HOSTGROUP_SVC_NOTIFICATIONS",
     {target::hostgroup, "SetServiceNotifications"}},
    {"DISABLE_HOSTGROUP_SVC_NOTIFICATIONS",
     {target::hostgroup, "SetServiceNotifications"}},
    {"ENABLE_HOSTGROUP_HOST_CHECKS", {target::hostgroup, ""}},
    {"DISABLE_HOSTGROUP_HOST_CHECKS", {target::hostgroup, ""}},
    {"ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS", {target::hostgroup, ""}},
    {"DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS", {target::hostgroup, ""}},
    {"ENABLE_HOSTGROUP_SVC_CHECKS", {target::hostgroup, ""}},
    {"DISABLE_HOSTGROUP_SVC_CHECKS", {target::hostgroup, ""}},
    {"ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS", {target::hostgroup, ""}},
    {"DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS", {target::hostgroup, ""}},
    {"SCHEDULE_HOSTGROUP_HOST_DOWNTIME",
     {target::hostgroup, "ScheduleDowntime"}},
    {"SCHEDULE_HOSTGROUP_SVC_DOWNTIME",
     {target::hostgroup, "ScheduleDowntime"}},
    {"ADD_SVC_COMMENT", {target::service, "AddServiceComment"}},
    {"DEL_SVC_COMMENT", {target::comment, "DeleteComment"}},
    {"DEL_ALL_SVC_COMMENTS", {target::service, "DeleteAllServiceComments"}},
    {"SCHEDULE_SVC_CHECK", {target::service, ""}},
    {"SCHEDULE_FORCED_SVC_CHECK", {target::service, ""}},
    {"ENABLE_SVC_CHECK", {target::service, ""}},
    {"DISABLE_SVC_CHECK", {target::service, ""}},
    {"ENABLE_PASSIVE_SVC_CHECKS", {target::service, ""}},
    {"DISABLE_PASSIVE_SVC_CHECKS", {target::service, ""}},
    {"DELAY_SVC_NOTIFICATION", {target::service, ""}},
    {"ENABLE_SVC_NOTIFICATIONS", {target::service, "SetServiceNotifications"}},
    {"DISABLE_SVC_NOTIFICATIONS", {target::service, "SetServiceNotifications"}},
    {"PROCESS_SERVICE_CHECK_RESULT", {target::service, ""}},
    {"PROCESS_HOST_CHECK_RESULT", {target::host, ""}},
    {"ENABLE_SVC_EVENT_HANDLER", {target::service, ""}},
    {"DISABLE_SVC_EVENT_HANDLER", {target::service, ""}},
    {"ENABLE_SVC_FLAP_DETECTION", {target::service, ""}},
    {"DISABLE_SVC_FLAP_DETECTION", {target::service, ""}},
    {"SCHEDULE_SVC_DOWNTIME", {target::service, "ScheduleDowntime"}},
    {"DEL_SVC_DOWNTIME", {target::downtime, "DeleteDowntime"}},
    {"DEL_SVC_DOWNTIME_FULL", {target::downtime, "DeleteDowntime"}},
    {"ACKNOWLEDGE_SVC_PROBLEM", {target::service, "AcknowledgeServiceProblem"}},
    {"REMOVE_SVC_ACKNOWLEDGEMENT",
     {target::service, "RemoveServiceAcknowledgement"}},
    {"START_OBSESSING_OVER_SVC", {target::service, ""}},
    {"STOP_OBSESSING_OVER_SVC", {target::service, ""}},
    {"CHANGE_SVC_EVENT_HANDLER", {target::service, ""}},
    {"CHANGE_SVC_CHECK_COMMAND", {target::service, ""}},
    {"CHANGE_NORMAL_SVC_CHECK_INTERVAL", {target::service, ""}},
    {"CHANGE_RETRY_SVC_CHECK_INTERVAL", {target::service, ""}},
    {"CHANGE_MAX_SVC_CHECK_ATTEMPTS", {target::service, ""}},
    {"SET_SVC_NOTIFICATION_NUMBER",
     {target::service, "SetServiceNotificationNumber"}},
    {"CHANGE_SVC_CHECK_TIMEPERIOD", {target::service, ""}},
    {"CHANGE_CUSTOM_SVC_VAR", {target::service, ""}},
    {"CHANGE_CUSTOM_CONTACT_VAR", {target::contact, ""}},
    {"SEND_CUSTOM_SVC_NOTIFICATION",
     {target::service, "SendCustomServiceNotification"}},
    {"CHANGE_SVC_NOTIFICATION_TIMEPERIOD",
     {target::service, "SetServiceNotificationPeriod"}},
    {"CHANGE_SVC_MODATTR", {target::service, ""}},
    {"ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS",
     {target::servicegroup, "SetHostNotifications"}},
    {"DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS",
     {target::servicegroup, "SetHostNotifications"}},
    {"ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS",
     {target::servicegroup, "SetServiceNotifications"}},
    {"DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS",
     {target::servicegroup, "SetServiceNotifications"}},
    {"ENABLE_SERVICEGROUP_HOST_CHECKS", {target::servicegroup, ""}},
    {"DISABLE_SERVICEGROUP_HOST_CHECKS", {target::servicegroup, ""}},
    {"ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS", {target::servicegroup, ""}},
    {"DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS", {target::servicegroup, ""}},
    {"ENABLE_SERVICEGROUP_SVC_CHECKS", {target::servicegroup, ""}},
    {"DISABLE_SERVICEGROUP_SVC_CHECKS", {target::servicegroup, ""}},
    {"ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS", {target::servicegroup, ""}},
    {"DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS", {target::servicegroup, ""}},
    {"SCHEDULE_SERVICEGROUP_HOST_DOWNTIME",
     {target::servicegroup, "ScheduleDowntime"}},
    {"SCHEDULE_SERVICEGROUP_SVC_DOWNTIME",
     {target::servicegroup, "ScheduleDowntime"}},
    {"ENABLE_CONTACT_HOST_NOTIFICATIONS",
     {target::contact, "SetContactHostNotifications"}},
    {"DISABLE_CONTACT_HOST_NOTIFICATIONS",
     {target::contact, "SetContactHostNotifications"}},
    {"ENABLE_CONTACT_SVC_NOTIFICATIONS",
     {target::contact, "SetContactServiceNotifications"}},
    {"DISABLE_CONTACT_SVC_NOTIFICATIONS",
     {target::contact, "SetContactServiceNotifications"}},
    {"CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD",
     {target::contact, "SetContactHostNotificationPeriod"}},
    {"CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD",
     {target::contact, "SetContactServiceNotificationPeriod"}},
    {"CHANGE_CONTACT_MODATTR", {target::contact, ""}},
    {"CHANGE_CONTACT_MODHATTR", {target::contact, ""}},
    {"CHANGE_CONTACT_MODSATTR", {target::contact, ""}},
    {"ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS",
     {target::contactgroup, "SetContactgroupHostNotifications"}},
    {"DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS",
     {target::contactgroup, "SetContactgroupHostNotifications"}},
    {"ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS",
     {target::contactgroup, "SetContactgroupServiceNotifications"}},
    {"DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS",
     {target::contactgroup, "SetContactgroupServiceNotifications"}},
    {"NEW_THRESHOLDS_FILE", {target::process, ""}},
    {"PROCESS_FILE", {target::process, ""}},
    {"CHANGE_ANOMALYDETECTION_SENSITIVITY", {target::service, ""}},
};

}  // namespace

/**
 * @brief Look a legacy external command up by name.
 *
 * @param name The command name, e.g. "SCHEDULE_FORCED_SVC_CHECK".
 *
 * @return Its target and Broker counterpart, or nullopt if unknown.
 */
std::optional<command_info> lookup(std::string_view name) {
  auto it = table.find(name);
  if (it == table.end())
    return std::nullopt;
  return it->second;
}

/**
 * @brief Human readable name of a target, for logs and error messages.
 *
 * @param t The target.
 *
 * @return Its name.
 */
std::string_view to_string(target t) {
  switch (t) {
    case target::process:
      return "process";
    case target::global:
      return "global";
    case target::host:
      return "host";
    case target::service:
      return "service";
    case target::hostgroup:
      return "hostgroup";
    case target::servicegroup:
      return "servicegroup";
    case target::contact:
      return "contact";
    case target::contactgroup:
      return "contactgroup";
    case target::downtime:
      return "downtime";
    case target::comment:
      return "comment";
  }
  return "unknown";
}

}  // namespace com::centreon::common::external_commands
