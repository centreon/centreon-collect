/*
** mapping.cpp for CentreonBroker in ./src
** 
** Made by Matthieu Kermagoret <mkermagoret@merethis.com>
** 
** Copyright Merethis
** See LICENSE file for details.
** 
** Started on  06/01/09 Matthieu Kermagoret
** Last update 06/12/09 Matthieu Kermagoret
*/

#include "host.h"
#include "host_status.h"
#include "logging.h"
#include "mapping.h"

using namespace CentreonBroker;

/**
 *  Connection mapping.
 */
DB::Mapping<Connection> CentreonBroker::connection_mapping;

static void InitConnectionMapping()
{
  logging.AddDebug("Initializing Connection mapping...");
  connection_mapping.SetTable("connection_info");
  connection_mapping.AddStringField("agent_name",
				    &Connection::GetAgentName);
  connection_mapping.AddStringField("agent_version",
				    &Connection::GetAgentVersion);
  connection_mapping.AddIntField("bytes_processed",
				 &Connection::GetBytesProcessed);
  connection_mapping.AddStringField("connect_source",
				    &Connection::GetConnectSource);
  connection_mapping.AddTimeField("connect_time",
				  &Connection::GetConnectTime);
  connection_mapping.AddStringField("connect_type",
				    &Connection::GetConnectType);
  connection_mapping.AddTimeField("data_start_time",
				  &Connection::GetDataStartTime);
  connection_mapping.AddIntField("entries_processed",
				 &Connection::GetEntriesProcessed);
  connection_mapping.AddIntField("lines_processed",
				 &Connection::GetLinesProcessed);
  connection_mapping.AddTimeField("data_end_time",
				  &Connection::GetDataEndTime);
  connection_mapping.AddTimeField("disconnect_time",
				  &Connection::GetDisconnectTime);
  connection_mapping.AddTimeField("last_checkin_time",
				  &Connection::GetLastCheckinTime);
  return ;
}

/**
 *  Host mapping.
 */
DB::Mapping<Host> CentreonBroker::host_mapping;

