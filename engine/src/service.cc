/**
 * Copyright 2011 - 2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <absl/strings/match.h>

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/configuration/whitelist.hh"
#include "com/centreon/engine/deleter/listmember.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/neberrors.hh"
#include "com/centreon/engine/notification.hh"
#include "com/centreon/engine/objects.hh"
#include "com/centreon/engine/sehandlers.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/timezone_locker.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

std::array<std::pair<uint32_t, std::string>, 4> const
    service::tab_service_states{{{NSLOG_SERVICE_OK, "OK"},
                                 {NSLOG_SERVICE_WARNING, "WARNING"},
                                 {NSLOG_SERVICE_CRITICAL, "CRITICAL"},
                                 {NSLOG_SERVICE_CRITICAL, "UNKNOWN"}}};

service_map service::services;
service_id_map service::services_by_id;

service::service(const std::string& hostname,
                 const std::string& description,
                 const std::string& display_name,
                 const std::string& check_command,
                 bool checks_enabled,
                 bool accept_passive_checks,
                 uint32_t check_interval,
                 uint32_t retry_interval,
                 uint32_t notification_interval,
                 int max_attempts,
                 uint32_t first_notification_delay,
                 uint32_t recovery_notification_delay,
                 const std::string& notification_period,
                 bool notifications_enabled,
                 bool is_volatile,
                 const std::string& check_period,
                 const std::string& event_handler,
                 bool event_handler_enabled,
                 const std::string& notes,
                 const std::string& notes_url,
                 const std::string& action_url,
                 const std::string& icon_image,
                 const std::string& icon_image_alt,
                 bool flap_detection_enabled,
                 double low_flap_threshold,
                 double high_flap_threshold,
                 bool check_freshness,
                 int freshness_threshold,
                 bool obsess_over,
                 const std::string& timezone,
                 uint64_t icon_id,
                 service_type st)
    : notifier{service_notification,
               description,
               display_name,
               check_command,
               checks_enabled,
               accept_passive_checks,
               check_interval,
               retry_interval,
               notification_interval,
               max_attempts,
               0u,  // notify
               0u,  // stalk
               first_notification_delay,
               recovery_notification_delay,
               notification_period,
               notifications_enabled,
               check_period,
               event_handler,
               event_handler_enabled,
               notes,
               notes_url,
               action_url,
               icon_image,
               icon_image_alt,
               flap_detection_enabled,
               low_flap_threshold,
               high_flap_threshold,
               check_freshness,
               freshness_threshold,
               obsess_over,
               timezone,
               0,
               0,
               is_volatile,
               icon_id},
      _service_type{st},
      _host_id{0},
      _service_id{0},
      _hostname{hostname},
      _process_performance_data{0},
      _check_flapping_recovery_notification{0},
      _last_time_ok{0},
      _last_time_warning{0},
      _last_time_unknown{0},
      _last_time_critical{0},
      _initial_state{service::state_ok},
      _current_state{_initial_state},
      _last_hard_state{_initial_state},
      _last_state{_initial_state},
      _host_ptr{nullptr},
      _host_problem_at_last_check{false} {
  if (st == NONE) {
    if (absl::StartsWith(hostname, "_Module_Meta") &&
        absl::StartsWith(description, "meta_"))
      _service_type = METASERVICE;
    else if (absl::StartsWith(hostname, "_Module_BAM") &&
             absl::StartsWith(description, "ba_"))
      _service_type = BA;
    else
      _service_type = SERVICE;
  }
  set_current_attempt(1);
}

service::~service() noexcept {
  std::shared_ptr<commands::command> cmd = get_check_command_ptr();
  if (cmd) {
    cmd->remove_caller(this);
    cmd->unregister_host_serv(_hostname, description());
  }
}

time_t service::get_last_time_ok() const {
  return _last_time_ok;
}

void service::set_last_time_ok(time_t last_time) {
  _last_time_ok = last_time;
}

time_t service::get_last_time_warning() const {
  return _last_time_warning;
}

void service::set_last_time_warning(time_t last_time) {
  _last_time_warning = last_time;
}

time_t service::get_last_time_unknown() const {
  return _last_time_unknown;
}

void service::set_last_time_unknown(time_t last_time) {
  _last_time_unknown = last_time;
}

time_t service::get_last_time_critical() const {
  return _last_time_critical;
}

void service::set_last_time_critical(time_t last_time) {
  _last_time_critical = last_time;
}

enum service::service_state service::get_current_state() const {
  return _current_state;
}

void service::set_current_state(enum service::service_state current_state) {
  _current_state = current_state;
}

enum service::service_state service::get_last_state() const {
  return _last_state;
}

void service::set_last_state(enum service::service_state last_state) {
  _last_state = last_state;
}

enum service::service_state service::get_last_hard_state() const {
  return _last_hard_state;
}

void service::set_last_hard_state(enum service::service_state last_hard_state) {
  _last_hard_state = last_hard_state;
}

enum service::service_state service::get_initial_state() const {
  return _initial_state;
}

void service::set_initial_state(enum service::service_state current_state) {
  _initial_state = current_state;
}

int service::get_process_performance_data(void) const {
  return _process_performance_data;
}

void service::set_process_performance_data(int perf_data) {
  _process_performance_data = perf_data;
}

bool service::get_check_flapping_recovery_notification(void) const {
  return _check_flapping_recovery_notification;
}

void service::set_check_flapping_recovery_notification(bool check) {
  _check_flapping_recovery_notification = check;
}

bool service::recovered() const {
  return _current_state == service::state_ok;
}

int service::get_current_state_int() const {
  return static_cast<int>(_current_state);
}

/**
 *  Dump a service_map content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The service_map to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, service_map_unsafe const& obj) {
  for (service_map_unsafe::const_iterator it(obj.begin()), end(obj.end());
       it != end; ++it)
    os << "(" << it->first.first << ", " << it->first.second
       << (std::next(it) != obj.end() ? "), " : ")");
  return os;
}

/**
 *  Dump service content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The service to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::service const& obj) {
  std::string evt_str;
  if (obj.get_event_handler_ptr())
    evt_str = obj.get_event_handler_ptr()->get_name();
  std::string cmd_str;
  if (obj.get_check_command_ptr())
    cmd_str = obj.get_check_command_ptr()->get_name();
  std::string chk_period_str;
  if (obj.check_period_ptr)
    chk_period_str = obj.check_period_ptr->get_name();
  std::string notif_period_str;
  if (obj.get_notification_period_ptr())
    notif_period_str = obj.get_notification_period_ptr()->get_name();
  std::string svcgrp_str;
  if (!obj.get_parent_groups().empty())
    svcgrp_str = obj.get_parent_groups().front()->get_group_name();

  std::string cg_oss;
  std::string c_oss;

  if (obj.get_contactgroups().empty())
    cg_oss = "\"nullptr\"";
  else {
    std::ostringstream oss;
    oss << obj.get_contactgroups();
    cg_oss = oss.str();
  }
  if (obj.contacts().empty())
    c_oss = "\"nullptr\"";
  else {
    std::ostringstream oss;
    oss << obj.contacts();
    c_oss = oss.str();
  }

  std::string notifications;
  {
    std::ostringstream oss;
    for (int i = 0; i < 6; i++) {
      notification* s{obj.get_current_notifications()[i].get()};
      if (s)
        oss << "  notification_" << i << ": " << *s;
    }
    notifications = oss.str();
  }

  os << "service {\n"
        "  host_name:                            "
     << obj.get_hostname()
     << "\n"
        "  description:                          "
     << obj.description()
     << "\n"
        "  display_name:                         "
     << obj.get_display_name()
     << "\n"
        "  service_check_command:                "
     << obj.check_command()
     << "\n"
        "  event_handler:                        "
     << obj.event_handler()
     << "\n"
        "  initial_state:                        "
     << obj.get_initial_state()
     << "\n"
        "  check_interval:                       "
     << obj.check_interval()
     << "\n"
        "  retry_interval:                       "
     << obj.retry_interval()
     << "\n"
        "  max_attempts:                         "
     << obj.max_check_attempts()
     << "\n"
        "  contact_groups:                       "
     << cg_oss
     << "\n"
        "  contacts:                             "
     << c_oss
     << "\n"
        "  notification_interval:                "
     << obj.get_notification_interval()
     << "\n"
        "  first_notification_delay:             "
     << obj.get_first_notification_delay()
     << "\n"
        "  recovery_notification_delay:          "
     << obj.get_recovery_notification_delay()
     << "\n"
        "  notify_on_unknown:                    "
     << obj.get_notify_on(notifier::unknown)
     << "\n"
        "  notify_on_warning:                    "
     << obj.get_notify_on(notifier::warning)
     << "\n"
        "  notify_on_critical:                   "
     << obj.get_notify_on(notifier::critical)
     << "\n"
        "  notify_on_recovery:                   "
     << obj.get_notify_on(notifier::ok)
     << "\n"
        "  notify_on_flappingstart:              "
     << obj.get_notify_on(notifier::flappingstart)
     << "\n"
        "  notify_on_flappingstop:               "
     << obj.get_notify_on(notifier::flappingstop)
     << "\n"
        "  notify_on_flappingdisabled:           "
     << obj.get_notify_on(notifier::flappingdisabled)
     << "\n"
        "  notify_on_downtime:                   "
     << obj.get_notify_on(notifier::downtime)
     << "\n"
        "  stalk_on_ok:                          "
     << obj.get_stalk_on(notifier::ok)
     << "\n"
        "  stalk_on_warning:                     "
     << obj.get_stalk_on(notifier::warning)
     << "\n"
        "  stalk_on_unknown:                     "
     << obj.get_stalk_on(notifier::unknown)
     << "\n"
        "  stalk_on_critical:                    "
     << obj.get_stalk_on(notifier::critical)
     << "\n"
        "  is_volatile:                          "
     << obj.get_is_volatile()
     << "\n"
        "  notification_period:                  "
     << obj.notification_period() << "\n"
     << notifications
     << "  check_period:                         " << obj.check_period()
     << "\n"
        "  flap_detection_enabled:               "
     << obj.flap_detection_enabled()
     << "\n"
        "  low_flap_threshold:                   "
     << obj.get_low_flap_threshold()
     << "\n"
        "  high_flap_threshold:                  "
     << obj.get_high_flap_threshold()
     << "\n"
        "  flap_detection_on_ok:                 "
     << obj.get_flap_detection_on(notifier::ok)
     << "\n"
        "  flap_detection_on_warning:            "
     << obj.get_flap_detection_on(notifier::warning)
     << "\n"
        "  flap_detection_on_unknown:            "
     << obj.get_flap_detection_on(notifier::unknown)
     << "\n"
        "  flap_detection_on_critical:           "
     << obj.get_flap_detection_on(notifier::critical)
     << "\n"
        "  process_performance_data:             "
     << obj.get_process_performance_data()
     << "\n"
        "  check_freshness:                      "
     << obj.check_freshness_enabled()
     << "\n"
        "  freshness_threshold:                  "
     << obj.get_freshness_threshold()
     << "\n"
        "  accept_passive_service_checks:        "
     << obj.passive_checks_enabled()
     << "\n  event_handler_enabled:                "
     << obj.event_handler_enabled()
     << "\n  checks_enabled:                       "
     << obj.active_checks_enabled()
     << "\n  retain_status_information:            "
     << obj.get_retain_status_information()
     << "\n  retain_nonstatus_information:         "
     << obj.get_retain_nonstatus_information()
     << "\n  notifications_enabled:                "
     << obj.get_notifications_enabled()
     << "\n  obsess_over_service:                  " << obj.obsess_over()
     << "\n  notes:                                " << obj.get_notes()
     << "\n  notes_url:                            " << obj.get_notes_url()
     << "\n  action_url:                           " << obj.get_action_url()
     << "\n  icon_image:                           " << obj.get_icon_image()
     << "\n  icon_image_alt:                       " << obj.get_icon_image_alt()
     << "\n  problem_has_been_acknowledged:        "
     << obj.problem_has_been_acknowledged()
     << "\n  acknowledgement_type:                 "
     << obj.get_acknowledgement()
     << "\n  host_problem_at_last_check:           "
     << obj.get_host_problem_at_last_check()
     << "\n  check_type:                           " << obj.get_check_type()
     << "\n  current_state:                        " << obj.get_current_state()
     << "\n  last_state:                           " << obj.get_last_state()
     << "\n  last_hard_state:                      "
     << obj.get_last_hard_state()
     << "\n  plugin_output:                        " << obj.get_plugin_output()
     << "\n  long_plugin_output:                   "
     << obj.get_long_plugin_output()
     << "\n  perf_data:                            " << obj.get_perf_data()
     << "\n  state_type:                           " << obj.get_state_type()
     << "\n  next_check:                           "
     << string::ctime(obj.get_next_check())
     << "\n  should_be_scheduled:                  "
     << obj.get_should_be_scheduled()
     << "\n  last_check:                           "
     << string::ctime(obj.get_last_check())
     << "\n  current_attempt:                      "
     << obj.get_current_attempt()
     << "\n  current_event_id:                     "
     << obj.get_current_event_id()
     << "\n  last_event_id:                        " << obj.get_last_event_id()
     << "\n  current_problem_id:                   "
     << obj.get_current_problem_id()
     << "\n  last_problem_id:                      "
     << obj.get_last_problem_id()
     << "\n  last_notification:                    "
     << string::ctime(obj.get_last_notification())
     << "\n  next_notification:                    "
     << string::ctime(obj.get_next_notification())
     << "\n  no_more_notifications:                "
     << obj.get_no_more_notifications()
     << "\n  last_state_change:                    "
     << string::ctime(obj.get_last_state_change())
     << "\n  last_hard_state_change:               "
     << string::ctime(obj.get_last_hard_state_change())
     << "\n  last_time_ok:                         "
     << string::ctime(obj.get_last_time_ok())
     << "\n  last_time_warning:                    "
     << string::ctime(obj.get_last_time_warning())
     << "\n  last_time_unknown:                    "
     << string::ctime(obj.get_last_time_unknown())
     << "\n  last_time_critical:                   "
     << string::ctime(obj.get_last_time_critical())
     << "\n  has_been_checked:                     " << obj.has_been_checked()
     << "\n  is_being_freshened:                   "
     << obj.get_is_being_freshened()
     << "\n  notified_on_unknown:                  "
     << obj.get_notified_on(notifier::unknown)
     << "\n  notified_on_warning:                  "
     << obj.get_notified_on(notifier::warning)
     << "\n  notified_on_critical:                 "
     << obj.get_notified_on(notifier::critical)
     << "\n  current_notification_number:          "
     << obj.get_notification_number()
     << "\n  current_notification_id:              "
     << obj.get_current_notification_id()
     << "\n  latency:                              " << obj.get_latency()
     << "\n  execution_time:                       " << obj.get_execution_time()
     << "\n  is_executing:                         " << obj.get_is_executing()
     << "\n  check_options:                        " << obj.get_check_options()
     << "\n  scheduled_downtime_depth:             "
     << obj.get_scheduled_downtime_depth()
     << "\n  pending_flex_downtime:                "
     << obj.get_pending_flex_downtime() << "\n";

  os << "  state_history:                        ";
  for (size_t i{0}, end{obj.get_state_history().size()}; i < end; ++i)
    os << obj.get_state_history()[i] << (i + 1 < end ? ", " : "\n");

  os << "  state_history_index:                  "
     << obj.get_state_history_index()
     << "\n  is_flapping:                          " << obj.get_is_flapping()
     << "\n  flapping_comment_id:                  "
     << obj.get_flapping_comment_id()
     << "\n  percent_state_change:                 "
     << obj.get_percent_state_change()
     << "\n  modified_attributes:                  "
     << obj.get_modified_attributes()
     << "\n  host_ptr:                             "
     << (obj.get_host_ptr() ? obj.get_host_ptr()->name() : "\"nullptr\"")
     << "\n  event_handler_ptr:                    " << evt_str
     << "\n  event_handler_args:                   "
     << obj.get_event_handler_args()
     << "\n  check_command_ptr:                    " << cmd_str
     << "\n  check_command_args:                   "
     << obj.get_check_command_args()
     << "\n  check_period_ptr:                     " << chk_period_str
     << "\n  notification_period_ptr:              " << notif_period_str
     << "\n  servicegroups_ptr:                    " << svcgrp_str << "\n";

  for (auto const& cv : obj.custom_variables)
    os << cv.first << " ; ";

  os << "\n}\n";
  return os;
}

/**
 *  Add a new service to the list in memory.
 *
 *  @param[in] host_name                    Name of the host this
 *                                          service is running on.
 *  @param[in] description                  Service description.
 *  @param[in] display_name                 Display name.
 *  @param[in] check_period                 Check timeperiod name.
 *  @param[in] max_attempts                 Max check attempts.
 *  @param[in] accept_passive_checks        Does this service accept
 *                                          check result submission ?
 *  @param[in] check_interval               Normal check interval.
 *  @param[in] retry_interval               Retry check interval.
 *  @param[in] notification_interval        Notification interval.
 *  @param[in] first_notification_delay     First notification delay.
 *  @param[in] recovery_notification_delay  Recovery notification delay.
 *  @param[in] notification_period          Notification timeperiod
 *                                          name.
 *  @param[in] notify_recovery              Does this service notify
 *                                          when recovering ?
 *  @param[in] notify_unknown               Does this service notify in
 *                                          unknown state ?
 *  @param[in] notify_warning               Does this service notify in
 *                                          warning state ?
 *  @param[in] notify_critical              Does this service notify in
 *                                          critical state ?
 *  @param[in] notify_flapping              Does this service notify
 *                                          when flapping ?
 *  @param[in] notify_downtime              Does this service notify on
 *                                          downtime ?
 *  @param[in] notifications_enabled        Are notifications enabled
 *                                          for this service ?
 *  @param[in] is_volatile                  Is this service volatile ?
 *  @param[in] event_handler                Event handler command name.
 *  @param[in] event_handler_enabled        Whether or not event handler
 *                                          is enabled.
 *  @param[in] check_command                Active check command name.
 *  @param[in] checks_enabled               Are active checks enabled ?
 *  @param[in] flap_detection_enabled       Whether or not flap
 *                                          detection is enabled.
 *  @param[in] low_flap_threshold           Low flap threshold.
 *  @param[in] high_flap_threshold          High flap threshold.
 *  @param[in] flap_detection_on_ok         Is flap detection enabled
 *                                          for ok state ?
 *  @param[in] flap_detection_on_warning    Is flap detection enabled
 *                                          for warning state ?
 *  @param[in] flap_detection_on_unknown    Is flap detection enabled
 *                                          for unknown state ?
 *  @param[in] flap_detection_on_critical   Is flap detection enabled
 *                                          for critical state ?
 *  @param[in] stalk_on_ok                  Stalk on ok state ?
 *  @param[in] stalk_on_warning             Stalk on warning state ?
 *  @param[in] stalk_on_unknown             Stalk on unknown state ?
 *  @param[in] stalk_on_critical            Stalk on critical state ?
 *  @param[in] process_perfdata             Whether or not service
 *                                          performance data should be
 *                                          processed.
 *  @param[in] check_freshness              Enable freshness check ?
 *  @param[in] freshness_threshold          Freshness threshold.
 *  @param[in] notes                        Notes.
 *  @param[in] notes_url                    URL.
 *  @param[in] action_url                   Action URL.
 *  @param[in] icon_image                   Icon image.
 *  @param[in] icon_image_alt               Alternative icon image.
 *  @param[in] retain_status_information    Should Engine retain service
 *                                          status information ?
 *  @param[in] retain_nonstatus_information Should Engine retain service
 *                                          non-status information ?
 *  @param[in] obsess_over_service          Should we obsess over
 *                                          service ?
 *
 *  @return New service.
 */
