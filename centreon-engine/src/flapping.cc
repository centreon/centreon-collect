/*
** Copyright 2001-2009 Ethan Galstad
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
#include <iomanip>
#include "engine.hh"
#include "comments.hh"
#include "globals.hh"
#include "statusdata.hh"
#include "broker.hh"
#include "notifications.hh"
#include "logging/logger.hh"
#include "flapping.hh"

using namespace com::centreon::engine::logging;

/******************************************************************/
/******************** FLAP DETECTION FUNCTIONS ********************/
/******************************************************************/

/* detects service flapping */
void check_for_service_flapping(service* svc,
				int update,
                                int allow_flapstart_notification) {
  int update_history = TRUE;
  int is_flapping = FALSE;
  unsigned int x = 0;
  unsigned int y = 0;
  int last_state_history_value = STATE_OK;
  double curved_changes = 0.0;
  double curved_percent_change = 0.0;
  double low_threshold = 0.0;
  double high_threshold = 0.0;
  double low_curve_value = 0.75;
  double high_curve_value = 1.25;

  /* large install tweaks skips all flap detection logic - including state change calculation */

  logger(dbg_functions, basic) << "check_for_service_flapping()";

  if (svc == NULL)
    return;

  logger(dbg_flapping, more)
    << "Checking service '" << svc->description
    << "' on host '" << svc->host_name << "' for flapping...";

  /* if this is a soft service state and not a soft recovery, don't record this in the history */
  /* only hard states and soft recoveries get recorded for flap detection */
  if (svc->state_type == SOFT_STATE && svc->current_state != STATE_OK)
    return;

  /* what threshold values should we use (global or service-specific)? */
  low_threshold = (svc->low_flap_threshold <= 0.0)
    ? config.get_low_service_flap_threshold() : svc->low_flap_threshold;
  high_threshold = (svc->high_flap_threshold <= 0.0)
    ? config.get_high_service_flap_threshold() : svc->high_flap_threshold;

  update_history = update;

  /* should we update state history for this state? */
  if (update_history == TRUE) {
    if (svc->current_state == STATE_OK
        && svc->flap_detection_on_ok == FALSE)
      update_history = FALSE;
    if (svc->current_state == STATE_WARNING
        && svc->flap_detection_on_warning == FALSE)
      update_history = FALSE;
    if (svc->current_state == STATE_UNKNOWN
        && svc->flap_detection_on_unknown == FALSE)
      update_history = FALSE;
    if (svc->current_state == STATE_CRITICAL
        && svc->flap_detection_on_critical == FALSE)
      update_history = FALSE;
  }

  /* record current service state */
  if (update_history == TRUE) {
    /* record the current state in the state history */
    svc->state_history[svc->state_history_index] = svc->current_state;

    /* increment state history index to next available slot */
    svc->state_history_index++;
    if (svc->state_history_index >= MAX_STATE_HISTORY_ENTRIES)
      svc->state_history_index = 0;
  }

  /* calculate overall and curved percent state changes */
  for (x = 0, y = svc->state_history_index; x < MAX_STATE_HISTORY_ENTRIES; x++) {
    if (x == 0) {
      last_state_history_value = svc->state_history[y];
      y++;
      if (y >= MAX_STATE_HISTORY_ENTRIES)
        y = 0;
      continue;
    }

    if (last_state_history_value != svc->state_history[y])
      curved_changes += (((double)(x - 1)
			  * (high_curve_value - low_curve_value))
			 / ((double)(MAX_STATE_HISTORY_ENTRIES - 2))) + low_curve_value;

    last_state_history_value = svc->state_history[y];

    y++;
    if (y >= MAX_STATE_HISTORY_ENTRIES)
      y = 0;
  }

  /* calculate overall percent change in state */
  curved_percent_change = (double)(((double)curved_changes * 100.0)
				   / (double)(MAX_STATE_HISTORY_ENTRIES - 1));

  svc->percent_state_change = curved_percent_change;

  logger(dbg_flapping, most)
    << fixed << setprecision(2)
    << "LFT=" << low_threshold
    << ", HFT=" << high_threshold
    << ", CPC=" << curved_percent_change
    << ", PSC=" << curved_percent_change << "%";

  /* don't do anything if we don't have flap detection enabled on a program-wide basis */
  if (config.get_enable_flap_detection() == false)
    return;

  /* don't do anything if we don't have flap detection enabled for this service */
  if (svc->flap_detection_enabled == FALSE)
    return;

  /* are we flapping, undecided, or what?... */

  /* we're undecided, so don't change the current flap state */
  if (curved_percent_change > low_threshold
      && curved_percent_change < high_threshold)
    return;
  /* we're below the lower bound, so we're not flapping */
  else if (curved_percent_change <= low_threshold)
    is_flapping = FALSE;
  /* else we're above the upper bound, so we are flapping */
  else if (curved_percent_change >= high_threshold)
    is_flapping = TRUE;

  logger(dbg_flapping, more)
    << fixed << setprecision(2)
    << "Service " << (is_flapping == true ? "is" : "is not")
    << " flapping (" << curved_percent_change << "% state change).";

  /* did the service just start flapping? */
  if (is_flapping == TRUE && svc->is_flapping == FALSE)
    set_service_flap(svc,
		     curved_percent_change,
		     high_threshold,
                     low_threshold,
		     allow_flapstart_notification);

  /* did the service just stop flapping? */
  else if (is_flapping == FALSE && svc->is_flapping == TRUE)
    clear_service_flap(svc,
		       curved_percent_change,
		       high_threshold,
                       low_threshold);
}