static void InitHostMapping()
{
  logging.AddDebug("Initializing Host mapping...");
  host_mapping.SetTable("hosts");
  host_mapping.AddShortField("acknowledgement_type",
			     &Host::GetAcknowledgementType);
  host_mapping.AddStringField("action_url",
			      &Host::GetActionUrl);
  host_mapping.AddShortField("active_checks_enabled",
			     &Host::GetActiveChecksEnabled);
  host_mapping.AddStringField("address",
			      &Host::GetAddress);
  host_mapping.AddStringField("alias",
			      &Host::GetAlias);
  host_mapping.AddStringField("check_command",
			      &Host::GetCheckCommand);
  host_mapping.AddDoubleField("check_interval",
			      &Host::GetCheckInterval);
  host_mapping.AddShortField("check_type",
			     &Host::GetCheckType);
  host_mapping.AddShortField("current_check_attempt",
			     &Host::GetCurrentCheckAttempt);
  host_mapping.AddShortField("current_notification_number",
			     &Host::GetCurrentNotificationNumber);
  host_mapping.AddShortField("current_state",
			     &Host::GetCurrentState);
  host_mapping.AddStringField("display_name",
			      &Host::GetDisplayName);
  host_mapping.AddStringField("event_handler",
			      &Host::GetEventHandler);
  host_mapping.AddShortField("event_handler_enabled",
			     &Host::GetEventHandlerEnabled);
  host_mapping.AddDoubleField("execution_time",
			      &Host::GetExecutionTime);
  host_mapping.AddShortField("failure_prediction_enabled",
			     &Host::GetFailurePredictionEnabled);
  host_mapping.AddDoubleField("first_notification_delay",
			      &Host::GetFirstNotificationDelay);
  host_mapping.AddShortField("flap_detection_enabled",
			     &Host::GetFlapDetectionEnabled);
  host_mapping.AddShortField("flap_detection_on_down",
			     &Host::GetFlapDetectionOnDown);
  host_mapping.AddShortField("flap_detection_on_unreachable",
			     &Host::GetFlapDetectionOnUnreachable);
  host_mapping.AddShortField("flap_detection_on_up",
			     &Host::GetFlapDetectionOnUp);
  host_mapping.AddShortField("freshness_threshold",
			     &Host::GetFreshnessThreshold);
  host_mapping.AddShortField("has_been_checked",
			      &Host::GetHasBeenChecked);
  host_mapping.AddShortField("have_2d_coords",
			     &Host::GetHave2DCoords);
  host_mapping.AddDoubleField("high_flap_threshold",
			      &Host::GetHighFlapThreshold);
  host_mapping.AddStringField("host_name",
			      &Host::GetHostName);
  host_mapping.AddStringField("icon_image",
			      &Host::GetIconImage);
  host_mapping.AddStringField("icon_image_alt",
			      &Host::GetIconImageAlt);
  host_mapping.AddShortField("is_flapping",
			     &Host::GetIsFlapping);
  host_mapping.AddTimeField("last_check",
			    &Host::GetLastCheck);
  host_mapping.AddShortField("last_hard_state",
			     &Host::GetLastHardState);
  host_mapping.AddTimeField("last_hard_state_change",
			    &Host::GetLastHardStateChange);
  host_mapping.AddTimeField("last_notification",
			    &Host::GetLastNotification);
  host_mapping.AddTimeField("last_state_change",
			    &Host::GetLastStateChange);
  host_mapping.AddTimeField("last_time_down",
			    &Host::GetLastTimeDown);
  host_mapping.AddTimeField("last_time_unreachable",
			    &Host::GetLastTimeUnreachable);
  host_mapping.AddTimeField("last_time_up",
			     &Host::GetLastTimeUp);
  host_mapping.AddDoubleField("latency",
			      &Host::GetLatency);
  host_mapping.AddDoubleField("low_flap_threshold",
			      &Host::GetLowFlapThreshold);
  host_mapping.AddShortField("max_check_attempts",
			     &Host::GetMaxCheckAttempts);
  host_mapping.AddIntField("modified_host_attributes",
			   &Host::GetModifiedAttributes);
  host_mapping.AddTimeField("next_check",
			    &Host::GetNextCheck);
  host_mapping.AddTimeField("next_notification",
			    &Host::GetNextNotification);
  host_mapping.AddShortField("no_more_notifications",
			     &Host::GetNoMoreNotifications);
  host_mapping.AddStringField("notes",
			      &Host::GetNotes);
  host_mapping.AddStringField("notes_url",
			      &Host::GetNotesUrl);
  host_mapping.AddDoubleField("notification_interval",
			      &Host::GetNotificationInterval);
  host_mapping.AddShortField("notifications_enabled",
			     &Host::GetNotificationsEnabled);
  host_mapping.AddShortField("notify_on_down",
			     &Host::GetNotifyOnDown);
  host_mapping.AddShortField("notify_on_downtime",
			     &Host::GetNotifyOnDowntime);
  host_mapping.AddShortField("notify_on_flapping",
			     &Host::GetNotifyOnFlapping);
  host_mapping.AddShortField("notify_on_recovery",
			     &Host::GetNotifyOnRecovery);
  host_mapping.AddShortField("notify_on_unreachable",
			     &Host::GetNotifyOnUnreachable);
  host_mapping.AddShortField("obsess_over_host",
			     &Host::GetObsessOver);
  host_mapping.AddStringField("output",
			      &Host::GetOutput);
  host_mapping.AddShortField("passive_checks_enabled",
			     &Host::GetPassiveChecksEnabled);
  host_mapping.AddDoubleField("percent_state_change",
			      &Host::GetPercentStateChange);
  host_mapping.AddStringField("perfdata",
			      &Host::GetPerfdata);
  host_mapping.AddShortField("problem_has_been_acknowledged",
			     &Host::GetProblemHasBeenAcknowledged);
  host_mapping.AddShortField("process_performance_data",
			     &Host::GetProcessPerformanceData);
  host_mapping.AddShortField("retain_nonstatus_information",
			     &Host::GetRetainNonstatusInformation);
  host_mapping.AddShortField("retain_status_information",
			     &Host::GetRetainStatusInformation);
  host_mapping.AddDoubleField("retry_interval",
			      &Host::GetRetryInterval);
  host_mapping.AddShortField("scheduled_downtime_depth",
			     &Host::GetScheduledDowntimeDepth);
  host_mapping.AddShortField("should_be_scheduled",
			     &Host::GetShouldBeScheduled);
  host_mapping.AddShortField("stalk_on_down",
			     &Host::GetStalkOnDown);
  host_mapping.AddShortField("stalk_on_unreachable",
			     &Host::GetStalkOnUnreachable);
  host_mapping.AddShortField("stalk_on_up",
			     &Host::GetStalkOnUp);
  host_mapping.AddShortField("state_type",
			     &Host::GetStateType);
  host_mapping.AddTimeField("status_update_time",
			    &Host::GetStatusUpdateTime);
  host_mapping.AddStringField("statusmap_image",
			      &Host::GetStatusmapImage);
  host_mapping.AddStringField("vrml_image",
			      &Host::GetVrmlImage);
  host_mapping.AddShortField("x_2d",
			     &Host::GetX2D);
  host_mapping.AddShortField("y_2d",
			     &Host::GetY2D);
  return ;
}