com::centreon::engine::service* add_service(
    uint64_t host_id,
    uint64_t service_id,
    const std::string& host_name,
    const std::string& description,
    const std::string& display_name,
    const std::string& check_period,
    int max_attempts,
    double check_interval,
    double retry_interval,
    double notification_interval,
    uint32_t first_notification_delay,
    uint32_t recovery_notification_delay,
    const std::string& notification_period,
    bool notify_recovery,
    bool notify_unknown,
    bool notify_warning,
    bool notify_critical,
    bool notify_flapping,
    bool notify_downtime,
    bool notifications_enabled,
    bool is_volatile,
    const std::string& event_handler,
    bool event_handler_enabled,
    const std::string& check_command,
    bool checks_enabled,
    bool accept_passive_checks,
    bool flap_detection_enabled,
    double low_flap_threshold,
    double high_flap_threshold,
    bool flap_detection_on_ok,
    bool flap_detection_on_warning,
    bool flap_detection_on_unknown,
    bool flap_detection_on_critical,
    bool stalk_on_ok,
    bool stalk_on_warning,
    bool stalk_on_unknown,
    bool stalk_on_critical,
    int process_perfdata,
    bool check_freshness,
    int freshness_threshold,
    const std::string& notes,
    const std::string& notes_url,
    const std::string& action_url,
    const std::string& icon_image,
    const std::string& icon_image_alt,
    int retain_status_information,
    int retain_nonstatus_information,
    bool obsess_over_service,
    const std::string& timezone,
    uint64_t icon_id) {
  // Make sure we have everything we need.
  if (!service_id) {
    engine_logger(log_config_error, basic)
        << "Error: Service comes from a database, therefore its service id "
        << "must not be null";
    config_logger->error(
        "Error: Service comes from a database, therefore its service id must "
        "not be null");
    return nullptr;
  } else if (description.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: Service description is not set";
    config_logger->error("Error: Service description is not set");
    return nullptr;
  } else if (host_name.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: Host name of service '" << description << "' is not set";
    config_logger->error("Error: Host name of service '{}' is not set",
                         description);
    return nullptr;
  } else if (check_command.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: Check command of service '" << description << "' on host '"
        << host_name << "' is not set";
    config_logger->error(
        "Error: Check command of service '{}' on host '{}' is not set",
        description, host_name);
    return nullptr;
  }

  uint64_t hid = get_host_id(host_name);
  if (!host_id) {
    engine_logger(log_config_error, basic)
        << "Error: The service '" << description
        << "' cannot be created because"
        << " host '" << host_name << "' does not exist (host_id is null)";
    config_logger->error(
        "Error: The service '{}' cannot be created because host '{}' does not "
        "exist (host_id is null)",
        description, host_name);
    return nullptr;
  } else if (host_id != hid) {
    engine_logger(log_config_error, basic)
        << "Error: The service '" << description
        << "' cannot be created because the host id corresponding to the host"
        << " '" << host_name << "' is not the same as the one in configuration";
    config_logger->error(
        "Error: The service '{}' cannot be created because the host id "
        "corresponding to the host '{}' is not the same as the one in "
        "configuration",
        description, host_name);
    return nullptr;
  }

  // Check values.
  if (max_attempts <= 0 || check_interval < 0 || retry_interval <= 0 ||
      notification_interval < 0) {
    engine_logger(log_config_error, basic)
        << "Error: Invalid max_attempts, check_interval, retry_interval"
           ", or notification_interval value for service '"
        << description << "' on host '" << host_name << "'";
    config_logger->error(
        "Error: Invalid max_attempts, check_interval, retry_interval"
        ", or notification_interval value for service '{}' on host '{}'",
        description, host_name);
    return nullptr;
  }
  // Check if the service is already exist.
  std::pair<uint64_t, uint64_t> id(std::make_pair(host_id, service_id));
  if (is_service_exist(id)) {
    engine_logger(log_config_error, basic)
        << "Error: Service '" << description << "' on host '" << host_name
        << "' has already been defined";
    config_logger->error(
        "Error: Service '{}' on host '{}' has already been defined",
        description, host_name);
    return nullptr;
  }

  // Allocate memory.
  auto obj{std::make_shared<service>(
      host_name, description, display_name.empty() ? description : display_name,
      check_command, checks_enabled, accept_passive_checks, check_interval,
      retry_interval, notification_interval, max_attempts,
      first_notification_delay, recovery_notification_delay,
      notification_period, notifications_enabled, is_volatile, check_period,
      event_handler, event_handler_enabled, notes, notes_url, action_url,
      icon_image, icon_image_alt, flap_detection_enabled, low_flap_threshold,
      high_flap_threshold, check_freshness, freshness_threshold,
      obsess_over_service, timezone, icon_id)};
  try {
    obj->set_acknowledgement(AckType::NONE);
    obj->set_check_options(CHECK_OPTION_NONE);
    uint32_t flap_detection_on;
    flap_detection_on = none;
    flap_detection_on |=
        (flap_detection_on_critical > 0 ? notifier::critical : 0);
    flap_detection_on |= (flap_detection_on_ok > 0 ? notifier::ok : 0);
    flap_detection_on |=
        (flap_detection_on_unknown > 0 ? notifier::unknown : 0);
    flap_detection_on |=
        (flap_detection_on_warning > 0 ? notifier::warning : 0);
    obj->set_flap_detection_on(flap_detection_on);
    obj->set_modified_attributes(MODATTR_NONE);
    uint32_t notify_on;
    notify_on = none;
    notify_on |= (notify_critical > 0 ? notifier::critical : 0);
    notify_on |= (notify_downtime > 0 ? notifier::downtime : 0);
    notify_on |= (notify_flapping > 0
                      ? (notifier::flappingstart | notifier::flappingstop |
                         notifier::flappingdisabled)
                      : 0);
    notify_on |= (notify_recovery > 0 ? notifier::ok : 0);
    notify_on |= (notify_unknown > 0 ? notifier::unknown : 0);
    notify_on |= (notify_warning > 0 ? notifier::warning : 0);
    obj->set_notify_on(notify_on);
    obj->set_process_performance_data(process_perfdata > 0);
    obj->set_retain_nonstatus_information(retain_nonstatus_information > 0);
    obj->set_retain_status_information(retain_status_information > 0);
    obj->set_should_be_scheduled(true);
    uint32_t stalk_on = (stalk_on_critical ? notifier::critical : 0) |
                        (stalk_on_ok ? notifier::ok : 0) |
                        (stalk_on_unknown ? notifier::unknown : 0) |
                        (stalk_on_warning ? notifier::warning : 0);
    obj->set_stalk_on(stalk_on);
    obj->set_state_type(notifier::hard);

    // state_ok = 0, so we don't need to set state_history (memset
    // is used before).
    // for (unsigned int x(0); x < MAX_STATE_HISTORY_ENTRIES; ++x)
    //   obj->state_history[x] = state_ok;

    // Add new items to the list.
    service::services[{obj->get_hostname(), obj->description()}] = obj;
    service::services_by_id[{host_id, service_id}] = obj;
  } catch (...) {
    obj.reset();
  }

  return obj.get();
}

/**
 *  Check if acknowledgement on service expired.
 *
 */
void service::check_for_expired_acknowledgement() {
  if (problem_has_been_acknowledged()) {
    if (acknowledgement_timeout() > 0) {
      time_t now = time(nullptr);
      if (last_acknowledgement() + acknowledgement_timeout() >= now) {
        engine_logger(log_info_message, basic)
            << "Acknowledgement of service '" << description() << "' on host '"
            << this->get_host_ptr()->name() << "' just expired";
        SPDLOG_LOGGER_INFO(
            events_logger,
            "Acknowledgement of service '{}' on host '{}' just expired",
            description(), this->get_host_ptr()->name());
        set_acknowledgement(AckType::NONE);
        // FIXME DBO: could be improved with something smaller.
        // We will see later, I don't know if there are many events concerning
        // acks.
        update_status(STATUS_ACKNOWLEDGEMENT);
      }
    }
  }
}

/**
 *  Get service by host name and service description.
 *
 *  @param[in] host_name           The host name.
 *  @param[in] service_description The service_description.
 *
 *  @return The struct service or throw exception if the
 *          service is not found.
 */
com::centreon::engine::service& engine::find_service(uint64_t host_id,
                                                     uint64_t service_id) {
  service_id_map::const_iterator it(
      service::services_by_id.find({host_id, service_id}));
  if (it == service::services_by_id.end())
    throw engine_error() << "Service '" << service_id << "' on host '"
                         << host_id << "' was not found";
  return *it->second;
}

/**
 *  Get if service exist.
 *
 *  @param[in] id The service id.
 *
 *  @return True if the service is found, otherwise false.
 */
bool engine::is_service_exist(std::pair<uint64_t, uint64_t> const& id) {
  service_id_map::const_iterator it(service::services_by_id.find(id));
  return it != service::services_by_id.end();
}

/**
 * Get the host and service IDs of a service.
 *
 *  @param[in] host  The host name.
 *  @param[in] svc   The service description.
 *
 *  @return  Pair of ID if found, pair of 0 otherwise.
 */
std::pair<uint64_t, uint64_t> engine::get_host_and_service_id(
    const std::string& host,
    const std::string& svc) {
  service_map::const_iterator found = service::services.find({host, svc});
  return found != service::services.end()
             ? std::pair<uint64_t, uint64_t>{found->second->host_id(),
                                             found->second->service_id()}
             : std::pair<uint64_t, uint64_t>{0u, 0u};
}

/**
 * Get the host and service names of a service given by its ids.
 *
 *  @param[in] host  The host id.
 *  @param[in] svc   The service id.
 *
 *  @return  Pair of strings if found, pair of empty strings otherwise.
 */
std::pair<std::string, std::string> engine::get_host_and_service_names(
    const uint64_t host_id,
    const uint64_t service_id) {
  auto it = service::services_by_id.find(std::make_pair(host_id, service_id));
  if (it != service::services_by_id.end())
    return std::make_pair(it->second->get_hostname(),
                          it->second->description());
  else
    return {"", ""};
}

/**
 *  Get a service' ID.
 *
 *  @param[in] host  The host name.
 *  @param[in] svc   The service description.
 *
 *  @return The service ID if found, 0 otherwise.
 */
uint64_t engine::get_service_id(const std::string& host,
                                const std::string& svc) {
  return get_host_and_service_id(host, svc).second;
}

/**
 *  Schedule acknowledgement expiration check.
 *
 */
void service::schedule_acknowledgement_expiration() {
  if (acknowledgement_timeout() > 0 && last_acknowledgement() != (time_t)0) {
    events::loop::instance().schedule(
        std::make_unique<timed_event>(
            timed_event::EVENT_EXPIRE_SERVICE_ACK,
            last_acknowledgement() + acknowledgement_timeout(), false, 0,
            nullptr, true, this, nullptr, 0),
        false);
  }
}

void service::set_host_id(uint64_t host_id) {
  _host_id = host_id;
}

uint64_t service::host_id() const {
  return _host_id;
}

void service::set_service_id(uint64_t service_id) {
  _service_id = service_id;
}

uint64_t service::service_id() const {
  return _service_id;
}

/**
 * @brief update hostname and associate command
 *
 * @param name
 */
void service::set_hostname(const std::string& name) {
  if (_hostname == name) {
    return;
  }
  std::shared_ptr<commands::command> cmd = get_check_command_ptr();
  if (cmd) {
    cmd->unregister_host_serv(_hostname, description());
  }
  _hostname = name;
  if (cmd) {
    cmd->register_host_serv(_hostname, description());
  }
}

/**
 * @brief Get the hostname of the host associated with this downtime.
 *
 * @return A string reference to the host name.
 */
const std::string& service::get_hostname() const {
  return _hostname;
}

/**
 * @brief update service name
 * update also associate command
 *
 * @param desc
 */
void service::set_name(std::string const& desc) {
  if (desc == name()) {
    return;
  }
  std::shared_ptr<commands::command> cmd = get_check_command_ptr();
  if (cmd) {
    cmd->unregister_host_serv(_hostname, name());
  }
  notifier::set_name(desc);
  if (cmd) {
    cmd->register_host_serv(_hostname, name());
  }
}

void service::set_description(const std::string& desc) {
  set_name(desc);
}

/**
 * @brief Get the description of the service.
 *
 * @return A string reference to the description.
 */
const std::string& service::description() const {
  return name();
}

/**
 * @brief set the event handler arguments
 *
 *  @param[in] event_hdl_args the event handler arguments
 */
void service::set_event_handler_args(const std::string& event_hdl_args) {
  _event_handler_args = event_hdl_args;
}

/**
 * @brief Get the event handler arguments of the service.
 *
 * @return A string reference to the event handler arguments.
 */
const std::string& service::get_event_handler_args() const {
  return _event_handler_args;
}

/**
 * @brief set the command arguments
 *
 *  @param[in] cmd_args the command arguments
 */
void service::set_check_command_args(const std::string& cmd_args) {
  _check_command_args = cmd_args;
}

/**
 * @brief Get the event command arguments of the service.
 *
 * @return A string reference to the command arguments.
 */
const std::string& service::get_check_command_args() const {
  return _check_command_args;
}

static constexpr bool state_changes_use_cached_state = true;

/**
 * @brief Handle asynchronously the result of a check.
 *
 * @param queued_check_result The check result.
 *
 * @return OK or ERROR.
 *
 */
