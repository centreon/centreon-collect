/*
** Copyright 2011-2013 Merethis
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

#include "com/centreon/engine/configuration/service.hh"
#include "com/centreon/engine/configuration/serviceextinfo.hh"
#include "com/centreon/engine/error.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/misc/string.hh"
#include "com/centreon/hash.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::logging;

#define SETTER(type, method) \
  &object::setter<service, type, &service::method>::generic

static struct {
  std::string const name;
  bool (*func)(service&, std::string const&);
} gl_setters[] = {
  { "host",                         SETTER(std::string const&, _set_hosts) },
  { "hosts",                        SETTER(std::string const&, _set_hosts) },
  { "host_name",                    SETTER(std::string const&, _set_hosts) },
  { "service_description",          SETTER(std::string const&, _set_service_description) },
  { "description",                  SETTER(std::string const&, _set_service_description) },
  { "display_name",                 SETTER(std::string const&, _set_display_name) },
  { "hostgroup",                    SETTER(std::string const&, _set_hostgroups) },
  { "hostgroups",                   SETTER(std::string const&, _set_hostgroups) },
  { "hostgroup_name",               SETTER(std::string const&, _set_hostgroups) },
  { "service_groups",               SETTER(std::string const&, _set_servicegroups) },
  { "servicegroups",                SETTER(std::string const&, _set_servicegroups) },
  { "check_command",                SETTER(std::string const&, _set_check_command) },
  { "check_period",                 SETTER(std::string const&, _set_check_period) },
  { "event_handler",                SETTER(std::string const&, _set_event_handler) },
  { "notification_period",          SETTER(std::string const&, _set_notification_period) },
  { "contact_groups",               SETTER(std::string const&, _set_contactgroups) },
  { "contacts",                     SETTER(std::string const&, _set_contacts) },
  { "failure_prediction_options",   SETTER(std::string const&, _set_failure_prediction_options) },
  { "notes",                        SETTER(std::string const&, _set_notes) },
  { "notes_url",                    SETTER(std::string const&, _set_notes_url) },
  { "action_url",                   SETTER(std::string const&, _set_action_url) },
  { "icon_image",                   SETTER(std::string const&, _set_icon_image) },
  { "icon_image_alt",               SETTER(std::string const&, _set_icon_image_alt) },
  { "initial_state",                SETTER(std::string const&, _set_initial_state) },
  { "max_check_attempts",           SETTER(unsigned int, _set_max_check_attempts) },
  { "check_interval",               SETTER(unsigned int, _set_check_interval) },
  { "normal_check_interval",        SETTER(unsigned int, _set_check_interval) },
  { "retry_interval",               SETTER(unsigned int, _set_retry_interval) },
  { "retry_check_interval",         SETTER(unsigned int, _set_retry_interval) },
  { "active_checks_enabled",        SETTER(bool, _set_checks_active) },
  { "passive_checks_enabled",       SETTER(bool, _set_checks_passive) },
  { "parallelize_check",            SETTER(bool, _set_parallelize_check) },
  { "is_volatile",                  SETTER(bool, _set_is_volatile) },
  { "obsess_over_service",          SETTER(bool, _set_obsess_over_service) },
  { "event_handler_enabled",        SETTER(bool, _set_event_handler_enabled) },
  { "check_freshness",              SETTER(bool, _set_check_freshness) },
  { "freshness_threshold",          SETTER(unsigned int, _set_freshness_threshold) },
  { "low_flap_threshold",           SETTER(unsigned int, _set_low_flap_threshold) },
  { "high_flap_threshold",          SETTER(unsigned int, _set_high_flap_threshold) },
  { "flap_detection_enabled",       SETTER(bool, _set_flap_detection_enabled) },
  { "flap_detection_options",       SETTER(std::string const&, _set_flap_detection_options) },
  { "notification_options",         SETTER(std::string const&, _set_notification_options) },
  { "notifications_enabled",        SETTER(bool, _set_notifications_enabled) },
  { "notification_interval",        SETTER(unsigned int, _set_notification_interval) },
  { "first_notification_delay",     SETTER(unsigned int, _set_first_notification_delay) },
  { "stalking_options",             SETTER(std::string const&, _set_stalking_options) },
  { "process_perf_data",            SETTER(bool, _set_process_perf_data) },
  { "failure_prediction_enabled",   SETTER(bool, _set_failure_prediction_enabled) },
  { "retain_status_information",    SETTER(bool, _set_retain_status_information) },
  { "retain_nonstatus_information", SETTER(bool, _set_retain_nonstatus_information) }
};

// Default values.
static bool const           default_checks_active(true);
static bool const           default_checks_passive(true);
static bool const           default_check_freshness(0);
static unsigned int const   default_check_interval(5);
static bool const           default_event_handler_enabled(true);
static unsigned int const   default_first_notification_delay(0);
static bool const           default_flap_detection_enabled(true);
static unsigned short const default_flap_detection_options(service::ok | service::warning | service::unknown | service::critical);
static unsigned int const   default_freshness_threshold(0);
static unsigned int const   default_high_flap_threshold(0);
static unsigned int const   default_initial_state(STATE_OK);
static bool const           default_is_volatile(false);
static unsigned int const   default_low_flap_threshold(0);
static unsigned int const   default_max_check_attempts(0);
static bool const           default_notifications_enabled(true);
static unsigned int const   default_notification_interval(30);
static unsigned short const default_notification_options(service::ok | service::warning | service::critical | service::unknown | service::flapping | service::downtime);
static bool const           default_obsess_over_service(true);
static bool const           default_process_perf_data(true);
static bool const           default_retain_nonstatus_information(true);
static bool const           default_retain_status_information(true);
static unsigned int const   default_retry_interval(1);
static unsigned short const default_stalking_options(service::none);

/**
 *  Default constructor.
 */