DB::Mapping<HostStatus> CentreonBroker::host_status_mapping;

static void InitHostStatusMapping()
{
  logging.AddDebug("Initializing HostStatus mapping...");
  host_status_mapping.SetTable("hosts");
  host_status_mapping.AddShortField("acknowledgement_type",
                                    &HostStatus::GetAcknowledgementType);
  host_status_mapping.AddShortField("active_checks_enabled",
				    &HostStatus::GetActiveChecksEnabled);
  host_status_mapping.AddStringField("check_command",
				     &HostStatus::GetCheckCommand);
  host_status_mapping.AddDoubleField("check_interval",
				     &HostStatus::GetCheckInterval);
  host_status_mapping.AddShortField("check_type",
				    &HostStatus::GetCheckType);
  host_status_mapping.AddShortField("current_check_attempt",
				    &HostStatus::GetCurrentCheckAttempt);
  host_status_mapping.AddShortField("current_notification_number",
				    &HostStatus::GetCurrentNotificationNumber);
  host_status_mapping.AddShortField("current_state",
				    &HostStatus::GetCurrentState);
  host_status_mapping.AddStringField("event_handler",
				     &HostStatus::GetEventHandler);
  host_status_mapping.AddShortField("event_handler_enabled",
				    &HostStatus::GetEventHandlerEnabled);
  host_status_mapping.AddDoubleField("execution_time",
				     &HostStatus::GetExecutionTime);
  host_status_mapping.AddShortField("failure_prediction_enabled",
    &HostStatus::GetFailurePredictionEnabled);
  host_status_mapping.AddShortField("flap_detection_enabled",
				    &HostStatus::GetFlapDetectionEnabled);
  host_status_mapping.AddShortField("has_been_checked",
				    &HostStatus::GetHasBeenChecked);
  host_status_mapping.AddShortField("is_flapping",
				    &HostStatus::GetIsFlapping);
  host_status_mapping.AddTimeField("last_check",
				   &HostStatus::GetLastCheck);
  host_status_mapping.AddShortField("last_hard_state",
				    &HostStatus::GetLastHardState);
  host_status_mapping.AddTimeField("last_hard_state_change",
				   &HostStatus::GetLastHardStateChange);
  host_status_mapping.AddTimeField("last_notification",
				   &HostStatus::GetLastNotification);
  host_status_mapping.AddTimeField("last_state_change",
				   &HostStatus::GetLastStateChange);
  host_status_mapping.AddTimeField("last_time_down",
				   &HostStatus::GetLastTimeDown);
  host_status_mapping.AddTimeField("last_time_unreachable",
				   &HostStatus::GetLastTimeUnreachable);
  host_status_mapping.AddTimeField("last_time_up",
				   &HostStatus::GetLastTimeUp);
  host_status_mapping.AddDoubleField("latency",
				     &HostStatus::GetLatency);
  host_status_mapping.AddShortField("max_check_attempts",
				    &HostStatus::GetMaxCheckAttempts);
  host_status_mapping.AddIntField("modified_host_attributes",
				  &HostStatus::GetModifiedAttributes);
  host_status_mapping.AddTimeField("next_check",
				   &HostStatus::GetNextCheck);
  host_status_mapping.AddTimeField("next_notification",
				   &HostStatus::GetNextNotification);
  host_status_mapping.AddShortField("no_more_notifications",
				    &HostStatus::GetNoMoreNotifications);
  host_status_mapping.AddShortField("notifications_enabled",
				    &HostStatus::GetNotificationsEnabled);
  host_status_mapping.AddShortField("obsess_over_host",
				    &HostStatus::GetObsessOver);
  host_status_mapping.AddStringField("output",
				     &HostStatus::GetOutput);
  host_status_mapping.AddShortField("passive_checks_enabled",
				    &HostStatus::GetPassiveChecksEnabled);
  host_status_mapping.AddDoubleField("percent_state_change",
				     &HostStatus::GetPercentStateChange);
  host_status_mapping.AddStringField("perfdata",
				     &HostStatus::GetPerfdata);
  host_status_mapping.AddShortField("problem_has_been_acknowledged",
    &HostStatus::GetProblemHasBeenAcknowledged);
  host_status_mapping.AddShortField("process_performance_data",
				    &HostStatus::GetProcessPerformanceData);
  host_status_mapping.AddDoubleField("retry_interval",
				     &HostStatus::GetRetryInterval);
  host_status_mapping.AddShortField("scheduled_downtime_depth",
				    &HostStatus::GetScheduledDowntimeDepth);
  host_status_mapping.AddShortField("should_be_scheduled",
				    &HostStatus::GetShouldBeScheduled);
  host_status_mapping.AddShortField("state_type",
				    &HostStatus::GetStateType);
  host_status_mapping.AddTimeField("status_update_time",
				   &HostStatus::GetStatusUpdateTime);
  return ;
}