int service::handle_async_check_result(
    const check_result& queued_check_result) {
  time_t next_service_check = 0L;
  time_t preferred_time = 0L;
  time_t next_valid_time = 0L;
  int reschedule_check = false;
  int state_change = false;
  int hard_state_change = false;
  int first_host_check_initiated = false;
  host::host_state route_result = host::state_up;
  int state_was_logged = false;
  std::string old_plugin_output;
  std::list<service*> check_servicelist;
  com::centreon::engine::service* master_service = nullptr;
  int run_async_check = true;
  int flapping_check_done = false;
#ifdef LEGACY_CONF
  uint32_t interval_length = config->interval_length();
  bool accept_passive_service_checks = config->accept_passive_service_checks();
  bool log_passive_checks = config->log_passive_checks();
  uint32_t cached_host_check_horizon = config->cached_host_check_horizon();
  bool obsess_over_services = config->obsess_over_services();
  bool enable_predictive_service_dependency_checks =
      config->enable_predictive_service_dependency_checks();
  uint32_t cached_service_check_horizon =
      config->cached_service_check_horizon();
#else
  uint32_t interval_length = pb_config.interval_length();
  bool accept_passive_service_checks =
      pb_config.accept_passive_service_checks();
  bool log_passive_checks = pb_config.log_passive_checks();
  uint32_t cached_host_check_horizon = pb_config.cached_host_check_horizon();
  bool obsess_over_services = pb_config.obsess_over_services();
  bool enable_predictive_service_dependency_checks =
      pb_config.enable_predictive_service_dependency_checks();
  uint32_t cached_service_check_horizon =
      pb_config.cached_service_check_horizon();
#endif

  SPDLOG_LOGGER_TRACE(functions_logger,
                      "handle_async_service_check_result() service {} res:{}",
                      name(), queued_check_result);

  /* get the current time */
  time_t current_time = std::time(nullptr);

  /* update the execution time for this check (millisecond resolution) */
  double execution_time =
      static_cast<double>(queued_check_result.get_finish_time().tv_sec -
                          queued_check_result.get_start_time().tv_sec) +
      static_cast<double>(queued_check_result.get_finish_time().tv_usec -
                          queued_check_result.get_start_time().tv_usec) /
          1000000.0;
  if (execution_time < 0.0)
    execution_time = 0.0;

  engine_logger(dbg_checks, basic)
      << "** Handling check result for service '" << name() << "' on host '"
      << _hostname << "'...";
  SPDLOG_LOGGER_TRACE(
      checks_logger,
      "** Handling check result for service '{}' on host '{}'...", name(),
      _hostname);
  engine_logger(dbg_checks, more)
      << "HOST: " << _hostname << ", SERVICE: " << name() << ", CHECK TYPE: "
      << (queued_check_result.get_check_type() == check_active ? "Active"
                                                               : "Passive")
      << ", OPTIONS: " << queued_check_result.get_check_options()
      << ", RESCHEDULE: "
      << (queued_check_result.get_reschedule_check() ? "Yes" : "No")
      << ", EXITED OK: " << (queued_check_result.get_exited_ok() ? "Yes" : "No")
      << ", EXEC TIME: " << execution_time
      << ", return CODE: " << queued_check_result.get_return_code()
      << ", OUTPUT: " << queued_check_result.get_output();
  SPDLOG_LOGGER_DEBUG(
      checks_logger,
      "HOST: {}, SERVICE: {}, CHECK TYPE: {}, OPTIONS: {}, RESCHEDULE: {}, "
      "EXITED OK: {}, EXEC TIME: {}, return CODE: {}, OUTPUT: {}",
      _hostname, name(),
      queued_check_result.get_check_type() == check_active ? "Active"
                                                           : "Passive",
      queued_check_result.get_check_options(),
      queued_check_result.get_reschedule_check() ? "Yes" : "No",
      queued_check_result.get_exited_ok() ? "Yes" : "No", execution_time,
      queued_check_result.get_return_code(), queued_check_result.get_output());

  /* decrement the number of service checks still out there... */
  if (queued_check_result.get_check_type() == check_active &&
      currently_running_service_checks > 0)
    currently_running_service_checks--;

  /*
   * skip this service check results if its passive and we aren't accepting
   * passive check results */
  if (queued_check_result.get_check_type() == check_passive) {
    if (!accept_passive_service_checks) {
      engine_logger(dbg_checks, basic)
          << "Discarding passive service check result because passive "
             "service checks are disabled globally.";
      SPDLOG_LOGGER_TRACE(
          checks_logger,
          "Discarding passive service check result because passive "
          "service checks are disabled globally.");
      return ERROR;
    }
    if (!passive_checks_enabled()) {
      engine_logger(dbg_checks, basic)
          << "Discarding passive service check result because passive "
             "checks are disabled for this service.";
      SPDLOG_LOGGER_TRACE(
          checks_logger,
          "Discarding passive service check result because passive "
          "checks are disabled for this service.");
      return ERROR;
    }
  }

  /*
   * clear the freshening flag (it would have been set if this service was
   * determined to be stale)
   */
  if (queued_check_result.get_check_options() & CHECK_OPTION_FRESHNESS_CHECK)
    set_is_being_freshened(false);

  /* clear the execution flag if this was an active check */
  if (queued_check_result.get_check_type() == check_active)
    set_is_executing(false);

  /* DISCARD INVALID FRESHNESS CHECK RESULTS */
  /* If a services goes stale, Engine will initiate a forced check in
  ** order to freshen it.  There is a race condition whereby a passive
  ** check could arrive between the 1) initiation of the forced check
  ** and 2) the time when the forced check result is processed here.
  ** This would make the service fresh again, so we do a quick check to
  ** make sure the service is still stale before we accept the check
  ** result.
  */
  if ((queued_check_result.get_check_options() &
       CHECK_OPTION_FRESHNESS_CHECK) &&
      is_result_fresh(current_time, false)) {
    engine_logger(dbg_checks, basic)
        << "Discarding service freshness check result because the service "
           "is currently fresh (race condition avoided).";
    SPDLOG_LOGGER_TRACE(
        checks_logger,
        "Discarding service freshness check result because the service "
        "is currently fresh (race condition avoided).");
    return OK;
  }

  /* check latency is passed to us */
  set_latency(queued_check_result.get_latency());

  set_execution_time(execution_time);

  /* get the last check time */
  set_last_check(queued_check_result.get_start_time().tv_sec);

  /* was this check passive or active? */
  set_check_type(queued_check_result.get_check_type());

  /* update check statistics for passive checks */
  if (queued_check_result.get_check_type() == check_passive)
    update_check_stats(PASSIVE_SERVICE_CHECK_STATS,
                       queued_check_result.get_start_time().tv_sec);

  /*
   * should we reschedule the next service check? NOTE: This may be overridden
   * later...
   */
  reschedule_check = queued_check_result.get_reschedule_check();

  /* save the old service status info */
  _last_state = _current_state;

  /* save old plugin output */
  old_plugin_output = get_plugin_output();

  /*
   * if there was some error running the command, just skip it (this
   * shouldn't be happening)
   */
  if (!queued_check_result.get_exited_ok()) {
    engine_logger(log_runtime_warning, basic)
        << "Warning:  Check of service '" << name() << "' on host '"
        << _hostname << "' did not exit properly!";
    SPDLOG_LOGGER_WARN(
        runtime_logger,
        "Warning:  Check of service '{}' on host '{}' did not exit properly!",
        name(), _hostname);

    set_plugin_output("(Service check did not exit properly)");
    _current_state = service::state_unknown;
  }
  /* make sure the return code is within bounds */
  else if (queued_check_result.get_return_code() < 0 ||
           queued_check_result.get_return_code() > 3) {
    engine_logger(log_runtime_warning, basic)
        << "Warning: return (code of " << queued_check_result.get_return_code()
        << " for check of service '" << name() << "' on host '" << _hostname
        << "' was out of bounds."
        << (queued_check_result.get_return_code() == 126
                ? "Make sure the plugin you're trying to run is executable."
                : (queued_check_result.get_return_code() == 127
                       ? " Make sure the plugin you're trying to run actually "
                         "exists."
                       : ""));
    SPDLOG_LOGGER_WARN(
        runtime_logger,
        "Warning: return (code of {} for check of service '{}' on host '{}' "
        "was out of bounds.{}",
        queued_check_result.get_return_code(), name(), _hostname,
        (queued_check_result.get_return_code() == 126
             ? "Make sure the plugin you're trying to run is executable."
             : (queued_check_result.get_return_code() == 127
                    ? " Make sure the plugin you're trying to run actually "
                      "exists."
                    : "")));

    std::ostringstream oss;
    oss << "(Return code of " << queued_check_result.get_return_code()
        << " is out of bounds"
        << (queued_check_result.get_return_code() == 126
                ? " - plugin may not be executable"
                : (queued_check_result.get_return_code() == 127
                       ? " - plugin may be missing"
                       : ""))
        << ')';

    set_plugin_output(oss.str());
    _current_state = service::state_unknown;
  }
  /* else the return code is okay... */
  else {
    /*
     * parse check output to get: (1) short output, (2) long output,
     * (3) perf data
     */
    std::string output{queued_check_result.get_output()};
    std::string plugin_output;
    std::string long_plugin_output;
    std::string perf_data;
    parse_check_output(output, plugin_output, long_plugin_output, perf_data,
                       true, false);

    set_long_plugin_output(long_plugin_output);
    set_perf_data(perf_data);
    /* make sure the plugin output isn't null */
    if (plugin_output.empty())
      set_plugin_output("(No output returned from plugin)");
    else {
      std::replace(plugin_output.begin(), plugin_output.end(), ';', ':');

      /*
       * replace semicolons in plugin output (but not performance data) with
       * colons
       */
      set_plugin_output(plugin_output);
    }

    engine_logger(dbg_checks, most)
        << "Parsing check output...\n"
        << "Short Output:\n"
        << (get_plugin_output().empty() ? "nullptr" : get_plugin_output())
        << "\n"
        << "Long Output:\n"
        << (get_long_plugin_output().empty() ? "nullptr"
                                             : get_long_plugin_output())
        << "\n"
        << "Perf Data:\n"
        << (get_perf_data().empty() ? "nullptr" : get_perf_data());
    SPDLOG_LOGGER_DEBUG(
        checks_logger,
        "Parsing check output Short Output: {} Long Output: {} Perf Data: {}",
        get_plugin_output().empty() ? "nullptr" : get_plugin_output(),
        get_long_plugin_output().empty() ? "nullptr" : get_long_plugin_output(),
        get_perf_data().empty() ? "nullptr" : get_perf_data());

    /* grab the return code */
    _current_state = static_cast<service::service_state>(
        queued_check_result.get_return_code());
    SPDLOG_LOGGER_DEBUG(
        checks_logger, "now host:{} serv:{} _current_state={} state_type={}",
        _hostname, name(), static_cast<uint32_t>(_current_state),
        (get_state_type() == soft ? "SOFT" : "HARD"));
  }

  /* record the last state time */
  switch (_current_state) {
    case service::state_ok:
      set_last_time_ok(get_last_check());
      break;

    case service::state_warning:
      set_last_time_warning(get_last_check());
      break;

    case service::state_unknown:
      set_last_time_unknown(get_last_check());
      break;

    case service::state_critical:
      set_last_time_critical(get_last_check());
      break;

    default:
      break;
  }

  /*
   * log passive checks - we need to do this here, as some my bypass external
   * commands by getting dropped in checkresults dir
   */
  if (get_check_type() == check_passive) {
    if (log_passive_checks)
      engine_logger(log_passive_check, basic)
          << "PASSIVE SERVICE CHECK: " << _hostname << ";" << name() << ";"
          << _current_state << ";" << get_plugin_output();
    SPDLOG_LOGGER_INFO(checks_logger, "PASSIVE SERVICE CHECK: {};{};{};{}",
                       _hostname, name(), static_cast<uint32_t>(_current_state),
                       get_plugin_output());
  }

  host* hst{get_host_ptr()};
  /* if the service check was okay... */
  if (_current_state == service::state_ok) {
    /* if the host has never been checked before, verify its status
     * only do this if 1) the initial state was set to non-UP or 2) the host
     * is not scheduled to be checked soon (next 5 minutes)
     */
    if (!hst->has_been_checked() &&
        (hst->get_initial_state() != host::state_up ||
         (unsigned long)hst->get_next_check() == 0L ||
         (unsigned long)(hst->get_next_check() - current_time) > 300)) {
      /* set a flag to remember that we launched a check */
      first_host_check_initiated = true;

      hst->run_async_check(CHECK_OPTION_NONE, 0.0, false, false, nullptr,
                           nullptr);
    }
  }

  if (_last_state == state_ok && _current_state != _last_state)
    set_current_attempt(1);
  else if (get_state_type() == soft &&
           get_current_attempt() < max_check_attempts())
    add_current_attempt(1);

  engine_logger(dbg_checks, most)
      << "ST: " << (get_state_type() == soft ? "SOFT" : "HARD")
      << "  CA: " << get_current_attempt() << "  MA: " << max_check_attempts()
      << "  CS: " << _current_state << "  LS: " << _last_state
      << "  LHS: " << _last_hard_state;
  SPDLOG_LOGGER_DEBUG(
      checks_logger, "ST: {}  CA: {} MA: {} CS: {} LS: {} LHS: {}",
      (get_state_type() == soft ? "SOFT" : "HARD"), get_current_attempt(),
      max_check_attempts(), static_cast<uint32_t>(_current_state),
      static_cast<uint32_t>(_last_state),
      static_cast<uint32_t>(_last_hard_state));

  /* check for a state change (either soft or hard) */
  if (_current_state != _last_state) {
    engine_logger(dbg_checks, most)
        << "Service has changed state since last check!";
    SPDLOG_LOGGER_DEBUG(checks_logger,
                        "Service has changed state since last check {} => {}",
                        static_cast<uint32_t>(_last_state),
                        static_cast<uint32_t>(_current_state));
    state_change = true;
  }

  /*
   * checks for a hard state change where host was down at last service
   * check this occurs in the case where host goes down and service current
   * attempt gets reset to 1 if this check is not made, the service recovery
   * looks like a soft recovery instead of a hard one
   */
  if (_host_problem_at_last_check && _current_state == service::state_ok) {
    engine_logger(dbg_checks, most) << "Service had a HARD STATE CHANGE!!";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Service had a HARD STATE CHANGE!!");
    hard_state_change = true;
  }

  /*
   * check for a "normal" hard state change where max check attempts is
   * reached
   */
  if (get_current_attempt() >= max_check_attempts() &&
      (_current_state != _last_hard_state ||
       get_last_state_change() > get_last_hard_state_change())) {
    engine_logger(dbg_checks, most) << "Service had a HARD STATE CHANGE!!";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Service had a HARD STATE CHANGE!!");
    hard_state_change = true;
  }

  /* a state change occurred...
   * reset last and next notification times and acknowledgement flag if
   * necessary, misc other stuff
   */
  if (state_change || hard_state_change) {
    /* reschedule the service check */
    reschedule_check = true;

    /* reset notification times */
    set_last_notification(static_cast<time_t>(0));
    set_next_notification(static_cast<time_t>(0));

    /* reset notification suppression option */
    set_no_more_notifications(false);

    if (AckType::NORMAL == get_acknowledgement() &&
        (state_change || !hard_state_change)) {
      set_acknowledgement(AckType::NONE);

      /* remove any non-persistant comments associated with the ack */
      comment::delete_service_acknowledgement_comments(this);
    } else if (get_acknowledgement() == AckType::STICKY &&
               _current_state == service::state_ok) {
      set_acknowledgement(AckType::NONE);

      /* remove any non-persistant comments associated with the ack */
      comment::delete_service_acknowledgement_comments(this);
    }

    /*
     * do NOT reset current notification number!!!
     * hard changes between non-OK states should continue to be escalated,
     * so don't reset current notification number
     */
    /*this->current_notification_number=0; */
  }

  /* initialize the last host and service state change times if necessary */
  if (get_last_state_change() == (time_t)0)
    set_last_state_change(get_last_check());
  if (get_last_hard_state_change() == (time_t)0)
    set_last_hard_state_change(get_last_check());
  if (hst->get_last_state_change() == (time_t)0)
    hst->set_last_state_change(get_last_check());
  if (hst->get_last_hard_state_change() == (time_t)0)
    hst->set_last_hard_state_change(get_last_check());

  /* update last service state change times */
  if (state_change)
    set_last_state_change(get_last_check());
  if (hard_state_change)
    set_last_hard_state_change(get_last_check());

  /* update the event and problem ids */
  if (state_change) {
    /* always update the event id on a state change */
    set_last_event_id(get_current_event_id());
    set_current_event_id(next_event_id);
    next_event_id++;

    /* update the problem id when transitioning to a problem state */
    if (_last_state == service::state_ok) {
      /* don't reset last problem id, or it will be zero the next time a problem
       * is encountered */
      /* this->last_problem_id=this->current_problem_id; */
      set_current_problem_id(next_problem_id);
      next_problem_id++;
    }

    /* clear the problem id when transitioning from a problem state to an OK
     * state */
    if (_current_state == service::state_ok) {
      set_last_problem_id(get_current_problem_id());
      set_current_problem_id(0L);
    }
  }

  /**************************************/
  /******* SERVICE CHECK OK LOGIC *******/
  /**************************************/

  /* if the service is up and running OK... */
  if (_current_state == service::state_ok) {
    engine_logger(dbg_checks, more) << "Service is OK.";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Service is OK.");

    /* reset the acknowledgement flag (this should already have been done, but
     * just in case...) */
    set_acknowledgement(AckType::NONE);

    /* verify the route to the host and send out host recovery notifications */
    if (hst->get_current_state() != host::state_up) {
      engine_logger(dbg_checks, more)
          << "Host is NOT UP, so we'll check it to see if it recovered...";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Host is NOT UP, so we'll check it to see if it recovered...");

      /* 09/23/07 EG don't launch a new host check if we already did so earlier
       */
      if (first_host_check_initiated) {
        engine_logger(dbg_checks, more)
            << "First host check was already initiated, so we'll skip a "
               "new host check.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "First host check was already initiated, so we'll skip a "
            "new host check.");
      } else {
        /* can we use the last cached host state? */
        /* usually only use cached host state if no service state change has
         * occurred */
        if ((!state_change || state_changes_use_cached_state) &&
            hst->has_been_checked() &&
            (static_cast<unsigned long>(current_time - hst->get_last_check()) <=
             cached_host_check_horizon)) {
          engine_logger(dbg_checks, more)
              << "* Using cached host state: " << hst->get_current_state();
          SPDLOG_LOGGER_DEBUG(checks_logger, "* Using cached host state: {}",
                              static_cast<uint32_t>(hst->get_current_state()));
          update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
          update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);
        }

        /* else launch an async (parallel) check of the host */
        else
          hst->run_async_check(CHECK_OPTION_NONE, 0.0, false, false, nullptr,
                               nullptr);
      }
    }

    /* if a hard service recovery has occurred... */
    if (hard_state_change) {
      engine_logger(dbg_checks, more) << "Service experienced a HARD RECOVERY.";
      SPDLOG_LOGGER_DEBUG(checks_logger,
                          "Service experienced a HARD RECOVERY.");

      /* set the state type macro */
      set_state_type(hard);
      set_last_hard_state_change(get_last_check());
      set_current_attempt(1);

      /* log the service recovery */
      log_event();
      state_was_logged = true;

      /* 10/04/07 check to see if the service and/or associate host is flapping
       */
      /* this should be done before a notification is sent out to ensure the
       * host didn't just start flapping */
      check_for_flapping(true, true);
      hst->check_for_flapping(true, false, true);
      flapping_check_done = true;

      /* notify contacts about the service recovery */
      notify(reason_recovery, "", "", notification_option_none);

      /* run the service event handler to handle the hard state change */
      handle_service_event();
    }

    /* else if a soft service recovery has occurred... */
    else if (state_change) {
      engine_logger(dbg_checks, more) << "Service experienced a SOFT RECOVERY.";
      SPDLOG_LOGGER_DEBUG(checks_logger,
                          "Service experienced a SOFT RECOVERY.");

      /* this is a soft recovery */
      set_state_type(soft);
      int attempt = max_check_attempts() - 1;
      set_current_attempt(attempt < 1 ? 1 : attempt);

      /* log the soft recovery */
      log_event();
      state_was_logged = true;

      /* run the service event handler to handle the soft state change */
      handle_service_event();
    }

    /* else no service state change has occurred... */
    else {
      engine_logger(dbg_checks, more) << "Service did not change state.";
      SPDLOG_LOGGER_DEBUG(checks_logger, "Service did not change state.");
    }
    /* Check if we need to send a recovery notification */
    notify(reason_recovery, "", "", notification_option_none);

    /* should we obsessive over service checks? */
    if (obsess_over_services)
      obsessive_compulsive_service_check_processor();

    /* reset all service variables because its okay now... */
    _host_problem_at_last_check = false;

    _last_hard_state = service::state_ok;
    set_last_notification(static_cast<time_t>(0));
    set_next_notification(static_cast<time_t>(0));
    set_acknowledgement(AckType::NONE);
    set_no_more_notifications(false);

    if (reschedule_check)
      next_service_check =
          (time_t)(get_last_check() + check_interval() * interval_length);
  }

  /*******************************************/
  /******* SERVICE CHECK PROBLEM LOGIC *******/
  /*******************************************/

  /* hey, something's not working quite like it should... */
  else {
    engine_logger(dbg_checks, more) << "Service is in a non-OK state!";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Service is in a non-OK state!");

    /* check the route to the host if its up right now... */
    if (hst->get_current_state() == host::state_up) {
      engine_logger(dbg_checks, more)
          << "Host is currently UP, so we'll recheck its state to "
             "make sure...";
      SPDLOG_LOGGER_DEBUG(checks_logger,
                          "Host is currently UP, so we'll recheck its state to "
                          "make sure...");

      /* previous logic was to simply run a sync (serial) host check */
      /* can we use the last cached host state? */
      /* only use cached host state if no service state change has occurred */
      if ((!state_change || state_changes_use_cached_state) &&
          hst->has_been_checked() &&
          static_cast<unsigned long>(current_time - hst->get_last_check()) <=
              cached_host_check_horizon) {
        /* use current host state as route result */
        route_result = hst->get_current_state();
        engine_logger(dbg_checks, more)
            << "* Using cached host state: " << hst->get_current_state();
        SPDLOG_LOGGER_DEBUG(checks_logger, "* Using cached host state: {}",
                            static_cast<uint32_t>(hst->get_current_state()));
        update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
        update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);
      }

      /* else launch an async (parallel) check of the host */
      /* CHANGED 02/15/08 only if service changed state since service was last
         checked */
      else if (state_change) {
        /* use current host state as route result */
        route_result = hst->get_current_state();
        hst->run_async_check(CHECK_OPTION_NONE, 0.0, false, false, nullptr,
                             nullptr);
      }

      /* ADDED 02/15/08 */
      /* else assume same host state */
      else {
        route_result = hst->get_current_state();
        engine_logger(dbg_checks, more)
            << "* Using last known host state: " << hst->get_current_state();
        SPDLOG_LOGGER_DEBUG(checks_logger, "* Using last known host state: {}",
                            static_cast<uint32_t>(hst->get_current_state()));
        update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
        update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);
      }
    }

    /* else the host is either down or unreachable, so recheck it if necessary
     */
    else {
      engine_logger(dbg_checks, more) << "Host is currently DOWN/UNREACHABLE.";
      SPDLOG_LOGGER_DEBUG(checks_logger, "Host is currently DOWN/UNREACHABLE.");

      /* the service wobbled between non-OK states, so check the host... */
      if ((state_change && !state_changes_use_cached_state) &&
          _last_hard_state != service::state_ok) {
        engine_logger(dbg_checks, more)
            << "Service wobbled between non-OK states, so we'll recheck"
               " the host state...";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "Service wobbled between non-OK states, so we'll recheck"
            " the host state...");
        /* previous logic was to simply run a sync (serial) host check */
        /* use current host state as route result */
        route_result = hst->get_current_state();
        hst->run_async_check(CHECK_OPTION_NONE, 0.0, false, false, nullptr,
                             nullptr);
        /*perform_on_demand_host_check(hst,&route_result,CHECK_OPTION_NONE,true,config->cached_host_check_horizon());
         */
      }

      /* else fake the host check, but (possibly) resend host notifications to
         contacts... */
      else {
        engine_logger(dbg_checks, more)
            << "Assuming host is in same state as before...";
        SPDLOG_LOGGER_DEBUG(checks_logger,
                            "Assuming host is in same state as before...");

        /* if the host has never been checked before, set the checked flag and
         * last check time */
        /* 03/11/06 EG Note: This probably never evaluates to false, present for
         * historical reasons only, can probably be removed in the future */
        if (!hst->has_been_checked()) {
          hst->set_has_been_checked(true);
          hst->set_last_check(get_last_check());
        }

        /* fake the route check result */
        route_result = hst->get_current_state();

        /* possibly re-send host notifications... */
        hst->notify(reason_normal, "", "", notification_option_none);
      }
    }

    /* if the host is down or unreachable ... */
    /* 05/29/2007 NOTE: The host might be in a SOFT problem state due to host
     * check retries/caching.  Not sure if we should take that into account and
     * do something different or not... */
    if (route_result != host::state_up) {
      engine_logger(dbg_checks, most)
          << "Host is not UP, so we mark state changes if appropriate";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Host is not UP, so we mark state changes if appropriate");

      /* "fake" a hard state change for the service - well, its not really fake,
       * but it didn't get caught earlier... */
      if (_last_hard_state != _current_state)
        hard_state_change = true;

      /* update last state change times */
      if (state_change || hard_state_change)
        set_last_state_change(get_last_check());
      if (hard_state_change) {
        set_last_hard_state_change(get_last_check());
        set_state_type(hard);
        _last_hard_state = _current_state;
      }

      /* put service into a hard state without attempting check retries and
       * don't send out notifications about it */
      _host_problem_at_last_check = true;
    }

    /* the host is up - it recovered since the last time the service was
       checked... */
    else if (_host_problem_at_last_check) {
      /* next time the service is checked we shouldn't get into this same
       * case... */
      _host_problem_at_last_check = false;

      /* reset the current check counter, so we give the service a chance */
      /* this helps prevent the case where service has N max check attempts, N-1
       * of which have already occurred. */
      /* if we didn't do this, the next check might fail and result in a hard
       * problem - we should really give it more time */
      /* ADDED IF STATEMENT 01-17-05 EG */
      /* 01-17-05: Services in hard problem states before hosts went down would
       * sometimes come back as soft problem states after */
      /* the hosts recovered.  This caused problems, so hopefully this will fix
       * it */
      if (get_state_type() == soft)
        set_current_attempt(1);
    }

    engine_logger(dbg_checks, more)
        << "Current/Max Attempt(s): " << get_current_attempt() << '/'
        << max_check_attempts();
    SPDLOG_LOGGER_DEBUG(checks_logger, "Current/Max Attempt(s): {}/{}",
                        get_current_attempt(), max_check_attempts());

    /* if we should retry the service check, do so (except it the host is down
     * or unreachable!) */
    if (get_current_attempt() < max_check_attempts()) {
      /* the host is down or unreachable, so don't attempt to retry the service
       * check */
      if (route_result != host::state_up) {
        engine_logger(dbg_checks, more)
            << "Host isn't UP, so we won't retry the service check...";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "Host isn't UP, so we won't retry the service check...");

        /* the host is not up, so reschedule the next service check at regular
         * interval */
        if (reschedule_check)
          next_service_check =
              (time_t)(get_last_check() + check_interval() * interval_length);

        /* log the problem as a hard state if the host just went down */
        if (hard_state_change) {
          log_event();
          state_was_logged = true;

          /* run the service event handler to handle the hard state */
          handle_service_event();
        }
      }

      /* the host is up, so continue to retry the service check */
      else {
        engine_logger(dbg_checks, more)
            << "Host is UP, so we'll retry the service check...";
        SPDLOG_LOGGER_DEBUG(checks_logger,
                            "Host is UP, so we'll retry the service check...");

        /* this is a soft state */
        set_state_type(soft);

        /* log the service check retry */
        log_event();
        state_was_logged = true;

        /* run the service event handler to handle the soft state */
        handle_service_event();

        if (reschedule_check)
          next_service_check =
              (time_t)(get_last_check() + retry_interval() * interval_length);
      }

      /* perform dependency checks on the second to last check of the service */
      if (enable_predictive_service_dependency_checks &&
          get_current_attempt() == max_check_attempts() - 1) {
        engine_logger(dbg_checks, more)
            << "Looking for services to check for predictive "
               "dependency checks...";
        SPDLOG_LOGGER_DEBUG(checks_logger,
                            "Looking for services to check for predictive "
                            "dependency checks...");

        /* check services that THIS ONE depends on for notification AND
         * execution */
        /* we do this because we might be sending out a notification soon and we
         * want the dependency logic to be accurate */
        std::pair<std::string, std::string> id({_hostname, name()});
        auto p(servicedependency::servicedependencies.equal_range(id));
        for (servicedependency_mmap::const_iterator it{p.first}, end{p.second};
             it != end; ++it) {
          servicedependency* temp_dependency{it->second.get()};

          if (temp_dependency->dependent_service_ptr == this &&
              temp_dependency->master_service_ptr) {
            master_service = temp_dependency->master_service_ptr;
            engine_logger(dbg_checks, most)
                << "Predictive check of service '"
                << master_service->description() << "' on host '"
                << master_service->get_hostname() << "' queued.";
            SPDLOG_LOGGER_DEBUG(
                checks_logger,
                "Predictive check of service '{}' on host '{}' queued.",
                master_service->description(), master_service->get_hostname());
            check_servicelist.push_back(master_service);
          }
        }
      }
    }

    /* we've reached the maximum number of service rechecks, so handle the error
     */
    else {
      engine_logger(dbg_checks, more)
          << "Service has reached max number of rechecks, so we'll "
             "handle the error...";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Service has reached max number of rechecks, so we'll "
          "handle the error...");

      /* this is a hard state */
      set_state_type(hard);

      /* if we've hard a hard state change... */
      if (hard_state_change) {
        /* log the service problem (even if host is not up, which is new in
         * 0.0.5) */
        log_event();
        state_was_logged = true;
      }

      /* else log the problem (again) if this service is flagged as being
         volatile */
      else if (get_is_volatile()) {
        log_event();
        state_was_logged = true;
      }

      /* check for start of flexible (non-fixed) scheduled downtime if we just
       * had a hard error */
      /* we need to check for both, state_change (SOFT) and hard_state_change
       * (HARD) values */
      if ((hard_state_change || state_change) &&
          get_pending_flex_downtime() > 0)
        downtime_manager::instance().check_pending_flex_service_downtime(this);

      /* 10/04/07 check to see if the service and/or associate host is flapping
       */
      /* this should be done before a notification is sent out to ensure the
       * host didn't just start flapping */
      check_for_flapping(true, true);
      hst->check_for_flapping(true, false, true);
      flapping_check_done = true;

      /* (re)send notifications out about this service problem if the host is up
       * (and was at last check also) and the dependencies were okay... */
      notify(reason_normal, "", "", notification_option_none);

      /* run the service event handler if we changed state from the last hard
       * state or if this service is flagged as being volatile */
      if (hard_state_change || get_is_volatile())
        handle_service_event();

      /* save the last hard state */
      _last_hard_state = _current_state;

      /* reschedule the next check at the regular interval */
      if (reschedule_check)
        next_service_check =
            (time_t)(get_last_check() + check_interval() * interval_length);
    }

    /* should we obsessive over service checks? */
    if (obsess_over_services)
      obsessive_compulsive_service_check_processor();
  }

  bool need_update = false;
  /* reschedule the next service check ONLY for active, scheduled checks */
  if (reschedule_check) {
    engine_logger(dbg_checks, more) << "Rescheduling next check of service at "
                                    << my_ctime(&next_service_check);
    SPDLOG_LOGGER_DEBUG(checks_logger,
                        "Rescheduling next check of service at {}",
                        my_ctime(&next_service_check));

    /* default is to reschedule service check unless a test below fails... */
    set_should_be_scheduled(true);

    /* next check time was calculated above */
    set_next_check(next_service_check);

    /* make sure we don't get ourselves into too much trouble... */
    if (current_time > get_next_check())
      set_next_check(current_time);

    // Make sure we rescheduled the next service check at a valid time.
    {
      timezone_locker lock(get_timezone());
      preferred_time = get_next_check();
      get_next_valid_time(preferred_time, &next_valid_time,
                          this->check_period_ptr);
      set_next_check(next_valid_time);
    }

    /* services with non-recurring intervals do not get rescheduled */
    if (check_interval() == 0)
      set_should_be_scheduled(false);

    /* services with active checks disabled do not get rescheduled */
    if (!active_checks_enabled())
      set_should_be_scheduled(false);

    /* schedule a non-forced check if we can */
    if (get_should_be_scheduled()) {
      /* No update_status is asked but we store in need_update if it is needed,
       * to send later. */
      need_update = schedule_check(get_next_check(), CHECK_OPTION_NONE, true);
    }
  }

  /* if we're stalking this state type and state was not already logged AND the
   * plugin output changed since last check, log it now.. */
  if (get_state_type() == hard && !state_change && !state_was_logged &&
      old_plugin_output != get_plugin_output()) {
    if ((_current_state == service::state_ok && get_stalk_on(ok)))
      log_event();

    else if ((_current_state == service::state_warning &&
              get_stalk_on(warning)))
      log_event();

    else if ((_current_state == service::state_unknown &&
              get_stalk_on(unknown)))
      log_event();

    else if ((_current_state == service::state_critical &&
              get_stalk_on(critical)))
      log_event();
  }

  /* send data to event broker */
  broker_service_check(NEBTYPE_SERVICECHECK_PROCESSED, this, get_check_type(),
                       nullptr);

  if (!(reschedule_check && get_should_be_scheduled() && has_been_checked()) ||
      !active_checks_enabled()) {
    /* set the checked flag */
    set_has_been_checked(true);
    /* update the current service status log */
    need_update = true;
  }
  if (need_update)
    update_status();

  /* check to see if the service and/or associate host is flapping */
  if (!flapping_check_done) {
    check_for_flapping(true, true);
    hst->check_for_flapping(true, false, true);
  }

  /* update service performance info */
  update_service_performance_data();

  /* run async checks of all services we added above */
  /* don't run a check if one is already executing or we can get by with a
   * cached state */
  for (auto svc : check_servicelist) {
    run_async_check = true;

    /* we can get by with a cached state, so don't check the service */
    if (static_cast<unsigned long>(current_time - svc->get_last_check()) <=
        cached_service_check_horizon) {
      run_async_check = false;

      /* update check statistics */
      update_check_stats(ACTIVE_CACHED_SERVICE_CHECK_STATS, current_time);
    }

    if (svc->get_is_executing())
      run_async_check = false;

    if (run_async_check)
      svc->run_async_check(CHECK_OPTION_NONE, 0.0, false, false, nullptr,
                           nullptr);
  }
  return OK;
}