/* detects host flapping */
void check_for_host_flapping(host* hst,
			     int update,
			     int actual_check,
                             int allow_flapstart_notification) {
  int update_history = TRUE;
  int is_flapping = FALSE;
  unsigned int x = 0;
  unsigned int y = 0;
  int last_state_history_value = HOST_UP;
  unsigned long wait_threshold = 0L;
  double curved_changes = 0.0;
  double curved_percent_change = 0.0;
  time_t current_time = 0L;
  double low_threshold = 0.0;
  double high_threshold = 0.0;
  double low_curve_value = 0.75;
  double high_curve_value = 1.25;

  logger(dbg_functions, basic) << "check_for_host_flapping()";

  if (hst == NULL)
    return;

  logger(dbg_flapping, more)
    << "Checking host '" << hst->name << "' for flapping...";

  time(&current_time);

  /* period to wait for updating archived state info if we have no state change */
  if (hst->total_services == 0)
    wait_threshold = static_cast<unsigned long>(hst->notification_interval * config.get_interval_length());
  else
    wait_threshold = static_cast<unsigned long>((hst->total_service_check_interval
                                                 * config.get_interval_length()) / hst->total_services);

  update_history = update;

  /* should we update state history for this state? */
  if (update_history == TRUE) {
    if (hst->current_state == HOST_UP && hst->flap_detection_on_up == FALSE)
      update_history = FALSE;
    if (hst->current_state == HOST_DOWN && hst->flap_detection_on_down == FALSE)
      update_history = FALSE;
    if (hst->current_state == HOST_UNREACHABLE && hst->flap_detection_on_unreachable == FALSE)
      update_history = FALSE;
  }

  /* if we didn't have an actual check, only update if we've waited long enough */
  if (update_history == TRUE && actual_check == FALSE
      && static_cast<unsigned long>(current_time - hst->last_state_history_update) < wait_threshold) {
    update_history = FALSE;
  }

  /* what thresholds should we use (global or host-specific)? */
  low_threshold = (hst->low_flap_threshold <= 0.0)
    ? config.get_low_host_flap_threshold() : hst->low_flap_threshold;
  high_threshold = (hst->high_flap_threshold <= 0.0)
    ? config.get_high_host_flap_threshold() : hst->high_flap_threshold;

  /* record current host state */
  if (update_history == TRUE) {
    /* update the last record time */
    hst->last_state_history_update = current_time;

    /* record the current state in the state history */
    hst->state_history[hst->state_history_index] = hst->current_state;

    /* increment state history index to next available slot */
    hst->state_history_index++;
    if (hst->state_history_index >= MAX_STATE_HISTORY_ENTRIES)
      hst->state_history_index = 0;
  }

  /* calculate overall changes in state */
  for (x = 0, y = hst->state_history_index;
       x < MAX_STATE_HISTORY_ENTRIES;
       x++) {

    if (x == 0) {
      last_state_history_value = hst->state_history[y];
      y++;
      if (y >= MAX_STATE_HISTORY_ENTRIES)
        y = 0;
      continue;
    }

    if (last_state_history_value != hst->state_history[y])
      curved_changes += (((double)(x - 1) * (high_curve_value - low_curve_value))
			 / ((double)(MAX_STATE_HISTORY_ENTRIES - 2))) + low_curve_value;

    last_state_history_value = hst->state_history[y];

    y++;
    if (y >= MAX_STATE_HISTORY_ENTRIES)
      y = 0;
  }

  /* calculate overall percent change in state */
  curved_percent_change = (double)(((double)curved_changes * 100.0)
				   / (double)(MAX_STATE_HISTORY_ENTRIES - 1));

  hst->percent_state_change = curved_percent_change;

  logger(dbg_flapping, most)
    << fixed << setprecision(2)
    << "LFT=" << low_threshold
    << ", HFT=" << high_threshold
    << ", CPC=" << curved_percent_change
    << ", PSC=" << curved_percent_change << "%";

  /* don't do anything if we don't have flap detection enabled on a program-wide basis */
  if (config.get_enable_flap_detection() == false)
    return;

  /* don't do anything if we don't have flap detection enabled for this host */
  if (hst->flap_detection_enabled == FALSE)
    return;

  /* are we flapping, undecided, or what?... */

  /* we're undecided, so don't change the current flap state */
  if (curved_percent_change > low_threshold
      && curved_percent_change < high_threshold)
    return;

  /* we're below the lower bound, so we're not flapping */
  else if (curved_percent_change <= low_threshold)
    is_flapping = FALSE;

  /* else we're above the upper bound, so we are flapping */
  else if (curved_percent_change >= high_threshold)
    is_flapping = TRUE;

  logger(dbg_flapping, more)
    << "Host " << (is_flapping == true ? "is" : "is not")
    << " flapping (" << curved_percent_change << "% state change).";

  /* did the host just start flapping? */
  if (is_flapping == TRUE && hst->is_flapping == FALSE)
    set_host_flap(hst,
		  curved_percent_change,
		  high_threshold,
                  low_threshold,
		  allow_flapstart_notification);

  /* did the host just stop flapping? */
  else if (is_flapping == FALSE && hst->is_flapping == TRUE)
    clear_host_flap(hst,
		    curved_percent_change,
		    high_threshold,
                    low_threshold);
}

