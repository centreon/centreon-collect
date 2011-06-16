/*
** Copyright 1999-2010 Ethan Galstad
** Copyright 2009      Nagios Core Development Team and Community Contributors
** Copyright 2011      Merethis
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

/*********** COMMON HEADER FILES ***********/

#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "common.hh"
#include "globals.hh"
#include "objects.hh"
#include "statusdata.hh"
#include "macros.hh"
#include "engine.hh"
#include "sretention.hh"
#include "comments.hh"
#include "downtime.hh"
#include "flapping.hh"
#include "notifications.hh"
#include "utils.hh"
#include "logging.hh"

/**** STATE INFORMATION SPECIFIC HEADER FILES ****/

#include "xrddefault.hh"

static char*               xrddefault_retention_file = NULL;
static char*               xrddefault_temp_file = NULL;

/******************************************************************/
/********************* CONFIG INITIALIZATION  *********************/
/******************************************************************/

int xrddefault_grab_config_info(char* main_config_file) {
  char* input = NULL;
  mmapfile* thefile = NULL;
  nagios_macros* mac;

  mac = get_global_macros();

  /* open the main config file for reading */
  if ((thefile = mmap_fopen(main_config_file)) == NULL) {
    log_debug_info(DEBUGL_RETENTIONDATA, 2,
                   "Error: Cannot open main configuration file '%s' for reading!\n",
                   main_config_file);

    delete[] xrddefault_retention_file;
    delete[] xrddefault_temp_file;

    xrddefault_retention_file = NULL;
    xrddefault_temp_file = NULL;

    return (ERROR);
  }

  /* read in all lines from the main config file */
  while (1) {
    /* free memory */
    delete[] input;

    /* read the next line */
    if ((input = mmap_fgets_multiline(thefile)) == NULL)
      break;

    strip(input);

    /* skip blank lines and comments */
    if (input[0] == '#' || input[0] == '\x0')
      continue;

    xrddefault_grab_config_directives(input);
  }

  /* free memory and close the file */
  delete[] input;
  mmap_fclose(thefile);

  /* initialize locations if necessary  */
  if (xrddefault_retention_file == NULL)
    xrddefault_retention_file = my_strdup(DEFAULT_RETENTION_FILE);
  if (xrddefault_temp_file == NULL)
    xrddefault_temp_file = my_strdup(DEFAULT_TEMP_FILE);

  /* save the retention file macro */
  delete[] mac->x[MACRO_RETENTIONDATAFILE];
  mac->x[MACRO_RETENTIONDATAFILE] = my_strdup(xrddefault_retention_file);
  strip(mac->x[MACRO_RETENTIONDATAFILE]);

  return (OK);
}

/* process a single config directive */
int xrddefault_grab_config_directives(char* input) {
  char const* temp_ptr = NULL;
  char* varname = NULL;
  char* varvalue = NULL;

  /* get the variable name */
  if ((temp_ptr = my_strtok(input, "=")) == NULL)
    return (ERROR);
  varname = my_strdup(temp_ptr);

  /* get the variable value */
  if ((temp_ptr = my_strtok(NULL, "\n")) == NULL) {
    delete[] varname;
    return (ERROR);
  }
  varvalue = my_strdup(temp_ptr);

  /* retention file definition */
  if (!strcmp(varname, "xrddefault_retention_file")
      || !strcmp(varname, "state_retention_file"))
    xrddefault_retention_file = my_strdup(varvalue);

  /* temp file definition */
  else if (!strcmp(varname, "temp_file"))
    xrddefault_temp_file = my_strdup(varvalue);

  /* free memory */
  delete[] varname;
  delete[] varvalue;

  return (OK);
}

/******************************************************************/
/********************* INIT/CLEANUP FUNCTIONS *********************/
/******************************************************************/

/* initialize retention data */
int xrddefault_initialize_retention_data(char* config_file) {
  /* grab configuration data */
  if (xrddefault_grab_config_info(config_file) == ERROR)
    return (ERROR);
  return (OK);
}

/* cleanup retention data before terminating */
int xrddefault_cleanup_retention_data(char* config_file) {
  (void)config_file;

  /* free memory */
  delete[] xrddefault_retention_file;
  delete[] xrddefault_temp_file;

  xrddefault_retention_file = NULL;
  xrddefault_temp_file = NULL;

  return (OK);
}

/******************************************************************/
/**************** DEFAULT STATE OUTPUT FUNCTION *******************/
/******************************************************************/