/**
 *  Log service event information.
 *  This function has been DEPRECATED.
 *
 *  @param[in] svc The service to log.
 *
 *  @return Return true on success.
 */
int service::log_event() {
#ifdef LEGACY_CONF
  bool log_service_retries = config->log_service_retries();
#else
  bool log_service_retries = pb_config.log_service_retries();
#endif
  if (get_state_type() == soft && !log_service_retries)
    return OK;

  uint32_t log_options{NSLOG_SERVICE_UNKNOWN};
  char const* state{"UNKNOWN"};
  if (_current_state >= 0 &&
      (unsigned int)_current_state < tab_service_states.size()) {
    log_options = tab_service_states[_current_state].first;
    state = tab_service_states[_current_state].second.c_str();
  }
  const std::string& state_type{tab_state_type[get_state_type()]};

  engine_logger(log_options, basic)
      << "SERVICE ALERT: " << _hostname << ";" << name() << ";" << state << ";"
      << state_type << ";" << get_current_attempt() << ";"
      << get_plugin_output();
  SPDLOG_LOGGER_INFO(events_logger, "SERVICE ALERT: {};{};{};{};{};{}",
                     _hostname, name(), state, state_type,
                     get_current_attempt(), get_plugin_output());
  return OK;
}

// int service::get_check_viability(...)  << check_service_check_viability()
/* detects service flapping */
void service::check_for_flapping(bool update,
                                 bool allow_flapstart_notification) {
  bool update_history;
  bool is_flapping = false;
  unsigned int x = 0;
  unsigned int y = 0;
  int last_state_history_value = service::state_ok;
  double curved_changes = 0.0;
  double curved_percent_change = 0.0;
  double low_threshold = 0.0;
  double high_threshold = 0.0;
  double low_curve_value = 0.75;
  double high_curve_value = 1.25;

  float low_service_flap_threshold;
  float high_service_flap_threshold;
  bool enable_flap_detection;

#ifdef LEGACY_CONF
  low_service_flap_threshold = config->low_service_flap_threshold();
  high_service_flap_threshold = config->high_service_flap_threshold();
  enable_flap_detection = config->enable_flap_detection();
#else
  low_service_flap_threshold = pb_config.low_service_flap_threshold();
  high_service_flap_threshold = pb_config.high_service_flap_threshold();
  enable_flap_detection = pb_config.enable_flap_detection();
#endif

  /* large install tweaks skips all flap detection logic - including state
   * change calculation */

  engine_logger(dbg_functions, basic) << "check_for_flapping()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_for_flapping()");

  engine_logger(dbg_flapping, more)
      << "Checking service '" << name() << "' on host '" << _hostname
      << "' for flapping...";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Checking service '{}' on host '{}' for flapping...",
                      name(), _hostname);

  /* what threshold values should we use (global or service-specific)? */
  low_threshold = (get_low_flap_threshold() <= 0.0) ? low_service_flap_threshold
                                                    : get_low_flap_threshold();
  high_threshold = (get_high_flap_threshold() <= 0.0)
                       ? high_service_flap_threshold
                       : get_high_flap_threshold();

  update_history = update;

  /* should we update state history for this state? */
  if (update_history) {
    if ((_current_state == service::state_ok && !get_flap_detection_on(ok)) ||
        (_current_state == service::state_warning &&
         !get_flap_detection_on(warning)) ||
        (_current_state == service::state_unknown &&
         !get_flap_detection_on(unknown)) ||
        (_current_state == service::state_critical &&
         !get_flap_detection_on(critical)) ||
        (get_host_ptr()->get_current_state() != host::state_up))
      update_history = false;
  }

  /* record current service state */
  if (update_history) {
    /* record the current state in the state history */
    get_state_history()[get_state_history_index()] = _current_state;

    /* increment state history index to next available slot */
    set_state_history_index(get_state_history_index() + 1);
    if (get_state_history_index() >= MAX_STATE_HISTORY_ENTRIES)
      set_state_history_index(0);
  }

  /* calculate overall and curved percent state changes */
  for (x = 0, y = get_state_history_index(); x < MAX_STATE_HISTORY_ENTRIES;
       x++) {
    if (x == 0) {
      last_state_history_value = get_state_history()[y];
      y++;
      if (y >= MAX_STATE_HISTORY_ENTRIES)
        y = 0;
      continue;
    }

    if (last_state_history_value != get_state_history()[y])
      curved_changes +=
          (((double)(x - 1) * (high_curve_value - low_curve_value)) /
           ((double)(MAX_STATE_HISTORY_ENTRIES - 2))) +
          low_curve_value;

    last_state_history_value = get_state_history()[y];

    y++;
    if (y >= MAX_STATE_HISTORY_ENTRIES)
      y = 0;
  }

  /* calculate overall percent change in state */
  curved_percent_change = (double)(((double)curved_changes * 100.0) /
                                   (double)(MAX_STATE_HISTORY_ENTRIES - 1));

  set_percent_state_change(curved_percent_change);

  engine_logger(dbg_flapping, most)
      << com::centreon::logging::setprecision(2) << "LFT=" << low_threshold
      << ", HFT=" << high_threshold << ", CPC=" << curved_percent_change
      << ", PSC=" << curved_percent_change << "%";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "LFT={:.2f}, HFT={:.2f}, CPC={:.2f}, PSC={:.2f}%",
                      low_threshold, high_threshold, curved_percent_change,
                      curved_percent_change);

  /* don't do anything if we don't have flap detection enabled on a program-wide
   * basis */
  if (!enable_flap_detection)
    return;

  /* don't do anything if we don't have flap detection enabled for this service
   */
  if (!flap_detection_enabled())
    return;

  /* are we flapping, undecided, or what?... */

  /* we're undecided, so don't change the current flap state */
  if (curved_percent_change > low_threshold &&
      curved_percent_change < high_threshold)
    return;
  /* we're below the lower bound, so we're not flapping */
  else if (curved_percent_change < low_threshold)
    is_flapping = false;
  /* else we're above the upper bound, so we are flapping */
  else if (curved_percent_change >= high_threshold) {
    /* start flapping on !OK states which makes more sense */
    if ((_current_state != service::state_ok) || get_is_flapping())
      is_flapping = true;
  }
  engine_logger(dbg_flapping, more)
      << com::centreon::logging::setprecision(2) << "Service "
      << (is_flapping ? "is" : "is not") << " flapping ("
      << curved_percent_change << "% state change).";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Service {} flapping ({:.2f}% state change).",
                      is_flapping ? "is" : "is not", curved_percent_change);

  /* did the service just start flapping? */
  if (is_flapping && !get_is_flapping())
    set_flap(curved_percent_change, high_threshold, low_threshold,
             allow_flapstart_notification);

  /* did the service just stop flapping? */
  else if (!is_flapping && get_is_flapping())
    clear_flap(curved_percent_change, high_threshold, low_threshold);
}