/******************************************************************/
/********************* FLAP HANDLING FUNCTIONS ********************/
/******************************************************************/

/* handles a service that is flapping */
void set_service_flap(service* svc,
		      double percent_change,
                      double high_threshold,
		      double low_threshold,
                      int allow_flapstart_notification) {
  logger(dbg_functions, basic) << "set_service_flap()";

  if (svc == NULL)
    return;

  logger(dbg_flapping, more)
    << "Service '" << svc->description << "' on host '"
    << svc->host_name << "' started flapping!";

  /* log a notice - this one is parsed by the history CGI */
  logger(log_runtime_warning, basic)
    << fixed << setprecision(1)
    << "SERVICE FLAPPING ALERT: " << svc->host_name
    << ";" << svc->description
    << ";STARTED; Service appears to have started flapping ("
    << percent_change << "% change >= " << high_threshold << "% threshold)";

  /* add a non-persistent comment to the service */
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1)
      << "Notifications for this service are being suppressed because it was detected as "
      << "having been flapping between different states ("
      << percent_change << "% change >= " << high_threshold
      << "% threshold).  When the service state stabilizes and the flapping "
      << "stops, notifications will be re-enabled.";

  add_new_service_comment(FLAPPING_COMMENT,
			  svc->host_name,
                          svc->description,
			  time(NULL),
                          "(Centreon Engine Process)",
			  oss.str().c_str(),
			  0,
                          COMMENTSOURCE_INTERNAL,
			  FALSE,
			  (time_t)0,
                          &(svc->flapping_comment_id));

  /* set the flapping indicator */
  svc->is_flapping = TRUE;

  /* send data to event broker */
  broker_flapping_data(NEBTYPE_FLAPPING_START,
		       NEBFLAG_NONE,
                       NEBATTR_NONE,
		       SERVICE_FLAPPING,
		       svc,
                       percent_change,
		       high_threshold,
		       low_threshold,
                       NULL);

  /* see if we should check to send a recovery notification out when flapping stops */
  if (svc->current_state != STATE_OK
      && svc->current_notification_number > 0)
    svc->check_flapping_recovery_notification = TRUE;
  else
    svc->check_flapping_recovery_notification = FALSE;

  /* send a notification */
  if (allow_flapstart_notification == TRUE)
    service_notification(svc,
			 NOTIFICATION_FLAPPINGSTART,
			 NULL,
			 NULL,
                         NOTIFICATION_OPTION_NONE);
}