service::service()
  : object(object::service),
    _checks_active(default_checks_active),
    _checks_passive(default_checks_passive),
    _check_freshness(default_check_freshness),
    _check_interval(default_check_interval),
    _event_handler_enabled(default_event_handler_enabled),
    _first_notification_delay(default_first_notification_delay),
    _flap_detection_enabled(default_flap_detection_enabled),
    _flap_detection_options(default_flap_detection_options),
    _freshness_threshold(default_freshness_threshold),
    _high_flap_threshold(default_high_flap_threshold),
    _initial_state(default_initial_state),
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
    _stalking_options(default_stalking_options) {

}

/**
 *  Copy constructor.
 *
 *  @param[in] right The service to copy.
 */
service::service(service const& right)
  : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
service::~service() throw () {

}

/**
 *  Copy constructor.
 *
 *  @param[in] right The service to copy.
 *
 *  @return This service.
 */
service& service::operator=(service const& right) {
  if (this != &right) {
    object::operator=(right);
    _action_url = right._action_url;
    _checks_active = right._checks_active;
    _checks_passive = right._checks_passive;
    _check_command = right._check_command;
    _check_command_is_important = right._check_command_is_important;
    _check_freshness = right._check_freshness;
    _check_interval = right._check_interval;
    _check_period = right._check_period;
    _contactgroups = right._contactgroups;
    _contacts = right._contacts;
    _customvariables = right._customvariables;
    _display_name = right._display_name;
    _event_handler = right._event_handler;
    _event_handler_enabled = right._event_handler_enabled;
    _first_notification_delay = right._first_notification_delay;
    _flap_detection_enabled = right._flap_detection_enabled;
    _flap_detection_options = right._flap_detection_options;
    _freshness_threshold = right._freshness_threshold;
    _high_flap_threshold = right._high_flap_threshold;
    _hostgroups = right._hostgroups;
    _hosts = right._hosts;
    _icon_image = right._icon_image;
    _icon_image_alt = right._icon_image_alt;
    _initial_state = right._initial_state;
    _is_volatile = right._is_volatile;
    _low_flap_threshold = right._low_flap_threshold;
    _max_check_attempts = right._max_check_attempts;
    _notes = right._notes;
    _notes_url = right._notes_url;
    _notifications_enabled = right._notifications_enabled;
    _notification_interval = right._notification_interval;
    _notification_options = right._notification_options;
    _notification_period = right._notification_period;
    _obsess_over_service = right._obsess_over_service;
    _process_perf_data = right._process_perf_data;
    _retain_nonstatus_information = right._retain_nonstatus_information;
    _retain_status_information = right._retain_status_information;
    _retry_interval = right._retry_interval;
    _servicegroups = right._servicegroups;
    _service_description = right._service_description;
    _stalking_options = right._stalking_options;
  }
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The service to compare.
 *
 *  @return True if is the same service, otherwise false.
 */
bool service::operator==(service const& right) const throw () {
  return (object::operator==(right)
          && _action_url == right._action_url
          && _checks_active == right._checks_active
          && _checks_passive == right._checks_passive
          && _check_command == right._check_command
          && _check_command_is_important == right._check_command_is_important
          && _check_freshness == right._check_freshness
          && _check_interval == right._check_interval
          && _check_period == right._check_period
          && _contactgroups == right._contactgroups
          && _contacts == right._contacts
          && std::operator==(_customvariables, right._customvariables)
          && _display_name == right._display_name
          && _event_handler == right._event_handler
          && _event_handler_enabled == right._event_handler_enabled
          && _first_notification_delay == right._first_notification_delay
          && _flap_detection_enabled == right._flap_detection_enabled
          && _flap_detection_options == right._flap_detection_options
          && _freshness_threshold == right._freshness_threshold
          && _high_flap_threshold == right._high_flap_threshold
          && _hostgroups == right._hostgroups
          && _hosts == right._hosts
          && _icon_image == right._icon_image
          && _icon_image_alt == right._icon_image_alt
          && _initial_state == right._initial_state
          && _is_volatile == right._is_volatile
          && _low_flap_threshold == right._low_flap_threshold
          && _max_check_attempts == right._max_check_attempts
          && _notes == right._notes
          && _notes_url == right._notes_url
          && _notifications_enabled == right._notifications_enabled
          && _notification_interval == right._notification_interval
          && _notification_options == right._notification_options
          && _notification_period == right._notification_period
          && _obsess_over_service == right._obsess_over_service
          && _process_perf_data == right._process_perf_data
          && _retain_nonstatus_information == right._retain_nonstatus_information
          && _retain_status_information == right._retain_status_information
          && _retry_interval == right._retry_interval
          && _servicegroups == right._servicegroups
          && _service_description == right._service_description
          && _stalking_options == right._stalking_options);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The service to compare.
 *
 *  @return True if is not the same service, otherwise false.
 */
bool service::operator!=(service const& right) const throw () {
  return (!operator==(right));
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool service::operator<(service const& right) const throw () {
  if (_hosts != right._hosts)
    return (_hosts < right._hosts);
  else if (_service_description != right._service_description)
    return (_service_description < right._service_description);
  else if (_action_url != right._action_url)
    return (_action_url < right._action_url);
  else if (_checks_active != right._checks_active)
    return (_checks_active < right._checks_active);
  else if (_checks_passive != right._checks_passive)
    return (_checks_passive < right._checks_passive);
  else if (_check_command != right._check_command)
    return (_check_command < right._check_command);
  else if (_check_command_is_important
           != right._check_command_is_important)
    return (_check_command_is_important
            < right._check_command_is_important);
  else if (_check_freshness != right._check_freshness)
    return (_check_freshness < right._check_freshness);
  else if (_check_interval != right._check_interval)
    return (_check_interval < right._check_interval);
  else if (_check_period != right._check_period)
    return (_check_period < right._check_period);
  else if (_contactgroups != right._contactgroups)
    return (_contactgroups < right._contactgroups);
  else if (_contacts != right._contacts)
    return (_contacts < right._contacts);
  else if (_customvariables != right._customvariables)
    return (_customvariables < right._customvariables);
  else if (_display_name != right._display_name)
    return (_display_name < right._display_name);
  else if (_event_handler != right._event_handler)
    return (_event_handler < right._event_handler);
  else if (_event_handler_enabled != right._event_handler_enabled)
    return (_event_handler_enabled < right._event_handler_enabled);
  else if (_first_notification_delay
           != right._first_notification_delay)
    return (_first_notification_delay
            < right._first_notification_delay);
  else if (_flap_detection_enabled != right._flap_detection_enabled)
    return (_flap_detection_enabled < right._flap_detection_enabled);
  else if (_flap_detection_options != right._flap_detection_options)
    return (_flap_detection_options < right._flap_detection_options);
  else if (_freshness_threshold != right._freshness_threshold)
    return (_freshness_threshold < right._freshness_threshold);
  else if (_high_flap_threshold != right._high_flap_threshold)
    return (_high_flap_threshold < right._high_flap_threshold);
  else if (_hostgroups != right._hostgroups)
    return (_hostgroups < right._hostgroups);
  else if (_icon_image != right._icon_image)
    return (_icon_image < right._icon_image);
  else if (_icon_image_alt != right._icon_image_alt)
    return (_icon_image_alt < right._icon_image_alt);
  else if (_initial_state != right._initial_state)
    return (_initial_state < right._initial_state);
  else if (_is_volatile != right._is_volatile)
    return (_is_volatile < right._is_volatile);
  else if (_low_flap_threshold != right._low_flap_threshold)
    return (_low_flap_threshold < right._low_flap_threshold);
  else if (_max_check_attempts != right._max_check_attempts)
    return (_max_check_attempts < right._max_check_attempts);
  else if (_notes != right._notes)
    return (_notes < right._notes);
  else if (_notes_url != right._notes_url)
    return (_notes_url < right._notes_url);
  else if (_notifications_enabled != right._notifications_enabled)
    return (_notifications_enabled < right._notifications_enabled);
  else if (_notification_interval != right._notification_interval)
    return (_notification_interval < right._notification_interval);
  else if (_notification_options != right._notification_options)
    return (_notification_options < right._notification_options);
  else if (_notification_period != right._notification_period)
    return (_notification_period < right._notification_period);
  else if (_obsess_over_service != right._obsess_over_service)
    return (_obsess_over_service < right._obsess_over_service);
  else if (_process_perf_data != right._process_perf_data)
    return (_process_perf_data < right._process_perf_data);
  else if (_retain_nonstatus_information
           != right._retain_nonstatus_information)
    return (_retain_nonstatus_information
            < right._retain_nonstatus_information);
  else if (_retain_status_information
           != right._retain_status_information)
    return (_retain_status_information
            < right._retain_status_information);
  else if (_retry_interval != right._retry_interval)
    return (_retry_interval < right._retry_interval);
  else if (_servicegroups != right._servicegroups)
    return (_servicegroups < right._servicegroups);
  return (_stalking_options < right._stalking_options);
}

/**
 *  Get the unique object id.
 *
 *  @return The object id.
 */
std::size_t service::id() const throw () {
  if (!_id) {
    hash_combine(_id, _hosts.get().begin(), _hosts.get().end());
    hash_combine(_id, _service_description);
  }
  return (_id);
}

/**
 *  Check if the object is valid.
 *
 *  @return True if is a valid object, otherwise false.
 */
void service::check_validity() const {
  if (_service_description.empty())
    throw (engine_error() << "configuration: invalid service property "
           "service_description is missing");
  if (_hosts.empty() && _hostgroups.empty())
    throw (engine_error() << "configuration: invalid service property "
           "host or hostgroup is missing");
  if (_check_command.empty())
    throw (engine_error() << "configuration: invalid service property "
           "check_command is missing");
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void service::merge(configuration::serviceextinfo const& obj) {
  _action_url = obj.action_url();
  _icon_image = obj.icon_image();
  _icon_image_alt = obj.icon_image_alt();
  _notes = obj.notes();
  _notes_url = obj.notes_url();
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void service::merge(object const& obj) {
  if (obj.type() != _type)
    throw (engine_error() << "merge failed: invalid object type");
  service const& tmpl(static_cast<service const&>(obj));

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
  MRG_INHERIT(_hostgroups);
  MRG_INHERIT(_hosts);
  MRG_DEFAULT(_icon_image);
  MRG_DEFAULT(_icon_image_alt);
  MRG_OPTION(_initial_state);
  MRG_OPTION(_is_volatile);
  MRG_OPTION(_low_flap_threshold);
  MRG_OPTION(_max_check_attempts);
  MRG_DEFAULT(_notes);
  MRG_DEFAULT(_notes_url);
  MRG_OPTION(_notifications_enabled);
  MRG_OPTION(_notification_interval);
  MRG_OPTION(_notification_options);
  MRG_DEFAULT(_notification_period);
  MRG_OPTION(_obsess_over_service);
  MRG_OPTION(_process_perf_data);
  MRG_OPTION(_retain_nonstatus_information);
  MRG_OPTION(_retain_status_information);
  MRG_OPTION(_retry_interval);
  MRG_INHERIT(_servicegroups);
  MRG_DEFAULT(_service_description);
  MRG_OPTION(_stalking_options);
}

/**
 *  Parse and set the service property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool service::parse(
       std::string const& key,
       std::string const& value) {
  for (unsigned int i(0);
       i < sizeof(gl_setters) / sizeof(gl_setters[0]);
       ++i)
    if (gl_setters[i].name == key)
      return ((gl_setters[i].func)(*this, value));
  if (!key.empty() && key[0] == '_') {
    _customvariables[key.substr(1)] = value;
    return (true);
  }
  return (false);
}

/**
 *  Get action_url.
 *
 *  @return The action_url.
 */
std::string const& service::action_url() const throw () {
  return (_action_url);
}

/**
 *  Get checks_active.
 *
 *  @return The checks_active.
 */
bool service::checks_active() const throw () {
  return (_checks_active);
}

/**
 *  Get checks_passive.
 *
 *  @return The checks_passive.
 */
bool service::checks_passive() const throw () {
  return (_checks_passive);
}

/**
 *  Get check_command.
 *
 *  @return The check_command.
 */
std::string const& service::check_command() const throw () {
  return (_check_command);
}

/**
 *  Get check_command_is_important.
 *
 *  @return The check_command_is_important.
 */
bool service::check_command_is_important() const throw () {
  return (_check_command_is_important);
}

/**
 *  Get check_freshness.
 *
 *  @return The check_freshness.
 */
bool service::check_freshness() const throw () {
  return (_check_freshness);
}

/**
 *  Get check_interval.
 *
 *  @return The check_interval.
 */
unsigned int service::check_interval() const throw () {
  return (_check_interval);
}

/**
 *  Get check_period.
 *
 *  @return The check_period.
 */
std::string const& service::check_period() const throw () {
  return (_check_period);
}

/**
 *  Get contactgroups.
 *
 *  @return The contactgroups.
 */
list_string const& service::contactgroups() const throw () {
  return (_contactgroups.get());
}

/**
 *  Get contacts.
 *
 *  @return The contacts.
 */
list_string const& service::contacts() const throw () {
  return (_contacts.get());
}

/**
 *  Get customvariables.
 *
 *  @return The customvariables.
 */
properties const& service::customvariables() const throw () {
  return (_customvariables);
}

/**
 *  Get display_name.
 *
 *  @return The display_name.
 */
std::string const& service::display_name() const throw () {
  return (_display_name);
}

/**
 *  Get event_handler.
 *
 *  @return The event_handler.
 */
std::string const& service::event_handler() const throw () {
  return (_event_handler);
}

/**
 *  Get event_handler_enabled.
 *
 *  @return The event_handler_enabled.
 */
bool service::event_handler_enabled() const throw () {
  return (_event_handler_enabled);
}

/**
 *  Get first_notification_delay.
 *
 *  @return The first_notification_delay.
 */
unsigned int service::first_notification_delay() const throw () {
  return (_first_notification_delay);
}

/**
 *  Get flap_detection_enabled.
 *
 *  @return The flap_detection_enabled.
 */
bool service::flap_detection_enabled() const throw () {
  return (_flap_detection_enabled);
}

/**
 *  Get flap_detection_options.
 *
 *  @return The flap_detection_options.
 */
unsigned short service::flap_detection_options() const throw () {
  return (_flap_detection_options);
}

/**
 *  Get freshness_threshold.
 *
 *  @return The freshness_threshold.
 */
unsigned int service::freshness_threshold() const throw () {
  return (_freshness_threshold);
}

/**
 *  Get high_flap_threshold.
 *
 *  @return The high_flap_threshold.
 */
unsigned int service::high_flap_threshold() const throw () {
  return (_high_flap_threshold);
}

/**
 *  Get hostgroups.
 *
 *  @return The hostgroups.
 */
list_string& service::hostgroups() throw () {
  return (_hostgroups.get());
}

/**
 *  Get hostgroups.
 *
 *  @return The hostgroups.
 */
list_string const& service::hostgroups() const throw () {
  return (_hostgroups.get());
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
list_string& service::hosts() throw () {
  return (_hosts.get());
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
list_string const& service::hosts() const throw () {
  return (_hosts.get());
}

/**
 *  Get icon_image.
 *
 *  @return The icon_image.
 */
std::string const& service::icon_image() const throw () {
  return (_icon_image);
}

/**
 *  Get icon_image_alt.
 *
 *  @return The icon_image_alt.
 */
std::string const& service::icon_image_alt() const throw () {
  return (_icon_image_alt);
}

/**
 *  Get initial_state.
 *
 *  @return The initial_state.
 */
unsigned int service::initial_state() const throw () {
  return (_initial_state);
}

/**
 *  Get is_volatile.
 *
 *  @return The is_volatile.
 */
bool service::is_volatile() const throw () {
  return (_is_volatile);
}

/**
 *  Get low_flap_threshold.
 *
 *  @return The low_flap_threshold.
 */
unsigned int service::low_flap_threshold() const throw () {
  return (_low_flap_threshold);
}

/**
 *  Get max_check_attempts.
 *
 *  @return The max_check_attempts.
 */
unsigned int service::max_check_attempts() const throw () {
  return (_max_check_attempts);
}

/**
 *  Get notes.
 *
 *  @return The notes.
 */
std::string const& service::notes() const throw () {
  return (_notes);
}

/**
 *  Get notes_url.
 *
 *  @return The notes_url.
 */
std::string const& service::notes_url() const throw () {
  return (_notes_url);
}

/**
 *  Get notifications_enabled.
 *
 *  @return The notifications_enabled.
 */
bool service::notifications_enabled() const throw () {
  return (_notifications_enabled);
}

/**
 *  Get notification_interval.
 *
 *  @return The notification_interval.
 */
unsigned int service::notification_interval() const throw () {
  return (_notification_interval);
}

/**
 *  Get notification_options.
 *
 *  @return The notification_options.
 */
unsigned short service::notification_options() const throw () {
  return (_notification_options);
}

/**
 *  Get notification_period.
 *
 *  @return The notification_period.
 */
std::string const& service::notification_period() const throw () {
  return (_notification_period);
}

/**
 *  Get obsess_over_service.
 *
 *  @return The obsess_over_service.
 */
bool service::obsess_over_service() const throw () {
  return (_obsess_over_service);
}

/**
 *  Get process_perf_data.
 *
 *  @return The process_perf_data.
 */
bool service::process_perf_data() const throw () {
  return (_process_perf_data);
}

/**
 *  Get retain_nonstatus_information.
 *
 *  @return The retain_nonstatus_information.
 */
bool service::retain_nonstatus_information() const throw () {
  return (_retain_nonstatus_information);
}

/**
 *  Get retain_status_information.
 *
 *  @return The retain_status_information.
 */
bool service::retain_status_information() const throw () {
  return (_retain_status_information);
}

/**
 *  Get retry_interval.
 *
 *  @return The retry_interval.
 */
unsigned int service::retry_interval() const throw () {
  return (_retry_interval);
}

/**
 *  Get servicegroups.
 *
 *  @return The servicegroups.
 */
list_string const& service::servicegroups() const throw () {
  return (_servicegroups.get());
}

/**
 *  Get service_description.
 *
 *  @return The service_description.
 */
std::string const& service::service_description() const throw () {
  return (_service_description);
}

/**
 *  Get stalking_options.
 *
 *  @return The stalking_options.
 */
unsigned short service::stalking_options() const throw () {
  return (_stalking_options);
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
  return (true);
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
  }
  else
    _check_command = value;
  return (true);
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
  return (true);
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
  return (true);
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
  return (true);
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
  return (true);
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
  return (true);
}

/**
 *  Set contactgroups value.
 *
 *  @param[in] value The new contactgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_contactgroups(std::string const& value) {
  _contactgroups.set(value);
  return (true);
}

/**
 *  Set contacts value.
 *
 *  @param[in] value The new contacts value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_contacts(std::string const& value) {
  _contacts.set(value);
  return (true);
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
  return (true);
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
  return (true);
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
  return (true);
}

/**
 *  Set failure_prediction_enabled value.
 *
 *  @param[in] value The new failure_prediction_enabled value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_failure_prediction_enabled(bool value) {
  (void)value;
  logger(log_config_warning, basic)
    << "Warning: service failure_prediction_enabled was ignored";
  return (true);
}

/**
 *  Set failure_prediction_options value.
 *
 *  @param[in] value The new failure_prediction_options value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_failure_prediction_options(std::string const& value) {
  (void)value;
  logger(log_config_warning, basic)
    << "Warning: service failure_prediction_options was ignored";
  return (true);
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
  return (true);
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
  return (true);
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
  std::list<std::string> values;
  misc::split(value, values, ',');
  for (std::list<std::string>::iterator
         it(values.begin()), end(values.end());
       it != end;
       ++it) {
    misc::trim(*it);
    if (*it == "o" || *it == "ok")
      options |= ok;
    else if (*it == "w" || *it == "warning")
      options |= warning;
    else if (*it == "u" || *it == "unknown")
      options |= unknown;
    else if (*it == "c" || *it == "critical")
      options |= critical;
    else if (*it == "n" || *it == "none")
      options = none;
    else if (*it == "a" || *it == "all")
      options = ok | warning | unknown | critical;
    else
      return (false);
  }
  _flap_detection_options = options;
  return (true);
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
  return (true);
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
  return (true);
}

/**
 *  Set hostgroups value.
 *
 *  @param[in] value The new hostgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_hostgroups(std::string const& value) {
  _hostgroups.set(value);
  return (true);
}

/**
 *  Set hosts value.
 *
 *  @param[in] value The new hosts value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_hosts(std::string const& value) {
  _hosts.set(value);
  _id = 0;
  return (true);
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
  return (true);
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
  return (true);
}

/**
 *  Set initial_state value.
 *
 *  @param[in] value The new initial_state value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_initial_state(std::string const& value) {
  std::string data(value);
  misc::trim(data);
  if (data == "o" || data == "ok")
    _initial_state = STATE_OK;
  else if (data == "w" || data == "warning")
    _initial_state = STATE_WARNING;
  else if (data == "u" || data == "unknown")
    _initial_state = STATE_UNKNOWN;
  else if (data == "c" || data == "critical")
    _initial_state = STATE_CRITICAL;
  else
    return (false);
  return (true);
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
  return (true);
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
  return (true);
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
    return (false);
  _max_check_attempts = value;
  return (true);
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
  return (true);
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
  return (true);
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
  return (true);
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
  std::list<std::string> values;
  misc::split(value, values, ',');
  for (std::list<std::string>::iterator
         it(values.begin()), end(values.end());
       it != end;
       ++it) {
    misc::trim(*it);
    if (*it == "u" || *it == "unknown")
      options |= unknown;
    else if (*it == "w" || *it == "warning")
      options |= warning;
    else if (*it == "c" || *it == "critical")
      options |= critical;
    else if (*it == "r" || *it == "recovery")
      options |= ok;
    else if (*it == "f" || *it == "flapping")
      options |= flapping;
    else if (*it == "s" || *it == "downtime")
      options |= downtime;
    else if (*it == "n" || *it == "none")
      options = none;
    else if (*it == "a" || *it == "all")
      options = unknown | warning | critical | ok | flapping | downtime;
    else
      return (false);
  }
  _notification_options = options;
  return (true);
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
  return (true);
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
  return (true);
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
  return (true);
}

/**
 *  Set parallelize_check value.
 *
 *  @param[in] value The new parallelize_check value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_parallelize_check(bool value) {
  (void)value;
  logger(log_config_warning, basic)
    << "Warning: service parallelize_check was ignored";
  return (true);
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
  return (true);
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
  return (true);
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
  return (true);
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
    return (false);
  _retry_interval = value;
  return (true);
}

/**
 *  Set servicegroups value.
 *
 *  @param[in] value The new servicegroups value.
 *
 *  @return True on success, otherwise false.
 */
bool service::_set_servicegroups(std::string const& value) {
  _servicegroups.set(value);
  return (true);
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
  _id = 0;
  return (true);
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
  std::list<std::string> values;
  misc::split(value, values, ',');
  for (std::list<std::string>::iterator
         it(values.begin()), end(values.end());
       it != end;
       ++it) {
    misc::trim(*it);
    if (*it == "o" || *it == "ok")
      options |= ok;
    else if (*it == "w" || *it == "warning")
      options |= warning;
    else if (*it == "u" || *it == "unknown")
      options |= unknown;
    else if (*it == "c" || *it == "critical")
      options |= critical;
    else if (*it == "n" || *it == "none")
      options = none;
    else if (*it == "a" || *it == "all")
      options = ok | warning | unknown | critical;
    else
      return (false);
  }
  _stalking_options = options;
  return (true);
}
