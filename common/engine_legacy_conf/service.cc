/**
 * Copyright 2011-2013,2015-2017,2019-2024 Centreon
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

#include "service.hh"
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include "bbdo/bam_state.pb.h"
#include "bbdo/neb.pb.h"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<service, type, &service::method>::generic

std::unordered_map<std::string, service::setter_func> const service::_setters{
    {"host", SETTER(std::string const&, _set_host_name)},
    {"hosts", SETTER(std::string const&, _set_host_name)},
    {"host_name", SETTER(std::string const&, _set_host_name)},
    {"service_description",
     SETTER(std::string const&, _set_service_description)},
    {"service_id", SETTER(uint64_t, set_service_id)},
    {"_SERVICE_ID", SETTER(uint64_t, set_service_id)},
    {"acknowledgement_timeout", SETTER(int, set_acknowledgement_timeout)},
    {"description", SETTER(std::string const&, _set_service_description)},
    {"display_name", SETTER(std::string const&, _set_display_name)},
    {"service_groups", SETTER(std::string const&, _set_servicegroups)},
    {"servicegroups", SETTER(std::string const&, _set_servicegroups)},
    {"check_command", SETTER(std::string const&, _set_check_command)},
    {"check_period", SETTER(std::string const&, _set_check_period)},
    {"event_handler", SETTER(std::string const&, _set_event_handler)},
    {"notification_period",
     SETTER(std::string const&, _set_notification_period)},
    {"contact_groups", SETTER(std::string const&, _set_contactgroups)},
    {"contacts", SETTER(std::string const&, _set_contacts)},
    {"notes", SETTER(std::string const&, _set_notes)},
    {"notes_url", SETTER(std::string const&, _set_notes_url)},
    {"action_url", SETTER(std::string const&, _set_action_url)},
    {"icon_image", SETTER(std::string const&, _set_icon_image)},
    {"icon_image_alt", SETTER(std::string const&, _set_icon_image_alt)},
    {"max_check_attempts", SETTER(unsigned int, _set_max_check_attempts)},
    {"check_interval", SETTER(unsigned int, _set_check_interval)},
    {"normal_check_interval", SETTER(unsigned int, _set_check_interval)},
    {"retry_interval", SETTER(unsigned int, _set_retry_interval)},
    {"retry_check_interval", SETTER(unsigned int, _set_retry_interval)},
    {"recovery_notification_delay",
     SETTER(unsigned int, _set_recovery_notification_delay)},
    {"active_checks_enabled", SETTER(bool, _set_checks_active)},
    {"passive_checks_enabled", SETTER(bool, _set_checks_passive)},
    {"is_volatile", SETTER(bool, _set_is_volatile)},
    {"obsess_over_service", SETTER(bool, _set_obsess_over_service)},
    {"event_handler_enabled", SETTER(bool, _set_event_handler_enabled)},
    {"check_freshness", SETTER(bool, _set_check_freshness)},
    {"freshness_threshold", SETTER(unsigned int, _set_freshness_threshold)},
    {"low_flap_threshold", SETTER(unsigned int, _set_low_flap_threshold)},
    {"high_flap_threshold", SETTER(unsigned int, _set_high_flap_threshold)},
    {"flap_detection_enabled", SETTER(bool, _set_flap_detection_enabled)},
    {"flap_detection_options",
     SETTER(std::string const&, _set_flap_detection_options)},
    {"notification_options",
     SETTER(std::string const&, _set_notification_options)},
    {"notifications_enabled", SETTER(bool, _set_notifications_enabled)},
    {"notification_interval", SETTER(unsigned int, _set_notification_interval)},
    {"first_notification_delay",
     SETTER(unsigned int, _set_first_notification_delay)},
    {"stalking_options", SETTER(std::string const&, _set_stalking_options)},
    {"process_perf_data", SETTER(bool, _set_process_perf_data)},
    {"retain_status_information", SETTER(bool, _set_retain_status_information)},
    {"retain_nonstatus_information",
     SETTER(bool, _set_retain_nonstatus_information)},
    {"timezone", SETTER(std::string const&, _set_timezone)},
    {"severity", SETTER(uint64_t, _set_severity_id)},
    {"severity_id", SETTER(uint64_t, _set_severity_id)},
    {"category_tags", SETTER(std::string const&, _set_category_tags)},
    {"group_tags", SETTER(std::string const&, _set_group_tags)},
    {"icon_id", SETTER(uint64_t, _set_icon_id)},
};

// Default values.
static int default_acknowledgement_timeout(0);
static bool const default_checks_active(true);
static bool const default_checks_passive(true);
static bool const default_check_freshness(0);
static unsigned int const default_check_interval(5);
static bool const default_event_handler_enabled(true);
static unsigned int const default_first_notification_delay(0);
static bool const default_flap_detection_enabled(true);
static unsigned short const default_flap_detection_options(service::ok |
                                                           service::warning |
                                                           service::unknown |
                                                           service::critical);
static unsigned int const default_freshness_threshold(0);
static unsigned int const default_high_flap_threshold(0);
static bool const default_is_volatile(false);
static unsigned int const default_low_flap_threshold(0);
static unsigned int const default_max_check_attempts(3);
static bool const default_notifications_enabled(true);
static unsigned int const default_notification_interval(0);
static unsigned short const default_notification_options(
    service::ok | service::warning | service::critical | service::unknown |
    service::flapping | service::downtime);
static std::string const default_notification_period;
static bool const default_obsess_over_service(true);
static bool const default_process_perf_data(true);
static bool const default_retain_nonstatus_information(true);
static bool const default_retain_status_information(true);
static unsigned int const default_retry_interval(1);
static unsigned short const default_stalking_options(service::none);

/**
 *  Default constructor.
 */