/* handles a service that has stopped flapping */
void clear_service_flap(service* svc,
			double percent_change,
                        double high_threshold,
			double low_threshold) {

  logger(dbg_functions, basic) << "clear_service_flap()";

  if (svc == NULL)
    return;

  logger(dbg_flapping, more)
    << "Service '" << svc->description << "' on host '"
    << svc->host_name << "' stopped flapping.";

  /* log a notice - this one is parsed by the history CGI */
  logger(log_info_message, basic)
    << fixed << setprecision(1)
    << "SERVICE FLAPPING ALERT: " << svc->host_name
    << ";" << svc->description
    << ";STOPPED; Service appears to have stopped flapping ("
    << percent_change << "% change < " << low_threshold << "% threshold)";

  /* delete the comment we added earlier */
  if (svc->flapping_comment_id != 0)
    delete_service_comment(svc->flapping_comment_id);
  svc->flapping_comment_id = 0;

  /* clear the flapping indicator */
  svc->is_flapping = FALSE;

  /* send data to event broker */
  broker_flapping_data(NEBTYPE_FLAPPING_STOP,
		       NEBFLAG_NONE,
                       NEBATTR_FLAPPING_STOP_NORMAL,
		       SERVICE_FLAPPING,
                       svc,
		       percent_change,
		       high_threshold,
                       low_threshold,
		       NULL);

  /* send a notification */
  service_notification(svc,
		       NOTIFICATION_FLAPPINGSTOP,
		       NULL,
		       NULL,
                       NOTIFICATION_OPTION_NONE);

  /* should we send a recovery notification? */
  if (svc->check_flapping_recovery_notification == TRUE
      && svc->current_state == STATE_OK)
    service_notification(svc,
			 NOTIFICATION_NORMAL,
			 NULL,
			 NULL,
                         NOTIFICATION_OPTION_NONE);

  /* clear the recovery notification flag */
  svc->check_flapping_recovery_notification = FALSE;

}

/* handles a host that is flapping */
void set_host_flap(host* hst,
		   double percent_change,
                   double high_threshold,
		   double low_threshold,
                   int allow_flapstart_notification) {
  logger(dbg_functions, basic) << "set_host_flap()";

  if (hst == NULL)
    return;

  logger(dbg_flapping, more) << "Host '" << hst->name << "' started flapping!";

  /* log a notice - this one is parsed by the history CGI */
  logger(log_runtime_warning, basic)
    << fixed << setprecision(1)
    << "HOST FLAPPING ALERT: " << hst->name
    << ";STARTED; Host appears to have started flapping ("
    << percent_change << "% change > " << high_threshold << "% threshold)";

  /* add a non-persistent comment to the host */
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1)
      << "Notifications for this host are being suppressed because it was detected as "
      << "having been flapping between different states ("
      << percent_change << "% change > " << high_threshold
      << "% threshold).  When the host state stabilizes and the "
      << "flapping stops, notifications will be re-enabled.";

  add_new_host_comment(FLAPPING_COMMENT,
		       hst->name,
		       time(NULL),
                       "(Centreon Engine Process)",
		       oss.str().c_str(),
		       0,
                       COMMENTSOURCE_INTERNAL,
		       FALSE,
		       (time_t)0,
                       &(hst->flapping_comment_id));

  /* set the flapping indicator */
  hst->is_flapping = TRUE;

  /* send data to event broker */
  broker_flapping_data(NEBTYPE_FLAPPING_START,
		       NEBFLAG_NONE,
                       NEBATTR_NONE,
		       HOST_FLAPPING,
		       hst,
		       percent_change,
                       high_threshold,
		       low_threshold,
		       NULL);

  /* see if we should check to send a recovery notification out when flapping stops */
  if (hst->current_state != HOST_UP
      && hst->current_notification_number > 0)
    hst->check_flapping_recovery_notification = TRUE;
  else
    hst->check_flapping_recovery_notification = FALSE;

  /* send a notification */
  if (allow_flapstart_notification == TRUE)
    host_notification(hst,
		      NOTIFICATION_FLAPPINGSTART,
		      NULL,
		      NULL,
                      NOTIFICATION_OPTION_NONE);
}