/* handles changes in the state of a service */
int service::handle_service_event() {
  nagios_macros* mac(get_global_macros());

  engine_logger(dbg_functions, basic) << "handle_service_event()";
  SPDLOG_LOGGER_TRACE(functions_logger, "handle_service_event()");

  /* bail out if we shouldn't be running event handlers */
#ifdef LEGACY_CONF
  bool enable_event_handlers = config->enable_event_handlers();
#else
  bool enable_event_handlers = pb_config.enable_event_handlers();
#endif
  if (!enable_event_handlers)
    return OK;
  if (!event_handler_enabled())
    return OK;

  /* find the host */
  if (!get_host_ptr())
    return ERROR;

  /* update service macros */
  grab_host_macros_r(mac, get_host_ptr());
  grab_service_macros_r(mac, this);

  /* run the global service event handler */
  run_global_service_event_handler(mac, this);

  /* run the event handler command if there is one */
  if (!event_handler().empty())
    run_service_event_handler(mac, this);
  clear_volatile_macros_r(mac);

  /* send data to event broker */
  broker_external_command(NEBTYPE_EXTERNALCOMMAND_CHECK, CMD_NONE, nullptr,
                          nullptr);

  return OK;
}

/* handles service check results in an obsessive compulsive manner... */
int service::obsessive_compulsive_service_check_processor() {
  std::string raw_command;
  std::string processed_command;
  host* temp_host{get_host_ptr()};
  bool early_timeout = false;
  double exectime = 0.0;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
  nagios_macros* mac(get_global_macros());

  bool obsess_over_services;
  uint32_t ocsp_timeout;
#ifdef LEGACY_CONF
  obsess_over_services = config->obsess_over_services();
  const std::string& ocsp_command = config->ocsp_command();
  ocsp_timeout = config->ocsp_timeout();
#else
  obsess_over_services = pb_config.obsess_over_services();
  const std::string& ocsp_command = pb_config.ocsp_command();
  ocsp_timeout = pb_config.ocsp_timeout();
#endif

  engine_logger(dbg_functions, basic)
      << "obsessive_compulsive_service_check_processor()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "obsessive_compulsive_service_check_processor()");

  /* bail out if we shouldn't be obsessing */
  if (!obsess_over_services)
    return OK;
  if (!obsess_over())
    return OK;

  /* if there is no valid command, exit */
  if (ocsp_command.empty())
    return ERROR;

  /* find the associated host */
  if (temp_host == nullptr)
    return ERROR;

  /* update service macros */
  grab_host_macros_r(mac, temp_host);
  grab_service_macros_r(mac, this);

  /* get the raw command line */
  get_raw_command_line_r(mac, ocsp_command_ptr, ocsp_command.c_str(),
                         raw_command, macro_options);
  if (raw_command.empty()) {
    clear_volatile_macros_r(mac);
    return ERROR;
  }

  engine_logger(dbg_checks, most)
      << "Raw obsessive compulsive service processor "
         "command line: "
      << raw_command;
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Raw obsessive compulsive service processor "
                      "command line: {}",
                      raw_command);

  /* process any macros in the raw command line */
  process_macros_r(mac, raw_command, processed_command, macro_options);
  if (processed_command.empty()) {
    clear_volatile_macros_r(mac);
    return ERROR;
  }

  engine_logger(dbg_checks, most) << "Processed obsessive compulsive service "
                                     "processor command line: "
                                  << processed_command;
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Processed obsessive compulsive service "
                      "processor command line: {}",
                      processed_command);

  if (command_is_allowed_by_whitelist(processed_command, OBSESS_TYPE)) {
    /* run the command */
    try {
      std::string tmp;
      my_system_r(mac, processed_command, ocsp_timeout, &early_timeout,
                  &exectime, tmp, 0);
    } catch (std::exception const& e) {
      engine_logger(log_runtime_error, basic)
          << "Error: can't execute compulsive service processor command line '"
          << processed_command << "' : " << e.what();
      SPDLOG_LOGGER_ERROR(runtime_logger,
                          "Error: can't execute compulsive service processor "
                          "command line '{}' : "
                          "{}",
                          processed_command, e.what());
    }
  } else {
    runtime_logger->error(
        "Error: can't execute compulsive service processor command line '{}' : "
        "it is not allowed by the whitelist",
        processed_command);
  }

  clear_volatile_macros_r(mac);

  /* check to see if the command timed out */
  if (early_timeout)
    engine_logger(log_runtime_warning, basic)
        << "Warning: OCSP command '" << processed_command << "' for service '"
        << name() << "' on host '" << _hostname << "' timed out after "
        << ocsp_timeout << " seconds";
  SPDLOG_LOGGER_WARN(
      runtime_logger,
      "Warning: OCSP command '{}' for service '{}' on host '{}' timed out "
      "after {} seconds",
      processed_command, name(), _hostname, ocsp_timeout);

  return OK;
}

/* updates service performance data */
int service::update_service_performance_data() {
  /* should we be processing performance data for anything? */
#ifdef LEGACY_CONF
  bool process_pd = config->process_performance_data();
#else
  bool process_pd = pb_config.process_performance_data();
#endif
  if (!process_pd)
    return OK;

  /* should we process performance data for this service? */
  if (!this->get_process_performance_data())
    return OK;

  return OK;
}

/* executes a scheduled service check */
int service::run_scheduled_check(int check_options, double latency) {
  int result = OK;
  time_t current_time = 0L;
  time_t preferred_time = 0L;
  time_t next_valid_time = 0L;
  bool time_is_valid = true;

  engine_logger(dbg_functions, basic) << "run_scheduled_service_check()";
  SPDLOG_LOGGER_TRACE(functions_logger, "run_scheduled_service_check()");
  engine_logger(dbg_checks, basic)
      << "Attempting to run scheduled check of service '" << name()
      << "' on host '" << _hostname << "': check options=" << check_options
      << ", latency=" << latency;
  SPDLOG_LOGGER_TRACE(
      checks_logger,
      "Attempting to run scheduled check of service '{}' on host '{}': check "
      "options={}, latency={}",
      name(), _hostname, check_options, latency);

  /* attempt to run the check */
  result = run_async_check(check_options, latency, true, true, &time_is_valid,
                           &preferred_time);

  /* an error occurred, so reschedule the check */
  if (result == ERROR) {
    engine_logger(dbg_checks, more)
        << "Unable to run scheduled service check at this time";
    SPDLOG_LOGGER_DEBUG(checks_logger,
                        "Unable to run scheduled service check at this time");

    /* only attempt to (re)schedule checks that should get checked... */
    if (get_should_be_scheduled()) {
      /* get current time */
      time(&current_time);

      /*
       * determine next time we should check the service if needed
       * if service has no check interval, schedule it again for 5
       * minutes from now
       * */
      if (current_time >= preferred_time) {
#ifdef LEGACY_CONF
        uint32_t interval_length = config->interval_length();
#else
        uint32_t interval_length = pb_config.interval_length();
#endif
        preferred_time =
            current_time +
            static_cast<time_t>(check_interval() <= 0
                                    ? 300
                                    : check_interval() * interval_length);
      }

      // Make sure we rescheduled the next service check at a valid time.
      {
        timezone_locker lock(get_timezone());
        get_next_valid_time(preferred_time, &next_valid_time,
                            this->check_period_ptr);

        // The service could not be rescheduled properly.
        // Set the next check time for next week.
        if (!time_is_valid && !check_time_against_period(
                                  next_valid_time, this->check_period_ptr)) {
          set_next_check((time_t)(next_valid_time + 60 * 60 * 24 * 7));
          engine_logger(log_runtime_warning, basic)
              << "Warning: Check of service '" << name() << "' on host '"
              << _hostname
              << "' could not be "
                 "rescheduled properly. Scheduling check for next week...";
          SPDLOG_LOGGER_WARN(
              runtime_logger,
              "Warning: Check of service '{}' on host '{}' could not be "
              "rescheduled properly. Scheduling check for next week...",
              name(), _hostname);
          engine_logger(dbg_checks, more)
              << "Unable to find any valid times to reschedule the next "
                 "service check!";
          SPDLOG_LOGGER_DEBUG(
              checks_logger,
              "Unable to find any valid times to reschedule the next "
              "service check!");
        }
        // This service could be rescheduled...
        else {
          set_next_check(next_valid_time);
          set_should_be_scheduled(true);
          engine_logger(dbg_checks, more)
              << "Rescheduled next service check for "
              << my_ctime(&next_valid_time);
          SPDLOG_LOGGER_DEBUG(checks_logger,
                              "Rescheduled next service check for {}",
                              my_ctime(&next_valid_time));
        }
      }
    }

    /*
     * reschedule the next service check - unless we couldn't find a valid
     * next check time
     * 10/19/07 EG - keep original check options
     */
    bool sent = false;
    if (get_should_be_scheduled())
      sent = schedule_check(get_next_check(), check_options);

    /* update the status log */
    if (!sent)
      update_status();
    return ERROR;
  }

  return OK;
}

