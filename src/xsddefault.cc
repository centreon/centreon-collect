/*
** Copyright 2000-2009      Ethan Galstad
** Copyright 2009           Nagios Core Development Team and Community Contributors
** Copyright 2011-2013,2015 Merethis
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/objects/comment.hh"
#include "com/centreon/engine/downtimes/downtime.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/xsddefault.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::configuration::applier;

static int xsddefault_status_log_fd(-1);

/******************************************************************/
/********************* INIT/CLEANUP FUNCTIONS *********************/
/******************************************************************/

/* initialize status data */
int xsddefault_initialize_status_data() {
  if (verify_config || config->status_file().empty())
    return OK;

  if (xsddefault_status_log_fd == -1) {
    // delete the old status log (it might not exist).
    unlink(config->status_file().c_str());

    if ((xsddefault_status_log_fd = open(
                                      config->status_file().c_str(),
                                      O_WRONLY | O_CREAT,
                                      S_IRUSR | S_IWUSR | S_IRGRP)) == -1) {
      logger(engine::logging::log_runtime_error, engine::logging::basic)
        << "Error: Unable to open status data file '"
        << config->status_file() << "': " << strerror(errno);
      return ERROR;
    }
    set_cloexec(xsddefault_status_log_fd);
  }
  return OK;
}

// cleanup status data before terminating.
int xsddefault_cleanup_status_data(int delete_status_data) {
  if (verify_config)
    return OK;

  // delete the status log.
  if (delete_status_data && !config->status_file().empty()) {
    if (unlink(config->status_file().c_str()))
      return ERROR;
  }

  if (xsddefault_status_log_fd != -1) {
    close(xsddefault_status_log_fd);
    xsddefault_status_log_fd = -1;
  }
  return OK;
}

/******************************************************************/
/****************** STATUS DATA OUTPUT FUNCTIONS ******************/
/******************************************************************/