int xrddefault_save_state_information(void) {
  char* temp_file = NULL;
  customvariablesmember* temp_customvariablesmember = NULL;
  time_t current_time = 0L;
  int result = OK;
  FILE* fp = NULL;
  host* temp_host = NULL;
  service* temp_service = NULL;
  contact* temp_contact = NULL;
  comment* temp_comment = NULL;
  scheduled_downtime* temp_downtime = NULL;
  unsigned int x = 0;
  int fd = 0;
  unsigned long host_attribute_mask = 0L;
  unsigned long service_attribute_mask = 0L;
  unsigned long contact_attribute_mask = 0L;
  unsigned long contact_host_attribute_mask = 0L;
  unsigned long contact_service_attribute_mask = 0L;
  unsigned long process_host_attribute_mask = 0L;
  unsigned long process_service_attribute_mask = 0L;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "xrddefault_save_state_information()\n");

  /* make sure we have everything */
  if (xrddefault_retention_file == NULL || xrddefault_temp_file == NULL) {
    logit(NSLOG_RUNTIME_ERROR, TRUE,
          "Error: We don't have the required file names to store retention data!\n");
    return (ERROR);
  }

  /* open a safe temp file for output */
  std::ostringstream oss;
  oss << xrddefault_temp_file << "XXXXXX";
  temp_file = my_strdup(oss.str().c_str());
  if ((fd = mkstemp(temp_file)) == -1)
    return (ERROR);

  log_debug_info(DEBUGL_RETENTIONDATA, 2,
                 "Writing retention data to temp file '%s'\n",
                 temp_file);

  fp = (FILE*) fdopen(fd, "w");
  if (fp == NULL) {

    close(fd);
    unlink(temp_file);

    logit(NSLOG_RUNTIME_ERROR, TRUE,
          "Error: Could not open temp state retention file '%s' for writing!\n",
          temp_file);

    delete[] temp_file;

    return (ERROR);
  }

  /* what attributes should be masked out? */
  /* NOTE: host/service/contact-specific values may be added in the future, but for now we only have global masks */
  process_host_attribute_mask = config.get_retained_process_host_attribute_mask();
  process_service_attribute_mask = config.get_retained_process_host_attribute_mask();
  host_attribute_mask = config.get_retained_host_attribute_mask();
  service_attribute_mask = config.get_retained_host_attribute_mask();
  contact_host_attribute_mask = config.get_retained_contact_host_attribute_mask();
  contact_service_attribute_mask = config.get_retained_contact_service_attribute_mask();

  /* write version info to status file */
  fprintf(fp, "##############################################\n");
  fprintf(fp, "#    CENTREON ENGINE STATE RETENTION FILE    #\n");
  fprintf(fp, "#                                            #\n");
  fprintf(fp, "# THIS FILE IS AUTOMATICALLY GENERATED BY    #\n");
  fprintf(fp, "# CENTREON ENGINE. DO NOT MODIFY THIS FILE ! #\n");
  fprintf(fp, "##############################################\n");

  time(&current_time);

  /* write file info */
  fprintf(fp, "info {\n");
  fprintf(fp, "created=%lu\n", static_cast<unsigned long>(current_time));
  fprintf(fp, "}\n");

  /* save program state information */
  fprintf(fp, "program {\n");
  fprintf(fp, "modified_host_attributes=%lu\n", (modified_host_process_attributes & ~process_host_attribute_mask));
  fprintf(fp, "modified_service_attributes=%lu\n", (modified_service_process_attributes & ~process_service_attribute_mask));
  fprintf(fp, "enable_notifications=%d\n", config.get_enable_notifications());
  fprintf(fp, "active_service_checks_enabled=%d\n", config.get_execute_service_checks());
  fprintf(fp, "passive_service_checks_enabled=%d\n", config.get_accept_passive_service_checks());
  fprintf(fp, "active_host_checks_enabled=%d\n", config.get_execute_host_checks());
  fprintf(fp, "passive_host_checks_enabled=%d\n", config.get_accept_passive_host_checks());
  fprintf(fp, "enable_event_handlers=%d\n", config.get_enable_event_handlers());
  fprintf(fp, "obsess_over_services=%d\n", config.get_obsess_over_services());
  fprintf(fp, "obsess_over_hosts=%d\n", config.get_obsess_over_hosts());
  fprintf(fp, "check_service_freshness=%d\n", config.get_check_service_freshness());
  fprintf(fp, "check_host_freshness=%d\n", config.get_check_host_freshness());
  fprintf(fp, "enable_flap_detection=%d\n", config.get_enable_flap_detection());
  fprintf(fp, "enable_failure_prediction=%d\n", config.get_enable_failure_prediction());
  fprintf(fp, "process_performance_data=%d\n", config.get_process_performance_data());
  fprintf(fp, "global_host_event_handler=%s\n", qPrintable(config.get_global_host_event_handler()));
  fprintf(fp, "global_service_event_handler=%s\n", qPrintable(config.get_global_service_event_handler()));
  fprintf(fp, "next_comment_id=%lu\n", next_comment_id);
  fprintf(fp, "next_downtime_id=%lu\n", next_downtime_id);
  fprintf(fp, "next_event_id=%lu\n", next_event_id);
  fprintf(fp, "next_problem_id=%lu\n", next_problem_id);
  fprintf(fp, "next_notification_id=%lu\n", next_notification_id);
  fprintf(fp, "}\n");

  /* save host state information */
  for (temp_host = host_list;
       temp_host != NULL;
       temp_host = temp_host->next) {

    fprintf(fp, "host {\n");
    fprintf(fp, "host_name=%s\n", temp_host->name);
    fprintf(fp, "modified_attributes=%lu\n", (temp_host->modified_attributes & ~host_attribute_mask));
    fprintf(fp, "check_command=%s\n", (temp_host->host_check_command == NULL) ? "" : temp_host->host_check_command);
    fprintf(fp, "check_period=%s\n", (temp_host->check_period == NULL) ? "" : temp_host->check_period);
    fprintf(fp, "notification_period=%s\n", (temp_host->notification_period == NULL) ? "" : temp_host->notification_period);
    fprintf(fp, "event_handler=%s\n", (temp_host->event_handler == NULL) ? "" : temp_host->event_handler);
    fprintf(fp, "has_been_checked=%d\n", temp_host->has_been_checked);
    fprintf(fp, "check_execution_time=%.3f\n", temp_host->execution_time);
    fprintf(fp, "check_latency=%.3f\n", temp_host->latency);
    fprintf(fp, "check_type=%d\n", temp_host->check_type);
    fprintf(fp, "current_state=%d\n", temp_host->current_state);
    fprintf(fp, "last_state=%d\n", temp_host->last_state);
    fprintf(fp, "last_hard_state=%d\n", temp_host->last_hard_state);
    fprintf(fp, "last_event_id=%lu\n", temp_host->last_event_id);
    fprintf(fp, "current_event_id=%lu\n", temp_host->current_event_id);
    fprintf(fp, "current_problem_id=%lu\n", temp_host->current_problem_id);
    fprintf(fp, "last_problem_id=%lu\n", temp_host->last_problem_id);
    fprintf(fp, "plugin_output=%s\n", (temp_host->plugin_output == NULL) ? "" : temp_host->plugin_output);
    fprintf(fp, "long_plugin_output=%s\n", (temp_host->long_plugin_output == NULL) ? "" : temp_host->long_plugin_output);
    fprintf(fp, "performance_data=%s\n", (temp_host->perf_data == NULL) ? "" : temp_host->perf_data);
    fprintf(fp, "last_check=%lu\n", static_cast<unsigned long>(temp_host->last_check));
    fprintf(fp, "next_check=%lu\n", static_cast<unsigned long>(temp_host->next_check));
    fprintf(fp, "check_options=%d\n", temp_host->check_options);
    fprintf(fp, "current_attempt=%d\n", temp_host->current_attempt);
    fprintf(fp, "max_attempts=%d\n", temp_host->max_attempts);
    fprintf(fp, "normal_check_interval=%f\n", temp_host->check_interval);
    fprintf(fp, "retry_check_interval=%f\n", temp_host->check_interval);
    fprintf(fp, "state_type=%d\n", temp_host->state_type);
    fprintf(fp, "last_state_change=%lu\n", static_cast<unsigned long>(temp_host->last_state_change));
    fprintf(fp, "last_hard_state_change=%lu\n", static_cast<unsigned long>(temp_host->last_hard_state_change));
    fprintf(fp, "last_time_up=%lu\n", static_cast<unsigned long>(temp_host->last_time_up));
    fprintf(fp, "last_time_down=%lu\n", static_cast<unsigned long>(temp_host->last_time_down));
    fprintf(fp, "last_time_unreachable=%lu\n", static_cast<unsigned long>(temp_host->last_time_unreachable));
    fprintf(fp, "notified_on_down=%d\n", temp_host->notified_on_down);
    fprintf(fp, "notified_on_unreachable=%d\n", temp_host->notified_on_unreachable);
    fprintf(fp, "last_notification=%lu\n", static_cast<unsigned long>(temp_host->last_host_notification));
    fprintf(fp, "current_notification_number=%d\n", temp_host->current_notification_number);
    fprintf(fp, "current_notification_id=%lu\n", temp_host->current_notification_id);
    fprintf(fp, "notifications_enabled=%d\n", temp_host->notifications_enabled);
    fprintf(fp, "problem_has_been_acknowledged=%d\n", temp_host->problem_has_been_acknowledged);
    fprintf(fp, "acknowledgement_type=%d\n", temp_host->acknowledgement_type);
    fprintf(fp, "active_checks_enabled=%d\n", temp_host->checks_enabled);
    fprintf(fp, "passive_checks_enabled=%d\n", temp_host->accept_passive_host_checks);
    fprintf(fp, "event_handler_enabled=%d\n", temp_host->event_handler_enabled);
    fprintf(fp, "flap_detection_enabled=%d\n", temp_host->flap_detection_enabled);
    fprintf(fp, "failure_prediction_enabled=%d\n", temp_host->failure_prediction_enabled);
    fprintf(fp, "process_performance_data=%d\n", temp_host->process_performance_data);
    fprintf(fp, "obsess_over_host=%d\n", temp_host->obsess_over_host);
    fprintf(fp, "is_flapping=%d\n", temp_host->is_flapping);
    fprintf(fp, "percent_state_change=%.2f\n", temp_host->percent_state_change);
    fprintf(fp, "check_flapping_recovery_notification=%d\n", temp_host->check_flapping_recovery_notification);
    fprintf(fp, "state_history=");
    for (x = 0; x < MAX_STATE_HISTORY_ENTRIES; x++)
      fprintf(fp, "%s%d", (x > 0) ? "," : "", temp_host->state_history[(x + temp_host->state_history_index) % MAX_STATE_HISTORY_ENTRIES]);
    fprintf(fp, "\n");

    /* custom variables */
    for (temp_customvariablesmember = temp_host->custom_variables;
         temp_customvariablesmember != NULL;
         temp_customvariablesmember = temp_customvariablesmember->next) {
      if (temp_customvariablesmember->variable_name)
        fprintf(fp, "_%s=%d;%s\n",
                temp_customvariablesmember->variable_name,
                temp_customvariablesmember->has_been_modified,
                (temp_customvariablesmember->variable_value == NULL) ? "" : temp_customvariablesmember->variable_value);
    }

    fprintf(fp, "}\n");
  }

  /* save service state information */
  for (temp_service = service_list;
       temp_service != NULL;
       temp_service = temp_service->next) {

    fprintf(fp, "service {\n");
    fprintf(fp, "host_name=%s\n", temp_service->host_name);
    fprintf(fp, "service_description=%s\n", temp_service->description);
    fprintf(fp, "modified_attributes=%lu\n", (temp_service->modified_attributes & ~service_attribute_mask));
    fprintf(fp, "check_command=%s\n", (temp_service->service_check_command == NULL) ? "" : temp_service->service_check_command);
    fprintf(fp, "check_period=%s\n", (temp_service->check_period == NULL) ? "" : temp_service->check_period);
    fprintf(fp, "notification_period=%s\n", (temp_service->notification_period == NULL) ? "" : temp_service->notification_period);
    fprintf(fp, "event_handler=%s\n", (temp_service->event_handler == NULL) ? "" : temp_service->event_handler);
    fprintf(fp, "has_been_checked=%d\n", temp_service->has_been_checked);
    fprintf(fp, "check_execution_time=%.3f\n", temp_service->execution_time);
    fprintf(fp, "check_latency=%.3f\n", temp_service->latency);
    fprintf(fp, "check_type=%d\n", temp_service->check_type);
    fprintf(fp, "current_state=%d\n", temp_service->current_state);
    fprintf(fp, "last_state=%d\n", temp_service->last_state);
    fprintf(fp, "last_hard_state=%d\n", temp_service->last_hard_state);
    fprintf(fp, "last_event_id=%lu\n", temp_service->last_event_id);
    fprintf(fp, "current_event_id=%lu\n", temp_service->current_event_id);
    fprintf(fp, "current_problem_id=%lu\n", temp_service->current_problem_id);
    fprintf(fp, "last_problem_id=%lu\n", temp_service->last_problem_id);
    fprintf(fp, "current_attempt=%d\n", temp_service->current_attempt);
    fprintf(fp, "max_attempts=%d\n", temp_service->max_attempts);
    fprintf(fp, "normal_check_interval=%f\n", temp_service->check_interval);
    fprintf(fp, "retry_check_interval=%f\n", temp_service->retry_interval);
    fprintf(fp, "state_type=%d\n", temp_service->state_type);
    fprintf(fp, "last_state_change=%lu\n", static_cast<unsigned long>(temp_service->last_state_change));
    fprintf(fp, "last_hard_state_change=%lu\n", static_cast<unsigned long>(temp_service->last_hard_state_change));
    fprintf(fp, "last_time_ok=%lu\n", static_cast<unsigned long>(temp_service->last_time_ok));
    fprintf(fp, "last_time_warning=%lu\n", static_cast<unsigned long>(temp_service->last_time_warning));
    fprintf(fp, "last_time_unknown=%lu\n", static_cast<unsigned long>(temp_service->last_time_unknown));
    fprintf(fp, "last_time_critical=%lu\n", static_cast<unsigned long>(temp_service->last_time_critical));
    fprintf(fp, "plugin_output=%s\n", (temp_service->plugin_output == NULL) ? "" : temp_service->plugin_output);
    fprintf(fp, "long_plugin_output=%s\n", (temp_service->long_plugin_output == NULL) ? "" : temp_service->long_plugin_output);
    fprintf(fp, "performance_data=%s\n", (temp_service->perf_data == NULL) ? "" : temp_service->perf_data);
    fprintf(fp, "last_check=%lu\n", static_cast<unsigned long>(temp_service->last_check));
    fprintf(fp, "next_check=%lu\n", static_cast<unsigned long>(temp_service->next_check));
    fprintf(fp, "check_options=%d\n", temp_service->check_options);
    fprintf(fp, "notified_on_unknown=%d\n", temp_service->notified_on_unknown);
    fprintf(fp, "notified_on_warning=%d\n", temp_service->notified_on_warning);
    fprintf(fp, "notified_on_critical=%d\n", temp_service->notified_on_critical);
    fprintf(fp, "current_notification_number=%d\n", temp_service->current_notification_number);
    fprintf(fp, "current_notification_id=%lu\n", temp_service->current_notification_id);
    fprintf(fp, "last_notification=%lu\n", static_cast<unsigned long>(temp_service->last_notification));
    fprintf(fp, "notifications_enabled=%d\n", temp_service->notifications_enabled);
    fprintf(fp, "active_checks_enabled=%d\n", temp_service->checks_enabled);
    fprintf(fp, "passive_checks_enabled=%d\n", temp_service->accept_passive_service_checks);
    fprintf(fp, "event_handler_enabled=%d\n", temp_service->event_handler_enabled);
    fprintf(fp, "problem_has_been_acknowledged=%d\n", temp_service->problem_has_been_acknowledged);
    fprintf(fp, "acknowledgement_type=%d\n", temp_service->acknowledgement_type);
    fprintf(fp, "flap_detection_enabled=%d\n", temp_service->flap_detection_enabled);
    fprintf(fp, "failure_prediction_enabled=%d\n", temp_service->failure_prediction_enabled);
    fprintf(fp, "process_performance_data=%d\n", temp_service->process_performance_data);
    fprintf(fp, "obsess_over_service=%d\n", temp_service->obsess_over_service);
    fprintf(fp, "is_flapping=%d\n", temp_service->is_flapping);
    fprintf(fp, "percent_state_change=%.2f\n", temp_service->percent_state_change);
    fprintf(fp, "check_flapping_recovery_notification=%d\n", temp_service->check_flapping_recovery_notification);

    fprintf(fp, "state_history=");
    for (x = 0; x < MAX_STATE_HISTORY_ENTRIES; x++)
      fprintf(fp, "%s%d", (x > 0) ? "," : "", temp_service->state_history[(x + temp_service->state_history_index) % MAX_STATE_HISTORY_ENTRIES]);
    fprintf(fp, "\n");

    /* custom variables */
    for (temp_customvariablesmember = temp_service->custom_variables;
         temp_customvariablesmember != NULL;
         temp_customvariablesmember = temp_customvariablesmember->next) {
      if (temp_customvariablesmember->variable_name)
        fprintf(fp, "_%s=%d;%s\n",
                temp_customvariablesmember->variable_name,
                temp_customvariablesmember->has_been_modified,
                (temp_customvariablesmember->variable_value == NULL) ? "" : temp_customvariablesmember->variable_value);
    }

    fprintf(fp, "}\n");
  }

  /* save contact state information */
  for (temp_contact = contact_list;
       temp_contact != NULL;
       temp_contact = temp_contact->next) {

    fprintf(fp, "contact {\n");
    fprintf(fp, "contact_name=%s\n", temp_contact->name);
    fprintf(fp, "modified_attributes=%lu\n", (temp_contact->modified_attributes & ~contact_attribute_mask));
    fprintf(fp, "modified_host_attributes=%lu\n", (temp_contact->modified_host_attributes & ~contact_host_attribute_mask));
    fprintf(fp, "modified_service_attributes=%lu\n", (temp_contact->modified_service_attributes & ~contact_service_attribute_mask));
    fprintf(fp, "host_notification_period=%s\n", (temp_contact->host_notification_period == NULL) ? "" : temp_contact->host_notification_period);
    fprintf(fp, "service_notification_period=%s\n", (temp_contact->service_notification_period == NULL) ? "" : temp_contact->service_notification_period);
    fprintf(fp, "last_host_notification=%lu\n", static_cast<unsigned long>(temp_contact->last_host_notification));
    fprintf(fp, "last_service_notification=%lu\n", static_cast<unsigned long>(temp_contact->last_service_notification));
    fprintf(fp, "host_notifications_enabled=%d\n", temp_contact->host_notifications_enabled);
    fprintf(fp, "service_notifications_enabled=%d\n", temp_contact->service_notifications_enabled);

    /* custom variables */
    for (temp_customvariablesmember = temp_contact->custom_variables;
         temp_customvariablesmember != NULL;
         temp_customvariablesmember = temp_customvariablesmember->next) {
      if (temp_customvariablesmember->variable_name)
        fprintf(fp, "_%s=%d;%s\n",
                temp_customvariablesmember->variable_name,
                temp_customvariablesmember->has_been_modified,
                (temp_customvariablesmember->variable_value == NULL) ? "" : temp_customvariablesmember->variable_value);
    }

    fprintf(fp, "}\n");
  }

  /* save all comments */
  for (temp_comment = comment_list;
       temp_comment != NULL;
       temp_comment = temp_comment->next) {

    if (temp_comment->comment_type == HOST_COMMENT)
      fprintf(fp, "hostcomment {\n");
    else
      fprintf(fp, "servicecomment {\n");
    fprintf(fp, "host_name=%s\n", temp_comment->host_name);
    if (temp_comment->comment_type == SERVICE_COMMENT)
      fprintf(fp, "service_description=%s\n", temp_comment->service_description);
    fprintf(fp, "entry_type=%d\n", temp_comment->entry_type);
    fprintf(fp, "comment_id=%lu\n", temp_comment->comment_id);
    fprintf(fp, "source=%d\n", temp_comment->source);
    fprintf(fp, "persistent=%d\n", temp_comment->persistent);
    fprintf(fp, "entry_time=%lu\n", static_cast<unsigned long>(temp_comment->entry_time));
    fprintf(fp, "expires=%d\n", temp_comment->expires);
    fprintf(fp, "expire_time=%lu\n", static_cast<unsigned long>(temp_comment->expire_time));
    fprintf(fp, "author=%s\n", temp_comment->author);
    fprintf(fp, "comment_data=%s\n", temp_comment->comment_data);
    fprintf(fp, "}\n");
  }

  /* save all downtime */
  for (temp_downtime = scheduled_downtime_list;
       temp_downtime != NULL;
       temp_downtime = temp_downtime->next) {

    if (temp_downtime->type == HOST_DOWNTIME)
      fprintf(fp, "hostdowntime {\n");
    else
      fprintf(fp, "servicedowntime {\n");
    fprintf(fp, "host_name=%s\n", temp_downtime->host_name);
    if (temp_downtime->type == SERVICE_DOWNTIME)
      fprintf(fp, "service_description=%s\n", temp_downtime->service_description);
    fprintf(fp, "downtime_id=%lu\n", temp_downtime->downtime_id);
    fprintf(fp, "entry_time=%lu\n", static_cast<unsigned long>(temp_downtime->entry_time));
    fprintf(fp, "start_time=%lu\n", static_cast<unsigned long>(temp_downtime->start_time));
    fprintf(fp, "end_time=%lu\n", static_cast<unsigned long>(temp_downtime->end_time));
    fprintf(fp, "triggered_by=%lu\n", temp_downtime->triggered_by);
    fprintf(fp, "fixed=%d\n", temp_downtime->fixed);
    fprintf(fp, "duration=%lu\n", temp_downtime->duration);
    fprintf(fp, "author=%s\n", temp_downtime->author);
    fprintf(fp, "comment=%s\n", temp_downtime->comment);
    fprintf(fp, "}\n");
  }

  fflush(fp);
  result = fclose(fp);
  fsync(fd);

  /* save/close was successful */
  if (result == 0) {
    result = OK;

    /* move the temp file to the retention file (overwrite the old retention file) */
    if (my_rename(temp_file, xrddefault_retention_file)) {
      unlink(temp_file);
      logit(NSLOG_RUNTIME_ERROR, TRUE,
            "Error: Unable to update retention file '%s': %s",
            xrddefault_retention_file, strerror(errno));
      result = ERROR;
    }
  }
  /* a problem occurred saving the file */
  else {
    result = ERROR;

    /* remove temp file and log an error */
    unlink(temp_file);
    logit(NSLOG_RUNTIME_ERROR, TRUE,
          "Error: Unable to save retention file: %s", strerror(errno));
  }

  /* free memory */
  delete[] temp_file;

  return (result);
}