/* handles a host that has stopped flapping */
void clear_host_flap(host* hst,
		     double percent_change,
                     double high_threshold,
		     double low_threshold) {

  logger(dbg_functions, basic) << "clear_host_flap()";

  if (hst == NULL)
    return;

  logger(dbg_flapping, basic) << "Host '" << hst->name << "' stopped flapping.";

  /* log a notice - this one is parsed by the history CGI */
  logger(log_info_message, basic)
    << fixed << setprecision(1)
    << "HOST FLAPPING ALERT: " << hst->name
    << ";STOPPED; Host appears to have stopped flapping (" << percent_change
    << "% change < " << low_threshold << "% threshold)";

  /* delete the comment we added earlier */
  if (hst->flapping_comment_id != 0)
    delete_host_comment(hst->flapping_comment_id);
  hst->flapping_comment_id = 0;

  /* clear the flapping indicator */
  hst->is_flapping = FALSE;

  /* send data to event broker */
  broker_flapping_data(NEBTYPE_FLAPPING_STOP,
		       NEBFLAG_NONE,
                       NEBATTR_FLAPPING_STOP_NORMAL,
		       HOST_FLAPPING,
		       hst,
                       percent_change,
		       high_threshold,
		       low_threshold,
                       NULL);

  /* send a notification */
  host_notification(hst,
		    NOTIFICATION_FLAPPINGSTOP,
		    NULL,
		    NULL,
                    NOTIFICATION_OPTION_NONE);

  /* should we send a recovery notification? */
  if (hst->check_flapping_recovery_notification == TRUE
      && hst->current_state == HOST_UP)
    host_notification(hst,
		      NOTIFICATION_NORMAL,
		      NULL,
		      NULL,
		      NOTIFICATION_OPTION_NONE);

  /* clear the recovery notification flag */
  hst->check_flapping_recovery_notification = FALSE;
}

/******************************************************************/
/***************** FLAP DETECTION STATUS FUNCTIONS ****************/
/******************************************************************/

/* enables flap detection on a program wide basis */
void enable_flap_detection_routines(void) {
  host* temp_host = NULL;
  service* temp_service = NULL;
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  logger(dbg_functions, basic) << "enable_flap_detection_routines()";

  /* bail out if we're already set */
  if (config.get_enable_flap_detection() == true)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

  /* set flap detection flag */
  config.set_enable_flap_detection(true);

  /* send data to event broker */
  broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE,
                               NEBFLAG_NONE, NEBATTR_NONE,
			       CMD_NONE,
                               attr,
			       modified_host_process_attributes,
                               attr,
                               modified_service_process_attributes,
                               NULL);

  /* update program status */
  update_program_status(FALSE);

  /* check for flapping */
  for (temp_host = host_list; temp_host != NULL; temp_host = temp_host->next)
    check_for_host_flapping(temp_host, FALSE, FALSE, TRUE);
  for (temp_service = service_list; temp_service != NULL; temp_service = temp_service->next)
    check_for_service_flapping(temp_service, FALSE, TRUE);
}