DB::Mapping<ProgramStatus> CentreonBroker::program_status_mapping;

static void InitProgramStatusMapping()
{
  logging.AddDebug("Initializing ProgramStatus mapping");
  program_status_mapping.AddShortField("active_host_checks_enabled",
    &ProgramStatus::GetActiveHostChecksEnabled);
  program_status_mapping.AddShortField("active_service_checks_enabled",
    &ProgramStatus::GetActiveServiceChecksEnabled);
  program_status_mapping.AddShortField("daemon_mode",
				       &ProgramStatus::GetDaemonMode);
  program_status_mapping.AddShortField("event_handler_enabled",
				       &ProgramStatus::GetEventHandlerEnabled);
  program_status_mapping.AddShortField("failure_prediction_enabled",
    &ProgramStatus::GetFailurePredictionEnabled);
  program_status_mapping.AddShortField("flap_detection_enabled",
    &ProgramStatus::GetFlapDetectionEnabled);
  program_status_mapping.AddStringField("global_host_event_handler",
    &ProgramStatus::GetGlobalHostEventHandler);
  program_status_mapping.AddStringField("global_service_event_handler",
    &ProgramStatus::GetGlobalServiceEventHandler);
  program_status_mapping.AddShortField("is_currently_running",
				       &ProgramStatus::GetIsCurrentlyRunning);
  program_status_mapping.AddTimeField("last_command_check",
				      &ProgramStatus::GetLastCommandCheck);
  program_status_mapping.AddTimeField("last_log_rotation",
				      &ProgramStatus::GetLastLogRotation);
  program_status_mapping.AddIntField("modified_host_attributes",
    &ProgramStatus::GetModifiedHostAttributes);
  program_status_mapping.AddIntField("modified_service_attributes",
    &ProgramStatus::GetModifiedServiceAttributes);
  program_status_mapping.AddShortField("notifications_enabled",
    &ProgramStatus::GetNotificationsEnabled);
  program_status_mapping.AddShortField("obsess_over_hosts",
				       &ProgramStatus::GetObsessOverHosts);
  program_status_mapping.AddShortField("obsess_over_services",
				       &ProgramStatus::GetObsessOverServices);
  program_status_mapping.AddShortField("passive_host_checks_enabled",
    &ProgramStatus::GetPassiveHostChecksEnabled);
  program_status_mapping.AddShortField("passive_service_checks_enabled",
    &ProgramStatus::GetPassiveServiceChecksEnabled);
  program_status_mapping.AddIntField("process_id",
				     &ProgramStatus::GetProcessId);
  program_status_mapping.AddShortField("process_performance_data",
    &ProgramStatus::GetProcessPerformanceData);
  program_status_mapping.AddTimeField("program_end_time",
				      &ProgramStatus::GetProgramEndTime);
  program_status_mapping.AddTimeField("program_start_time",
				      &ProgramStatus::GetProgramStartTime);
  program_status_mapping.AddTimeField("status_update_time",
				      &ProgramStatus::GetStatusUpdateTime);
  return ;
}

