/**
 * Copyright 2009-2013,2015 Centreon
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

#include "com/centreon/broker/neb/service.hh"

#include "com/centreon/broker/sql/table_max_size.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::neb;

/**
 *  @brief Default constructor.
 *
 *  Initialize members to 0, NULL or equivalent.
 */
service::service() : service_status(service::static_type()) {
  _zero_initialize();
}

/**
 *  @brief Build a service from a service_status.
 *
 *  Copy all members from service_status to the current instance and
 *  zero-initialize remaining members.
 *
 *  @param[in] ss  Object to copy.
 */
service::service(service_status const& ss) : service_status(ss) {
  _zero_initialize();
}

/**
 *  @brief Copy constructor.
 *
 *  Copy all members of the given service object to the current
 *  instance.
 *
 *  @param[in] other  Object to copy.
 */
service::service(service const& other)
    : host_service(other), service_status(other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
service::~service() {}

/**
 *  @brief Assignment operator.
 *
 *  Copy all members of the given service object to the current
 *  instance.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
service& service::operator=(service const& other) {
  if (this != &other) {
    host_service::operator=(other);
    service_status::operator=(other);
    _internal_copy(other);
  }
  return *this;
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  @brief Copy members of the service object to the current instance.
 *
 *  Copy all members defined within the service class. This method is
 *  used by the copy constructor and the assignment operator.
 *
 *  @param[in] other  Object to copy.
 */
void service::_internal_copy(service const& other) {
  flap_detection_on_critical = other.flap_detection_on_critical;
  flap_detection_on_ok = other.flap_detection_on_ok;
  flap_detection_on_unknown = other.flap_detection_on_unknown;
  flap_detection_on_warning = other.flap_detection_on_warning;
  is_volatile = other.is_volatile;
  notify_on_critical = other.notify_on_critical;
  notify_on_unknown = other.notify_on_unknown;
  notify_on_warning = other.notify_on_warning;
  stalk_on_critical = other.stalk_on_critical;
  stalk_on_ok = other.stalk_on_ok;
  stalk_on_unknown = other.stalk_on_unknown;
  stalk_on_warning = other.stalk_on_warning;
}

/**
 *  @brief Initialize members to 0, NULL or equivalent.
 *
 *  This method initializes members defined within the Service class to
 *  0, NULL or equivalent. This method is used by some constructors.
 */
void service::_zero_initialize() {
  flap_detection_on_critical = false;
  flap_detection_on_ok = false;
  flap_detection_on_unknown = false;
  flap_detection_on_warning = false;
  is_volatile = false;
  notify_on_critical = false;
  notify_on_unknown = false;
  notify_on_warning = false;
  stalk_on_critical = false;
  stalk_on_ok = false;
  stalk_on_unknown = false;
  stalk_on_warning = false;
}

/**************************************
 *                                     *
 *           Static Objects            *
 *                                     *
 **************************************/

// Mapping. Some pointer-to-member are explicitely casted because they
// are from the host_service class which does not inherit from io::data.
mapping::entry const service::entries[] = {
    mapping::entry(&service::acknowledged, "acknowledged"),
    mapping::entry(&service::acknowledgement_type, "acknowledgement_type"),
    mapping::entry(static_cast<std::string(service::*)>(&service::action_url),
                   "action_url",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_action_url)),
    mapping::entry(&service::active_checks_enabled, "active_checks"),
    mapping::entry(static_cast<bool(service::*)>(&service::check_freshness),
                   "check_freshness"),
    mapping::entry(&service::check_interval, "check_interval"),
    mapping::entry(&service::check_period,
                   "check_period",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_check_period)),
    mapping::entry(&service::check_type, "check_type"),
    mapping::entry(&service::current_check_attempt, "check_attempt"),
    mapping::entry(&service::current_state, "state"),
    mapping::entry(
        static_cast<bool(service::*)>(&service::default_active_checks_enabled),
        "default_active_checks"),
    mapping::entry(
        static_cast<bool(service::*)>(&service::default_event_handler_enabled),
        "default_event_handler_enabled"),
    mapping::entry(
        static_cast<bool(service::*)>(&service::default_flap_detection_enabled),
        "default_flap_detection"),
    mapping::entry(
        static_cast<bool(service::*)>(&service::default_notifications_enabled),
        "default_notify"),
    mapping::entry(
        static_cast<bool(service::*)>(&service::default_passive_checks_enabled),
        "default_passive_checks"),
    mapping::entry(&service::downtime_depth, "scheduled_downtime_depth"),
    mapping::entry(static_cast<std::string(service::*)>(&service::display_name),
                   "display_name",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_display_name)),
    mapping::entry(&service::enabled, "enabled"),
    mapping::entry(&service::event_handler,
                   "event_handler",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_event_handler)),
    mapping::entry(&service::event_handler_enabled, "event_handler_enabled"),
    mapping::entry(&service::execution_time, "execution_time"),
    mapping::entry(
        static_cast<double(service::*)>(&service::first_notification_delay),
        "first_notification_delay"),
    mapping::entry(&service::flap_detection_enabled, "flap_detection"),
    mapping::entry(&service::flap_detection_on_critical,
                   "flap_detection_on_critical"),
    mapping::entry(&service::flap_detection_on_ok, "flap_detection_on_ok"),
    mapping::entry(&service::flap_detection_on_unknown,
                   "flap_detection_on_unknown"),
    mapping::entry(&service::flap_detection_on_warning,
                   "flap_detection_on_warning"),
    mapping::entry(
        static_cast<double(service::*)>(&service::freshness_threshold),
        "freshness_threshold"),
    mapping::entry(&service::has_been_checked, "checked"),
    mapping::entry(
        static_cast<double(service::*)>(&service::high_flap_threshold),
        "high_flap_threshold"),
    mapping::entry(&service::host_id,
                   "host_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::host_name, nullptr, 0),
    mapping::entry(static_cast<std::string(service::*)>(&service::icon_image),
                   "icon_image",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_icon_image)),
    mapping::entry(
        static_cast<std::string(service::*)>(&service::icon_image_alt),
        "icon_image_alt",
        get_centreon_storage_services_col_size(
            centreon_storage_services_icon_image_alt)),
    mapping::entry(&service::service_id,
                   "service_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::is_flapping, "flapping"),
    mapping::entry(&service::is_volatile, "volatile"),
    mapping::entry(&service::last_check,
                   "last_check",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::last_hard_state, "last_hard_state"),
    mapping::entry(&service::last_hard_state_change,
                   "last_hard_state_change",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::last_notification,
                   "last_notification",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::last_state_change,
                   "last_state_change",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::last_time_critical,
                   "last_time_critical",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::last_time_ok,
                   "last_time_ok",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::last_time_unknown,
                   "last_time_unknown",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::last_time_warning,
                   "last_time_warning",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::last_update,
                   "last_update",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::latency, "latency"),
    mapping::entry(
        static_cast<double(service::*)>(&service::low_flap_threshold),
        "low_flap_threshold"),
    mapping::entry(&service::max_check_attempts, "max_check_attempts"),
    mapping::entry(&service::next_check,
                   "next_check",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::next_notification,
                   "next_notification",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&service::no_more_notifications, "no_more_notifications"),
    mapping::entry(static_cast<std::string(service::*)>(&service::notes),
                   "notes",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_notes)),
    mapping::entry(static_cast<std::string(service::*)>(&service::notes_url),
                   "notes_url",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_notes_url)),
    mapping::entry(
        static_cast<double(service::*)>(&service::notification_interval),
        "notification_interval"),
    mapping::entry(&service::notification_number,
                   "notification_number",
                   mapping::entry::invalid_on_negative),
    mapping::entry(
        static_cast<std::string(service::*)>(&service::notification_period),
        "notification_period",
        get_centreon_storage_services_col_size(
            centreon_storage_services_notification_period)),
    mapping::entry(&service_status::notifications_enabled, "notify"),
    mapping::entry(&service::notify_on_critical, "notify_on_critical"),
    mapping::entry(static_cast<bool(service::*)>(&service::notify_on_downtime),
                   "notify_on_downtime"),
    mapping::entry(static_cast<bool(service::*)>(&service::notify_on_flapping),
                   "notify_on_flapping"),
    mapping::entry(static_cast<bool(service::*)>(&service::notify_on_recovery),
                   "notify_on_recovery"),
    mapping::entry(&service::notify_on_unknown, "notify_on_unknown"),
    mapping::entry(&service::notify_on_warning, "notify_on_warning"),
    mapping::entry(&service::obsess_over, "obsess_over_service"),
    mapping::entry(&service::passive_checks_enabled, "passive_checks"),
    mapping::entry(&service::percent_state_change, "percent_state_change"),
    mapping::entry(&service::retry_interval, "retry_interval"),
    mapping::entry(&service::service_description,
                   "description",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_description)),
    mapping::entry(&service::should_be_scheduled, "should_be_scheduled"),
    mapping::entry(&service::stalk_on_critical, "stalk_on_critical"),
    mapping::entry(&service::stalk_on_ok, "stalk_on_ok"),
    mapping::entry(&service::stalk_on_unknown, "stalk_on_unknown"),
    mapping::entry(&service::stalk_on_warning, "stalk_on_warning"),
    mapping::entry(&service::state_type, "state_type"),
    mapping::entry(&service::check_command,
                   "check_command",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_check_command)),
    mapping::entry(&service::output,
                   "output",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_output)),
    mapping::entry(&service::perf_data,
                   "perfdata",
                   get_centreon_storage_services_col_size(
                       centreon_storage_services_perfdata)),
    mapping::entry(
        static_cast<bool(service::*)>(&service::retain_nonstatus_information),
        "retain_nonstatus_information"),
    mapping::entry(
        static_cast<bool(service::*)>(&service::retain_status_information),
        "retain_status_information"),
    mapping::entry()};

// Operations.
static io::data* new_service() {
  return new service;
}
io::event_info::event_operations const service::operations = {&new_service,
                                                              nullptr, nullptr};