/*
 * forks a child process to run a service check, but does not wait for the
 * service check result
 */
int service::run_async_check(int check_options,
                             double latency,
                             bool scheduled_check,
                             bool reschedule_check,
                             bool* time_is_valid,
                             time_t* preferred_time) noexcept {
  return run_async_check_local(check_options, latency, scheduled_check,
                               reschedule_check, time_is_valid, preferred_time,
                               this);
}

/**
 * @brief The big work of run_async_check is done here. The function has been
 * split because of anomalydetection that needs to call the same method but
 * with its dependency service. Then, macros have to be computed with the
 * dependency service.
 *
 * @param check_options
 * @param latency
 * @param scheduled_check
 * @param reschedule_check
 * @param time_is_valid
 * @param preferred_time
 * @param svc  A pointer to the service used to compute macros.
 *
 * @return OK or ERROR
 */
int service::run_async_check_local(int check_options,
                                   double latency,
                                   bool scheduled_check,
                                   bool reschedule_check,
                                   bool* time_is_valid,
                                   time_t* preferred_time,
                                   service* svc) noexcept {
  engine_logger(dbg_functions, basic)
      << "service::run_async_check, check_options=" << check_options
      << ", latency=" << latency << ", scheduled_check=" << scheduled_check
      << ", reschedule_check=" << reschedule_check;
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "service::run_async_check, check_options={}, latency={}, "
                      "scheduled_check={}, reschedule_check={}",
                      check_options, latency, scheduled_check,
                      reschedule_check);

  // Preamble.
  if (!get_check_command_ptr()) {
    engine_logger(log_runtime_error, basic)
        << "Error: Attempt to run active check on service '" << description()
        << "' on host '" << get_host_ptr()->name() << "' with no check command";
    SPDLOG_LOGGER_ERROR(
        runtime_logger,
        "Error: Attempt to run active check on service '{}' on host '{}' with "
        "no check command",
        description(), get_host_ptr()->name());
    return ERROR;
  }

  engine_logger(dbg_checks, basic)
      << "** Running async check of service '" << description() << "' on host '"
      << get_hostname() << "'...";
  SPDLOG_LOGGER_TRACE(checks_logger,
                      "** Running async check of service '{} on host '{}'...",
                      description(), get_hostname());

  // Check if the service is viable now.
  if (!verify_check_viability(check_options, time_is_valid, preferred_time))
    return ERROR;

  // Send broker event.
  timeval start_time = {0, 0};
  int res = broker_service_check(NEBTYPE_SERVICECHECK_ASYNC_PRECHECK, this,
                                 checkable::check_active, nullptr);

  // Service check was cancelled by NEB module. reschedule check later.
  if (NEBERROR_CALLBACKCANCEL == res) {
    if (preferred_time != nullptr) {
#ifdef LEGACY_CONF
      uint32_t interval_length = config->interval_length();
#else
      uint32_t interval_length = pb_config.interval_length();
#endif
      *preferred_time +=
          static_cast<time_t>(check_interval() * interval_length);
    }
    engine_logger(log_runtime_error, basic)
        << "Error: Some broker module cancelled check of service '"
        << description() << "' on host '" << get_hostname();
    SPDLOG_LOGGER_ERROR(
        runtime_logger,
        "Error: Some broker module cancelled check of service '{}' on host "
        "'{}'",
        description(), get_hostname());
    return ERROR;
  }
  // Service check was override by NEB module.
  else if (NEBERROR_CALLBACKOVERRIDE == res) {
    engine_logger(dbg_functions, basic)
        << "Some broker module overrode check of service '" << description()
        << "' on host '" << get_hostname() << "' so we'll bail out";
    SPDLOG_LOGGER_TRACE(
        functions_logger,
        "Some broker module overrode check of service '{}' on host '{}' so "
        "we'll bail out",
        description(), get_hostname());
    return OK;
  }

  // Checking starts.
  engine_logger(dbg_checks, basic) << "Checking service '" << description()
                                   << "' on host '" << get_hostname() << "'...";
  SPDLOG_LOGGER_TRACE(checks_logger, "Checking service '{}' on host '{}'...",
                      description(), get_hostname());

  // Clear check options.
  if (scheduled_check)
    set_check_options(CHECK_OPTION_NONE);

  // Update latency for event broker and macros.
  double old_latency(get_latency());
  set_latency(latency);

  // Get current host and service macros.
  nagios_macros* macros(get_global_macros());
  std::string processed_cmd = svc->get_check_command_line(macros);

  // Time to start command.
  gettimeofday(&start_time, nullptr);

  // Update the number of running service checks.
  ++currently_running_service_checks;
  engine_logger(dbg_checks, basic)
      << "Current running service checks: " << currently_running_service_checks;
  SPDLOG_LOGGER_TRACE(checks_logger, "Current running service checks: {}",
                      currently_running_service_checks);

  // Set the execution flag.
  set_is_executing(true);

  // Send event broker.
  res = broker_service_check(NEBTYPE_SERVICECHECK_INITIATE, this,
                             checkable::check_active, processed_cmd.c_str());

  // Restore latency.
  set_latency(old_latency);

  // Service check was override by neb_module.
  if (NEBERROR_CALLBACKOVERRIDE == res) {
    clear_volatile_macros_r(macros);
    return OK;
  }

  // Update statistics.
  update_check_stats(scheduled_check ? ACTIVE_SCHEDULED_SERVICE_CHECK_STATS
                                     : ACTIVE_ONDEMAND_SERVICE_CHECK_STATS,
                     start_time.tv_sec);

  // Init check result info.
  check_result::pointer check_result_info = std::make_shared<check_result>(
      service_check, this, checkable::check_active, check_options,
      reschedule_check, latency, start_time, start_time, false, true,
      service::state_ok, "");

  auto run_failure = [&](const std::string& reason) {
    // Update check result.
    timeval tv;
    gettimeofday(&tv, nullptr);
    check_result_info->set_finish_time(tv);
    check_result_info->set_early_timeout(false);
    check_result_info->set_return_code(service::state_unknown);
    check_result_info->set_exited_ok(true);
    check_result_info->set_output(reason);

    // Queue check result.
    checks::checker::instance().add_check_result_to_reap(check_result_info);
  };

#ifdef LEGACY_CONF
  bool use_host_down_disable_service_checks =
      config->use_host_down_disable_service_checks();
#else
  bool use_host_down_disable_service_checks =
      pb_config.host_down_disable_service_checks();
#endif
  bool has_to_execute_check = true;
  if (use_host_down_disable_service_checks) {
    auto hst = host::hosts_by_id.find(_host_id);
    if (hst != host::hosts_by_id.end() &&
        hst->second->get_current_state() != host::state_up) {
      run_failure(fmt::format("host {} is down", hst->second->name()));
      has_to_execute_check = false;
    }
  }

  // allowed by whitelist?
  if (has_to_execute_check &&
      !command_is_allowed_by_whitelist(processed_cmd, CHECK_TYPE)) {
    SPDLOG_LOGGER_ERROR(
        commands_logger,
        "service {}: this command cannot be executed because of "
        "security restrictions on the poller. A whitelist has "
        "been defined, and it does not include this command.",
        name());

    SPDLOG_LOGGER_DEBUG(commands_logger,
                        "service {}: command not allowed by whitelist {}",
                        name(), processed_cmd);
    run_failure(configuration::command_blacklist_output);
    has_to_execute_check = false;
  }

  if (has_to_execute_check) {
    bool retry;
    do {
      retry = false;
      try {
        // Run command.
#ifdef LEGACY_CONF
        uint32_t service_check_timeout = config->service_check_timeout();
#else
        uint32_t service_check_timeout = pb_config.service_check_timeout();
#endif
        uint64_t id = get_check_command_ptr()->run(processed_cmd, *macros,
                                                   service_check_timeout,
                                                   check_result_info, this);
        SPDLOG_LOGGER_DEBUG(checks_logger,
                            "run id={} {} for service {} host {}", id,
                            processed_cmd, _service_id, _hostname);

      } catch (std::exception const& e) {
        run_failure("(Execute command failed)");

        engine_logger(log_runtime_warning, basic)
            << "Error: Service check command execution failed: " << e.what();
        SPDLOG_LOGGER_WARN(runtime_logger,
                           "Error: Service check command execution failed: {}",
                           e.what());
      }
    } while (retry);
  }
  // Cleanup.
  clear_volatile_macros_r(macros);
  return OK;
}

/**
 *  Schedules an immediate or delayed service check.
 *
 *  @param[in] svc         Target service.
 *  @param[in] check_time  Desired check time.
 *  @param[in] options     Check options (FORCED, FRESHNESS, ...).
 *
 * @return A boolean telling if service_status has been sent or if
 * no_update_status_now is true, if it should be sent.
 */
bool service::schedule_check(time_t check_time,
                             uint32_t options,
                             bool no_update_status_now) {
  engine_logger(dbg_functions, basic) << "schedule_service_check()";
  SPDLOG_LOGGER_TRACE(functions_logger, "schedule_service_check()");

  engine_logger(dbg_checks, basic)
      << "Scheduling a "
      << (options & CHECK_OPTION_FORCE_EXECUTION ? "forced" : "non-forced")
      << ", active check of service '" << name() << "' on host '" << _hostname
      << "' @ " << my_ctime(&check_time);
  SPDLOG_LOGGER_TRACE(
      checks_logger,
      "Scheduling a {}, active check of service '{}' on host '{}' @ {}",
      options & CHECK_OPTION_FORCE_EXECUTION ? "forced" : "non-forced", name(),
      _hostname, my_ctime(&check_time));

  // Don't schedule a check if active checks
  // of this service are disabled.
  if (!active_checks_enabled() && !(options & CHECK_OPTION_FORCE_EXECUTION)) {
    engine_logger(dbg_checks, basic)
        << "Active checks of this service are disabled.";
    SPDLOG_LOGGER_TRACE(checks_logger,
                        "Active checks of this service are disabled.");
    return false;
  }

  // Default is to use the new event.
  bool use_original_event = false;
  timed_event_list::iterator found = events::loop::instance().find_event(
      events::loop::low, timed_event::EVENT_SERVICE_CHECK, this);

  // We found another service check event for this service in
  // the queue - what should we do?
  if (found != events::loop::instance().list_end(events::loop::low)) {
    auto& temp_event = *found;
    engine_logger(dbg_checks, most)
        << "Found another service check event for this service @ "
        << my_ctime(&temp_event->run_time);
    SPDLOG_LOGGER_DEBUG(
        checks_logger,
        "Found another service check event for this service @ {}",
        my_ctime(&temp_event->run_time));

    // Use the originally scheduled check unless we decide otherwise.
    use_original_event = true;

    // The original event is a forced check...
    if (temp_event->event_options & CHECK_OPTION_FORCE_EXECUTION) {
      // The new event is also forced and its execution time is earlier
      // than the original, so use it instead.
      if ((options & CHECK_OPTION_FORCE_EXECUTION) &&
          (check_time < temp_event->run_time)) {
        use_original_event = false;
        engine_logger(dbg_checks, most)
            << "New service check event is forced and occurs before the "
               "existing event, so the new event will be used instead.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "New service check event is forced and occurs before the "
            "existing event, so the new event will be used instead.");
      }
    }
    // The original event is not a forced check...
    else {
      // The new event is a forced check, so use it instead.
      if (options & CHECK_OPTION_FORCE_EXECUTION) {
        use_original_event = false;
        engine_logger(dbg_checks, most)
            << "New service check event is forced, so it will be used "
               "instead of the existing event.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "New service check event is forced, so it will be used "
            "instead of the existing event.");
      }
      // The new event is not forced either and its execution time is
      // earlier than the original, so use it instead.
      else if (check_time < temp_event->run_time) {
        use_original_event = false;
        engine_logger(dbg_checks, most)
            << "New service check event occurs before the existing "
               "(older) event, so it will be used instead.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "New service check event occurs before the existing "
            "(older) event, so it will be used instead.");
      }
      // The new event is older, so override the existing one.
      else {
        engine_logger(dbg_checks, most)
            << "New service check event occurs after the existing event, "
               "so we'll ignore it.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "New service check event occurs after the existing event, "
            "so we'll ignore it.");
      }
    }

    if (!use_original_event) {
      // We're using the new event, so remove the old one.
      events::loop::instance().remove_event(found, events::loop::low);
      no_update_status_now = true;
    } else {
      // Reset the next check time (it may be out of sync).
      set_next_check(temp_event->run_time);

      engine_logger(dbg_checks, most)
          << "Keeping original service check event (ignoring the new one).";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Keeping original service check event (ignoring the new one).");
    }
  }

  // Save check options for retention purposes.
  set_check_options(options);

  // Schedule a new event.
  if (!use_original_event) {
    engine_logger(dbg_checks, most) << "Scheduling new service check event.";
    SPDLOG_LOGGER_DEBUG(checks_logger, "Scheduling new service check event.");

    // Allocate memory for a new event item.
    try {
      // Set the next service check time.
      set_next_check(check_time);

      // Place the new event in the event queue.
      auto new_event{std::make_unique<timed_event>(
          timed_event::EVENT_SERVICE_CHECK, get_next_check(), false, 0L,
          nullptr, true, this, nullptr, options)};

      events::loop::instance().reschedule_event(std::move(new_event),
                                                events::loop::low);

      if (!active_checks_enabled())
        no_update_status_now = true;
    } catch (...) {
      // Update the status log.
      update_status();
      throw;
    }
  }

  // Update the status log.
  if (!no_update_status_now)
    update_status();
  return true;
}

void service::set_flap(double percent_change,
                       double high_threshold,
                       double low_threshold [[maybe_unused]],
                       int allow_flapstart_notification) {
  engine_logger(dbg_functions, basic) << "set_service_flap()";
  SPDLOG_LOGGER_TRACE(functions_logger, "set_service_flap()");

  engine_logger(dbg_flapping, more) << "Service '" << name() << "' on host '"
                                    << _hostname << "' started flapping!";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Service '{}' on host '{}' started flapping!", name(),
                      _hostname);

  /* log a notice - this one is parsed by the history CGI */
  engine_logger(log_runtime_warning, basic)
      << com::centreon::logging::setprecision(1)
      << "SERVICE FLAPPING ALERT: " << _hostname << ";" << name()
      << ";STARTED; Service appears to have started flapping ("
      << percent_change << "% change >= " << high_threshold << "% threshold)";
  SPDLOG_LOGGER_WARN(
      runtime_logger,
      "SERVICE FLAPPING ALERT: {};{};STARTED; Service appears to have started "
      "flapping ({:.1f}% change >= {:.1f}% threshold)",
      _hostname, name(), percent_change, high_threshold);

  /* add a non-persistent comment to the service */
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1)
      << "Notifications for this service are being suppressed because "
         "it was detected as "
      << "having been flapping between different "
         "states ("
      << percent_change << "% change >= " << high_threshold
      << "% threshold).  When the service state stabilizes and the "
         "flapping "
      << "stops, notifications will be re-enabled.";

  auto com{std::make_shared<comment>(
      comment::service, comment::flapping, host_id(), _service_id,
      time(nullptr), "(Centreon Engine Process)", oss.str(), false,
      comment::internal, false, (time_t)0)};

  comment::comments.insert({com->get_comment_id(), com});

  this->set_flapping_comment_id(com->get_comment_id());

  /* set the flapping indicator */
  set_is_flapping(true);

  /* send a notification */
  if (allow_flapstart_notification)
    notify(reason_flappingstart, "", "", notification_option_none);
}