DB::Mapping<Service> CentreonBroker::service_mapping;

static void InitServiceMapping()
{
  logging.AddDebug("Initializing Service mapping...");
  service_mapping.SetTable("services");
  service_mapping.AddShortField("acknowledgement_type",
				&Service::GetAcknowledgementType);
  service_mapping.AddStringField("action_url",
				 &Service::GetActionUrl);
  service_mapping.AddShortField("active_checks_enabled",
				&Service::GetActiveChecksEnabled);
  service_mapping.AddStringField("check_command",
				 &Service::GetCheckCommand);
  service_mapping.AddDoubleField("check_interval",
				 &Service::GetCheckInterval);
  service_mapping.AddShortField("check_type",
				&Service::GetCheckType);
  service_mapping.AddShortField("current_check_attempt",
				&Service::GetCurrentCheckAttempt);
  service_mapping.AddShortField("current_notification_number",
				&Service::GetCurrentNotificationNumber);
  service_mapping.AddShortField("current_state",
				&Service::GetCurrentState);
  service_mapping.AddShortField("default_active_checks_enabled",
				&Service::GetActiveChecksEnabled);
  service_mapping.AddShortField("default_event_handler_enabled",
				&Service::GetFailurePredictionEnabled);
  service_mapping.AddShortField("default_failure_prediction_enabled",
				&Service::GetFailurePredictionEnabled);
  service_mapping.AddShortField("default_flap_detection_enabled",
				&Service::GetFlapDetectionEnabled);
  service_mapping.AddShortField("default_notifications_enabled",
				&Service::GetNotificationsEnabled);
  service_mapping.AddShortField("default_passive_checks_enabled",
				&Service::GetPassiveChecksEnabled);
  service_mapping.AddShortField("default_process_performance_data",
				&Service::GetProcessPerformanceData);
  service_mapping.AddStringField("display_name",
				 &Service::GetDisplayName);
  service_mapping.AddStringField("event_handler",
				 &Service::GetEventHandler);
  service_mapping.AddShortField("event_handler_enabled",
				&Service::GetEventHandlerEnabled);
  service_mapping.AddDoubleField("execution_time",
				 &Service::GetExecutionTime);
  service_mapping.AddShortField("failure_prediction_enabled",
				&Service::GetFailurePredictionEnabled);
  service_mapping.AddStringField("failure_prediction_options",
				 &Service::GetFailurePredictionOptions);
  service_mapping.AddDoubleField("first_notification_delay",
				 &Service::GetFirstNotificationDelay);
  service_mapping.AddShortField("flap_detection_enabled",
				&Service::GetFlapDetectionEnabled);
  service_mapping.AddShortField("flap_detection_on_critical",
				&Service::GetFlapDetectionOnCritical);
  service_mapping.AddShortField("flap_detection_on_ok",
				&Service::GetFlapDetectionOnOk);
  service_mapping.AddShortField("flap_detection_on_unknown",
				&Service::GetFlapDetectionOnUnknown);
  service_mapping.AddShortField("flap_detection_on_warning",
				&Service::GetFlapDetectionOnWarning);
  service_mapping.AddShortField("freshness_checks_enabled",
				&Service::GetFreshnessChecksEnabled);
  service_mapping.AddShortField("freshness_threshold",
				&Service::GetFreshnessThreshold);
  service_mapping.AddShortField("has_been_checked",
				&Service::GetHasBeenChecked);
  service_mapping.AddDoubleField("high_flap_threshold",
				 &Service::GetHighFlapThreshold);
  service_mapping.AddStringField("host_name",
				 &Service::GetHostName);
  service_mapping.AddStringField("icon_image",
				 &Service::GetIconImage);
  service_mapping.AddStringField("icon_image_alt",
				 &Service::GetIconImageAlt);
  service_mapping.AddShortField("is_flapping",
				&Service::GetIsFlapping);
  service_mapping.AddShortField("is_volatile",
				&Service::GetIsVolatile);
  service_mapping.AddTimeField("last_check",
			       &Service::GetLastCheck);
  service_mapping.AddShortField("last_hard_state",
				&Service::GetLastHardState);
  service_mapping.AddTimeField("last_hard_state_change",
			       &Service::GetLastHardStateChange);
  service_mapping.AddTimeField("last_notification",
			       &Service::GetLastNotification);
  service_mapping.AddTimeField("last_state_change",
			       &Service::GetLastStateChange);
  service_mapping.AddTimeField("last_time_critical",
			       &Service::GetLastTimeCritical);
  service_mapping.AddTimeField("last_time_ok",
			       &Service::GetLastTimeOk);
  service_mapping.AddTimeField("last_time_unknown",
			       &Service::GetLastTimeUnknown);
  service_mapping.AddTimeField("last_time_warning",
			       &Service::GetLastTimeWarning);
  service_mapping.AddTimeField("latency",
			       &Service::GetLatency);
  service_mapping.AddDoubleField("low_flap_threshold",
				 &Service::GetLowFlapThreshold);
  service_mapping.AddShortField("max_check_attempts",
				&Service::GetMaxCheckAttempts);
  service_mapping.AddIntField("modified_service_attributes",
			      &Service::GetModifiedAttributes);
  service_mapping.AddTimeField("next_check",
			       &Service::GetNextCheck);
  service_mapping.AddTimeField("next_notification",
			       &Service::GetNextNotification);
  service_mapping.AddShortField("no_more_notifications",
				&Service::GetNoMoreNotifications);
  service_mapping.AddStringField("notes",
				 &Service::GetNotes);
  service_mapping.AddStringField("notes_url",
				 &Service::GetNotesUrl);
  service_mapping.AddDoubleField("notification_interval",
				 &Service::GetNotificationInterval);
  service_mapping.AddShortField("notifications_enabled",
				&Service::GetNotificationsEnabled);
  service_mapping.AddShortField("notify_on_critical",
				&Service::GetNotifyOnCritical);
  service_mapping.AddShortField("notify_on_downtime",
				&Service::GetNotifyOnDowntime);
  service_mapping.AddShortField("notify_on_flapping",
				&Service::GetNotifyOnFlapping);
  service_mapping.AddShortField("notify_on_recovery",
				&Service::GetNotifyOnRecovery);
  service_mapping.AddShortField("notify_on_unknown",
				&Service::GetNotifyOnUnknown);
  service_mapping.AddShortField("notify_on_warning",
				&Service::GetNotifyOnWarning);
  service_mapping.AddShortField("obsess_over_service",
				&Service::GetObsessOver);
  service_mapping.AddStringField("output",
				 &Service::GetOutput);
  service_mapping.AddShortField("passive_checks_enabled",
				&Service::GetPassiveChecksEnabled);
  service_mapping.AddDoubleField("percent_state_change",
				 &Service::GetPercentStateChange);
  service_mapping.AddStringField("perfdata",
				 &Service::GetPerfdata);
  service_mapping.AddShortField("problem_has_been_acknowledged",
				&Service::GetProblemHasBeenAcknowledged);
  service_mapping.AddShortField("process_performance_data",
				&Service::GetProcessPerformanceData);
  service_mapping.AddShortField("retain_nonstatus_information",
				&Service::GetRetainNonstatusInformation);
  service_mapping.AddShortField("retain_status_information",
				&Service::GetRetainStatusInformation);
  service_mapping.AddDoubleField("retry_interval",
				 &Service::GetRetryInterval);
  service_mapping.AddShortField("scheduled_downtime_depth",
				&Service::GetScheduledDowntimeDepth);
  service_mapping.AddStringField("service_description",
				 &Service::GetServiceDescription);
  service_mapping.AddShortField("should_be_scheduled",
				&Service::GetShouldBeScheduled);
  service_mapping.AddShortField("stalk_on_critical",
				&Service::GetStalkOnCritical);
  service_mapping.AddShortField("stalk_on_ok",
				&Service::GetStalkOnOk);
  service_mapping.AddShortField("stalk_on_unknown",
				&Service::GetStalkOnUnknown);
  service_mapping.AddShortField("stalk_on_warning",
				&Service::GetStalkOnWarning);
  service_mapping.AddShortField("state_type",
				&Service::GetStateType);
  service_mapping.AddTimeField("status_update_time",
			       &Service::GetStatusUpdateTime);
  return ;
}

