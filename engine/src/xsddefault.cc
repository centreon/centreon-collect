/**
 * Copyright 2000-2009      Ethan Galstad
 * Copyright 2009           Nagios Core Development Team and Community
 *Contributors
 * Copyright 2011-2013,2015 Merethis
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

#include "com/centreon/engine/xsddefault.hh"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/downtimes/downtime.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/statusdata.hh"

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
  const std::string& status_file = pb_config.status_file();
  if (verify_config || status_file.empty())
    return OK;

  if (xsddefault_status_log_fd == -1) {
    // delete the old status log (it might not exist).
    unlink(status_file.c_str());

    if ((xsddefault_status_log_fd =
             open(status_file.c_str(), O_WRONLY | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP)) == -1) {
      engine_logger(engine::logging::log_runtime_error, engine::logging::basic)
          << "Error: Unable to open status data file '" << status_file
          << "': " << strerror(errno);
      runtime_logger->error("Error: Unable to open status data file '{}': {}",
                            status_file, strerror(errno));
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

  const std::string& status_file = pb_config.status_file();
  // delete the status log.
  if (delete_status_data && !status_file.empty()) {
    if (unlink(status_file.c_str()))
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

  engine_logger(engine::logging::dbg_functions, engine::logging::basic)
      << "save_status_data()";
  functions_logger->trace("save_status_data()");

  bool check_external_commands = pb_config.check_external_commands();
  bool enable_notifications = pb_config.enable_notifications();
  bool execute_service_checks = pb_config.execute_service_checks();
  bool accept_passive_service_checks =
      pb_config.accept_passive_service_checks();
  bool execute_host_checks = pb_config.execute_host_checks();
  bool accept_passive_host_checks = pb_config.accept_passive_host_checks();
  bool enable_event_handlers = pb_config.enable_event_handlers();
  bool obsess_over_services = pb_config.obsess_over_services();
  bool obsess_over_hosts = pb_config.obsess_over_hosts();
  bool check_service_freshness = pb_config.check_service_freshness();
  bool check_host_freshness = pb_config.check_host_freshness();
  bool enable_flap_detection = pb_config.enable_flap_detection();
  bool process_performance_data = pb_config.process_performance_data();
  const std::string& global_host_event_handler =
      pb_config.global_host_event_handler();
  const std::string& global_service_event_handler =
      pb_config.global_service_event_handler();
  uint32_t external_command_buffer_slots =
      pb_config.external_command_buffer_slots();

  // get number of items in the command buffer
  if (check_external_commands) {
    used_external_command_buffer_slots = external_command_buffer.size();
    high_external_command_buffer_slots = external_command_buffer.high();
  }

  // generate check statistics
  generate_check_stats();

  std::ostringstream stream;

  time_t current_time;
  time(&current_time);

  // write version info to status file
  stream << "#############################################\n"
            "#        CENTREON ENGINE STATUS FILE        #\n"
            "#                                           #\n"
            "# THIS FILE IS AUTOMATICALLY GENERATED BY   #\n"
            "# CENTREON ENGINE. DO NOT MODIFY THIS FILE! #\n"
            "#############################################\n"
            "\n"
            "info {\n"
            "\tcreated="
         << static_cast<unsigned long>(current_time)
         << "\n"
            "\t}\n\n";

  // save program status data
  stream
      << "programstatus {\n"
         "\tmodified_host_attributes="
      << modified_host_process_attributes
      << "\n"
         "\tmodified_service_attributes="
      << modified_service_process_attributes
      << "\n"
         "\tnagios_pid="
      << static_cast<unsigned int>(getpid())
      << "\n"
         "\tprogram_start="
      << static_cast<long long>(program_start)
      << "\n"
         "\tlast_command_check="
      << static_cast<long long>(last_command_check)
      << "\n"
         "\tlast_log_rotation="
      << static_cast<long long>(last_log_rotation)
      << "\n"
         "\tenable_notifications="
      << enable_notifications
      << "\n"
         "\tactive_service_checks_enabled="
      << execute_service_checks
      << "\n"
         "\tpassive_service_checks_enabled="
      << accept_passive_service_checks
      << "\n"
         "\tactive_host_checks_enabled="
      << execute_host_checks
      << "\n"
         "\tpassive_host_checks_enabled="
      << accept_passive_host_checks
      << "\n"
         "\tenable_event_handlers="
      << enable_event_handlers
      << "\n"
         "\tobsess_over_services="
      << obsess_over_services
      << "\n"
         "\tobsess_over_hosts="
      << obsess_over_hosts
      << "\n"
         "\tcheck_service_freshness="
      << check_service_freshness
      << "\n"
         "\tcheck_host_freshness="
      << check_host_freshness
      << "\n"
         "\tenable_flap_detection="
      << enable_flap_detection
      << "\n"
         "\tprocess_performance_data="
      << process_performance_data
      << "\n"
         "\tglobal_host_event_handler="
      << global_host_event_handler
      << "\n"
         "\tglobal_service_event_handler="
      << global_service_event_handler
      << "\n"
         "\tnext_comment_id="
      << comment::get_next_comment_id()
      << "\n"
         "\tnext_event_id="
      << next_event_id
      << "\n"
         "\tnext_problem_id="
      << next_problem_id
      << "\n"
         "\tnext_notification_id="
      << next_notification_id
      << "\n"
         "\ttotal_external_command_buffer_slots="
      << external_command_buffer_slots
      << "\n"
         "\tused_external_command_buffer_slots="
      << used_external_command_buffer_slots
      << "\n"
         "\thigh_external_command_buffer_slots="
      << high_external_command_buffer_slots
      << "\n"
         "\tactive_scheduled_host_check_stats="
      << check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[0]
      << ","
      << check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[1]
      << ","
      << check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[2]
      << "\n"
         "\tactive_ondemand_host_check_stats="
      << check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[0]
      << ","
      << check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[1]
      << ","
      << check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[2]
      << "\n"
         "\tpassive_host_check_stats="
      << check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[0] << ","
      << check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[1] << ","
      << check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[2]
      << "\n"
         "\tactive_scheduled_service_check_stats="
      << check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[0]
      << ","
      << check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[1]
      << ","
      << check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[2]
      << "\n"
         "\tactive_ondemand_service_check_stats="
      << check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[0]
      << ","
      << check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[1]
      << ","
      << check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[2]
      << "\n"
         "\tpassive_service_check_stats="
      << check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[0] << ","
      << check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[1] << ","
      << check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[2]
      << "\n"
         "\tcached_host_check_stats="
      << check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[0] << ","
      << check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[1] << ","
      << check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[2]
      << "\n"
         "\tcached_service_check_stats="
      << check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[0]
      << ","
      << check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[1]
      << ","
      << check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[2]
      << "\n"
         "\texternal_command_stats="
      << check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[0] << ","
      << check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[1] << ","
      << check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[2]
      << "\n"
         "\tparallel_host_check_stats="
      << check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[0] << ","
      << check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[1] << ","
      << check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[2]
      << "\n"
         "\tserial_host_check_stats="
      << check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[0] << ","
      << check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[1] << ","
      << check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[2]
      << "\n"
         "\t}\n\n";

  /* save host status data */
  for (host_map::iterator it(com::centreon::engine::host::hosts.begin()),
       end(com::centreon::engine::host::hosts.end());
       it != end; ++it) {
    stream
        << "hoststatus {\n"
           "\thost_name="
        << it->second->name()
        << "\n"
           "\tmodified_attributes="
        << it->second->get_modified_attributes()
        << "\n"
           "\tcheck_command="
        << it->second->check_command()
        << "\n"
           "\tcheck_period="
        << it->second->check_period()
        << "\n"
           "\tnotification_period="
        << it->second->notification_period()
        << "\n"
           "\tcheck_interval="
        << it->second->check_interval()
        << "\n"
           "\tretry_interval="
        << it->second->retry_interval()
        << "\n"
           "\tevent_handler="
        << it->second->event_handler()
        << "\n"
           "\thas_been_checked="
        << it->second->has_been_checked()
        << "\n"
           "\tshould_be_scheduled="
        << it->second->get_should_be_scheduled()
        << "\n"
           "\tcheck_execution_time="
        << std::setprecision(3) << std::fixed
        << it->second->get_execution_time()
        << "\n"
           "\tcheck_latency="
        << std::setprecision(3) << std::fixed << it->second->get_latency()
        << "\n"
           "\tcheck_type="
        << it->second->get_check_type()
        << "\n"
           "\tcurrent_state="
        << it->second->get_current_state()
        << "\n"
           "\tlast_hard_state="
        << it->second->get_last_hard_state()
        << "\n"
           "\tlast_event_id="
        << it->second->get_last_event_id()
        << "\n"
           "\tcurrent_event_id="
        << it->second->get_current_event_id()
        << "\n"
           "\tcurrent_problem_id="
        << it->second->get_current_problem_id()
        << "\n"
           "\tlast_problem_id="
        << it->second->get_last_problem_id()
        << "\n"
           "\tplugin_output="
        << it->second->get_plugin_output()
        << "\n"
           "\tlong_plugin_output="
        << it->second->get_long_plugin_output()
        << "\n"
           "\tperformance_data="
        << it->second->get_perf_data()
        << "\n"
           "\tlast_check="
        << static_cast<unsigned long>(it->second->get_last_check())
        << "\n"
           "\tnext_check="
        << static_cast<unsigned long>(it->second->get_next_check())
        << "\n"
           "\tcheck_options="
        << it->second->get_check_options()
        << "\n"
           "\tcurrent_attempt="
        << it->second->get_current_attempt()
        << "\n"
           "\tmax_attempts="
        << it->second->max_check_attempts()
        << "\n"
           "\tstate_type="
        << it->second->get_state_type()
        << "\n"
           "\tlast_state_change="
        << static_cast<unsigned long>(it->second->get_last_state_change())
        << "\n"
           "\tlast_hard_state_change="
        << static_cast<unsigned long>(it->second->get_last_hard_state_change())
        << "\n"
           "\tlast_time_up="
        << static_cast<unsigned long>(it->second->get_last_time_up())
        << "\n"
           "\tlast_time_down="
        << static_cast<unsigned long>(it->second->get_last_time_down())
        << "\n"
           "\tlast_time_unreachable="
        << static_cast<unsigned long>(it->second->get_last_time_unreachable())
        << "\n"
           "\tlast_notification="
        << static_cast<unsigned long>(it->second->get_last_notification())
        << "\n"
           "\tnext_notification="
        << static_cast<unsigned long>(it->second->get_next_notification())
        << "\n"
           "\tno_more_notifications="
        << it->second->get_no_more_notifications()
        << "\n"
           "\tcurrent_notification_number="
        << it->second->get_notification_number()
        << "\n"
           "\tcurrent_notification_id="
        << it->second->get_current_notification_id()
        << "\n"
           "\tnotifications_enabled="
        << it->second->get_notifications_enabled()
        << "\n"
           "\tproblem_has_been_acknowledged="
        << it->second->problem_has_been_acknowledged()
        << "\n"
           "\tacknowledgement_type="
        << it->second->get_acknowledgement()
        << "\n"
           "\tactive_checks_enabled="
        << it->second->active_checks_enabled()
        << "\n"
           "\tpassive_checks_enabled="
        << it->second->passive_checks_enabled()
        << "\n"
           "\tevent_handler_enabled="
        << it->second->event_handler_enabled()
        << "\n"
           "\tflap_detection_enabled="
        << it->second->flap_detection_enabled()
        << "\n"
           "\tprocess_performance_data="
        << it->second->get_process_performance_data()
        << "\n"
           "\tobsess_over_host="
        << it->second->obsess_over()
        << "\n"
           "\tlast_update="
        << static_cast<unsigned long>(current_time)
        << "\n"
           "\tis_flapping="
        << it->second->get_is_flapping()
        << "\n"
           "\tpercent_state_change="
        << std::setprecision(2) << std::fixed
        << it->second->get_percent_state_change()
        << "\n"
           "\tscheduled_downtime_depth="
        << it->second->get_scheduled_downtime_depth() << "\n";

    // custom variables
    for (auto const& cv : it->second->custom_variables) {
      if (!cv.first.empty())
        stream << "\t_" << cv.first << "=" << cv.second.has_been_modified()
               << ";" << cv.second.value() << "\n";
    }
    stream << "\t}\n\n";
  }

  // save service status data
  for (service_map::iterator it(service::services.begin()),
       end(service::services.end());
       it != end; ++it) {
    stream << "servicestatus {\n"
              "\thost_name="
           << it->second->get_hostname()
           << "\n"
              "\tservice_description="
           << it->second->description()
           << "\n"
              "\tmodified_attributes="
           << it->second->get_modified_attributes()
           << "\n"
              "\tcheck_command="
           << it->second->check_command()
           << "\n"
              "\tcheck_period="
           << it->second->check_period()
           << "\n"
              "\tnotification_period="
           << it->second->notification_period()
           << "\n"
              "\tcheck_interval="
           << it->second->check_interval()
           << "\n"
              "\tretry_interval="
           << it->second->retry_interval()
           << "\n"
              "\tevent_handler="
           << it->second->event_handler()
           << "\n"
              "\thas_been_checked="
           << it->second->has_been_checked()
           << "\n"
              "\tshould_be_scheduled="
           << it->second->get_should_be_scheduled()
           << "\n"
              "\tcheck_execution_time="
           << std::setprecision(3) << std::fixed
           << it->second->get_execution_time()
           << "\n"
              "\tcheck_latency="
           << std::setprecision(3) << std::fixed << it->second->get_latency()
           << "\n"
              "\tcheck_type="
           << it->second->get_check_type()
           << "\n"
              "\tcurrent_state="
           << it->second->get_current_state()
           << "\n"
              "\tlast_hard_state="
           << it->second->get_last_hard_state()
           << "\n"
              "\tlast_event_id="
           << it->second->get_last_event_id()
           << "\n"
              "\tcurrent_event_id="
           << it->second->get_current_event_id()
           << "\n"
              "\tcurrent_problem_id="
           << it->second->get_current_problem_id()
           << "\n"
              "\tlast_problem_id="
           << it->second->get_last_problem_id()
           << "\n"
              "\tcurrent_attempt="
           << it->second->get_current_attempt()
           << "\n"
              "\tmax_attempts="
           << it->second->max_check_attempts()
           << "\n"
              "\tstate_type="
           << it->second->get_state_type()
           << "\n"
              "\tlast_state_change="
           << static_cast<unsigned long>(it->second->get_last_state_change())
           << "\n"
              "\tlast_hard_state_change="
           << static_cast<unsigned long>(
                  it->second->get_last_hard_state_change())
           << "\n"
              "\tlast_time_ok="
           << static_cast<unsigned long>(it->second->get_last_time_ok())
           << "\n"
              "\tlast_time_warning="
           << static_cast<unsigned long>(it->second->get_last_time_warning())
           << "\n"
              "\tlast_time_unknown="
           << static_cast<unsigned long>(it->second->get_last_time_unknown())
           << "\n"
              "\tlast_time_critical="
           << static_cast<unsigned long>(it->second->get_last_time_critical())
           << "\n"
              "\tplugin_output="
           << it->second->get_plugin_output()
           << "\n"
              "\tlong_plugin_output="
           << it->second->get_long_plugin_output()
           << "\n"
              "\tperformance_data="
           << it->second->get_perf_data()
           << "\n"
              "\tlast_check="
           << static_cast<unsigned long>(it->second->get_last_check())
           << "\n"
              "\tnext_check="
           << static_cast<unsigned long>(it->second->get_next_check())
           << "\n"
              "\tcheck_options="
           << it->second->get_check_options()
           << "\n"
              "\tcurrent_notification_number="
           << it->second->get_notification_number()
           << "\n"
              "\tcurrent_notification_id="
           << it->second->get_current_notification_id()
           << "\n"
              "\tlast_notification="
           << static_cast<unsigned long>(it->second->get_last_notification())
           << "\n"
              "\tnext_notification="
           << static_cast<unsigned long>(it->second->get_next_notification())
           << "\n"
              "\tno_more_notifications="
           << it->second->get_no_more_notifications()
           << "\n"
              "\tnotifications_enabled="
           << it->second->get_notifications_enabled()
           << "\n"
              "\tactive_checks_enabled="
           << it->second->active_checks_enabled()
           << "\n"
              "\tpassive_checks_enabled="
           << it->second->passive_checks_enabled()
           << "\n"
              "\tevent_handler_enabled="
           << it->second->event_handler_enabled()
           << "\n"
              "\tproblem_has_been_acknowledged="
           << it->second->problem_has_been_acknowledged()
           << "\n"
              "\tacknowledgement_type="
           << it->second->get_acknowledgement()
           << "\n"
              "\tflap_detection_enabled="
           << it->second->flap_detection_enabled()
           << "\n"
              "\tprocess_performance_data="
           << it->second->get_process_performance_data()
           << "\n"
              "\tobsess_over_service="
           << it->second->obsess_over()
           << "\n"
              "\tlast_update="
           << static_cast<unsigned long>(current_time)
           << "\n"
              "\tis_flapping="
           << it->second->get_is_flapping()
           << "\n"
              "\tpercent_state_change="
           << std::setprecision(2) << std::fixed
           << it->second->get_percent_state_change()
           << "\n"
              "\tscheduled_downtime_depth="
           << it->second->get_scheduled_downtime_depth() << "\n";

    // custom variables
    for (auto const& cv : it->second->custom_variables) {
      if (!cv.first.empty())
        stream << "\t_" << cv.first << "=" << cv.second.has_been_modified()
               << ";" << cv.second.value() << "\n";
    }
    stream << "\t}\n\n";
  }

  // save contact status data
  for (contact_map::const_iterator it{contact::contacts.begin()},
       end{contact::contacts.end()};
       it != end; ++it) {
    contact* cntct(it->second.get());
    stream << "contactstatus {\n"
              "\tcontact_name="
           << cntct->get_name()
           << "\n"
              "\tmodified_attributes="
           << cntct->get_modified_attributes()
           << "\n"
              "\tmodified_host_attributes="
           << cntct->get_modified_host_attributes()
           << "\n"
              "\tmodified_service_attributes="
           << cntct->get_modified_service_attributes()
           << "\n"
              "\thost_notification_period="
           << cntct->get_host_notification_period()
           << "\n"
              "\tservice_notification_period="
           << cntct->get_service_notification_period()
           << "\n"
              "\tlast_host_notification="
           << static_cast<unsigned long>(cntct->get_last_host_notification())
           << "\n"
              "\tlast_service_notification="
           << static_cast<unsigned long>(cntct->get_last_service_notification())
           << "\n"
              "\thost_notifications_enabled="
           << cntct->get_host_notifications_enabled()
           << "\n"
              "\tservice_notifications_enabled="
           << cntct->get_service_notifications_enabled() << "\n";
    // custom variables
    for (auto const& cv : cntct->get_custom_variables()) {
      if (!cv.first.empty())
        stream << "\t_" << cv.first << "=" << cv.second.has_been_modified()
               << ";" << cv.second.value() << "\n";
    }
    stream << "\t}\n\n";
  }

  // save all comments
  for (comment_map::iterator it(comment::comments.begin()),
       end(comment::comments.end());
       it != end; ++it) {
    if (it->second->get_comment_type() == com::centreon::engine::comment::host)
      stream << "hostcomment {\n";
    else
      stream << "servicecomment {\n";
    stream << "\thost_id=" << it->second->get_host_id() << "\n";
    if (it->second->get_comment_type() ==
        com::centreon::engine::comment::service)
      stream << "\tservice_id=" << it->second->get_service_id() << "\n";
    stream << "\tentry_type=" << it->second->get_entry_type()
           << "\n"
              "\tcomment_id="
           << it->first
           << "\n"
              "\tsource="
           << it->second->get_source()
           << "\n"
              "\tpersistent="
           << it->second->get_persistent()
           << "\n"
              "\tentry_time="
           << static_cast<unsigned long>(it->second->get_entry_time())
           << "\n"
              "\texpires="
           << it->second->get_expires()
           << "\n"
              "\texpire_time="
           << static_cast<unsigned long>(it->second->get_expire_time())
           << "\n"
              "\tauthor="
           << it->second->get_author()
           << "\n"
              "\tcomment_data="
           << it->second->get_comment_data()
           << "\n"
              "\t}\n\n";
  }

  // save all downtime
  for (auto it = downtime_manager::instance().get_scheduled_downtimes().begin();
       it != downtime_manager::instance().get_scheduled_downtimes().end(); ++it)
    stream << *it->second;

  // Write data in buffer.
  stream.flush();

  const std::string& status_file = pb_config.status_file();

  // Prepare status file for overwrite.
  if ((ftruncate(xsddefault_status_log_fd, 0) == -1) ||
      (fsync(xsddefault_status_log_fd) == -1) ||
      (lseek(xsddefault_status_log_fd, 0, SEEK_SET) == (off_t)-1)) {
    char const* msg(strerror(errno));
    engine_logger(engine::logging::log_runtime_error, engine::logging::basic)
        << "Error: Unable to update status data file '" << status_file
        << "': " << msg;
    runtime_logger->error("Error: Unable to update status data file '{}': {}",
                          status_file, msg);
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
      engine_logger(engine::logging::log_runtime_error, engine::logging::basic)
          << "Error: Unable to update status data file '" << status_file
          << "': " << msg;
      runtime_logger->error("Error: Unable to update status data file '{}': {}",
                            status_file, msg);
      return ERROR;
    }
    data_ptr += wb;
    size -= wb;
  }

  return OK;
}