/* handles a service that has stopped flapping */
void service::clear_flap(double percent_change,
                         double high_threshold [[maybe_unused]],
                         double low_threshold) {
  engine_logger(dbg_functions, basic) << "clear_service_flap()";
  SPDLOG_LOGGER_TRACE(functions_logger, "clear_service_flap()");

  engine_logger(dbg_flapping, more) << "Service '" << name() << "' on host '"
                                    << _hostname << "' stopped flapping.";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Service '{}' on host '{}' stopped flapping.", name(),
                      _hostname);

  /* log a notice - this one is parsed by the history CGI */
  engine_logger(log_info_message, basic)
      << com::centreon::logging::setprecision(1)
      << "SERVICE FLAPPING ALERT: " << _hostname << ";" << name()
      << ";STOPPED; Service appears to have stopped flapping ("
      << percent_change << "% change < " << low_threshold << "% threshold)";
  SPDLOG_LOGGER_INFO(
      events_logger,
      "SERVICE FLAPPING ALERT: {};{};STOPPED; Service appears to have stopped "
      "flapping ({:.1f}% change < {:.1f}% threshold)",
      _hostname, name(), percent_change, low_threshold);

  /* delete the comment we added earlier */
  if (this->get_flapping_comment_id() != 0)
    comment::delete_comment(this->get_flapping_comment_id());
  this->set_flapping_comment_id(0);

  /* clear the flapping indicator */
  set_is_flapping(false);

  /* send a notification */
  notify(reason_flappingstop, "", "", notification_option_none);

  /* should we send a recovery notification? */
  notify(reason_recovery, "", "", notification_option_none);
}

/* enables flap detection for a specific service */
void service::enable_flap_detection() {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  engine_logger(dbg_functions, basic) << "service::enable_flap_detection()";
  SPDLOG_LOGGER_TRACE(functions_logger, "service::enable_flap_detection()");

  engine_logger(dbg_flapping, more)
      << "Enabling flap detection for service '" << name() << "' on host '"
      << _hostname << "'.";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Enabling flap detection for service '{}' on host '{}'.",
                      name(), _hostname);

  /* nothing to do... */
  if (flap_detection_enabled())
    return;

  /* set the attribute modified flag */
  add_modified_attributes(attr);

  /* set the flap detection enabled flag */
  set_flap_detection_enabled(true);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               this, attr);

  /* check for flapping */
  check_for_flapping(false, true);

  /* update service status */
  // FIXME DBO: Since we are just talking about flapping,
  // we could improve this message.
  update_status();
}

/* disables flap detection for a specific service */
void service::disable_flap_detection() {
  unsigned long attr = MODATTR_FLAP_DETECTION_ENABLED;

  engine_logger(dbg_functions, basic) << "disable_service_flap_detection()";
  SPDLOG_LOGGER_TRACE(functions_logger, "disable_service_flap_detection()");

  engine_logger(dbg_flapping, more)
      << "Disabling flap detection for service '" << name() << "' on host '"
      << _hostname << "'.";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Disabling flap detection for service '{}' on host '{}'.",
                      name(), _hostname);

  /* nothing to do... */
  if (!flap_detection_enabled())
    return;

  /* set the attribute modified flag */
  add_modified_attributes(attr);

  /* set the flap detection enabled flag */
  set_flap_detection_enabled(false);

  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               this, attr);

  /* handle the details... */
  handle_flap_detection_disabled();
}

/**
 * @brief Updates the status of the service partially.
 *
 * @param status_attributes A bits field based on status_attribute enum (default
 * value: STATUS_ALL).
 */
void service::update_status(uint32_t status_attributes) {
  broker_service_status(this, status_attributes);
}

/**
 * @brief Several configurations could be initially handled by service status
 * events. But they are configurations and do not have to be handled like this.
 * So, to have service_status lighter, we removed these items from it but we
 * add a new adaptive service event containing these ones. it is sent when this
 * method is called.
 */
void service::update_adaptive_data() {
  /* send data to event broker */
  broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                               this, get_modified_attributes());
}

/* checks viability of performing a service check */
bool service::verify_check_viability(int check_options,
                                     bool* time_is_valid,
                                     time_t* new_time) {
  bool perform_check = true;
  time_t current_time = 0L;
  time_t preferred_time = 0L;
  int check_interval = 0;

  engine_logger(dbg_functions, basic) << "check_service_check_viability()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_service_check_viability()");

  /* get the check interval to use if we need to reschedule the check */
#ifdef LEGACY_CONF
  uint32_t interval_length = config->interval_length();
#else
  uint32_t interval_length = pb_config.interval_length();
#endif
  if (get_state_type() == soft && _current_state != service::state_ok)
    check_interval = static_cast<int>(retry_interval() * interval_length);
  else
    check_interval = static_cast<int>(this->check_interval() * interval_length);

  /* get the current time */
  time(&current_time);

  /* initialize the next preferred check time */
  preferred_time = current_time;

  /* can we check the host right now? */
  if (!(check_options & CHECK_OPTION_FORCE_EXECUTION)) {
    /* if checks of the service are currently disabled... */
    if (!active_checks_enabled()) {
      preferred_time = current_time + check_interval;
      perform_check = false;

      engine_logger(dbg_checks, most)
          << "Active checks of the service are currently disabled.";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Active checks of the service are currently disabled.");
    }

    // Make sure this is a valid time to check the service.
    {
      timezone_locker lock(get_timezone());
      if (!check_time_against_period((unsigned long)current_time,
                                     this->check_period_ptr)) {
        preferred_time = current_time;
        if (time_is_valid)
          *time_is_valid = false;
        perform_check = false;
        engine_logger(dbg_checks, most)
            << "This is not a valid time for this service to be actively "
               "checked.";
        SPDLOG_LOGGER_DEBUG(
            checks_logger,
            "This is not a valid time for this service to be actively "
            "checked.");
      }
    }

    /* check service dependencies for execution */
    if (!authorized_by_dependencies(hostdependency::execution)) {
      preferred_time = current_time + check_interval;
      perform_check = false;

      engine_logger(dbg_checks, most)
          << "Execution dependencies for this service failed, so it will "
             "not be actively checked.";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Execution dependencies for this service failed, so it will "
          "not be actively checked.");
    }
  }

  /* pass back the next viable check time */
  if (new_time)
    *new_time = preferred_time;

  return perform_check;
}

void service::grab_macros_r(nagios_macros* mac) {
  grab_host_macros_r(mac, _host_ptr);
  grab_service_macros_r(mac, this);
}

/* notify a specific contact about a service problem or recovery */
int service::notify_contact(nagios_macros* mac,
                            contact* cntct,
                            reason_type type,
                            const std::string& not_author,
                            const std::string& not_data,
                            int options __attribute__((unused)),
                            int escalated) {
  std::string raw_command;
  std::string processed_command;
  bool early_timeout = false;
  double exectime;
  struct timeval start_time, end_time;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

  engine_logger(dbg_functions, basic) << "notify_contact_of_service()";
  SPDLOG_LOGGER_TRACE(functions_logger, "notify_contact_of_service()");
  engine_logger(dbg_notifications, most)
      << "** Notifying contact '" << cntct->get_name() << "'";
  notifications_logger->debug("** Notifying contact '{}'", cntct->get_name());

  /* get start time */
  gettimeofday(&start_time, nullptr);

  /* process all the notification commands this user has */
  for (std::shared_ptr<commands::command> const& cmd :
       cntct->get_service_notification_commands()) {
    /* get the raw command line */
    get_raw_command_line_r(mac, cmd, cmd->get_command_line().c_str(),
                           raw_command, macro_options);
    if (raw_command.empty())
      continue;

    engine_logger(dbg_notifications, most)
        << "Raw notification command: " << raw_command;
    notifications_logger->debug("Raw notification command: {}", raw_command);

    /* process any macros contained in the argument */
    process_macros_r(mac, raw_command, processed_command, macro_options);
    if (processed_command.empty())
      continue;

    /* run the notification command... */

    engine_logger(dbg_notifications, most)
        << "Processed notification command: " << processed_command;
    notifications_logger->trace("Processed notification command: {}",
                                processed_command);

    /* log the notification to program log file */
#ifdef LEGACY_CONF
    bool log_notifications = config->log_notifications();
#else
    bool log_notifications = pb_config.log_notifications();
#endif
    if (log_notifications) {
      char const* service_state_str("UNKNOWN");
      if ((unsigned int)_current_state < tab_service_states.size())
        service_state_str = tab_service_states[_current_state].second.c_str();

      char const* notification_str("");
      if ((unsigned int)type < tab_notification_str.size())
        notification_str = tab_notification_str[type].c_str();

      std::string info;
      if (type == reason_custom)
        notification_str = "CUSTOM";
      else if (type == reason_acknowledgement)
        info.append(";").append(not_author).append(";").append(not_data);

      std::string service_notification_state;
      if (strcmp(notification_str, "NORMAL") == 0)
        service_notification_state.append(service_state_str);
      else
        service_notification_state.append(notification_str)
            .append(" (")
            .append(service_state_str)
            .append(")");

      engine_logger(log_service_notification, basic)
          << "SERVICE NOTIFICATION: " << cntct->get_name() << ';'
          << get_hostname() << ';' << description() << ';'
          << service_notification_state << ";" << cmd->get_name() << ';'
          << get_plugin_output() << info;
      notifications_logger->info("SERVICE NOTIFICATION: {};{};{};{};{};{};{}",
                                 cntct->get_name(), get_hostname(),
                                 description(), service_notification_state,
                                 cmd->get_name(), get_plugin_output(), info);
    }

    /* run the notification command */
    if (command_is_allowed_by_whitelist(processed_command, NOTIF_TYPE)) {
#ifdef LEGACY_CONF
      uint32_t notification_timeout = config->notification_timeout();
#else
      uint32_t notification_timeout = pb_config.notification_timeout();
#endif
      try {
        std::string tmp;
        my_system_r(mac, processed_command, notification_timeout,
                    &early_timeout, &exectime, tmp, 0);
      } catch (std::exception const& e) {
        engine_logger(log_runtime_error, basic)
            << "Error: can't execute service notification for contact '"
            << cntct->get_name() << "' : " << e.what();
        SPDLOG_LOGGER_ERROR(
            runtime_logger,
            "Error: can't execute service notification for contact '{}': {}",
            cntct->get_name(), e.what());
      }
    } else {
      SPDLOG_LOGGER_ERROR(runtime_logger,
                          "Error: can't execute service notification for "
                          "contact '{}' : it is not allowed by the whitelist",
                          cntct->get_name());
    }

    /* check to see if the notification command timed out */
    if (early_timeout) {
      engine_logger(log_service_notification | log_runtime_warning, basic)
          << "Warning: Contact '" << cntct->get_name()
          << "' service notification command '" << processed_command
          << "' timed out after " << notification_timeout << " seconds";
      notifications_logger->info(
          "Warning: Contact '{}' service notification command '{}' timed out "
          "after {} seconds",
          cntct->get_name(), processed_command, notification_timeout);
    }
  }

  /* get end time */
  gettimeofday(&end_time, nullptr);

  /* update the contact's last service notification time */
  cntct->set_last_service_notification(start_time.tv_sec);

  return OK;
}

void service::update_notification_flags() {
  /* update notifications flags */
  if (_current_state == service::state_unknown)
    add_notified_on(unknown);
  else if (_current_state == service::state_warning)
    add_notified_on(warning);
  else if (_current_state == service::state_critical)
    add_notified_on(critical);
}

/*
 * checks to see if a service escalation entry is a match for the current
 * service notification
 */
bool service::is_valid_escalation_for_notification(escalation const* e,
                                                   int options) const {
  uint32_t notification_number;
  time_t current_time;

  engine_logger(dbg_functions, basic)
      << "service::is_valid_escalation_for_notification()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "service::is_valid_escalation_for_notification()");

  /* get the current time */
  time(&current_time);

  /*
   * if this is a recovery, really we check for who got notified about a
   * previous problem
   */
  if (_current_state == service::state_ok)
    notification_number = get_notification_number() - 1;
  else
    notification_number = get_notification_number();

  /* find the service this escalation entry is associated with */
  if (e->notifier_ptr != this)
    return false;

  /*** EXCEPTION ***/
  /* broadcast options go to everyone, so this escalation is valid */
  if (options & notification_option_broadcast)
    return true;

  /* skip this escalation if it happens later */
  if (e->get_first_notification() > notification_number)
    return false;

  /* skip this escalation if it has already passed */
  if (e->get_last_notification() != 0 &&
      e->get_last_notification() < notification_number)
    return false;

  /*
   * skip this escalation if it has a timeperiod and the current time isn't
   * valid
   */
  if (!e->get_escalation_period().empty() &&
      !check_time_against_period(current_time, e->escalation_period_ptr))
    return false;

  /* skip this escalation if the state options don't match */
  if (_current_state == service::state_ok && !e->get_escalate_on(ok))
    return false;
  else if (_current_state == service::state_warning &&
           !e->get_escalate_on(warning))
    return false;
  else if (_current_state == service::state_unknown &&
           !e->get_escalate_on(unknown))
    return false;
  else if (_current_state == service::state_critical &&
           !e->get_escalate_on(critical))
    return false;

  return true;
}

/* tests whether or not a service's check results are fresh */
bool service::is_result_fresh(time_t current_time, int log_this) {
  int freshness_threshold;
  time_t expiration_time = 0L;
  int days = 0;
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  int tdays = 0;
  int thours = 0;
  int tminutes = 0;
  int tseconds = 0;

  engine_logger(dbg_checks, most)
      << "Checking freshness of service '" << this->description()
      << "' on host '" << this->get_hostname() << "'...";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Checking freshness of service '{}' on host '{}'...",
                      this->description(), this->get_hostname());

  uint32_t interval_length;
  int32_t additional_freshness_latency;
  uint32_t max_service_check_spread;
#ifdef LEGACY_CONF
  interval_length = config->interval_length();
  additional_freshness_latency = config->additional_freshness_latency();
  max_service_check_spread = config->max_service_check_spread();
#else
  interval_length = pb_config.interval_length();
  additional_freshness_latency = pb_config.additional_freshness_latency();
  max_service_check_spread = pb_config.max_service_check_spread();