/******************************************************************/
/***************** DEFAULT STATE INPUT FUNCTION *******************/
/******************************************************************/

int xrddefault_read_state_information(void) {
  char* input = NULL;
  char* inputbuf = NULL;
  char* temp_ptr = NULL;
  mmapfile* thefile;
  char* host_name = NULL;
  char* service_description = NULL;
  char* contact_name = NULL;
  char* author = NULL;
  char* comment_data = NULL;
  unsigned int data_type = XRDDEFAULT_NO_DATA;
  unsigned int x = 0;
  host* temp_host = NULL;
  service* temp_service = NULL;
  contact* temp_contact = NULL;
  command* temp_command = NULL;
  timeperiod* temp_timeperiod = NULL;
  customvariablesmember* temp_customvariablesmember = NULL;
  char* customvarname = NULL;
  char* var = NULL;
  char* val = NULL;
  char* tempval = NULL;
  char* ch = NULL;
  unsigned long comment_id = 0;
  int persistent = FALSE;
  int expires = FALSE;
  time_t expire_time = 0L;
  unsigned int entry_type = USER_COMMENT;
  int source = COMMENTSOURCE_INTERNAL;
  time_t entry_time = 0L;
  time_t creation_time;
  time_t current_time;
  int scheduling_info_is_ok = FALSE;
  unsigned long downtime_id = 0;
  time_t start_time = 0L;
  time_t end_time = 0L;
  int fixed = FALSE;
  unsigned long triggered_by = 0;
  unsigned long duration = 0L;
  unsigned long host_attribute_mask = 0L;
  unsigned long service_attribute_mask = 0L;
  unsigned long contact_attribute_mask = 0L;
  unsigned long contact_host_attribute_mask = 0L;
  unsigned long contact_service_attribute_mask = 0L;
  unsigned long process_host_attribute_mask = 0L;
  unsigned long process_service_attribute_mask = 0L;
  int remove_comment = FALSE;
  int ack = FALSE;
  int was_flapping = FALSE;
  int allow_flapstart_notification = TRUE;
  struct timeval tv[2];
  double runtime[2];
  int found_directive = FALSE;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "xrddefault_read_state_information() start\n");

  /* make sure we have what we need */
  if (xrddefault_retention_file == NULL) {
    logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: We don't have a filename for retention data!\n");
    return (ERROR);
  }

  if (test_scheduling == TRUE)
    gettimeofday(&tv[0], NULL);

  /* open the retention file for reading */
  if ((thefile = mmap_fopen(xrddefault_retention_file)) == NULL)
    return (ERROR);

  /* what attributes should be masked out? */
  /* NOTE: host/service/contact-specific values may be added in the future, but for now we only have global masks */
  process_host_attribute_mask = config.get_retained_process_host_attribute_mask();
  process_service_attribute_mask = config.get_retained_process_host_attribute_mask();
  host_attribute_mask = config.get_retained_host_attribute_mask();
  service_attribute_mask = config.get_retained_host_attribute_mask();
  contact_host_attribute_mask = config.get_retained_contact_host_attribute_mask();
  contact_service_attribute_mask = config.get_retained_contact_service_attribute_mask();

  /* Big speedup when reading retention.dat in bulk */
  defer_downtime_sorting = 1;
  defer_comment_sorting = 1;

  /* read all lines in the retention file */
  while (1) {
    /* free memory */
    delete[] inputbuf;

    /* read the next line */
    if ((inputbuf = mmap_fgets(thefile)) == NULL)
      break;

    input = inputbuf;

    /* far better than strip()ing */
    if (input[0] == '\t')
      input++;

    strip(input);

    if (!strcmp(input, "service {"))
      data_type = XRDDEFAULT_SERVICESTATUS_DATA;
    else if (!strcmp(input, "host {"))
      data_type = XRDDEFAULT_HOSTSTATUS_DATA;
    else if (!strcmp(input, "contact {"))
      data_type = XRDDEFAULT_CONTACTSTATUS_DATA;
    else if (!strcmp(input, "hostcomment {"))
      data_type = XRDDEFAULT_HOSTCOMMENT_DATA;
    else if (!strcmp(input, "servicecomment {"))
      data_type = XRDDEFAULT_SERVICECOMMENT_DATA;
    else if (!strcmp(input, "hostdowntime {"))
      data_type = XRDDEFAULT_HOSTDOWNTIME_DATA;
    else if (!strcmp(input, "servicedowntime {"))
      data_type = XRDDEFAULT_SERVICEDOWNTIME_DATA;
    else if (!strcmp(input, "info {"))
      data_type = XRDDEFAULT_INFO_DATA;
    else if (!strcmp(input, "program {"))
      data_type = XRDDEFAULT_PROGRAMSTATUS_DATA;
    else if (!strcmp(input, "}")) {
      switch (data_type) {
      case XRDDEFAULT_INFO_DATA:
        break;

      case XRDDEFAULT_PROGRAMSTATUS_DATA:
        /* adjust modified attributes if necessary */
        if (config.get_use_retained_program_state() == false) {
          modified_host_process_attributes = MODATTR_NONE;
          modified_service_process_attributes = MODATTR_NONE;
        }
        break;

      case XRDDEFAULT_HOSTSTATUS_DATA:
        if (temp_host != NULL) {
          /* adjust modified attributes if necessary */
          if (temp_host->retain_nonstatus_information == FALSE)
            temp_host->modified_attributes = MODATTR_NONE;

          /* adjust modified attributes if no custom variables have been changed */
          if (temp_host->modified_attributes & MODATTR_CUSTOM_VARIABLE) {
            for (temp_customvariablesmember = temp_host->custom_variables;
                 temp_customvariablesmember != NULL;
                 temp_customvariablesmember = temp_customvariablesmember->next) {
              if (temp_customvariablesmember->has_been_modified == TRUE)
                break;
            }
            if (temp_customvariablesmember == NULL)
              temp_host->modified_attributes -= MODATTR_CUSTOM_VARIABLE;
          }

          /* calculate next possible notification time */
          if (temp_host->current_state != HOST_UP
              && temp_host->last_host_notification != (time_t)0)
            temp_host->next_host_notification = get_next_host_notification_time(temp_host, temp_host->last_host_notification);

          /* ADDED 01/23/2009 adjust current check attempts if host in hard problem state (max attempts may have changed in config since restart) */
          if (temp_host->current_state != HOST_UP
              && temp_host->state_type == HARD_STATE)
            temp_host->current_attempt = temp_host->max_attempts;

          /* ADDED 02/20/08 assume same flapping state if large install tweaks enabled */
          if (config.get_use_large_installation_tweaks() == true)
            temp_host->is_flapping = was_flapping;
          /* else use normal startup flap detection logic */
          else {
            /* host was flapping before program started */
            /* 11/10/07 don't allow flapping notifications to go out */
            if (was_flapping == TRUE)
              allow_flapstart_notification = FALSE;
            else
              /* flapstart notifications are okay */
              allow_flapstart_notification = TRUE;

            /* check for flapping */
            check_for_host_flapping(temp_host, FALSE, FALSE,
				    allow_flapstart_notification);

            /* host was flapping before and isn't now, so clear recovery check variable if host isn't flapping now */
            if (was_flapping == TRUE && temp_host->is_flapping == FALSE)
              temp_host->check_flapping_recovery_notification = FALSE;
          }

          /* handle new vars added in 2.x */
          if (temp_host->last_hard_state_change == (time_t)0)
            temp_host->last_hard_state_change = temp_host->last_state_change;

          /* update host status */
          update_host_status(temp_host, FALSE);
        }

        /* reset vars */
        was_flapping = FALSE;
        allow_flapstart_notification = TRUE;

        delete[] host_name;
        host_name = NULL;
        temp_host = NULL;
        break;

      case XRDDEFAULT_SERVICESTATUS_DATA:
        if (temp_service != NULL) {
          /* adjust modified attributes if necessary */
          if (temp_service->retain_nonstatus_information == FALSE)
            temp_service->modified_attributes = MODATTR_NONE;

          /* adjust modified attributes if no custom variables have been changed */
          if (temp_service->modified_attributes & MODATTR_CUSTOM_VARIABLE) {
            for (temp_customvariablesmember = temp_service->custom_variables;
                 temp_customvariablesmember != NULL;
                 temp_customvariablesmember = temp_customvariablesmember->next) {
              if (temp_customvariablesmember->has_been_modified == TRUE)
                break;
            }
            if (temp_customvariablesmember == NULL)
              temp_service->modified_attributes -= MODATTR_CUSTOM_VARIABLE;
          }

          /* calculate next possible notification time */
          if (temp_service->current_state != STATE_OK
              && temp_service->last_notification != (time_t)0)
            temp_service->next_notification = get_next_service_notification_time(temp_service,
										 temp_service->last_notification);

          /* fix old vars */
          if (temp_service->has_been_checked == FALSE
              && temp_service->state_type == SOFT_STATE)
            temp_service->state_type = HARD_STATE;

          /* ADDED 01/23/2009 adjust current check attempt if service is in hard problem state (max attempts may have changed in config since restart) */
          if (temp_service->current_state != STATE_OK
              && temp_service->state_type == HARD_STATE)
            temp_service->current_attempt = temp_service->max_attempts;


          /* ADDED 02/20/08 assume same flapping state if large install tweaks enabled */
          if (config.get_use_large_installation_tweaks() == true)
            temp_service->is_flapping = was_flapping;
          /* else use normal startup flap detection logic */
          else {
            /* service was flapping before program started */
            /* 11/10/07 don't allow flapping notifications to go out */
            if (was_flapping == TRUE)
              allow_flapstart_notification = FALSE;
            else
              /* flapstart notifications are okay */
              allow_flapstart_notification = TRUE;

            /* check for flapping */
            check_for_service_flapping(temp_service, FALSE,
				       allow_flapstart_notification);

            /* service was flapping before and isn't now, so clear recovery check variable if service isn't flapping now */
            if (was_flapping == TRUE
                && temp_service->is_flapping == FALSE)
              temp_service->check_flapping_recovery_notification = FALSE;
          }

          /* handle new vars added in 2.x */
          if (temp_service->last_hard_state_change == (time_t)0)
            temp_service->last_hard_state_change = temp_service->last_state_change;

          /* update service status */
          update_service_status(temp_service, FALSE);
        }

        /* reset vars */
        was_flapping = FALSE;
        allow_flapstart_notification = TRUE;

        delete[] host_name;
        delete[] service_description;
        host_name = NULL;
        service_description = NULL;
        temp_service = NULL;
        break;

      case XRDDEFAULT_CONTACTSTATUS_DATA:
        if (temp_contact != NULL) {
          /* adjust modified attributes if necessary */
          if (temp_contact->retain_nonstatus_information == FALSE)
            temp_contact->modified_attributes = MODATTR_NONE;

          /* adjust modified attributes if no custom variables have been changed */
          if (temp_contact->modified_attributes & MODATTR_CUSTOM_VARIABLE) {
            for (temp_customvariablesmember = temp_contact->custom_variables;
                 temp_customvariablesmember != NULL;
                 temp_customvariablesmember = temp_customvariablesmember->next) {
              if (temp_customvariablesmember->has_been_modified == TRUE)
                break;
            }
            if (temp_customvariablesmember == NULL)
              temp_contact->modified_attributes -= MODATTR_CUSTOM_VARIABLE;
          }

          /* update contact status */
          update_contact_status(temp_contact, FALSE);
        }

        delete[] contact_name;
        contact_name = NULL;
        temp_contact = NULL;
        break;

      case XRDDEFAULT_HOSTCOMMENT_DATA:
      case XRDDEFAULT_SERVICECOMMENT_DATA:
        /* add the comment */
        add_comment((data_type == XRDDEFAULT_HOSTCOMMENT_DATA) ? HOST_COMMENT : SERVICE_COMMENT,
		    entry_type,
		    host_name,
		    service_description,
		    entry_time,
		    author,
                    comment_data,
		    comment_id,
		    persistent,
		    expires,
		    expire_time,
		    source);

        /* delete the comment if necessary */
        /* it seems a bit backwards to add and then immediately delete the comment, but its necessary to track comment deletions in the event broker */
        remove_comment = FALSE;
        /* host no longer exists */
        if ((temp_host = find_host(host_name)) == NULL)
          remove_comment = TRUE;
        /* service no longer exists */
        else if (data_type == XRDDEFAULT_SERVICECOMMENT_DATA
                 && (temp_service = find_service(host_name, service_description)) == NULL)
          remove_comment = TRUE;
        /* acknowledgement comments get deleted if they're not persistent and the original problem is no longer acknowledged */
        else if (entry_type == ACKNOWLEDGEMENT_COMMENT) {
          ack = FALSE;
          if (data_type == XRDDEFAULT_HOSTCOMMENT_DATA)
            ack = temp_host->problem_has_been_acknowledged;
          else
            ack = temp_service->problem_has_been_acknowledged;
          if (ack == FALSE && persistent == FALSE)
            remove_comment = TRUE;
        }
        /* non-persistent comments don't last past restarts UNLESS they're acks (see above) */
        else if (persistent == FALSE)
          remove_comment = TRUE;

        if (remove_comment == TRUE)
          delete_comment((data_type == XRDDEFAULT_HOSTCOMMENT_DATA) ? HOST_COMMENT : SERVICE_COMMENT, comment_id);

        /* free temp memory */
        delete[] host_name;
        delete[] service_description;
        delete[] author;
        delete[] comment_data;

        host_name = NULL;
        service_description = NULL;
        author = NULL;
        comment_data = NULL;

        /* reset defaults */
        entry_type = USER_COMMENT;
        comment_id = 0;
        source = COMMENTSOURCE_INTERNAL;
        persistent = FALSE;
        entry_time = 0L;
        expires = FALSE;
        expire_time = 0L;
        break;

      case XRDDEFAULT_HOSTDOWNTIME_DATA:
      case XRDDEFAULT_SERVICEDOWNTIME_DATA:
        /* add the downtime */
        if (data_type == XRDDEFAULT_HOSTDOWNTIME_DATA)
          add_host_downtime(host_name,
			    entry_time,
			    author,
			    comment_data,
                            start_time,
			    end_time,
			    fixed,
			    triggered_by,
                            duration,
			    downtime_id);
        else
          add_service_downtime(host_name,
			       service_description,
                               entry_time,
			       author,
			       comment_data,
                               start_time,
			       end_time,
			       fixed,
                               triggered_by,
			       duration,
			       downtime_id);

        /* must register the downtime with Centreon Engine so it can schedule it, add comments, etc. */
        register_downtime((data_type == XRDDEFAULT_HOSTDOWNTIME_DATA)
			  ? HOST_DOWNTIME : SERVICE_DOWNTIME, downtime_id);

        /* free temp memory */
        delete[] host_name;
        delete[] service_description;
        delete[] author;
        delete[] comment_data;

        host_name = NULL;
        service_description = NULL;
        author = NULL;
        comment_data = NULL;

        /* reset defaults */
        downtime_id = 0;
        entry_time = 0L;
        start_time = 0L;
        end_time = 0L;
        fixed = FALSE;
        triggered_by = 0;
        duration = 0L;
        break;

      default:
        break;
      }

      data_type = XRDDEFAULT_NO_DATA;
    }
    else if (data_type != XRDDEFAULT_NO_DATA) {
      /* slightly faster than strtok () */
      var = input;
      if ((val = strchr(input, '=')) == NULL)
        continue;
      val[0] = '\x0';
      val++;

      found_directive = TRUE;

      switch (data_type) {
      case XRDDEFAULT_INFO_DATA:
        if (!strcmp(var, "created")) {
          creation_time = strtoul(val, NULL, 10);
          time(&current_time);
          if ((current_time - creation_time)
              < static_cast<time_t>(config.get_retention_scheduling_horizon()))
            scheduling_info_is_ok = TRUE;
          else
            scheduling_info_is_ok = FALSE;
        }
        // Ignore update-related fields.
        else if (!strcmp(var, "version")
                 || !strcmp(var, "update_available")
                 || !strcmp(var, "update_uid")
                 || !strcmp(var, "last_version")
                 || !strcmp(var, "new_version"))
          break;

      case XRDDEFAULT_PROGRAMSTATUS_DATA:
        if (!strcmp(var, "modified_host_attributes")) {
          modified_host_process_attributes = strtoul(val, NULL, 10);

          /* mask out attributes we don't want to retain */
          modified_host_process_attributes &= ~process_host_attribute_mask;
        }
        else if (!strcmp(var, "modified_service_attributes")) {
          modified_service_process_attributes = strtoul(val, NULL, 10);

          /* mask out attributes we don't want to retain */
          modified_service_process_attributes &= ~process_service_attribute_mask;
        }
        if (config.get_use_retained_program_state() == true) {
          if (!strcmp(var, "enable_notifications")) {
            if (modified_host_process_attributes & MODATTR_NOTIFICATIONS_ENABLED)
              config.set_enable_notifications((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "active_service_checks_enabled")) {
            if (modified_service_process_attributes & MODATTR_ACTIVE_CHECKS_ENABLED)
              config.set_execute_service_checks((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "passive_service_checks_enabled")) {
            if (modified_service_process_attributes & MODATTR_PASSIVE_CHECKS_ENABLED)
              config.set_accept_passive_service_checks((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "active_host_checks_enabled")) {
            if (modified_host_process_attributes & MODATTR_ACTIVE_CHECKS_ENABLED)
              config.set_execute_host_checks((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "passive_host_checks_enabled")) {
            if (modified_host_process_attributes & MODATTR_PASSIVE_CHECKS_ENABLED)
              config.set_accept_passive_host_checks((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "enable_event_handlers")) {
            if (modified_host_process_attributes & MODATTR_EVENT_HANDLER_ENABLED)
              config.set_enable_event_handlers((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "obsess_over_services")) {
            if (modified_service_process_attributes & MODATTR_OBSESSIVE_HANDLER_ENABLED)
              config.set_obsess_over_services((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "obsess_over_hosts")) {
            if (modified_host_process_attributes & MODATTR_OBSESSIVE_HANDLER_ENABLED)
              config.set_obsess_over_hosts((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "check_service_freshness")) {
            if (modified_service_process_attributes & MODATTR_FRESHNESS_CHECKS_ENABLED)
              config.set_check_service_freshness((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "check_host_freshness")) {
            if (modified_host_process_attributes & MODATTR_FRESHNESS_CHECKS_ENABLED)
              config.set_check_host_freshness((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "enable_flap_detection")) {
            if (modified_host_process_attributes & MODATTR_FLAP_DETECTION_ENABLED)
              config.set_enable_flap_detection((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "enable_failure_prediction")) {
            if (modified_host_process_attributes & MODATTR_FAILURE_PREDICTION_ENABLED)
              config.set_enable_failure_prediction((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "process_performance_data")) {
            if (modified_host_process_attributes & MODATTR_PERFORMANCE_DATA_ENABLED)
              config.set_process_performance_data((atoi(val) > 0) ? TRUE : FALSE);
          }
          else if (!strcmp(var, "global_host_event_handler")) {
            if (modified_host_process_attributes & MODATTR_EVENT_HANDLER_COMMAND) {
              /* make sure the check command still exists... */
              tempval = my_strdup(val);
              temp_ptr = my_strtok(tempval, "!");
              temp_command = find_command(temp_ptr);
              temp_ptr = my_strdup(val);
              delete[] tempval;

              if (temp_command != NULL && temp_ptr != NULL) {
                config.set_global_host_event_handler(temp_ptr);
              }
            }
          }
          else if (!strcmp(var, "global_service_event_handler")) {
            if (modified_service_process_attributes & MODATTR_EVENT_HANDLER_COMMAND) {
              /* make sure the check command still exists... */
              tempval = my_strdup(val);
              temp_ptr = my_strtok(tempval, "!");
              temp_command = find_command(temp_ptr);
              temp_ptr = my_strdup(val);
              delete[] tempval;

              if (temp_command != NULL && temp_ptr != NULL) {
                config.set_global_service_event_handler(temp_ptr);
              }
            }
          }
          else if (!strcmp(var, "next_comment_id"))
            next_comment_id = strtoul(val, NULL, 10);
          else if (!strcmp(var, "next_downtime_id"))
            next_downtime_id = strtoul(val, NULL, 10);
          else if (!strcmp(var, "next_event_id"))
            next_event_id = strtoul(val, NULL, 10);
          else if (!strcmp(var, "next_problem_id"))
            next_problem_id = strtoul(val, NULL, 10);
          else if (!strcmp(var, "next_notification_id"))
            next_notification_id = strtoul(val, NULL, 10);
        }
        break;

      case XRDDEFAULT_HOSTSTATUS_DATA:
        if (temp_host == NULL) {
          if (!strcmp(var, "host_name")) {
            host_name = my_strdup(val);
            temp_host = find_host(host_name);
          }
        }
        else {
          if (!strcmp(var, "modified_attributes")) {
            temp_host->modified_attributes = strtoul(val, NULL, 10);

            /* mask out attributes we don't want to retain */
            temp_host->modified_attributes &= ~host_attribute_mask;

            /* break out */
            break;
          }
          if (temp_host->retain_status_information == TRUE) {
            if (!strcmp(var, "has_been_checked"))
              temp_host->has_been_checked = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "check_execution_time"))
              temp_host->execution_time = strtod(val, NULL);
            else if (!strcmp(var, "check_latency"))
              temp_host->latency = strtod(val, NULL);
            else if (!strcmp(var, "check_type"))
              temp_host->check_type = atoi(val);
            else if (!strcmp(var, "current_state"))
              temp_host->current_state = atoi(val);
            else if (!strcmp(var, "last_state"))
              temp_host->last_state = atoi(val);
            else if (!strcmp(var, "last_hard_state"))
              temp_host->last_hard_state = atoi(val);
            else if (!strcmp(var, "plugin_output")) {
              delete[] temp_host->plugin_output;
              temp_host->plugin_output = my_strdup(val);
            }
            else if (!strcmp(var, "long_plugin_output")) {
              delete[] temp_host->long_plugin_output;
              temp_host->long_plugin_output = my_strdup(val);
            }
            else if (!strcmp(var, "performance_data")) {
              delete[] temp_host->perf_data;
              temp_host->perf_data = my_strdup(val);
            }
            else if (!strcmp(var, "last_check"))
              temp_host->last_check = strtoul(val, NULL, 10);
            else if (!strcmp(var, "next_check")) {
              if (config.get_use_retained_scheduling_info() == true
                  && scheduling_info_is_ok == TRUE)
                temp_host->next_check = strtoul(val, NULL, 10);
            }
            else if (!strcmp(var, "check_options")) {
              if (config.get_use_retained_scheduling_info() == true
                  && scheduling_info_is_ok == TRUE)
                temp_host->check_options = atoi(val);
            }
            else if (!strcmp(var, "current_attempt"))
              temp_host->current_attempt = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "current_event_id"))
              temp_host->current_event_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_event_id"))
              temp_host->last_event_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "current_problem_id"))
              temp_host->current_problem_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_problem_id"))
              temp_host->last_problem_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "state_type"))
              temp_host->state_type = atoi(val);
            else if (!strcmp(var, "last_state_change"))
              temp_host->last_state_change = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_hard_state_change"))
              temp_host->last_hard_state_change = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_time_up"))
              temp_host->last_time_up = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_time_down"))
              temp_host->last_time_down = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_time_unreachable"))
              temp_host->last_time_unreachable = strtoul(val, NULL, 10);
            else if (!strcmp(var, "notified_on_down"))
              temp_host->notified_on_down = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "notified_on_unreachable"))
              temp_host->notified_on_unreachable = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "last_notification"))
              temp_host->last_host_notification = strtoul(val, NULL, 10);
            else if (!strcmp(var, "current_notification_number"))
              temp_host->current_notification_number = atoi(val);
            else if (!strcmp(var, "current_notification_id"))
              temp_host->current_notification_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "is_flapping"))
              was_flapping = atoi(val);
            else if (!strcmp(var, "percent_state_change"))
              temp_host->percent_state_change = strtod(val, NULL);
            else
              if (!strcmp(var, "check_flapping_recovery_notification"))
                temp_host->check_flapping_recovery_notification = atoi(val);
              else if (!strcmp(var, "state_history")) {
                temp_ptr = val;
                for (x = 0; x < MAX_STATE_HISTORY_ENTRIES; x++) {
                  if ((ch = my_strsep(&temp_ptr, ",")) != NULL)
                    temp_host->state_history[x] = atoi(ch);
                  else
                    break;
                }
                temp_host->state_history_index = 0;
              }
              else
                found_directive = FALSE;
          }
          if (temp_host->retain_nonstatus_information == TRUE) {
            /* null-op speeds up logic */
            if (found_directive == TRUE);

            else if (!strcmp(var, "problem_has_been_acknowledged"))
              temp_host->problem_has_been_acknowledged = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "acknowledgement_type"))
              temp_host->acknowledgement_type = atoi(val);
            else if (!strcmp(var, "notifications_enabled")) {
              if (temp_host->modified_attributes & MODATTR_NOTIFICATIONS_ENABLED)
                temp_host->notifications_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "active_checks_enabled")) {
              if (temp_host->modified_attributes & MODATTR_ACTIVE_CHECKS_ENABLED)
                temp_host->checks_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "passive_checks_enabled")) {
              if (temp_host->modified_attributes & MODATTR_PASSIVE_CHECKS_ENABLED)
                temp_host->accept_passive_host_checks = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "event_handler_enabled")) {
              if (temp_host->modified_attributes & MODATTR_EVENT_HANDLER_ENABLED)
                temp_host->event_handler_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "flap_detection_enabled")) {
              if (temp_host->modified_attributes & MODATTR_FLAP_DETECTION_ENABLED)
                temp_host->flap_detection_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "failure_prediction_enabled")) {
              if (temp_host->modified_attributes & MODATTR_FAILURE_PREDICTION_ENABLED)
                temp_host->failure_prediction_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "process_performance_data")) {
              if (temp_host->modified_attributes & MODATTR_PERFORMANCE_DATA_ENABLED)
                temp_host->process_performance_data = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "obsess_over_host")) {
              if (temp_host->modified_attributes & MODATTR_OBSESSIVE_HANDLER_ENABLED)
                temp_host->obsess_over_host = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "check_command")) {
              if (temp_host->modified_attributes & MODATTR_CHECK_COMMAND) {
                /* make sure the check command still exists... */
                tempval = my_strdup(val);
                temp_ptr = my_strtok(tempval, "!");
                temp_command = find_command(temp_ptr);
                temp_ptr = my_strdup(val);
                delete[] tempval;

                if (temp_command != NULL && temp_ptr != NULL) {
                  delete[] temp_host->host_check_command;
                  temp_host->host_check_command = temp_ptr;
                }
                else
                  temp_host->modified_attributes -= MODATTR_CHECK_COMMAND;
              }
            }
            else if (!strcmp(var, "check_period")) {
              if (temp_host->modified_attributes & MODATTR_CHECK_TIMEPERIOD) {
                /* make sure the timeperiod still exists... */
                temp_timeperiod = find_timeperiod(val);
                temp_ptr = my_strdup(val);

                if (temp_timeperiod != NULL && temp_ptr != NULL) {
                  delete[] temp_host->check_period;
                  temp_host->check_period = temp_ptr;
                }
                else
                  temp_host->modified_attributes -= MODATTR_CHECK_TIMEPERIOD;
              }
            }
            else if (!strcmp(var, "notification_period")) {
              if (temp_host->modified_attributes & MODATTR_NOTIFICATION_TIMEPERIOD) {
                /* make sure the timeperiod still exists... */
                temp_timeperiod = find_timeperiod(val);
                temp_ptr = my_strdup(val);

                if (temp_timeperiod != NULL && temp_ptr != NULL) {
                  delete[] temp_host->notification_period;
                  temp_host->notification_period = temp_ptr;
                }
                else
                  temp_host->modified_attributes -= MODATTR_NOTIFICATION_TIMEPERIOD;
              }
            }
            else if (!strcmp(var, "event_handler")) {
              if (temp_host->modified_attributes & MODATTR_EVENT_HANDLER_COMMAND) {
                /* make sure the check command still exists... */
                tempval = my_strdup(val);
                temp_ptr = my_strtok(tempval, "!");
                temp_command = find_command(temp_ptr);
                temp_ptr = my_strdup(val);
                delete[] tempval;

                if (temp_command != NULL && temp_ptr != NULL) {
                  delete[] temp_host->event_handler;
                  temp_host->event_handler = temp_ptr;
                }
                else
                  temp_host->modified_attributes -= MODATTR_EVENT_HANDLER_COMMAND;
              }
            }
            else if (!strcmp(var, "normal_check_interval")) {
              if (temp_host->modified_attributes & MODATTR_NORMAL_CHECK_INTERVAL
                  && strtod(val, NULL) >= 0)
                temp_host->check_interval = strtod(val, NULL);
            }
            else if (!strcmp(var, "retry_check_interval")) {
              if (temp_host->modified_attributes & MODATTR_RETRY_CHECK_INTERVAL
                  && strtod(val, NULL) >= 0)
                temp_host->retry_interval = strtod(val, NULL);
            }
            else if (!strcmp(var, "max_attempts")) {
              if (temp_host->modified_attributes & MODATTR_MAX_CHECK_ATTEMPTS
                  && atoi(val) >= 1) {
                temp_host->max_attempts = atoi(val);

                /* adjust current attempt number if in a hard state */
                if (temp_host->state_type == HARD_STATE
                    && temp_host->current_state != HOST_UP
                    && temp_host->current_attempt > 1)
                  temp_host->current_attempt = temp_host->max_attempts;
              }
            }
            /* custom variables */
            else if (var[0] == '_') {
              if (temp_host->modified_attributes & MODATTR_CUSTOM_VARIABLE) {

                /* get the variable name */
                customvarname = var + 1;

                for (temp_customvariablesmember = temp_host->custom_variables;
                     temp_customvariablesmember != NULL;
                     temp_customvariablesmember = temp_customvariablesmember->next) {
                  if (!strcmp(customvarname, temp_customvariablesmember->variable_name)) {
                    if ((x = atoi(val)) > 0 && strlen(val) > 3) {
                      delete[] temp_customvariablesmember->variable_value;
                      temp_customvariablesmember->variable_value = my_strdup(val + 2);
                      temp_customvariablesmember->has_been_modified = (x > 0) ? TRUE : FALSE;
                    }
                    break;
                  }
                }
              }
            }
          }
        }
        break;

      case XRDDEFAULT_SERVICESTATUS_DATA:
        if (temp_service == NULL) {
          if (!strcmp(var, "host_name")) {
            host_name = my_strdup(val);

            /*temp_service=find_service(host_name,service_description); */

            /* break out */
            break;
          }
          else if (!strcmp(var, "service_description")) {
            service_description = my_strdup(val);
            temp_service = find_service(host_name, service_description);

            /* break out */
            break;
          }
        }
        else {
          if (!strcmp(var, "modified_attributes")) {
            temp_service->modified_attributes = strtoul(val, NULL, 10);

            /* mask out attributes we don't want to retain */
            temp_service->modified_attributes &= ~service_attribute_mask;
          }
          if (temp_service->retain_status_information == TRUE) {
            if (!strcmp(var, "has_been_checked"))
              temp_service->has_been_checked = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "check_execution_time"))
              temp_service->execution_time = strtod(val, NULL);
            else if (!strcmp(var, "check_latency"))
              temp_service->latency = strtod(val, NULL);
            else if (!strcmp(var, "check_type"))
              temp_service->check_type = atoi(val);
            else if (!strcmp(var, "current_state"))
              temp_service->current_state = atoi(val);
            else if (!strcmp(var, "last_state"))
              temp_service->last_state = atoi(val);
            else if (!strcmp(var, "last_hard_state"))
              temp_service->last_hard_state = atoi(val);
            else if (!strcmp(var, "current_attempt"))
              temp_service->current_attempt = atoi(val);
            else if (!strcmp(var, "current_event_id"))
              temp_service->current_event_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_event_id"))
              temp_service->last_event_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "current_problem_id"))
              temp_service->current_problem_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_problem_id"))
              temp_service->last_problem_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "state_type"))
              temp_service->state_type = atoi(val);
            else if (!strcmp(var, "last_state_change"))
              temp_service->last_state_change = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_hard_state_change"))
              temp_service->last_hard_state_change = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_time_ok"))
              temp_service->last_time_ok = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_time_warning"))
              temp_service->last_time_warning = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_time_unknown"))
              temp_service->last_time_unknown = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_time_critical"))
              temp_service->last_time_critical = strtoul(val, NULL, 10);
            else if (!strcmp(var, "plugin_output")) {
              delete[] temp_service->plugin_output;
              temp_service->plugin_output = my_strdup(val);
            }
            else if (!strcmp(var, "long_plugin_output")) {
              delete[] temp_service->long_plugin_output;
              temp_service->long_plugin_output = my_strdup(val);
            }
            else if (!strcmp(var, "performance_data")) {
              delete[] temp_service->perf_data;
              temp_service->perf_data = my_strdup(val);
            }
            else if (!strcmp(var, "last_check"))
              temp_service->last_check = strtoul(val, NULL, 10);
            else if (!strcmp(var, "next_check")) {
              if (config.get_use_retained_scheduling_info() == true
                  && scheduling_info_is_ok == TRUE)
                temp_service->next_check = strtoul(val, NULL, 10);
            }
            else if (!strcmp(var, "check_options")) {
              if (config.get_use_retained_scheduling_info() == true
                  && scheduling_info_is_ok == TRUE)
                temp_service->check_options = atoi(val);
            }
            else if (!strcmp(var, "notified_on_unknown"))
              temp_service->notified_on_unknown = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "notified_on_warning"))
              temp_service->notified_on_warning = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "notified_on_critical"))
              temp_service->notified_on_critical = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "current_notification_number"))
              temp_service->current_notification_number = atoi(val);
            else if (!strcmp(var, "current_notification_id"))
              temp_service->current_notification_id = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_notification"))
              temp_service->last_notification = strtoul(val, NULL, 10);
            else if (!strcmp(var, "is_flapping"))
              was_flapping = atoi(val);
            else if (!strcmp(var, "percent_state_change"))
              temp_service->percent_state_change = strtod(val, NULL);
            else
              if (!strcmp(var, "check_flapping_recovery_notification"))
                temp_service->check_flapping_recovery_notification = atoi(val);
              else if (!strcmp(var, "state_history")) {
                temp_ptr = val;
                for (x = 0; x < MAX_STATE_HISTORY_ENTRIES; x++) {
                  if ((ch = my_strsep(&temp_ptr, ",")) != NULL)
                    temp_service->state_history[x] = atoi(ch);
                  else
                    break;
                }
                temp_service->state_history_index = 0;
              }
              else
                found_directive = FALSE;
          }
          if (temp_service->retain_nonstatus_information == TRUE) {
            /* null-op speeds up logic */
            if (found_directive == TRUE);

            else if (!strcmp(var, "problem_has_been_acknowledged"))
              temp_service->problem_has_been_acknowledged = (atoi(val) > 0) ? TRUE : FALSE;
            else if (!strcmp(var, "acknowledgement_type"))
              temp_service->acknowledgement_type = atoi(val);
            else if (!strcmp(var, "notifications_enabled")) {
              if (temp_service->modified_attributes & MODATTR_NOTIFICATIONS_ENABLED)
                temp_service->notifications_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "active_checks_enabled")) {
              if (temp_service->modified_attributes & MODATTR_ACTIVE_CHECKS_ENABLED)
                temp_service->checks_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "passive_checks_enabled")) {
              if (temp_service->modified_attributes & MODATTR_PASSIVE_CHECKS_ENABLED)
                temp_service->accept_passive_service_checks = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "event_handler_enabled")) {
              if (temp_service->modified_attributes & MODATTR_EVENT_HANDLER_ENABLED)
                temp_service->event_handler_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "flap_detection_enabled")) {
              if (temp_service->modified_attributes & MODATTR_FLAP_DETECTION_ENABLED)
                temp_service->flap_detection_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "failure_prediction_enabled")) {
              if (temp_service->modified_attributes & MODATTR_FAILURE_PREDICTION_ENABLED)
                temp_service->failure_prediction_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "process_performance_data")) {
              if (temp_service->modified_attributes & MODATTR_PERFORMANCE_DATA_ENABLED)
                temp_service->process_performance_data = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "obsess_over_service")) {
              if (temp_service->modified_attributes & MODATTR_OBSESSIVE_HANDLER_ENABLED)
                temp_service->obsess_over_service = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "check_command")) {
              if (temp_service->modified_attributes & MODATTR_CHECK_COMMAND) {
                /* make sure the check command still exists... */
                tempval = my_strdup(val);
                temp_ptr = my_strtok(tempval, "!");
                temp_command = find_command(temp_ptr);
                temp_ptr = my_strdup(val);
                delete[] tempval;

                if (temp_command != NULL && temp_ptr != NULL) {
                  delete[] temp_service->service_check_command;
                  temp_service->service_check_command = temp_ptr;
                }
                else
                  temp_service->modified_attributes -= MODATTR_CHECK_COMMAND;
              }
            }
            else if (!strcmp(var, "check_period")) {
              if (temp_service->modified_attributes & MODATTR_CHECK_TIMEPERIOD) {
                /* make sure the timeperiod still exists... */
                temp_timeperiod = find_timeperiod(val);
                temp_ptr = my_strdup(val);

                if (temp_timeperiod != NULL && temp_ptr != NULL) {
                  delete[] temp_service->check_period;
                  temp_service->check_period = temp_ptr;
                }
                else
                  temp_service->modified_attributes -= MODATTR_CHECK_TIMEPERIOD;
              }
            }
            else if (!strcmp(var, "notification_period")) {
              if (temp_service->modified_attributes & MODATTR_NOTIFICATION_TIMEPERIOD) {
                /* make sure the timeperiod still exists... */
                temp_timeperiod = find_timeperiod(val);
                temp_ptr = my_strdup(val);

                if (temp_timeperiod != NULL && temp_ptr != NULL) {
                  delete[] temp_service->notification_period;
                  temp_service->notification_period = temp_ptr;
                }
                else
                  temp_service->modified_attributes -= MODATTR_NOTIFICATION_TIMEPERIOD;
              }
            }
            else if (!strcmp(var, "event_handler")) {
              if (temp_service->modified_attributes & MODATTR_EVENT_HANDLER_COMMAND) {
                /* make sure the check command still exists... */
                tempval = my_strdup(val);
                temp_ptr = my_strtok(tempval, "!");
                temp_command = find_command(temp_ptr);
                temp_ptr = my_strdup(val);
                delete[] tempval;

                if (temp_command != NULL && temp_ptr != NULL) {
                  delete[] temp_service->event_handler;
                  temp_service->event_handler = temp_ptr;
                }
                else
                  temp_service->modified_attributes -= MODATTR_EVENT_HANDLER_COMMAND;
              }
            }
            else if (!strcmp(var, "normal_check_interval")) {
              if (temp_service->modified_attributes & MODATTR_NORMAL_CHECK_INTERVAL
                  && strtod(val, NULL) >= 0)
                temp_service->check_interval = strtod(val, NULL);
            }
            else if (!strcmp(var, "retry_check_interval")) {
              if (temp_service->modified_attributes & MODATTR_RETRY_CHECK_INTERVAL
                  && strtod(val, NULL) >= 0)
                temp_service->retry_interval = strtod(val, NULL);
            }
            else if (!strcmp(var, "max_attempts")) {
              if (temp_service->modified_attributes & MODATTR_MAX_CHECK_ATTEMPTS
                  && atoi(val) >= 1) {
                temp_service->max_attempts = atoi(val);

                /* adjust current attempt number if in a hard state */
                if (temp_service->state_type == HARD_STATE
                    && temp_service->current_state != STATE_OK
                    && temp_service->current_attempt > 1)
                  temp_service->current_attempt = temp_service->max_attempts;
              }
            }
            /* custom variables */
            else if (var[0] == '_') {
              if (temp_service->modified_attributes & MODATTR_CUSTOM_VARIABLE) {

                /* get the variable name */
                customvarname = var + 1;

                for (temp_customvariablesmember = temp_service->custom_variables;
                     temp_customvariablesmember != NULL;
                     temp_customvariablesmember = temp_customvariablesmember->next) {
                  if (!strcmp(customvarname, temp_customvariablesmember->variable_name)) {
                    if ((x = atoi(val)) > 0 && strlen(val) > 3) {
                      delete[] temp_customvariablesmember->variable_value;
                      temp_customvariablesmember->variable_value = my_strdup(val + 2);
                      temp_customvariablesmember->has_been_modified = (x > 0) ? TRUE : FALSE;
                    }
                    break;
                  }
                }
              }
            }
          }
        }
        break;

      case XRDDEFAULT_CONTACTSTATUS_DATA:
        if (temp_contact == NULL) {
          if (!strcmp(var, "contact_name")) {
            contact_name = my_strdup(val);
            temp_contact = find_contact(contact_name);
          }
        }
        else {
          if (!strcmp(var, "modified_attributes")) {
            temp_contact->modified_attributes = strtoul(val, NULL, 10);

            /* mask out attributes we don't want to retain */
            temp_contact->modified_attributes &= ~contact_attribute_mask;
          }
          else if (!strcmp(var, "modified_host_attributes")) {
            temp_contact->modified_host_attributes = strtoul(val, NULL, 10);

            /* mask out attributes we don't want to retain */
            temp_contact->modified_host_attributes &= ~contact_host_attribute_mask;
          }
          else if (!strcmp(var, "modified_service_attributes")) {
            temp_contact->modified_service_attributes = strtoul(val, NULL, 10);

            /* mask out attributes we don't want to retain */
            temp_contact->modified_service_attributes &= ~contact_service_attribute_mask;
          }
          else if (temp_contact->retain_status_information == TRUE) {
            if (!strcmp(var, "last_host_notification"))
              temp_contact->last_host_notification = strtoul(val, NULL, 10);
            else if (!strcmp(var, "last_service_notification"))
              temp_contact->last_service_notification = strtoul(val, NULL, 10);
            else
              found_directive = FALSE;
          }
          if (temp_contact->retain_nonstatus_information == TRUE) {
            /* null-op speeds up logic */
            if (found_directive == TRUE);

            else if (!strcmp(var, "host_notification_period")) {
              if (temp_contact->modified_host_attributes & MODATTR_NOTIFICATION_TIMEPERIOD) {
                /* make sure the timeperiod still exists... */
                temp_timeperiod = find_timeperiod(val);
                temp_ptr = my_strdup(val);

                if (temp_timeperiod != NULL && temp_ptr != NULL) {
                  delete[] temp_contact->host_notification_period;
                  temp_contact->host_notification_period = temp_ptr;
                }
                else
                  temp_contact->modified_host_attributes -= MODATTR_NOTIFICATION_TIMEPERIOD;
              }
            }
            else if (!strcmp(var, "service_notification_period")) {
              if (temp_contact->modified_service_attributes & MODATTR_NOTIFICATION_TIMEPERIOD) {
                /* make sure the timeperiod still exists... */
                temp_timeperiod = find_timeperiod(val);
                temp_ptr = my_strdup(val);

                if (temp_timeperiod != NULL && temp_ptr != NULL) {
                  delete[] temp_contact->service_notification_period;
                  temp_contact->service_notification_period = temp_ptr;
                }
                else
                  temp_contact->modified_service_attributes -= MODATTR_NOTIFICATION_TIMEPERIOD;
              }
            }
            else if (!strcmp(var, "host_notifications_enabled")) {
              if (temp_contact->modified_host_attributes & MODATTR_NOTIFICATIONS_ENABLED)
                temp_contact->host_notifications_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            else if (!strcmp(var, "service_notifications_enabled")) {
              if (temp_contact->modified_service_attributes & MODATTR_NOTIFICATIONS_ENABLED)
                temp_contact->service_notifications_enabled = (atoi(val) > 0) ? TRUE : FALSE;
            }
            /* custom variables */
            else if (var[0] == '_') {
              if (temp_contact->modified_attributes & MODATTR_CUSTOM_VARIABLE) {
                /* get the variable name */
                customvarname = var + 1;

                for (temp_customvariablesmember = temp_contact->custom_variables;
                     temp_customvariablesmember != NULL;
                     temp_customvariablesmember = temp_customvariablesmember->next) {
                  if (!strcmp(customvarname, temp_customvariablesmember->variable_name)) {
                    if ((x = atoi(val)) > 0 && strlen(val) > 3) {
                      delete[] temp_customvariablesmember->variable_value;
                      temp_customvariablesmember->variable_value = my_strdup(val + 2);
                      temp_customvariablesmember->has_been_modified = (x > 0) ? TRUE : FALSE;
                    }
                    break;
                  }
                }
              }
            }
          }
        }
        break;

      case XRDDEFAULT_HOSTCOMMENT_DATA:
      case XRDDEFAULT_SERVICECOMMENT_DATA:
        if (!strcmp(var, "host_name"))
          host_name = my_strdup(val);
        else if (!strcmp(var, "service_description"))
          service_description = my_strdup(val);
        else if (!strcmp(var, "entry_type"))
          entry_type = atoi(val);
        else if (!strcmp(var, "comment_id"))
          comment_id = strtoul(val, NULL, 10);
        else if (!strcmp(var, "source"))
          source = atoi(val);
        else if (!strcmp(var, "persistent"))
          persistent = (atoi(val) > 0) ? TRUE : FALSE;
        else if (!strcmp(var, "entry_time"))
          entry_time = strtoul(val, NULL, 10);
        else if (!strcmp(var, "expires"))
          expires = (atoi(val) > 0) ? TRUE : FALSE;
        else if (!strcmp(var, "expire_time"))
          expire_time = strtoul(val, NULL, 10);
        else if (!strcmp(var, "author"))
          author = my_strdup(val);
        else if (!strcmp(var, "comment_data"))
          comment_data = my_strdup(val);
            break;

      case XRDDEFAULT_HOSTDOWNTIME_DATA:
      case XRDDEFAULT_SERVICEDOWNTIME_DATA:
        if (!strcmp(var, "host_name"))
          host_name = my_strdup(val);
        else if (!strcmp(var, "service_description"))
          service_description = my_strdup(val);
        else if (!strcmp(var, "downtime_id"))
          downtime_id = strtoul(val, NULL, 10);
        else if (!strcmp(var, "entry_time"))
          entry_time = strtoul(val, NULL, 10);
        else if (!strcmp(var, "start_time"))
          start_time = strtoul(val, NULL, 10);
        else if (!strcmp(var, "end_time"))
          end_time = strtoul(val, NULL, 10);
        else if (!strcmp(var, "fixed"))
          fixed = (atoi(val) > 0) ? TRUE : FALSE;
        else if (!strcmp(var, "triggered_by"))
          triggered_by = strtoul(val, NULL, 10);
        else if (!strcmp(var, "duration"))
          duration = strtoul(val, NULL, 10);
        else if (!strcmp(var, "author"))
          author = my_strdup(val);
        else if (!strcmp(var, "comment"))
          comment_data = my_strdup(val);
            break;

      default:
        break;
      }
    }
  }

  /* free memory and close the file */
  delete[] inputbuf;
  mmap_fclose(thefile);

  if (sort_downtime() != OK)
    return (ERROR);
  if (sort_comments() != OK)
    return (ERROR);

  if (test_scheduling == TRUE)
    gettimeofday(&tv[1], NULL);

  if (test_scheduling == TRUE) {
    runtime[0] = (double)((double)(tv[1].tv_sec - tv[0].tv_sec) +
                          (double)((tv[1].tv_usec - tv[0].tv_usec) / 1000.0) / 1000.0);

    runtime[1] = (double)((double)(tv[1].tv_sec - tv[0].tv_sec) +
                          (double)((tv[1].tv_usec - tv[0].tv_usec) / 1000.0) / 1000.0);

    printf("RETENTION DATA TIMES\n");
    printf("----------------------------------\n");
    printf("Read and Process:     %.6f sec\n", runtime[0]);
    printf("                      ============\n");
    printf("TOTAL:                %.6f sec\n", runtime[1]);
    printf("\n\n");
  }

  return (OK);
}