/* write all status data to file */
int xsddefault_save_status_data() {
  if (xsddefault_status_log_fd == -1)
    return OK;

  int used_external_command_buffer_slots(0);
  int high_external_command_buffer_slots(0);

  logger(engine::logging::dbg_functions, engine::logging::basic)
    << "save_status_data()";

  // get number of items in the command buffer
  if (config->check_external_commands()) {
    pthread_mutex_lock(&external_command_buffer.buffer_lock);
    used_external_command_buffer_slots = external_command_buffer.items;
    high_external_command_buffer_slots = external_command_buffer.high;
    pthread_mutex_unlock(&external_command_buffer.buffer_lock);
  }

  // generate check statistics
  generate_check_stats();

  std::ostringstream stream;

  time_t current_time;
  time(&current_time);

  // write version info to status file
  stream
    << "#############################################\n"
       "#        CENTREON ENGINE STATUS FILE        #\n"
       "#                                           #\n"
       "# THIS FILE IS AUTOMATICALLY GENERATED BY   #\n"
       "# CENTREON ENGINE. DO NOT MODIFY THIS FILE! #\n"
       "#############################################\n"
       "\n"
       "info {\n"
       "\tcreated=" << static_cast<unsigned long>(current_time) << "\n"
       "\t}\n\n";

  // save program status data
  stream
    << "programstatus {\n"
       "\tmodified_host_attributes=" << modified_host_process_attributes << "\n"
       "\tmodified_service_attributes=" << modified_service_process_attributes << "\n"
       "\tnagios_pid=" << static_cast<unsigned int>(getpid()) << "\n"
       "\tprogram_start=" << static_cast<long long>(program_start) << "\n"
       "\tlast_command_check=" << static_cast<long long>(last_command_check) << "\n"
       "\tlast_log_rotation=" << static_cast<long long>(last_log_rotation) << "\n"
       "\tenable_notifications=" << config->enable_notifications() << "\n"
       "\tactive_service_checks_enabled=" << config->execute_service_checks() << "\n"
       "\tpassive_service_checks_enabled=" << config->accept_passive_service_checks() << "\n"
       "\tactive_host_checks_enabled=" << config->execute_host_checks() << "\n"
       "\tpassive_host_checks_enabled=" << config->accept_passive_host_checks() << "\n"
       "\tenable_event_handlers=" << config->enable_event_handlers() << "\n"
       "\tobsess_over_services=" << config->obsess_over_services() << "\n"
       "\tobsess_over_hosts=" << config->obsess_over_hosts() << "\n"
       "\tcheck_service_freshness=" << config->check_service_freshness() << "\n"
       "\tcheck_host_freshness=" << config->check_host_freshness() << "\n"
       "\tenable_flap_detection=" << config->enable_flap_detection() << "\n"
       "\tprocess_performance_data=" << config->process_performance_data() << "\n"
       "\tglobal_host_event_handler=" << config->global_host_event_handler().c_str() << "\n"
       "\tglobal_service_event_handler=" << config->global_service_event_handler().c_str() << "\n"
       "\tnext_comment_id=" << next_comment_id << "\n"
       "\tnext_event_id=" << next_event_id << "\n"
       "\tnext_problem_id=" << next_problem_id << "\n"
       "\tnext_notification_id=" << next_notification_id << "\n"
       "\ttotal_external_command_buffer_slots=" << config->external_command_buffer_slots() << "\n"
       "\tused_external_command_buffer_slots=" << used_external_command_buffer_slots << "\n"
       "\thigh_external_command_buffer_slots=" << high_external_command_buffer_slots << "\n"
       "\tactive_scheduled_host_check_stats="
    << check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[2] << "\n"
       "\tactive_ondemand_host_check_stats="
    << check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[2] << "\n"
       "\tpassive_host_check_stats="
    << check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[2] << "\n"
       "\tactive_scheduled_service_check_stats="
    << check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[2] << "\n"
       "\tactive_ondemand_service_check_stats="
    << check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[2] << "\n"
       "\tpassive_service_check_stats="
    << check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[2] << "\n"
       "\tcached_host_check_stats="
    << check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[2] << "\n"
       "\tcached_service_check_stats="
    << check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[2] << "\n"
       "\texternal_command_stats="
    << check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[0] << ","
    << check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[1] << ","
    << check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[2] << "\n"
       "\tparallel_host_check_stats="
    << check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[2] << "\n"
       "\tserial_host_check_stats="
    << check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[0] << ","
    << check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[1] << ","
    << check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[2] << "\n"
       "\t}\n\n";

  /* save host status data */
  for (host* hst = host_list; hst; hst = hst->next) {
    stream
      << "hoststatus {\n"
         "\thost_name=" << hst->get_name() << "\n"
         "\tmodified_attributes=" << hst->get_modified_attributes() << "\n"
         "\tcheck_command=" << hst->get_host_check_command() << "\n"
         "\tcheck_period=" << hst->get_check_period() << "\n"
         "\tnotification_period=" << hst->get_notification_period() << "\n"
         "\tcheck_interval=" << hst->get_check_interval() << "\n"
         "\tretry_interval=" << hst->get_retry_interval() << "\n"
         "\tevent_handler=" << hst->get_event_handler() << "\n"
         "\thas_been_checked=" << hst->get_has_been_checked() << "\n"
         "\tshould_be_scheduled=" << hst->get_should_be_scheduled() << "\n"
         "\tcheck_execution_time=" << std::setprecision(3) << std::fixed << hst->get_execution_time() << "\n"
         "\tcheck_latency=" << std::setprecision(3) << std::fixed << hst->get_latency() << "\n"
         "\tcheck_type=" << hst->get_check_type() << "\n"
         "\tcurrent_state=" << hst->get_current_state() << "\n"
         "\tlast_hard_state=" << hst->get_last_hard_state() << "\n"
         "\tlast_event_id=" << hst->get_last_event_id() << "\n"
         "\tcurrent_event_id=" << hst->get_current_event_id() << "\n"
         "\tcurrent_problem_id=" << hst->get_current_problem_id() << "\n"
         "\tlast_problem_id=" << hst->get_last_problem_id() << "\n"
         "\tplugin_output=" << hst->get_plugin_output()<< "\n"
         "\tlong_plugin_output=" << hst->get_long_plugin_output() << "\n"
         "\tperformance_data=" << hst->get_perf_data() << "\n"
         "\tlast_check=" << static_cast<unsigned long>(hst->get_last_check()) << "\n"
         "\tnext_check=" << static_cast<unsigned long>(hst->get_next_check()) << "\n"
         "\tcheck_options=" << hst->get_check_options() << "\n"
         "\tcurrent_attempt=" << hst->get_current_attempt() << "\n"
         "\tmax_attempts=" << hst->get_max_attempts() << "\n"
         "\tstate_type=" << hst->get_state_type() << "\n"
         "\tlast_state_change=" << static_cast<unsigned long>(hst->get_last_state_change()) << "\n"
         "\tlast_hard_state_change=" << static_cast<unsigned long>(hst->get_last_hard_state_change()) << "\n"
         "\tlast_time_up=" << static_cast<unsigned long>(hst->get_last_time_up()) << "\n"
         "\tlast_time_down=" << static_cast<unsigned long>(hst->get_last_time_down()) << "\n"
         "\tlast_time_unreachable=" << static_cast<unsigned long>(hst->get_last_time_unreachable()) << "\n"
         "\tlast_notification=" << static_cast<unsigned long>(hst->get_last_host_notification()) << "\n"
         "\tnext_notification=" << static_cast<unsigned long>(hst->get_next_host_notification()) << "\n"
         "\tno_more_notifications=" << hst->get_no_more_notifications() << "\n"
         "\tcurrent_notification_number=" << hst->get_current_notification_number() << "\n"
         "\tcurrent_notification_id=" << hst->get_current_notification_id() << "\n"
         "\tnotifications_enabled=" << hst->get_notifications_enabled() << "\n"
         "\tproblem_has_been_acknowledged=" << hst->get_problem_has_been_acknowledged() << "\n"
         "\tacknowledgement_type=" << hst->get_acknowledgement_type() << "\n"
         "\tactive_checks_enabled=" << hst->get_checks_enabled() << "\n"
         "\tpassive_checks_enabled=" << hst->get_accept_passive_host_checks() << "\n"
         "\tevent_handler_enabled=" << hst->get_event_handler_enabled() << "\n"
         "\tflap_detection_enabled=" << hst->get_flap_detection_enabled() << "\n"
         "\tprocess_performance_data=" << hst->get_process_performance_data() << "\n"
         "\tobsess_over_host=" << hst->get_obsess_over_host() << "\n"
         "\tlast_update=" << static_cast<unsigned long>(current_time) << "\n"
         "\tis_flapping=" << hst->get_is_flapping() << "\n"
         "\tpercent_state_change=" << std::setprecision(2) << std::fixed << hst->get_percent_state_change() << "\n"
         "\tscheduled_downtime_depth=" << hst->get_scheduled_downtime_depth() << "\n";

    // custom variables
    for (std::pair<std::string, customvariable> const& cv : hst->custom_variables) {
      if (!cv.first.empty())
        stream << "\t_" << cv.first << "=" << cv.second.has_been_modified() << ";"
               << cv.second.get_value() << "\n";
    }
    stream << "\t}\n\n";
  }

  // save service status data
  for (service* svc = service_list; svc; svc = svc->next) {
    stream
      << "servicestatus {\n"
         "\thost_name=" << svc->host_name << "\n"
         "\tservice_description=" << svc->description << "\n"
         "\tmodified_attributes=" << svc->modified_attributes << "\n"
         "\tcheck_command=" << (svc->service_check_command ? svc->service_check_command : "") << "\n"
         "\tcheck_period=" << (svc->check_period ? svc->check_period : "") << "\n"
         "\tnotification_period=" << (svc->notification_period ? svc->notification_period : "") << "\n"
         "\tcheck_interval=" << svc->check_interval << "\n"
         "\tretry_interval=" << svc->retry_interval << "\n"
         "\tevent_handler=" << (svc->event_handler ? svc->event_handler : "") << "\n"
         "\thas_been_checked=" << svc->has_been_checked << "\n"
         "\tshould_be_scheduled=" << svc->should_be_scheduled << "\n"
         "\tcheck_execution_time=" << std::setprecision(3) << std::fixed << svc->execution_time << "\n"
         "\tcheck_latency=" << std::setprecision(3) << std::fixed << svc->latency << "\n"
         "\tcheck_type=" << svc->check_type << "\n"
         "\tcurrent_state=" << svc->current_state << "\n"
         "\tlast_hard_state=" << svc->last_hard_state << "\n"
         "\tlast_event_id=" << svc->last_event_id << "\n"
         "\tcurrent_event_id=" << svc->current_event_id << "\n"
         "\tcurrent_problem_id=" << svc->current_problem_id << "\n"
         "\tlast_problem_id=" << svc->last_problem_id << "\n"
         "\tcurrent_attempt=" << svc->current_attempt << "\n"
         "\tmax_attempts=" << svc->max_attempts << "\n"
         "\tstate_type=" << svc->state_type << "\n"
         "\tlast_state_change=" << static_cast<unsigned long>(svc->last_state_change) << "\n"
         "\tlast_hard_state_change=" << static_cast<unsigned long>(svc->last_hard_state_change) << "\n"
         "\tlast_time_ok=" << static_cast<unsigned long>(svc->last_time_ok) << "\n"
         "\tlast_time_warning=" << static_cast<unsigned long>(svc->last_time_warning) << "\n"
         "\tlast_time_unknown=" << static_cast<unsigned long>(svc->last_time_unknown) << "\n"
         "\tlast_time_critical=" << static_cast<unsigned long>(svc->last_time_critical) << "\n"
         "\tplugin_output=" << (svc->plugin_output ? svc->plugin_output : "") << "\n"
         "\tlong_plugin_output=" << (svc->long_plugin_output ? svc->long_plugin_output : "") << "\n"
         "\tperformance_data=" << (svc->perf_data ? svc->perf_data : "") << "\n"
         "\tlast_check=" << static_cast<unsigned long>(svc->last_check) << "\n"
         "\tnext_check=" << static_cast<unsigned long>(svc->next_check) << "\n"
         "\tcheck_options=" << svc->check_options << "\n"
         "\tcurrent_notification_number=" << svc->current_notification_number << "\n"
         "\tcurrent_notification_id=" << svc->current_notification_id << "\n"
         "\tlast_notification=" << static_cast<unsigned long>(svc->last_notification) << "\n"
         "\tnext_notification=" << static_cast<unsigned long>(svc->next_notification) << "\n"
         "\tno_more_notifications=" << svc->no_more_notifications << "\n"
         "\tnotifications_enabled=" << svc->notifications_enabled << "\n"
         "\tactive_checks_enabled=" << svc->checks_enabled << "\n"
         "\tpassive_checks_enabled=" << svc->accept_passive_service_checks << "\n"
         "\tevent_handler_enabled=" << svc->event_handler_enabled << "\n"
         "\tproblem_has_been_acknowledged=" << svc->problem_has_been_acknowledged << "\n"
         "\tacknowledgement_type=" << svc->acknowledgement_type << "\n"
         "\tflap_detection_enabled=" << svc->flap_detection_enabled << "\n"
         "\tprocess_performance_data=" << svc->process_performance_data << "\n"
         "\tobsess_over_service=" << svc->obsess_over_service << "\n"
         "\tlast_update=" << static_cast<unsigned long>(current_time) << "\n"
         "\tis_flapping=" << svc->is_flapping << "\n"
         "\tpercent_state_change=" << std::setprecision(2) << std::fixed << svc->percent_state_change << "\n"
         "\tscheduled_downtime_depth=" << svc->scheduled_downtime_depth << "\n";

    // custom variables
    for (std::pair<std::string, customvariable> const& cv : svc->custom_variables) {
      if (!cv.first.empty())
        stream << "\t_" << cv.first << "=" << cv.second.has_been_modified() << ";"
               << cv.second.get_value() << "\n";
    }
    stream << "\t}\n\n";
  }

  // save contact status data
  for (umap<std::string, std::shared_ptr<contact> >::const_iterator
         it(state::instance().contacts().begin()),
         end(state::instance().contacts().end());
       it != end;
       ++it) {
    contact *cntct(it->second.get());
    stream
      << "contactstatus {\n"
         "\tcontact_name=" << cntct->get_name() << "\n"
         "\tmodified_attributes=" << cntct->get_modified_attributes() << "\n"
         "\tmodified_host_attributes=" << cntct->get_modified_host_attributes() << "\n"
         "\tmodified_service_attributes=" << cntct->get_modified_service_attributes() << "\n"
         "\thost_notification_period=" << cntct->get_host_notification_period() << "\n"
         "\tservice_notification_period=" << cntct->get_service_notification_period() << "\n"
         "\tlast_host_notification=" << static_cast<unsigned long>(cntct->get_last_host_notification()) << "\n"
         "\tlast_service_notification=" << static_cast<unsigned long>(cntct->get_last_service_notification()) << "\n"
         "\thost_notifications_enabled=" << cntct->get_host_notifications_enabled() << "\n"
         "\tservice_notifications_enabled=" << cntct->get_service_notifications_enabled() << "\n";
    // custom variables
    for (std::pair<std::string, customvariable> const& cv : cntct->custom_variables) {
      if (!cv.first.empty())
        stream << "\t_" << cv.first << "=" << cv.second.has_been_modified() << ";"
               << cv.second.get_value() << "\n";
    }
    stream << "\t}\n\n";
  }

  // save all comments
  for (comment* com = comment_list; com; com = com->next) {
    if (com->comment_type == HOST_COMMENT)
      stream << "hostcomment {\n";
    else
      stream << "servicecomment {\n";
    stream << "\thost_name=" << com->host_name << "\n";
    if (com->comment_type == SERVICE_COMMENT)
      stream << "\tservice_description=" << com->service_description << "\n";
    stream
      << "\tentry_type=" << com->entry_type << "\n"
         "\tcomment_id=" << com->comment_id << "\n"
         "\tsource=" << com->source << "\n"
         "\tpersistent=" << com->persistent << "\n"
         "\tentry_time=" << static_cast<unsigned long>(com->entry_time) << "\n"
         "\texpires=" << com->expires << "\n"
         "\texpire_time=" << static_cast<unsigned long>(com->expire_time) << "\n"
         "\tauthor=" << com->author << "\n"
         "\tcomment_data=" << com->comment_data << "\n"
         "\t}\n\n";
  }

  // save all downtime
  for (std::pair<time_t, std::shared_ptr<downtime>> const& dt : downtime_manager::instance().get_scheduled_downtimes())
    stream << *dt.second;

  // Write data in buffer.
  stream.flush();

  // Prepare status file for overwrite.
  if ((ftruncate(xsddefault_status_log_fd, 0) == -1)
      || (fsync(xsddefault_status_log_fd) == -1)
      || (lseek(xsddefault_status_log_fd, 0, SEEK_SET) == (off_t)-1)) {
    char const* msg(strerror(errno));
    logger(engine::logging::log_runtime_error, engine::logging::basic)
      << "Error: Unable to update status data file '"
      << config->status_file() << "': " << msg;
    return ERROR;
  }

  // Write status file.
  std::string data(stream.str());
  char const* data_ptr(data.c_str());
  unsigned int size(data.size());
  while (size > 0) {
    ssize_t wb(write(xsddefault_status_log_fd, data_ptr, size));
    if (wb <= 0) {
      char const* msg(strerror(errno));
      logger(engine::logging::log_runtime_error, engine::logging::basic)
        << "Error: Unable to update status data file '"
        << config->status_file() << "': " << msg;
      return ERROR;
    }
    data_ptr += wb;
    size -= wb;
  }

  return OK;
}