#endif

  /* use user-supplied freshness threshold or auto-calculate a freshness
   * threshold to use? */
  if (get_freshness_threshold() == 0) {
    if (get_state_type() == hard || this->_current_state == service::state_ok)
      freshness_threshold =
          static_cast<int>(check_interval() * interval_length + get_latency() +
                           additional_freshness_latency);
    else
      freshness_threshold =
          static_cast<int>(this->retry_interval() * interval_length +
                           get_latency() + additional_freshness_latency);
  } else
    freshness_threshold = this->get_freshness_threshold();

  engine_logger(dbg_checks, most)
      << "Freshness thresholds: service=" << this->get_freshness_threshold()
      << ", use=" << freshness_threshold;
  SPDLOG_LOGGER_DEBUG(checks_logger, "Freshness thresholds: service={}, use={}",
                      this->get_freshness_threshold(), freshness_threshold);

  /* calculate expiration time */
  /* CHANGED 11/10/05 EG - program start is only used in expiration time
   * calculation if > last check AND active checks are enabled, so active checks
   * can become stale immediately upon program startup */
  /* CHANGED 02/25/06 SG - passive checks also become stale, so remove
   * dependence on active check logic */
  if (!this->has_been_checked())
    expiration_time = (time_t)(event_start + freshness_threshold);
  /* CHANGED 06/19/07 EG - Per Ton's suggestion (and user requests), only use
   * program start time over last check if no specific threshold has been set by
   * user.  Otheriwse use it.  Problems can occur if Engine is restarted more
   * frequently that freshness threshold intervals (services never go stale). */
  /* CHANGED 10/07/07 EG - Only match next condition for services that have
   * active checks enabled... */
  /* CHANGED 10/07/07 EG - Added max_service_check_spread to expiration time as
   * suggested by Altinity */
  else if (this->active_checks_enabled() && event_start > get_last_check() &&
           this->get_freshness_threshold() == 0)
    expiration_time = (time_t)(event_start + freshness_threshold +
                               max_service_check_spread * interval_length);
  else
    expiration_time = (time_t)(get_last_check() + freshness_threshold);

  engine_logger(dbg_checks, most)
      << "HBC: " << this->has_been_checked() << ", PS: " << program_start
      << ", ES: " << event_start << ", LC: " << get_last_check()
      << ", CT: " << current_time << ", ET: " << expiration_time;
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "HBC: {}, PS: {}, ES: {}, LC: {}, CT: {}, ET: {}",
                      this->has_been_checked(), program_start, event_start,
                      get_last_check(), current_time, expiration_time);

  /* the results for the last check of this service are stale */
  if (expiration_time < current_time) {
    get_time_breakdown((current_time - expiration_time), &days, &hours,
                       &minutes, &seconds);
    get_time_breakdown(freshness_threshold, &tdays, &thours, &tminutes,
                       &tseconds);

    /* log a warning */
    if (log_this)
      engine_logger(log_runtime_warning, basic)
          << "Warning: The results of service '" << this->description()
          << "' on host '" << this->get_hostname() << "' are stale by " << days
          << "d " << hours << "h " << minutes << "m " << seconds
          << "s (threshold=" << tdays << "d " << thours << "h " << tminutes
          << "m " << tseconds
          << "s).  I'm forcing an immediate check "
             "of the service.";
    SPDLOG_LOGGER_WARN(
        runtime_logger,
        "Warning: The results of service '{}' on host '{}' are stale by {}d "
        "{}h {}m {}s (threshold={}d {}h {}m {}s).  I'm forcing an immediate "
        "check "
        "of the service.",
        this->description(), this->get_hostname(), days, hours, minutes,
        seconds, tdays, thours, tminutes, tseconds);

    engine_logger(dbg_checks, more)
        << "Check results for service '" << this->description() << "' on host '"
        << this->get_hostname() << "' are stale by " << days << "d " << hours
        << "h " << minutes << "m " << seconds << "s (threshold=" << tdays
        << "d " << thours << "h " << tminutes << "m " << tseconds
        << "s).  Forcing an immediate check of "
           "the service...";
    SPDLOG_LOGGER_DEBUG(
        checks_logger,
        "Check results for service '{}' on host '{}' are stale by {}d {}h {}m "
        "{}s (threshold={}d {}h {}m {}s). Forcing an immediate check of the "
        "service...",
        this->description(), this->get_hostname(), days, hours, minutes,
        seconds, tdays, thours, tminutes, tseconds);

    return false;
  }

  engine_logger(dbg_checks, more)
      << "Check results for service '" << this->description() << "' on host '"
      << this->get_hostname() << "' are fresh.";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Check results for service '{}' on host '{}' are fresh.",
                      this->description(), this->get_hostname());

  return true;
}

/**
 * @brief Handles the details for a service when flap detection is disabled
 * (globally or per-service).
 */
void service::handle_flap_detection_disabled() {
  engine_logger(dbg_functions, basic)
      << "handle_service_flap_detection_disabled()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "handle_service_flap_detection_disabled()");

  /* if the service was flapping, remove the flapping indicator */
  if (get_is_flapping()) {
    set_is_flapping(false);

    /* delete the original comment we added earlier */
    if (get_flapping_comment_id() != 0)
      comment::delete_comment(get_flapping_comment_id());
    set_flapping_comment_id(0);

    /* log a notice - this one is parsed by the history CGI */
    engine_logger(log_info_message, basic)
        << "SERVICE FLAPPING ALERT: " << this->get_hostname() << ";"
        << this->description() << ";DISABLED; Flap detection has been disabled";
    events_logger->debug(
        "SERVICE FLAPPING ALERT: {};{};DISABLED; Flap detection has been "
        "disabled",
        this->get_hostname(), this->description());

    /* send a notification */
    notify(reason_flappingdisabled, "", "", notification_option_none);

    /* should we send a recovery notification? */
    notify(reason_recovery, "", "", notification_option_none);
  }

  /* update service status */
  update_status();
}

std::list<servicegroup*> const& service::get_parent_groups() const {
  return _servicegroups;
}

std::list<servicegroup*>& service::get_parent_groups() {
  return _servicegroups;
}

timeperiod* service::get_notification_timeperiod() const {
  /* if the service has no notification period, inherit one from the host */
  return get_notification_period_ptr()
             ? get_notification_period_ptr()
             : _host_ptr->get_notification_period_ptr();
}

/**
 *  This function returns a boolean telling if the master services of this one
 *  authorize it or forbide it to make its job (execution or notification).
 *
 * @param dependency_type execution / notification
 *
 * @return true if it is authorized.
 */
bool service::authorized_by_dependencies(
    dependency::types dependency_type) const {
  engine_logger(dbg_functions, basic)
      << "service::authorized_by_dependencies()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "service::authorized_by_dependencies()");

  auto p(
      servicedependency::servicedependencies.equal_range({_hostname, name()}));
  for (servicedependency_mmap::const_iterator it{p.first}, end{p.second};
       it != end; ++it) {
    servicedependency* dep{it->second.get()};
    /* Only check dependencies of the desired type (notification or execution)
     */
    if (dep->get_dependency_type() != dependency_type)
      continue;

    /* Find the service we depend on */
    if (!dep->master_service_ptr)
      continue;

    /* Skip this dependency if it has a timepriod and the the current time is
     * not valid */
    time_t current_time{std::time(nullptr)};
    if (!dep->get_dependency_period().empty() &&
        !check_time_against_period(current_time, dep->dependency_period_ptr))
      return true;

      /* Get the status to use (use last hard state if it's currently in a soft
       * state) */
#ifdef LEGACY_CONF
    bool soft_state_dependencies = config->soft_state_dependencies();
#else
    bool soft_state_dependencies = pb_config.soft_state_dependencies();
#endif
    service_state state =
        (dep->master_service_ptr->get_state_type() == notifier::soft &&
         !soft_state_dependencies)
            ? dep->master_service_ptr->get_last_hard_state()
            : dep->master_service_ptr->get_current_state();

    /* Is the service we depend on in state that fails the dependency tests? */
    if (dep->get_fail_on(state))
      return false;

    if (state == service::state_ok &&
        !dep->master_service_ptr->has_been_checked() &&
        dep->get_fail_on_pending())
      return false;

    /* Immediate dependencies ok at this point - check parent dependencies if
     * necessary */
    if (dep->get_inherits_parent()) {
      if (!dep->master_service_ptr->authorized_by_dependencies(dependency_type))
        return false;
    }
  }
  return true;
}

/* check for services that never returned from a check... */
void service::check_for_orphaned() {
  time_t current_time{0L};
  time_t expected_time{0L};

  engine_logger(dbg_functions, basic) << "check_for_orphaned_services()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_for_orphaned_services()");

  /* get the current time */
  time(&current_time);

  uint32_t service_check_timeout;
  uint32_t check_reaper_interval;
#ifdef LEGACY_CONF
  service_check_timeout = config->service_check_timeout();
  check_reaper_interval = config->check_reaper_interval();
#else
  service_check_timeout = pb_config.service_check_timeout();
  check_reaper_interval = pb_config.check_reaper_interval();
#endif
  /* check all services... */
  for (service_map::iterator it(service::services.begin()),
       end(service::services.end());
       it != end; ++it) {
    /* skip services that are not currently executing */
    if (!it->second->get_is_executing())
      continue;

    /* determine the time at which the check results should have come in (allow
     * 10 minutes slack time) */
    expected_time =
        (time_t)(it->second->get_next_check() + it->second->get_latency() +
                 service_check_timeout + check_reaper_interval + 600);

    /* this service was supposed to have executed a while ago, but for some
     * reason the results haven't come back in... */
    if (expected_time < current_time) {
      /* log a warning */
      engine_logger(log_runtime_warning, basic)
          << "Warning: The check of service '" << it->first.second
          << "' on host '" << it->first.first
          << "' looks like it was orphaned "
             "(results never came back).  I'm scheduling an immediate check "
             "of the service...";
      SPDLOG_LOGGER_WARN(
          runtime_logger,
          "Warning: The check of service '{}' on host '{}' looks like it was "
          "orphaned "
          "(results never came back).  I'm scheduling an immediate check "
          "of the service...",
          it->first.second, it->first.first);

      engine_logger(dbg_checks, more)
          << "Service '" << it->first.second << "' on host '" << it->first.first
          << "' was orphaned, so we're scheduling an immediate check...";
      SPDLOG_LOGGER_DEBUG(
          checks_logger,
          "Service '{}' on host '{}' was orphaned, so we're scheduling an "
          "immediate check...",
          it->first.second, it->first.first);

      /* decrement the number of running service checks */
      if (currently_running_service_checks > 0)
        currently_running_service_checks--;

      /* disable the executing flag */
      it->second->set_is_executing(false);

      /* schedule an immediate check of the service */
      it->second->schedule_check(current_time, CHECK_OPTION_ORPHAN_CHECK);
    }
  }
}

/* check freshness of service results */
void service::check_result_freshness() {
  time_t current_time{0L};

  engine_logger(dbg_functions, basic) << "check_service_result_freshness()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_service_result_freshness()");
  engine_logger(dbg_checks, more)
      << "Checking the freshness of service check results...";
  SPDLOG_LOGGER_DEBUG(checks_logger,
                      "Checking the freshness of service check results...");

  /* bail out if we're not supposed to be checking freshness */

#ifdef LEGACY_CONF
  bool check_service_freshness = config->check_service_freshness();
#else
  bool check_service_freshness = pb_config.check_service_freshness();
#endif
  if (!check_service_freshness) {
    engine_logger(dbg_checks, more)
        << "Service freshness checking is disabled.";
    SPDLOG_LOGGER_DEBUG(checks_logger,
                        "Service freshness checking is disabled.");
    return;
  }
  /* get the current time */
  time(&current_time);

  /* check all services... */
  for (service_map::iterator it(service::services.begin()),
       end(service::services.end());
       it != end; ++it) {
    /* skip services we shouldn't be checking for freshness */
    if (!it->second->check_freshness_enabled())
      continue;

    /* skip services that are currently executing (problems here will be caught
     * by orphaned service check) */
    if (it->second->get_is_executing())
      continue;

    /* skip services that have both active and passive checks disabled */
    if (!it->second->active_checks_enabled() &&
        !it->second->passive_checks_enabled())
      continue;

    /* skip services that are already being freshened */
    if (it->second->get_is_being_freshened())
      continue;

    // See if the time is right...
    {
      timezone_locker lock(it->second->get_timezone());
      if (!check_time_against_period(current_time,
                                     it->second->check_period_ptr))
        continue;
    }

    /* EXCEPTION */
    /* don't check freshness of services without regular check intervals if
     * we're using auto-freshness threshold */
    if (it->second->check_interval() == 0 &&
        it->second->get_freshness_threshold() == 0)
      continue;

    /* the results for the last check of this service are stale! */
    if (!it->second->is_result_fresh(current_time, true)) {
      /* set the freshen flag */
      it->second->set_is_being_freshened(true);

      /* schedule an immediate forced check of the service */
      it->second->schedule_check(
          current_time,
          CHECK_OPTION_FORCE_EXECUTION | CHECK_OPTION_FRESHNESS_CHECK);
    }
  }
}

const std::string& service::get_current_state_as_string() const {
  return tab_service_states[get_current_state()].second;
}

bool service::get_notify_on_current_state() const {
#ifdef LEGACY_CONF
  bool soft_state_dependencies = config->soft_state_dependencies();
#else
  bool soft_state_dependencies = pb_config.soft_state_dependencies();
#endif
  if (_host_ptr->get_current_state() != host::state_up &&
      (_host_ptr->get_state_type() || soft_state_dependencies))
    return false;
  notification_flag type[]{ok, warning, critical, unknown};
  return get_notify_on(type[get_current_state()]);
}

bool service::is_in_downtime() const {
  return get_scheduled_downtime_depth() > 0 ||
         _host_ptr->get_scheduled_downtime_depth() > 0;
}

void service::set_host_ptr(host* h) {
  _host_ptr = h;
}

const host* service::get_host_ptr() const {
  return _host_ptr;
}

host* service::get_host_ptr() {
  return _host_ptr;
}

void service::resolve(uint32_t& w, uint32_t& e) {
  uint32_t warnings = 0;
  uint32_t errors = 0;

  try {
    notifier::resolve(warnings, errors);
  } catch (std::exception const& e) {
    engine_logger(log_verification_error, basic)
        << "Error: Service description '" << name() << "' of host '"
        << _hostname << "' has problem in its notifier part: " << e.what();
    config_logger->error(
        "Error: Service description '{}' of host '{}' has problem in its "
        "notifier part: {}",
        name(), _hostname, e.what());
  }

  {
    /* check for a valid host */
    host_map::const_iterator it{host::hosts.find(_hostname)};

    /* we couldn't find an associated host! */

    if (it == host::hosts.end() || !it->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Host '" << _hostname
          << "' specified in service "
             "'"
          << name() << "' not defined anywhere!";
      config_logger->error(
          "Error: Host '{}' specified in service '{}' not defined anywhere!",
          _hostname, name());
      errors++;
      set_host_ptr(nullptr);
    } else {
      /* save the host pointer for later */
      set_host_ptr(it->second.get());

      /* add a reverse link from the host to the service for faster lookups
       * later
       */
      it->second->services.insert({{_hostname, name()}, this});

      // Notify event broker.
      broker_relation_data(NEBTYPE_PARENT_ADD, get_host_ptr(), nullptr, nullptr,
                           this);
    }
  }

  // Check for sane recovery options.
  if (get_notifications_enabled() && get_notify_on(notifier::ok) &&
      !get_notify_on(notifier::warning) && !get_notify_on(notifier::critical)) {
    engine_logger(log_verification_error, basic)
        << "Warning: Recovery notification option in service '" << name()
        << "' for host '" << _hostname
        << "' doesn't make any sense - specify warning and /or critical "
           "options as well";
    config_logger->warn(
        "Warning: Recovery notification option in service '{}' for host '{}' "
        "doesn't make any sense - specify warning and /or critical "
        "options as well",
        name(), _hostname);
    warnings++;
  }

  // See if the notification interval is less than the check interval.
  if (get_notifications_enabled() && get_notification_interval() &&
      get_notification_interval() < check_interval()) {
    engine_logger(log_verification_error, basic)
        << "Warning: Service '" << name() << "' on host '" << _hostname
        << "'  has a notification interval less than "
           "its check interval!  Notifications are only re-sent after "
           "checks are made, so the effective notification interval will "
           "be that of the check interval.";
    config_logger->warn(
        "Warning: Service '{}' on host '{}'  has a notification interval less "
        "than "
        "its check interval!  Notifications are only re-sent after "
        "checks are made, so the effective notification interval will "
        "be that of the check interval.",
        name(), _hostname);
    warnings++;
  }

  /* check for illegal characters in service description */
  if (contains_illegal_object_chars(name().c_str())) {
    engine_logger(log_verification_error, basic)
        << "Error: The description string for service '" << name()
        << "' on host '" << _hostname
        << "' contains one or more illegal characters.";
    config_logger->error(
        "Error: The description string for service '{}' on host '{}' contains "
        "one or more illegal characters.",
        name(), _hostname);
    errors++;
  }

  w += warnings;
  e += errors;

  if (errors)
    throw engine_error() << "Cannot resolve service '" << name()
                         << "' of host '" << _hostname << "'";
}

bool service::get_host_problem_at_last_check() const {
  return _host_problem_at_last_check;
}

/**
 * @brief Accessor to the service type: SERVICE, METASERVICE, ANOMALY DETECTION
 * or BA.
 *
 * @return the service sype.
 */
service_type service::get_service_type() const {
  return _service_type;
}

/**
 * @brief update command object
 *
 * @param cmd
 */
void service::set_check_command_ptr(
    const std::shared_ptr<commands::command>& cmd) {
  std::shared_ptr<commands::command> old = get_check_command_ptr();
  if (cmd == old) {
    return;
  }
  if (old) {
    old->unregister_host_serv(_hostname, description());
  }
  notifier::set_check_command_ptr(cmd);
  if (cmd) {
    cmd->register_host_serv(_hostname, description());
  }
}

/**
 * @brief calculate final check command with macros replaced
 *
 * @return std::string
 */
std::string service::get_check_command_line(nagios_macros* macros) {
  grab_host_macros_r(macros, get_host_ptr());
  grab_service_macros_r(macros, this);
  std::string tmp;
  get_raw_command_line_r(macros, get_check_command_ptr(),
                         check_command().c_str(), tmp, 0);
  return get_check_command_ptr()->process_cmd(macros);
}