/* disables flap detection on a program wide basis */
void disable_flap_detection_routines(void) {
  host* temp_host = NULL;
  service* temp_service = NULL;
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  logger(dbg_functions, basic) << "disable_flap_detection_routines()";

  /* bail out if we're already set */
  if (config.get_enable_flap_detection() == false)
    return;

  /* set the attribute modified flag */
  modified_host_process_attributes |= attr;
  modified_service_process_attributes |= attr;

  /* set flap detection flag */
  config.set_enable_flap_detection(false);

  /* send data to event broker */
  broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE,
                               NEBFLAG_NONE, NEBATTR_NONE,
			       CMD_NONE,
                               attr,
			       modified_host_process_attributes,
                               attr,
                               modified_service_process_attributes,
                               NULL);

  /* update program status */
  update_program_status(FALSE);

  /* handle the details... */
  for (temp_host = host_list; temp_host != NULL; temp_host = temp_host->next)
    handle_host_flap_detection_disabled(temp_host);
  for (temp_service = service_list; temp_service != NULL; temp_service = temp_service->next)
    handle_service_flap_detection_disabled(temp_service);
}

/* enables flap detection for a specific host */
void enable_host_flap_detection(host* hst) {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  logger(dbg_functions, basic) << "enable_host_flap_detection()";

  if (hst == NULL)
    return;

  logger(dbg_flapping, more)
    << "Enabling flap detection for host '" << hst->name << "'.";

  /* nothing to do... */
  if (hst->flap_detection_enabled == TRUE)
    return;

  /* set the attribute modified flag */
  hst->modified_attributes |= attr;

  /* set the flap detection enabled flag */
  hst->flap_detection_enabled = TRUE;

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE,
			    NEBFLAG_NONE,
                            NEBATTR_NONE,
			    hst,
			    CMD_NONE,
			    attr,
                            hst->modified_attributes,
			    NULL);

  /* check for flapping */
  check_for_host_flapping(hst, FALSE, FALSE, TRUE);

  /* update host status */
  update_host_status(hst, FALSE);
}

/* disables flap detection for a specific host */
void disable_host_flap_detection(host* hst) {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  logger(dbg_functions, basic) << "disable_host_flap_detection()";

  if (hst == NULL)
    return;

  logger(dbg_functions, more)
    << "Disabling flap detection for host '" << hst->name << "'.";

  /* nothing to do... */
  if (hst->flap_detection_enabled == FALSE)
    return;

  /* set the attribute modified flag */
  hst->modified_attributes |= attr;

  /* set the flap detection enabled flag */
  hst->flap_detection_enabled = FALSE;

  /* send data to event broker */
  broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE,
			    NEBFLAG_NONE,
                            NEBATTR_NONE,
			    hst,
			    CMD_NONE,
			    attr,
                            hst->modified_attributes,
			    NULL);

  /* handle the details... */
  handle_host_flap_detection_disabled(hst);
}

/* handles the details for a host when flap detection is disabled (globally or per-host) */
void handle_host_flap_detection_disabled(host* hst) {
  logger(dbg_functions, basic) << "handle_host_flap_detection_disabled()";

  if (hst == NULL)
    return;

  /* if the host was flapping, remove the flapping indicator */
  if (hst->is_flapping == TRUE) {
    hst->is_flapping = FALSE;

    /* delete the original comment we added earlier */
    if (hst->flapping_comment_id != 0)
      delete_host_comment(hst->flapping_comment_id);
    hst->flapping_comment_id = 0;

    /* log a notice - this one is parsed by the history CGI */
  logger(log_info_message, basic)
    << "HOST FLAPPING ALERT: " << hst->name
    << ";DISABLED; Flap detection has been disabled";

    /* send data to event broker */
    broker_flapping_data(NEBTYPE_FLAPPING_STOP,
			 NEBFLAG_NONE,
                         NEBATTR_FLAPPING_STOP_DISABLED,
			 HOST_FLAPPING,
                         hst,
			 hst->percent_state_change,
			 0.0,
			 0.0,
                         NULL);

    /* send a notification */
    host_notification(hst,
		      NOTIFICATION_FLAPPINGDISABLED,
		      NULL,
		      NULL,
                      NOTIFICATION_OPTION_NONE);

    /* should we send a recovery notification? */
    if (hst->check_flapping_recovery_notification == TRUE
        && hst->current_state == HOST_UP)
      host_notification(hst,
			NOTIFICATION_NORMAL,
			NULL,
			NULL,
                        NOTIFICATION_OPTION_NONE);

    /* clear the recovery notification flag */
    hst->check_flapping_recovery_notification = FALSE;
  }

  /* update host status */
  update_host_status(hst, FALSE);
}