service::service()
    : object(object::service),
      _acknowledgement_timeout(default_acknowledgement_timeout),
      _checks_active(default_checks_active),
      _checks_passive(default_checks_passive),
      _check_command_is_important(false),
      _check_freshness(default_check_freshness),
      _check_interval(default_check_interval),
      _event_handler_enabled(default_event_handler_enabled),
      _first_notification_delay(default_first_notification_delay),
      _flap_detection_enabled(default_flap_detection_enabled),
      _flap_detection_options(default_flap_detection_options),
      _freshness_threshold(default_freshness_threshold),
      _high_flap_threshold(default_high_flap_threshold),
      _is_volatile(default_is_volatile),
      _low_flap_threshold(default_low_flap_threshold),
      _max_check_attempts(default_max_check_attempts),
      _notifications_enabled(default_notifications_enabled),
      _notification_interval(default_notification_interval),
      _notification_options(default_notification_options),
      _obsess_over_service(default_obsess_over_service),
      _process_perf_data(default_process_perf_data),
      _retain_nonstatus_information(default_retain_nonstatus_information),
      _retain_status_information(default_retain_status_information),
      _retry_interval(default_retry_interval),
      _recovery_notification_delay(0),
      _host_id(0),
      _service_id(0),
      _stalking_options(default_stalking_options),
      _severity_id{0u},
      _icon_id{0u} {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  The service to copy.
 */
service::service(service const& other)
    : object(other),
      _acknowledgement_timeout(other._acknowledgement_timeout),
      _action_url(other._action_url),
      _checks_active(other._checks_active),
      _checks_passive(other._checks_passive),
      _check_command(other._check_command),
      _check_command_is_important(other._check_command_is_important),
      _check_freshness(other._check_freshness),
      _check_interval(other._check_interval),
      _check_period(other._check_period),
      _contactgroups(other._contactgroups),
      _contacts(other._contacts),
      _customvariables(other._customvariables),
      _display_name(other._display_name),
      _event_handler(other._event_handler),
      _event_handler_enabled(other._event_handler_enabled),
      _first_notification_delay(other._first_notification_delay),
      _flap_detection_enabled(other._flap_detection_enabled),
      _flap_detection_options(other._flap_detection_options),
      _freshness_threshold(other._freshness_threshold),
      _high_flap_threshold(other._high_flap_threshold),
      _host_name(other._host_name),
      _icon_image(other._icon_image),
      _icon_image_alt(other._icon_image_alt),
      _is_volatile(other._is_volatile),
      _low_flap_threshold(other._low_flap_threshold),
      _max_check_attempts(other._max_check_attempts),
      _notes(other._notes),
      _notes_url(other._notes_url),
      _notifications_enabled(other._notifications_enabled),
      _notification_interval(other._notification_interval),
      _notification_options(other._notification_options),
      _notification_period(other._notification_period),
      _obsess_over_service(other._obsess_over_service),
      _process_perf_data(other._process_perf_data),
      _retain_nonstatus_information(other._retain_nonstatus_information),
      _retain_status_information(other._retain_status_information),
      _retry_interval(other._retry_interval),
      _recovery_notification_delay(other._recovery_notification_delay),
      _servicegroups(other._servicegroups),
      _service_description(other._service_description),
      _host_id(other._host_id),
      _service_id(other._service_id),
      _stalking_options(other._stalking_options),
      _timezone(other._timezone),
      _severity_id{other._severity_id},
      _icon_id{other._icon_id},
      _tags{other._tags} {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  The service to copy.
 *
 *  @return This service.
 */
service& service::operator=(service const& other) {
  if (this != &other) {
    object::operator=(other);
    _acknowledgement_timeout = other._acknowledgement_timeout;
    _action_url = other._action_url;
    _checks_active = other._checks_active;
    _checks_passive = other._checks_passive;
    _check_command = other._check_command;
    _check_command_is_important = other._check_command_is_important;
    _check_freshness = other._check_freshness;
    _check_interval = other._check_interval;
    _check_period = other._check_period;
    _contactgroups = other._contactgroups;
    _contacts = other._contacts;
    _customvariables = other._customvariables;
    _display_name = other._display_name;
    _event_handler = other._event_handler;
    _event_handler_enabled = other._event_handler_enabled;
    _first_notification_delay = other._first_notification_delay;
    _flap_detection_enabled = other._flap_detection_enabled;
    _flap_detection_options = other._flap_detection_options;
    _freshness_threshold = other._freshness_threshold;
    _high_flap_threshold = other._high_flap_threshold;
    _host_name = other._host_name;
    _icon_image = other._icon_image;
    _icon_image_alt = other._icon_image_alt;
    _is_volatile = other._is_volatile;
    _low_flap_threshold = other._low_flap_threshold;
    _max_check_attempts = other._max_check_attempts;
    _notes = other._notes;
    _notes_url = other._notes_url;
    _notifications_enabled = other._notifications_enabled;
    _notification_interval = other._notification_interval;
    _notification_options = other._notification_options;
    _notification_period = other._notification_period;
    _obsess_over_service = other._obsess_over_service;
    _process_perf_data = other._process_perf_data;
    _retain_nonstatus_information = other._retain_nonstatus_information;
    _retain_status_information = other._retain_status_information;
    _retry_interval = other._retry_interval;
    _recovery_notification_delay = other._recovery_notification_delay;
    _servicegroups = other._servicegroups;
    _service_description = other._service_description;
    _host_id = other._host_id;
    _service_id = other._service_id;
    _stalking_options = other._stalking_options;
    _timezone = other._timezone;
    _severity_id = other._severity_id;
    _icon_id = other._icon_id;
    _tags = other._tags;
  }
  return *this;
}

/**
 *  Equality operator.
 *
 *  @param[in] other  The service to compare.
 *
 *  @return True if is the same service, otherwise false.
 */
bool service::operator==(service const& other) const noexcept {
  if (!object::operator==(other)) {
    _logger->debug("configuration::service::equality => object don't match");
    return false;
  }
  if (_acknowledgement_timeout != other._acknowledgement_timeout) {
    _logger->debug(
        "configuration::service::equality => "
        "acknowledgement_timeout don't match");
    return false;
  }
  if (_action_url != other._action_url) {
    _logger->debug(
        "configuration::service::equality => action_url don't match");
    return false;
  }
  if (_checks_active != other._checks_active) {
    _logger->debug(
        "configuration::service::equality => checks_active don't match");
    return false;
  }
  if (_checks_passive != other._checks_passive) {
    _logger->debug(
        "configuration::service::equality => checks_passive don't match");
    return false;
  }
  if (_check_command != other._check_command) {
    _logger->debug(
        "configuration::service::equality => checks_passive don't match");
    return false;
  }
  if (_check_command_is_important != other._check_command_is_important) {
    _logger->debug(
        "configuration::service::equality => check_command don't match");
    return false;
  }
  if (_check_freshness != other._check_freshness) {
    _logger->debug(
        "configuration::service::equality => check_freshness don't match");
    return false;
  }
  if (_check_interval != other._check_interval) {
    _logger->debug(
        "configuration::service::equality => check_interval don't match");
    return false;
  }
  if (_check_period != other._check_period) {
    _logger->debug(
        "configuration::service::equality => check_period don't match");
    return false;
  }
  if (_contactgroups != other._contactgroups) {
    _logger->debug(
        "configuration::service::equality => contactgroups don't match");
    return false;
  }
  if (_contacts != other._contacts) {
    _logger->debug("configuration::service::equality => contacts don't match");
    return false;
  }
  if (std::operator!=(_customvariables, other._customvariables)) {
    _logger->debug(
        "configuration::service::equality => customvariables don't match");
    return false;
  }
  if (_display_name != other._display_name) {
    _logger->debug(
        "configuration::service::equality => display_name don't match");
    return false;
  }
  if (_event_handler != other._event_handler) {
    _logger->debug(
        "configuration::service::equality => event_handler don't match");
    return false;
  }
  if (_event_handler_enabled != other._event_handler_enabled) {
    _logger->debug(
        "configuration::service::equality => event_handler don't match");
    return false;
  }
  if (_first_notification_delay != other._first_notification_delay) {
    _logger->debug(
        "configuration::service::equality => "
        "first_notification_delay don't match");
    return false;
  }
  if (_flap_detection_enabled != other._flap_detection_enabled) {
    _logger->debug(
        "configuration::service::equality => "
        "flap_detection_enabled don't match");
    return false;
  }
  if (_flap_detection_options != other._flap_detection_options) {
    _logger->debug(
        "configuration::service::equality => "
        "flap_detection_options don't match");
    return false;
  }
  if (_freshness_threshold != other._freshness_threshold) {
    _logger->debug(
        "configuration::service::equality => "
        "freshness_threshold don't match");
    return false;
  }
  if (_high_flap_threshold != other._high_flap_threshold) {
    _logger->debug(
        "configuration::service::equality => "
        "high_flap_threshold don't match");
    return false;
  }
  if (_host_name != other._host_name) {
    _logger->debug(
        "configuration::service::equality => _host_name don't match");
    return false;
  }
  if (_icon_image != other._icon_image) {
    _logger->debug(
        "configuration::service::equality => icon_image don't match");
    return false;
  }
  if (_icon_image_alt != other._icon_image_alt) {
    _logger->debug(
        "configuration::service::equality => icon_image_alt don't match");
    return false;
  }
  if (_is_volatile != other._is_volatile) {
    _logger->debug(
        "configuration::service::equality => is_volatile don't match");
    return false;
  }
  if (_low_flap_threshold != other._low_flap_threshold) {
    _logger->debug(
        "configuration::service::equality => low_flap_threshold don't match");
    return false;
  }
  if (_max_check_attempts != other._max_check_attempts) {
    _logger->debug(
        "configuration::service::equality => max_check_attempts don't match");
    return false;
  }
  if (_notes != other._notes) {
    _logger->debug("configuration::service::equality => notes don't match");
    return false;
  }
  if (_notes_url != other._notes_url) {
    _logger->debug("configuration::service::equality => notes_url don't match");
    return false;
  }
  if (_notifications_enabled != other._notifications_enabled) {
    _logger->debug(
        "configuration::service::equality => "
        "notifications_enabled don't match");
    return false;
  }
  if (_notification_interval != other._notification_interval) {
    _logger->debug(
        "configuration::service::equality => "
        "notification_interval don't match");
    return false;
  }
  if (_notification_options != other._notification_options) {
    _logger->debug(
        "configuration::service::equality => "
        "notification_options don't match");
    return false;
  }
  if (_notification_period != other._notification_period) {
    _logger->debug(
        "configuration::service::equality => "
        "notification_period don't match");
    return false;
  }
  if (_obsess_over_service != other._obsess_over_service) {
    _logger->debug(
        "configuration::service::equality => "
        "obsess_over_service don't match");
    return false;
  }
  if (_process_perf_data != other._process_perf_data) {
    _logger->debug(
        "configuration::service::equality => process_perf_data don't match");
    return false;
  }
  if (_retain_nonstatus_information != other._retain_nonstatus_information) {
    _logger->debug(
        "configuration::service::equality => "
        "retain_nonstatus_information don't match");
    return false;
  }
  if (_retain_status_information != other._retain_status_information) {
    _logger->debug(
        "configuration::service::equality => "
        "retain_status_information don't match");
    return false;
  }
  if (_retry_interval != other._retry_interval) {
    _logger->debug(
        "configuration::service::equality => retry_interval don't match");
    return false;
  }
  if (_recovery_notification_delay != other._recovery_notification_delay) {
    _logger->debug(
        "configuration::service::equality => "
        "recovery_notification_delay don't match");
    return false;
  }
  if (_servicegroups != other._servicegroups) {
    _logger->debug(
        "configuration::service::equality => servicegroups don't match");
    return false;
  }
  if (_service_description != other._service_description) {
    _logger->debug(
        "configuration::service::equality => "
        "service_description don't match");
    return false;
  }
  if (_host_id != other._host_id) {
    _logger->debug("configuration::service::equality => host_id don't match");
    return false;
  }
  if (_service_id != other._service_id) {
    _logger->debug(
        "configuration::service::equality => service_id don't match");
    return false;
  }
  if (_stalking_options != other._stalking_options) {
    _logger->debug(
        "configuration::service::equality => stalking_options don't match");
    return false;
  }
  if (_timezone != other._timezone) {
    _logger->debug("configuration::service::equality => timezone don't match");
    return false;
  }
  if (_severity_id != other._severity_id) {
    _logger->debug(
        "configuration::service::equality => severity id don't match");
    return false;
  }
  if (_icon_id != other._icon_id) {
    _logger->debug("configuration::service::equality => icon id don't match");
    return false;
  }
  if (_tags != other._tags) {
    _logger->debug("configuration::service::equality => tags don't match");
    return false;
  }
  _logger->debug("configuration::service::equality => OK");
  return true;
}

/**
 *  Inequality operator.
 *
 *  @param[in] other  The service to compare.
 *
 *  @return True if is not the same service, otherwise false.
 */
bool service::operator!=(service const& other) const noexcept {
  return !operator==(other);
}

/**
 *  Less-than operator.
 *
 *  @param[in] other  Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool service::operator<(service const& other) const noexcept {
  // The configuration diff mechanism relies on this.
  if (_host_id != other._host_id)
    return _host_id < other._host_id;
  else if (_service_id != other._service_id)
    return _service_id < other._service_id;
  else if (_host_name != other._host_name)
    return _host_name < other._host_name;
  else if (_service_description != other._service_description)
    return _service_description < other._service_description;
  else if (_acknowledgement_timeout != other._acknowledgement_timeout)
    return _acknowledgement_timeout < other._acknowledgement_timeout;
  else if (_action_url != other._action_url)
    return _action_url < other._action_url;
  else if (_checks_active != other._checks_active)
    return _checks_active < other._checks_active;
  else if (_checks_passive != other._checks_passive)
    return _checks_passive < other._checks_passive;
  else if (_check_command != other._check_command)
    return _check_command < other._check_command;
  else if (_check_command_is_important != other._check_command_is_important)
    return (_check_command_is_important < other._check_command_is_important);
  else if (_check_freshness != other._check_freshness)
    return _check_freshness < other._check_freshness;
  else if (_check_interval != other._check_interval)
    return _check_interval < other._check_interval;
  else if (_check_period != other._check_period)
    return _check_period < other._check_period;
  else if (_contactgroups != other._contactgroups)
    return _contactgroups < other._contactgroups;
  else if (_contacts != other._contacts)
    return _contacts < other._contacts;
  else if (_customvariables != other._customvariables)
    return _customvariables.size() < other._customvariables.size();
  else if (_display_name != other._display_name)
    return _display_name < other._display_name;
  else if (_event_handler != other._event_handler)
    return _event_handler < other._event_handler;
  else if (_event_handler_enabled != other._event_handler_enabled)
    return _event_handler_enabled < other._event_handler_enabled;
  else if (_first_notification_delay != other._first_notification_delay)
    return (_first_notification_delay < other._first_notification_delay);
  else if (_flap_detection_enabled != other._flap_detection_enabled)
    return _flap_detection_enabled < other._flap_detection_enabled;
  else if (_flap_detection_options != other._flap_detection_options)
    return _flap_detection_options < other._flap_detection_options;
  else if (_freshness_threshold != other._freshness_threshold)
    return _freshness_threshold < other._freshness_threshold;
  else if (_high_flap_threshold != other._high_flap_threshold)
    return _high_flap_threshold < other._high_flap_threshold;
  else if (_icon_image != other._icon_image)
    return _icon_image < other._icon_image;
  else if (_icon_image_alt != other._icon_image_alt)
    return _icon_image_alt < other._icon_image_alt;
  else if (_is_volatile != other._is_volatile)
    return _is_volatile < other._is_volatile;
  else if (_low_flap_threshold != other._low_flap_threshold)
    return _low_flap_threshold < other._low_flap_threshold;
  else if (_max_check_attempts != other._max_check_attempts)
    return _max_check_attempts < other._max_check_attempts;
  else if (_notes != other._notes)
    return _notes < other._notes;
  else if (_notes_url != other._notes_url)
    return _notes_url < other._notes_url;
  else if (_notifications_enabled != other._notifications_enabled)
    return _notifications_enabled < other._notifications_enabled;
  else if (_notification_interval != other._notification_interval)
    return _notification_interval < other._notification_interval;
  else if (_notification_options != other._notification_options)
    return _notification_options < other._notification_options;
  else if (_notification_period != other._notification_period)
    return _notification_period < other._notification_period;
  else if (_obsess_over_service != other._obsess_over_service)
    return _obsess_over_service < other._obsess_over_service;
  else if (_process_perf_data != other._process_perf_data)
    return _process_perf_data < other._process_perf_data;
  else if (_retain_nonstatus_information != other._retain_nonstatus_information)
    return _retain_nonstatus_information < other._retain_nonstatus_information;
  else if (_retain_status_information != other._retain_status_information)
    return _retain_status_information < other._retain_status_information;
  else if (_retry_interval != other._retry_interval)
    return _retry_interval < other._retry_interval;
  else if (_recovery_notification_delay != other._recovery_notification_delay)
    return _recovery_notification_delay < other._recovery_notification_delay;
  else if (_servicegroups != other._servicegroups)
    return _servicegroups < other._servicegroups;
  else if (_stalking_options != other._stalking_options)
    return _stalking_options < other._stalking_options;
  else if (_timezone != other._timezone)
    return _timezone < other._timezone;
  else if (_severity_id != other._severity_id)
    return _severity_id < other._severity_id;
  else if (_icon_id != other._icon_id)
    return _icon_id < other._icon_id;
  return _tags < other._tags;
}

/**
 *  Check if the object is valid.
 *
 *  @return True if is a valid object, otherwise false.
 */
void service::check_validity(error_cnt& err [[maybe_unused]]) const {
  if (_service_description.empty())
    throw msg_fmt(
        "Service has no description (property 'service_description')");
  if (_host_name.empty())
    throw msg_fmt(
        "Service '{}' is not attached to any host or host group (properties "
        "'host_name' or 'hostgroup_name', respectively)",
        _service_description);
  if (_check_command.empty())
    throw msg_fmt(
        "Service '{}' has no check command (property 'check_command')",
        _service_description);
}

/**
 *  Get the service key.
 *
 *  @return A pair with host name and service description.
 */
service::key_type service::key() const {
  key_type k{_host_id, _service_id};
  return k;
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void service::merge(object const& obj) {
  if (obj.type() != _type)
    throw msg_fmt("Cannot merge service with '{}'",
                  static_cast<uint32_t>(obj.type()));
  service const& tmpl(static_cast<service const&>(obj));

  MRG_OPTION(_acknowledgement_timeout);
  MRG_DEFAULT(_action_url);
  MRG_IMPORTANT(_check_command);
  MRG_OPTION(_checks_active);
  MRG_OPTION(_checks_passive);
  MRG_OPTION(_check_freshness);
  MRG_OPTION(_check_interval);
  MRG_DEFAULT(_check_period);
  MRG_INHERIT(_contactgroups);
  MRG_INHERIT(_contacts);
  MRG_MAP(_customvariables);
  MRG_DEFAULT(_display_name);
  MRG_DEFAULT(_event_handler);
  MRG_OPTION(_event_handler_enabled);
  MRG_OPTION(_first_notification_delay);
  MRG_OPTION(_flap_detection_enabled);
  MRG_OPTION(_flap_detection_options);
  MRG_OPTION(_freshness_threshold);
  MRG_OPTION(_high_flap_threshold);
  MRG_DEFAULT(_host_name);
  MRG_DEFAULT(_icon_image);
  MRG_DEFAULT(_icon_image_alt);
  MRG_OPTION(_is_volatile);
  MRG_OPTION(_low_flap_threshold);
  MRG_OPTION(_max_check_attempts);
  MRG_DEFAULT(_notes);
  MRG_DEFAULT(_notes_url);
  MRG_OPTION(_notifications_enabled);
  MRG_OPTION(_notification_interval);
  MRG_OPTION(_notification_options);
  MRG_OPTION(_notification_period);
  MRG_OPTION(_obsess_over_service);
  MRG_OPTION(_process_perf_data);
  MRG_OPTION(_recovery_notification_delay);
  MRG_OPTION(_retain_nonstatus_information);
  MRG_OPTION(_retain_status_information);
  MRG_OPTION(_retry_interval);
  MRG_INHERIT(_servicegroups);
  MRG_DEFAULT(_service_description);
  MRG_OPTION(_stalking_options);
  MRG_OPTION(_timezone);
  MRG_OPTION(_severity_id);
  MRG_OPTION(_icon_id);
  MRG_MAP(_tags);
}

/**
 *  Parse and set the service property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool service::parse(char const* key, char const* value) {
  std::unordered_map<std::string, service::setter_func>::const_iterator it{
      _setters.find(key)};
  if (it != _setters.end())
    return (it->second)(*this, value);

  if (key[0] == '_') {
    auto it = _customvariables.find(key + 1);
    if (it == _customvariables.end())
      _customvariables[key + 1] = customvariable(value);
    else
      it->second.set_value(value);

    return true;
  }
  return false;
}

/**
 *  Get action_url.
 *
 *  @return The action_url.
 */
std::string const& service::action_url() const noexcept {
  return _action_url;
}

/**
 *  Get checks_active.
 *
 *  @return The checks_active.
 */
bool service::checks_active() const noexcept {
  return _checks_active;
}

/**
 *  Get checks_passive.
 *
 *  @return The checks_passive.
 */
bool service::checks_passive() const noexcept {
  return _checks_passive;
}

/**
 *  Get check_command.
 *
 *  @return The check_command.
 */
std::string const& service::check_command() const noexcept {
  return _check_command;
}

/**
 *  Get check_command_is_important.
 *
 *  @return The check_command_is_important.
 */
bool service::check_command_is_important() const noexcept {
  return _check_command_is_important;
}

/**
 *  Get check_freshness.
 *
 *  @return The check_freshness.
 */
bool service::check_freshness() const noexcept {
  return _check_freshness;
}

/**
 *  Get check_interval.
 *
 *  @return The check_interval.
 */
unsigned int service::check_interval() const noexcept {
  return _check_interval;
}

/**
 *  Get check_period.
 *
 *  @return The check_period.
 */
std::string const& service::check_period() const noexcept {
  return _check_period;
}

/**
 *  Get contactgroups.
 *
 *  @return The contactgroups.
 */
set_string& service::contactgroups() noexcept {
  return *_contactgroups;
}

/**
 *  Get contactgroups.
 *
 *  @return The contactgroups.
 */
set_string const& service::contactgroups() const noexcept {
  return *_contactgroups;
}

/**
 *  Check if contactgroups were defined.
 *
 *  @return True if contactgroups were defined.
 */
bool service::contactgroups_defined() const noexcept {
  return _contactgroups.is_set();
}

/**
 *  Get contacts.
 *
 *  @return The contacts.
 */
set_string& service::contacts() noexcept {
  return *_contacts;
}

/**
 *  Get contacts.
 *
 *  @return The contacts.
 */
set_string const& service::contacts() const noexcept {
  return *_contacts;
}

/**
 *  Check if contacts were defined.
 *
 *  @return True if contacts were defined.
 */
bool service::contacts_defined() const noexcept {
  return _contacts.is_set();
}

/**
 *  Get customvariables.
 *
 *  @return The customvariables.
 */
const std::unordered_map<std::string, customvariable>&
service::customvariables() const noexcept {
  return _customvariables;
}

/**
 *  Get customvariables.
 *
 *  @return The customvariables.
 */
std::unordered_map<std::string, customvariable>&
service::mut_customvariables() noexcept {
  return _customvariables;
}

/**
 *  Get display_name.
 *
 *  @return The display_name.
 */
std::string const& service::display_name() const noexcept {
  return _display_name;
}

/**
 *  Get event_handler.
 *
 *  @return The event_handler.
 */
std::string const& service::event_handler() const noexcept {
  return _event_handler;
}

/**
 *  Get event_handler_enabled.
 *
 *  @return The event_handler_enabled.
 */
bool service::event_handler_enabled() const noexcept {
  return _event_handler_enabled;
}

/**
 *  Get first_notification_delay.
 *
 *  @return The first_notification_delay.
 */
unsigned int service::first_notification_delay() const noexcept {
  return _first_notification_delay;
}

/**
 *  Get flap_detection_enabled.
 *
 *  @return The flap_detection_enabled.
 */
bool service::flap_detection_enabled() const noexcept {
  return _flap_detection_enabled;
}

/**
 *  Get flap_detection_options.
 *
 *  @return The flap_detection_options.
 */
unsigned short service::flap_detection_options() const noexcept {
  return _flap_detection_options;
}

/**
 *  Get freshness_threshold.
 *
 *  @return The freshness_threshold.
 */
unsigned int service::freshness_threshold() const noexcept {
  return _freshness_threshold;
}

/**
 *  Get high_flap_threshold.
 *
 *  @return The high_flap_threshold.
 */
unsigned int service::high_flap_threshold() const noexcept {
  return _high_flap_threshold;
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
const std::string& service::host_name() const {
  return _host_name;
}

/**
 *  Get host ID.
 *
 *  @return Service's host's ID.
 */
uint64_t service::host_id() const noexcept {
  return _host_id;
}

/**
 *  Get icon_image.
 *
 *  @return The icon_image.
 */
std::string const& service::icon_image() const noexcept {
  return _icon_image;
}

/**
 *  Get icon_image_alt.
 *
 *  @return The icon_image_alt.
 */
std::string const& service::icon_image_alt() const noexcept {
  return _icon_image_alt;
}

/**
 *  Get is_volatile.
 *
 *  @return The is_volatile.
 */
bool service::is_volatile() const noexcept {
  return _is_volatile;
}

/**
 *  Get low_flap_threshold.
 *
 *  @return The low_flap_threshold.
 */
unsigned int service::low_flap_threshold() const noexcept {
  return _low_flap_threshold;
}

/**
 *  Get max_check_attempts.
 *
 *  @return The max_check_attempts.
 */
unsigned int service::max_check_attempts() const noexcept {
  return _max_check_attempts;
}

/**
 *  Get notes.
 *
 *  @return The notes.
 */
std::string const& service::notes() const noexcept {
  return _notes;
}

/**
 *  Get notes_url.
 *
 *  @return The notes_url.
 */
std::string const& service::notes_url() const noexcept {
  return _notes_url;
}

/**
 *  Get notifications_enabled.
 *
 *  @return The notifications_enabled.
 */
bool service::notifications_enabled() const noexcept {
  return _notifications_enabled;
}

/**
 *  Set notification_interval.
 *
 *  @param[in] interval Notification interval.
 */
void service::notification_interval(unsigned int interval) noexcept {
  _notification_interval = interval;
}

/**
 *  Check if notification interval has been set.
 *
 *  @return True if notification interval was set in configuration.
 */
bool service::notification_interval_defined() const noexcept {
  return _notification_interval.is_set();
}

/**
 *  Get notification_interval.
 *
 *  @return The notification_interval.
 */
unsigned int service::notification_interval() const noexcept {
  return _notification_interval;
}

/**
 *  Get notification_options.
 *
 *  @return The notification_options.
 */
unsigned short service::notification_options() const noexcept {
  return _notification_options;
}

/**
 *  Set notification_period.
 *
 *  @param[in] period Period.
 */
void service::notification_period(std::string const& period) {
  _notification_period = period;
}

/**
 *  Get notification_period.
 *
 *  @return The notification_period.
 */
std::string const& service::notification_period() const noexcept {
  return _notification_period;
}

/**
 *  Check if notification period has been set.
 *
 *  @return True if notification period was set in configuration.
 */
bool service::notification_period_defined() const noexcept {
  return _notification_period.is_set();
}

/**
 *  Get obsess_over_service.
 *
 *  @return The obsess_over_service.
 */
bool service::obsess_over_service() const noexcept {
  return _obsess_over_service;
}

/**
 *  Get process_perf_data.
 *
 *  @return The process_perf_data.
 */
bool service::process_perf_data() const noexcept {
  return _process_perf_data;
}

/**
 *  Get retain_nonstatus_information.
 *
 *  @return The retain_nonstatus_information.
 */
bool service::retain_nonstatus_information() const noexcept {
  return _retain_nonstatus_information;
}

/**
 *  Get retain_status_information.
 *
 *  @return The retain_status_information.
 */
bool service::retain_status_information() const noexcept {
  return _retain_status_information;
}

/**
 *  Get retry_interval.
 *
 *  @return The retry_interval.
 */
unsigned int service::retry_interval() const noexcept {
  return _retry_interval;
}

/**
 *  Get recovery_notification_delay.
 *
 *  @return The recovery_notification_delay.
 */
unsigned int service::recovery_notification_delay() const noexcept {
  return _recovery_notification_delay;
}

/**
 *  Get service groups.
 *
 *  @return The service groups.
 */
set_string& service::servicegroups() noexcept {
  return *_servicegroups;
}

/**
 *  Get servicegroups.
 *
 *  @return The servicegroups.
 */
set_string const& service::servicegroups() const noexcept {
  return *_servicegroups;
}

/**
 *  Get service_description.
 *
 *  @return The service_description.
 */
std::string& service::service_description() noexcept {
  return _service_description;
}

/**
 *  Get service_description.
 *
 *  @return The service_description.
 */
std::string const& service::service_description() const noexcept {
  return _service_description;
}

/**
 *  Get the service id.
 *
 *  @return  The service id.
 */
uint64_t service::service_id() const noexcept {
  return _service_id;
}

/**
 *  Get stalking_options.
 *
 *  @return The stalking_options.
 */
unsigned short service::stalking_options() const noexcept {
  return _stalking_options;
}

/**
 *  Set service timezone.
 *
 *  @param[in] time_zone  New service timezone.
 */
void service::timezone(std::string const& time_zone) {
  _timezone = time_zone;
}

/**
 *  Get service timezone.
 *
 *  @return This service timezone.
 */
std::string const& service::timezone() const noexcept {
  return _timezone;
}

/**
 *  Check if service timezone has been defined.
 *
 *  @return True if service timezone is already defined.
 */
bool service::timezone_defined() const noexcept {
  return _timezone.is_set();
}

/**
 *  Get service tags.
 *
 *  @return This service tags.
 */
const std::set<std::pair<uint64_t, uint16_t>>& service::tags() const noexcept {
  return _tags;
}

/**
 *  Get acknowledgement timeout.
 *
 *  @return Acknowledgement timeout.
 */
int service::acknowledgement_timeout() const noexcept {
  return _acknowledgement_timeout;
}

/**
 *  Set the acknowledgement timeout.
 *
 *  @param[in] value  New acknowledgement timeout.
 *
 *  @return True on success, false otherwise.
 */
bool service::set_acknowledgement_timeout(int value) {
  bool value_positive(value >= 0);
  if (value_positive)
    _acknowledgement_timeout = value;
  return value_positive;
}

/**
 *  Set action_url value.
 *
 *  @param[in] value The new action_url value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_action_url(std::string const& value) {
  _action_url = value;
  return true;
}

/**
 *  Set check_command value.
 *
 *  @param[in] value The new check_command value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_check_command(std::string const& value) {
  if (!value.empty() && value[0] == '!') {
    _check_command_is_important = true;
    _check_command = value.substr(1);
  } else {
    _check_command_is_important = false;
    _check_command = value;
  }
  return true;
}

/**
 *  Set checks_active value.
 *
 *  @param[in] value The new checks_active value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_checks_active(bool value) {
  _checks_active = value;
  return true;
}

/**
 *  Set checks_passive value.
 *
 *  @param[in] value The new checks_passive value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_checks_passive(bool value) {
  _checks_passive = value;
  return true;
}

/**
 *  Set check_freshness value.
 *
 *  @param[in] value The new check_freshness value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_check_freshness(bool value) {
  _check_freshness = value;
  return true;
}

/**
 *  Set check_interval value.
 *
 *  @param[in] value The new check_interval value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_check_interval(unsigned int value) {
  _check_interval = value;
  return true;
}

/**
 *  Set check_period value.
 *
 *  @param[in] value The new check_period value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_check_period(std::string const& value) {
  _check_period = value;
  return true;
}

/**
 *  Set contactgroups value.
 *
 *  @param[in] value The new contactgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_contactgroups(std::string const& value) {
  _contactgroups = value;
  return true;
}

/**
 *  Set contacts value.
 *
 *  @param[in] value The new contacts value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_contacts(std::string const& value) {
  _contacts = value;
  return true;
}

/**
 *  Set display_name value.
 *
 *  @param[in] value The new display_name value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_display_name(std::string const& value) {
  _display_name = value;
  return true;
}

/**
 *  Set event_handler value.
 *
 *  @param[in] value The new event_handler value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_event_handler(std::string const& value) {
  _event_handler = value;
  return true;
}

/**
 *  Set event_handler_enabled value.
 *
 *  @param[in] value The new event_handler_enabled value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_event_handler_enabled(bool value) {
  _event_handler_enabled = value;
  return true;
}

/**
 *  Set first_notification_delay value.
 *
 *  @param[in] value The new first_notification_delay value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_first_notification_delay(unsigned int value) {
  _first_notification_delay = value;
  return true;
}

/**
 *  Set flap_detection_enabled value.
 *
 *  @param[in] value The new flap_detection_enabled value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_flap_detection_enabled(bool value) {
  _flap_detection_enabled = value;
  return true;
}

/**
 *  Set flap_detection_options value.
 *
 *  @param[in] value The new flap_detection_options value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_flap_detection_options(std::string const& value) {
  unsigned short options(none);
  auto values = absl::StrSplit(value, ',');
  for (auto& val : values) {
    auto v = absl::StripAsciiWhitespace(val);
    if (v == "o" || v == "ok")
      options |= ok;
    else if (v == "w" || v == "warning")
      options |= warning;
    else if (v == "u" || v == "unknown")
      options |= unknown;
    else if (v == "c" || v == "critical")
      options |= critical;
    else if (v == "n" || v == "none")
      options = none;
    else if (v == "a" || v == "all")
      options = ok | warning | unknown | critical;
    else
      return false;
  }
  _flap_detection_options = options;
  return true;
}

/**
 *  Set freshness_threshold value.
 *
 *  @param[in] value The new freshness_threshold value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_freshness_threshold(unsigned int value) {
  _freshness_threshold = value;
  return true;
}

/**
 *  Set high_flap_threshold value.
 *
 *  @param[in] value The new high_flap_threshold value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_high_flap_threshold(unsigned int value) {
  _high_flap_threshold = value;
  return true;
}

/**
 *  Set host_name value.
 *
 *  @param[in] value The new host_name value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_host_name(const std::string& value) {
  _host_name = value;
  return true;
}

/**
 *  Set icon_image value.
 *
 *  @param[in] value The new icon_image value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_icon_image(std::string const& value) {
  _icon_image = value;
  return true;
}

/**
 *  Set icon_image_alt value.
 *
 *  @param[in] value The new icon_image_alt value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_icon_image_alt(std::string const& value) {
  _icon_image_alt = value;
  return true;
}

/**
 *  Set is_volatile value.
 *
 *  @param[in] value The new is_volatile value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_is_volatile(bool value) {
  _is_volatile = value;
  return true;
}

/**
 *  Set low_flap_threshold value.
 *
 *  @param[in] value The new low_flap_threshold value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_low_flap_threshold(unsigned int value) {
  _low_flap_threshold = value;
  return true;
}

/**
 *  Set max_check_attempts value.
 *
 *  @param[in] value The new max_check_attempts value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_max_check_attempts(unsigned int value) {
  if (value <= 0)
    return false;
  _max_check_attempts = value;
  return true;
}

/**
 *  Set notes value.
 *
 *  @param[in] value The new notes value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_notes(std::string const& value) {
  _notes = value;
  return true;
}

/**
 *  Set notes_url value.
 *
 *  @param[in] value The new notes_url value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_notes_url(std::string const& value) {
  _notes_url = value;
  return true;
}

/**
 *  Set notifications_enabled value.
 *
 *  @param[in] value The new notifications_enabled value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_notifications_enabled(bool value) {
  _notifications_enabled = value;
  return true;
}

/**
 *  Set notification_options value.
 *
 *  @param[in] value The new notification_options value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_notification_options(std::string const& value) {
  unsigned short options(none);
  auto values = absl::StrSplit(value, ',');
  for (auto& val : values) {
    auto v = absl::StripAsciiWhitespace(val);
    if (v == "u" || v == "unknown")
      options |= unknown;
    else if (v == "w" || v == "warning")
      options |= warning;
    else if (v == "c" || v == "critical")
      options |= critical;
    else if (v == "r" || v == "recovery")
      options |= ok;
    else if (v == "f" || v == "flapping")
      options |= flapping;
    else if (v == "s" || v == "downtime")
      options |= downtime;
    else if (v == "n" || v == "none")
      options = none;
    else if (v == "a" || v == "all")
      options = unknown | warning | critical | ok | flapping | downtime;
    else
      return false;
  }
  _notification_options = options;
  return true;
}

/**
 *  Set notification_interval value.
 *
 *  @param[in] value The new notification_interval value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_notification_interval(unsigned int value) {
  _notification_interval = value;
  return true;
}

/**
 *  Set notification_period value.
 *
 *  @param[in] value The new notification_period value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_notification_period(std::string const& value) {
  _notification_period = value;
  return true;
}

/**
 *  Set obsess_over_service value.
 *
 *  @param[in] value The new obsess_over_service value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_obsess_over_service(bool value) {
  _obsess_over_service = value;
  return true;
}

/**
 *  Set process_perf_data value.
 *
 *  @param[in] value The new process_perf_data value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_process_perf_data(bool value) {
  _process_perf_data = value;
  return true;
}

/**
 *  Set retain_nonstatus_information value.
 *
 *  @param[in] value The new retain_nonstatus_information value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_retain_nonstatus_information(bool value) {
  _retain_nonstatus_information = value;
  return true;
}

/**
 *  Set retain_status_information value.
 *
 *  @param[in] value The new retain_status_information value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_retain_status_information(bool value) {
  _retain_status_information = value;
  return true;
}

/**
 *  Set retry_interval value.
 *
 *  @param[in] value The new retry_interval value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_retry_interval(unsigned int value) {
  if (!value)
    return false;
  _retry_interval = value;
  return true;
}

/**
 *  Set recovery_notification_delay value.
 *
 *  @param[in] value  The new recovery_notification_delay value.
 *
 *  @return  True on success, otherwhise false.
 */
bool service::_set_recovery_notification_delay(unsigned int value) {
  _recovery_notification_delay = value;
  return true;
}

/**
 *  Set servicegroups value.
 *
 *  @param[in] value The new servicegroups value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_servicegroups(std::string const& value) {
  _servicegroups = value;
  return true;
}

/**
 *  Set service_description value.
 *
 *  @param[in] value The new service_description value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_service_description(std::string const& value) {
  _service_description = value;
  return true;
}

/**
 *  Set service_id value.
 *
 *  @param[in] value The new service_id value.
 *
 *  @return True on success, otherwise false.
 */
bool service::set_service_id(uint64_t value) {
  _service_id = value;
  return true;
}

/**
 *  Set stalking_options value.
 *
 *  @param[in] value The new stalking_options value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_stalking_options(std::string const& value) {
  unsigned short options(none);
  auto values = absl::StrSplit(value, ',');
  for (auto& val : values) {
    auto v = absl::StripAsciiWhitespace(val);
    if (v == "o" || v == "ok")
      options |= ok;
    else if (v == "w" || v == "warning")
      options |= warning;
    else if (v == "u" || v == "unknown")
      options |= unknown;
    else if (v == "c" || v == "critical")
      options |= critical;
    else if (v == "n" || v == "none")
      options = none;
    else if (v == "a" || v == "all")
      options = ok | warning | unknown | critical;
    else
      return false;
  }
  _stalking_options = options;
  return true;
}

/**
 *  Set service timezone.
 *
 *  @param[in] value  The new timezone.
 *
 *  @return True.
 */
bool service::_set_timezone(std::string const& value) {
  _timezone = value;
  return true;
}

/**
 *  Set host tags.
 *
 *  @param[in] value  The new tags.
 *
 *  @return True.
 */
bool service::_set_category_tags(const std::string& value) {
  bool ret = true;
  std::list<std::string_view> tags{absl::StrSplit(value, ',')};
  for (std::set<std::pair<uint64_t, uint16_t>>::iterator it(_tags.begin()),
       end(_tags.end());
       it != end;) {
    if (it->second == broker::TagType::SERVICECATEGORY)
      it = _tags.erase(it);
    else
      ++it;
  }

  for (auto& tag : tags) {
    int64_t id;
    bool parse_ok;
    parse_ok = absl::SimpleAtoi(tag, &id);
    if (parse_ok) {
      _tags.emplace(id, broker::TagType::SERVICECATEGORY);
    } else {
      _logger->warn("Warning: service ({}, {}) error for parsing tag {}",
                    _host_id, _service_id, value);
      ret = false;
    }
  }
  return ret;
}

/**
 *  Set service tags.
 *
 *  @param[in] value  The new tags.
 *
 *  @return True.
 */
bool service::_set_group_tags(const std::string& value) {
  bool ret = true;
  std::list<std::string_view> tags{absl::StrSplit(value, ',')};
  for (std::set<std::pair<uint64_t, uint16_t>>::iterator it(_tags.begin()),
       end(_tags.end());
       it != end;) {
    if (it->second == broker::TagType::SERVICEGROUP)
      it = _tags.erase(it);
    else
      ++it;
  }

  for (auto& tag : tags) {
    int64_t id;
    bool parse_ok;
    parse_ok = absl::SimpleAtoi(tag, &id);
    if (parse_ok) {
      _tags.emplace(id, broker::TagType::SERVICEGROUP);
    } else {
      _logger->warn("Warning: service ({}, {}) error for parsing tag {}",
                    _host_id, _service_id, value);
      ret = false;
    }
  }
  return ret;
}

/**
 *  Set the host id.
 *
 * @param value The host id.
 */
void service::set_host_id(uint64_t value) {
  _host_id = value;
}

/**
 * @brief Set the severity_id (or 0 when there is no severity).
 *
 * @param severity_id The severity_id or 0.
 *
 * @return true on success.
 */
bool service::_set_severity_id(uint64_t severity_id) {
  _severity_id = severity_id;
  return true;
}

/**
 * @brief Accessor to the severity_id.
 *
 * @return the severity_id or 0 if none.
 */
uint64_t service::severity_id() const noexcept {
  return _severity_id;
}

/**
 * @brief Set the icon_id (or 0 when there is no icon).
 *
 * @param icon_id The icon_id or 0.
 *
 * @return true on success.
 */
bool service::_set_icon_id(uint64_t icon_id) {
  _icon_id = icon_id;
  return true;
}

/**
 * @brief Accessor to the icon_id.
 *
 * @return the icon_id or 0 if none.
 */
uint64_t service::icon_id() const noexcept {
  return _icon_id;
}