DB::Mapping<ServiceStatus> CentreonBroker::service_status_mapping;

static void InitServiceStatusMapping()
{
  logging.AddDebug("Initializing ServiceStatus mapping...");
  service_status_mapping.SetTable("services");
  service_status_mapping.AddShortField("acknowledgement_type",
				       &ServiceStatus::GetAcknowledgementType);
  service_status_mapping.AddShortField("active_checks_enabled",
				       &ServiceStatus::GetActiveChecksEnabled);
  service_status_mapping.AddStringField("check_command",
					&ServiceStatus::GetCheckCommand);
  service_status_mapping.AddDoubleField("check_interval",
					&ServiceStatus::GetCheckInterval);
  service_status_mapping.AddShortField("check_type",
				       &ServiceStatus::GetCheckType);
  service_status_mapping.AddShortField("current_check_attempt",
				       &ServiceStatus::GetCurrentCheckAttempt);
  service_status_mapping.AddShortField("current_notification_number",
    &ServiceStatus::GetCurrentNotificationNumber);
  service_status_mapping.AddShortField("current_state",
				       &ServiceStatus::GetCurrentState);
  service_status_mapping.AddStringField("event_handler",
					&ServiceStatus::GetEventHandler);
  service_status_mapping.AddShortField("event_handler_enabled",
				       &ServiceStatus::GetEventHandlerEnabled);
  service_status_mapping.AddDoubleField("execution_time",
					&ServiceStatus::GetExecutionTime);
  service_status_mapping.AddShortField("failure_prediction_enabled",
    &ServiceStatus::GetFailurePredictionEnabled);
  service_status_mapping.AddShortField("flap_detection_enabled",
    &ServiceStatus::GetFlapDetectionEnabled);
  service_status_mapping.AddShortField("has_been_checked",
				       &ServiceStatus::GetHasBeenChecked);
  service_status_mapping.AddShortField("is_flapping",
				       &ServiceStatus::GetIsFlapping);
  service_status_mapping.AddTimeField("last_check",
				      &ServiceStatus::GetLastCheck);
  service_status_mapping.AddShortField("last_hard_state",
				       &ServiceStatus::GetLastHardState);
  service_status_mapping.AddTimeField("last_hard_state_change",
				      &ServiceStatus::GetLastHardStateChange);
  service_status_mapping.AddTimeField("last_notification",
				      &ServiceStatus::GetLastNotification);
  service_status_mapping.AddTimeField("last_state_change",
				      &ServiceStatus::GetLastStateChange);
  service_status_mapping.AddTimeField("last_time_critical",
				      &ServiceStatus::GetLastTimeCritical);
  service_status_mapping.AddTimeField("last_time_ok",
				      &ServiceStatus::GetLastTimeOk);
  service_status_mapping.AddTimeField("last_time_unknown",
				      &ServiceStatus::GetLastTimeUnknown);
  service_status_mapping.AddTimeField("last_time_warning",
				      &ServiceStatus::GetLastTimeWarning);
  service_status_mapping.AddDoubleField("latency",
					&ServiceStatus::GetLatency);
  service_status_mapping.AddShortField("max_check_attempts",
				       &ServiceStatus::GetMaxCheckAttempts);
  service_status_mapping.AddIntField("modified_service_attributes",
				     &ServiceStatus::GetModifiedAttributes);
  service_status_mapping.AddTimeField("next_check",
				      &ServiceStatus::GetNextCheck);
  service_status_mapping.AddTimeField("next_notification",
				      &ServiceStatus::GetNextCheck);
  service_status_mapping.AddShortField("no_more_notifications",
				       &ServiceStatus::GetNoMoreNotifications);
  service_status_mapping.AddShortField("notifications_enabled",
    &ServiceStatus::GetNotificationsEnabled);
  service_status_mapping.AddShortField("obsess_over_service",
				       &ServiceStatus::GetObsessOver);
  service_status_mapping.AddStringField("output",
					&ServiceStatus::GetOutput);
  service_status_mapping.AddShortField("passive_checks_enabled",
    &ServiceStatus::GetPassiveChecksEnabled);
  service_status_mapping.AddDoubleField("percent_state_change",
					&ServiceStatus::GetPercentStateChange);
  service_status_mapping.AddStringField("perfdata",
					&ServiceStatus::GetPerfdata);
  service_status_mapping.AddShortField("problem_has_been_acknowledged",
    &ServiceStatus::GetProblemHasBeenAcknowledged);
  service_status_mapping.AddShortField("process_performance_data",
    &ServiceStatus::GetProcessPerformanceData);
  service_status_mapping.AddDoubleField("retry_interval",
					&ServiceStatus::GetRetryInterval);
  service_status_mapping.AddShortField("scheduled_downtime_depth",
    &ServiceStatus::GetScheduledDowntimeDepth);
  service_status_mapping.AddShortField("should_be_scheduled",
				       &ServiceStatus::GetShouldBeScheduled);
  service_status_mapping.AddShortField("state_type",
				       &ServiceStatus::GetStateType);
  service_status_mapping.AddTimeField("status_update_time",
				      &ServiceStatus::GetStatusUpdateTime);
  return ;
}

void CentreonBroker::InitMappings()
{
  logging.AddDebug("Initializing Object-Relational mappings...");
  logging.Indent();
  InitConnectionMapping();
  InitHostMapping();
  InitHostStatusMapping();
  InitProgramStatusMapping();
  InitServiceMapping();
  InitServiceStatusMapping();
  logging.Deindent();
  logging.AddDebug("Object-Relational mappings initialized");
  return ;
}