/* enables flap detection for a specific service */
void enable_service_flap_detection(service* svc) {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  logger(dbg_functions, basic) << "enable_service_flap_detection()";

  if (svc == NULL)
    return;

  logger(dbg_flapping, more)
    << "Enabling flap detection for service '" << svc->description
    << "' on host '" << svc->host_name << "'.";

  /* nothing to do... */
  if (svc->flap_detection_enabled == TRUE)
    return;

  /* set the attribute modified flag */
  svc->modified_attributes |= attr;

  /* set the flap detection enabled flag */
  svc->flap_detection_enabled = TRUE;

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                               NEBFLAG_NONE,
			       NEBATTR_NONE,
			       svc,
                               CMD_NONE,
			       attr,
			       svc->modified_attributes,
                               NULL);

  /* check for flapping */
  check_for_service_flapping(svc, FALSE, TRUE);

  /* update service status */
  update_service_status(svc, FALSE);
}

/* disables flap detection for a specific service */
void disable_service_flap_detection(service* svc) {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  logger(dbg_functions, basic) << "disable_service_flap_detection()";

  if (svc == NULL)
    return;

  logger(dbg_flapping, more)
    << "Disabling flap detection for service '" << svc->description
    << "' on host '" << svc->host_name << "'.";

  /* nothing to do... */
  if (svc->flap_detection_enabled == FALSE)
    return;

  /* set the attribute modified flag */
  svc->modified_attributes |= attr;

  /* set the flap detection enabled flag */
  svc->flap_detection_enabled = FALSE;

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                               NEBFLAG_NONE,
			       NEBATTR_NONE,
			       svc,
                               CMD_NONE,
			       attr,
			       svc->modified_attributes,
                               NULL);

  /* handle the details... */
  handle_service_flap_detection_disabled(svc);
}

/* handles the details for a service when flap detection is disabled (globally or per-service) */
void handle_service_flap_detection_disabled(service* svc) {
  logger(dbg_functions, basic) << "handle_service_flap_detection_disabled()";

  if (svc == NULL)
    return;

  /* if the service was flapping, remove the flapping indicator */
  if (svc->is_flapping == TRUE) {
    svc->is_flapping = FALSE;

    /* delete the original comment we added earlier */
    if (svc->flapping_comment_id != 0)
      delete_service_comment(svc->flapping_comment_id);
    svc->flapping_comment_id = 0;

    /* log a notice - this one is parsed by the history CGI */
  logger(log_info_message, basic)
    << "SERVICE FLAPPING ALERT: " << svc->host_name
    << ";" << svc->description << ";DISABLED; Flap detection has been disabled";

    /* send data to event broker */
    broker_flapping_data(NEBTYPE_FLAPPING_STOP,
			 NEBFLAG_NONE,
                         NEBATTR_FLAPPING_STOP_DISABLED,
                         SERVICE_FLAPPING,
			 svc,
                         svc->percent_state_change,
			 0.0,
			 0.0,
			 NULL);

    /* send a notification */
    service_notification(svc,
			 NOTIFICATION_FLAPPINGDISABLED,
			 NULL,
			 NULL,
                         NOTIFICATION_OPTION_NONE);

    /* should we send a recovery notification? */
    if (svc->check_flapping_recovery_notification == TRUE
        && svc->current_state == STATE_OK)
      service_notification(svc,
			   NOTIFICATION_NORMAL,
			   NULL,
			   NULL,
                           NOTIFICATION_OPTION_NONE);

    /* clear the recovery notification flag */
    svc->check_flapping_recovery_notification = FALSE;
  }

  /* update service status */
  update_service_status(svc, FALSE);
}
