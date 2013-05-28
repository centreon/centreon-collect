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

#include "com/centreon/engine/configuration/host.hh"
#include "com/centreon/engine/error.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/misc/string.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

#define SETTER(type, method) \
  &configuration::object::setter< \
     configuration::host, \
     type, \
     &configuration::host::method>::generic

static struct {
  std::string const name;
  bool (*func)(configuration::host&, std::string const&);
} gl_setters[] = {
  { "host_name",                    SETTER(std::string const&, _set_host_name) },
  { "display_name",                 SETTER(std::string const&, _set_display_name) },
  { "alias",                        SETTER(std::string const&, _set_alias) },
  { "address",                      SETTER(std::string const&, _set_address) },
  { "parents",                      SETTER(std::string const&, _set_parents) },
  { "host_groups",                  SETTER(std::string const&, _set_hostgroups) },
  { "hostgroups",                   SETTER(std::string const&, _set_hostgroups) },
  { "contact_groups",               SETTER(std::string const&, _set_contactgroups) },
  { "contacts",                     SETTER(std::string const&, _set_contacts) },
  { "notification_period",          SETTER(std::string const&, _set_notification_period) },
  { "check_command",                SETTER(std::string const&, _set_check_command) },
  { "check_period",                 SETTER(std::string const&, _set_check_period) },
  { "event_handler",                SETTER(std::string const&, _set_event_handler) },
  { "failure_prediction_options",   SETTER(std::string const&, _set_failure_prediction_options) },
  { "notes",                        SETTER(std::string const&, _set_notes) },
  { "notes_url",                    SETTER(std::string const&, _set_notes_url) },
  { "action_url",                   SETTER(std::string const&, _set_action_url) },
  { "icon_image",                   SETTER(std::string const&, _set_icon_image) },
  { "icon_image_alt",               SETTER(std::string const&, _set_icon_image_alt) },
  { "vrml_image",                   SETTER(std::string const&, _set_vrml_image) },
  { "gd2_image",                    SETTER(std::string const&, _set_gd2_image) },
  { "statusmap_image",              SETTER(std::string const&, _set_statusmap_image) },
  { "initial_state",                SETTER(std::string const&, _set_initial_state) },
  { "check_interval",               SETTER(unsigned int, _set_check_interval) },
  { "normal_check_interval",        SETTER(unsigned int, _set_check_interval) },
  { "retry_interval",               SETTER(unsigned int, _set_retry_interval) },
  { "retry_check_interval",         SETTER(unsigned int, _set_retry_interval) },
  { "max_check_attempts",           SETTER(unsigned int, _set_max_check_attempts) },
  { "checks_enabled",               SETTER(bool, _set_checks_active) },
  { "active_checks_enabled",        SETTER(bool, _set_checks_active) },
  { "passive_checks_enabled",       SETTER(bool, _set_checks_passive) },
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
  { "2d_coords",                    SETTER(std::string const&, _set_2d_coords) },
  { "3d_coords",                    SETTER(std::string const&, _set_3d_coords) },
  { "obsess_over_host",             SETTER(bool, _set_obsess_over_host) },
  { "retain_status_information",    SETTER(bool, _set_retain_status_information) },
  { "retain_nonstatus_information", SETTER(bool, _set_retain_nonstatus_information) }
};

// XXX: check all default value from xodtemplate_inherit_object_properties !
// Default values.
static configuration::point_2d const default_2d_coords(-1, -1);
static configuration::point_3d const default_3d_coords(0.0, 0.0, 0.0);
static bool const                    default_checks_active(true);
static bool const                    default_checks_passive(true);
static bool const                    default_check_freshness(false);
static unsigned int const            default_check_interval(5);
static bool const                    default_event_handler_enabled(true);
static unsigned int const            default_first_notification_delay(0);
static bool const                    default_flap_detection_enabled(true);
static unsigned short const          default_flap_detection_options(
                                       configuration::host::up
                                       | configuration::host::down
                                       | configuration::host::unreachable);
static unsigned int const            default_freshness_threshold(0);
static unsigned int const            default_high_flap_threshold(0);
static unsigned short const          default_initial_state(HOST_UP);
static unsigned int const            default_low_flap_threshold(0);
static unsigned int const            default_max_check_attempts(0);
static bool const                    default_notifications_enabled(true);
static unsigned int const            default_notification_interval(30);
static unsigned short const          default_notification_options(configuration::host::none);
static bool const                    default_obsess_over_host(true);
static bool const                    default_process_perf_data(true);
static bool const                    default_retain_nonstatus_information(true);
static bool const                    default_retain_status_information(true);
static unsigned int const            default_retry_interval(1);
static unsigned short const          default_stalking_options(configuration::host::none);

/**
 *  Default constructor.
 */
configuration::host::host()
  : object(object::host, "host") {

}

/**
 *  Copy constructor.
 *
 *  @param[in] right The host to copy.
 */
configuration::host::host(host const& right)
  : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
configuration::host::~host() throw () {

}

/**
 *  Copy constructor.
 *
 *  @param[in] right The host to copy.
 *
 *  @return This host.
 */
configuration::host& configuration::host::operator=(host const& right) {
  if (this != &right) {
    object::operator=(right);
    _2d_coords = right._2d_coords;
    _3d_coords = right._3d_coords;
    _action_url = right._action_url;
    _address = right._address;
    _alias = right._alias;
    _checks_active = right._checks_active;
    _checks_passive = right._checks_passive;
    _check_command = right._check_command;
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
    _gd2_image = right._gd2_image;
    _high_flap_threshold = right._high_flap_threshold;
    _hostgroups = right._hostgroups;
    _host_name = right._host_name;
    _icon_image = right._icon_image;
    _icon_image_alt = right._icon_image_alt;
    _initial_state = right._initial_state;
    _low_flap_threshold = right._low_flap_threshold;
    _max_check_attempts = right._max_check_attempts;
    _notes = right._notes;
    _notes_url = right._notes_url;
    _notifications_enabled = right._notifications_enabled;
    _notification_interval = right._notification_interval;
    _notification_options = right._notification_options;
    _notification_period = right._notification_period;
    _obsess_over_host = right._obsess_over_host;
    _parents = right._parents;
    _process_perf_data = right._process_perf_data;
    _retain_nonstatus_information = right._retain_nonstatus_information;
    _retain_status_information = right._retain_status_information;
    _retry_interval = right._retry_interval;
    _stalking_options = right._stalking_options;
    _statusmap_image = right._statusmap_image;
    _vrml_image = right._vrml_image;
  }
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The host to compare.
 *
 *  @return True if is the same host, otherwise false.
 */
bool configuration::host::operator==(host const& right) const throw () {
  return (object::operator==(right)
          && _2d_coords == right._2d_coords
          && _3d_coords == right._3d_coords
          && _action_url == right._action_url
          && _address == right._address
          && _alias == right._alias
          && _checks_active == right._checks_active
          && _checks_passive == right._checks_passive
          && _check_command == right._check_command
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
          && _gd2_image == right._gd2_image
          && _high_flap_threshold == right._high_flap_threshold
          && _hostgroups == right._hostgroups
          && _host_name == right._host_name
          && _icon_image == right._icon_image
          && _icon_image_alt == right._icon_image_alt
          && _initial_state == right._initial_state
          && _low_flap_threshold == right._low_flap_threshold
          && _max_check_attempts == right._max_check_attempts
          && _notes == right._notes
          && _notes_url == right._notes_url
          && _notifications_enabled == right._notifications_enabled
          && _notification_interval == right._notification_interval
          && _notification_options == right._notification_options
          && _notification_period == right._notification_period
          && _obsess_over_host == right._obsess_over_host
          && _parents == right._parents
          && _process_perf_data == right._process_perf_data
          && _retain_nonstatus_information == right._retain_nonstatus_information
          && _retain_status_information == right._retain_status_information
          && _retry_interval == right._retry_interval
          && _stalking_options == right._stalking_options
          && _statusmap_image == right._statusmap_image
          && _vrml_image == right._vrml_image);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The host to compare.
 *
 *  @return True if is not the same host, otherwise false.
 */
bool configuration::host::operator!=(host const& right) const throw () {
  return (!operator==(right));
}

/**
 *  Get the unique object id.
 *
 *  @return The object id.
 */
std::size_t configuration::host::id() const throw () {
  return (_id);
}

/**
 *  Check if the object is valid.
 *
 *  @return True if is a valid object, otherwise false.
 */
bool configuration::host::is_valid() const throw () {
  return (!_host_name.empty()
          && !_address.empty());
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void configuration::host::merge(object const& obj) {
  if (obj.type() != _type)
    throw (engine_error() << "merge failed: invalid object type");
  host const& tmpl(static_cast<host const&>(obj));

  MRG_OPTION(_2d_coords);
  MRG_OPTION(_3d_coords);
  MRG_DEFAULT(_action_url);
  MRG_DEFAULT(_address);
  MRG_DEFAULT(_alias);
  MRG_OPTION(_checks_active);
  MRG_OPTION(_checks_passive);
  MRG_DEFAULT(_check_command);
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
  MRG_DEFAULT(_gd2_image);
  MRG_OPTION(_high_flap_threshold);
  MRG_INHERIT(_hostgroups);
  MRG_DEFAULT(_host_name);
  MRG_DEFAULT(_icon_image);
  MRG_DEFAULT(_icon_image_alt);
  MRG_OPTION(_initial_state);
  MRG_OPTION(_low_flap_threshold);
  MRG_OPTION(_max_check_attempts);
  MRG_DEFAULT(_notes);
  MRG_DEFAULT(_notes_url);
  MRG_OPTION(_notifications_enabled);
  MRG_OPTION(_notification_interval);
  MRG_OPTION(_notification_options);
  MRG_DEFAULT(_notification_period);
  MRG_OPTION(_obsess_over_host);
  MRG_INHERIT(_parents);
  MRG_OPTION(_process_perf_data);
  MRG_OPTION(_retain_nonstatus_information);
  MRG_OPTION(_retain_status_information);
  MRG_OPTION(_retry_interval);
  MRG_OPTION(_stalking_options);
  MRG_DEFAULT(_statusmap_image);
  MRG_DEFAULT(_vrml_image);
}

/**
 *  Parse and set the host property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::parse(
       std::string const& key,
       std::string const& value) {
  for (unsigned int i(0);
       i < sizeof(gl_setters) / sizeof(gl_setters[0]);
       ++i)
    if (gl_setters[i].name == key)
      return ((gl_setters[i].func)(*this, value));
  return (false);
}

/**
 *  Set 2d_coords value.
 *
 *  @param[in] value The new 2d_coords value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_2d_coords(std::string const& value) {
  std::list<std::string> coords;
  misc::split(value, coords, ',');
  if (coords.size() != 2)
    return (false);

  int x;
  if (!misc::to(misc::trim(coords.front()), x))
    return (false);
  coords.pop_front();

  int y;
  if (!misc::to(misc::trim(coords.front()), y))
    return (false);

  _2d_coords = point_2d(x, y);
  return (true);
}

/**
 *  Set 3d_coords value.
 *
 *  @param[in] value The new 3d_coords value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_3d_coords(std::string const& value) {
  std::list<std::string> coords;
  misc::split(value, coords, ',');
  if (coords.size() != 2)
    return (false);

  double x;
  if (!misc::to(misc::trim(coords.front()), x))
    return (false);
  coords.pop_front();

  double y;
  if (!misc::to(misc::trim(coords.front()), y))
    return (false);
  coords.pop_front();

  double z;
  if (!misc::to(misc::trim(coords.front()), z))
    return (false);

  _3d_coords = point_3d(x, y, z);
  return (true);
}

/**
 *  Set action_url value.
 *
 *  @param[in] value The new action_url value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_action_url(std::string const& value) {
  _action_url = value;
  return (true);
}

/**
 *  Set address value.
 *
 *  @param[in] value The new address value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_address(std::string const& value) {
  _address = value;
  return (true);
}

/**
 *  Set alias value.
 *
 *  @param[in] value The new alias value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_alias(std::string const& value) {
  _alias = value;
  return (true);
}

/**
 *  Set checks_active value.
 *
 *  @param[in] value The new checks_active value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_checks_active(bool value) {
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
bool configuration::host::_set_checks_passive(bool value) {
  _checks_passive = value;
  return (true);
}

/**
 *  Set check_command value.
 *
 *  @param[in] value The new check_command value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_check_command(std::string const& value) {
  _check_command = value;
  return (true);
}

/**
 *  Set check_freshness value.
 *
 *  @param[in] value The new check_freshness value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_check_freshness(bool value) {
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
bool configuration::host::_set_check_interval(unsigned int value) {
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
bool configuration::host::_set_check_period(std::string const& value) {
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
bool configuration::host::_set_contactgroups(std::string const& value) {
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
bool configuration::host::_set_contacts(std::string const& value) {
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
bool configuration::host::_set_display_name(std::string const& value) {
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
bool configuration::host::_set_event_handler(std::string const& value) {
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
bool configuration::host::_set_event_handler_enabled(bool value) {
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
bool configuration::host::_set_failure_prediction_enabled(bool value) {
  (void)value;
  logger(log_config_warning, basic)
    << "warning: host failure_prediction_enabled was ignored";
  return (true);
}

/**
 *  Set failure_prediction_options value.
 *
 *  @param[in] value The new failure_prediction_options value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_failure_prediction_options(
       std::string const& value) {
  (void)value;
  logger(log_config_warning, basic)
    << "warning: service failure_prediction_options was ignored";
  return (true);
}

/**
 *  Set first_notification_delay value.
 *
 *  @param[in] value The new first_notification_delay value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_first_notification_delay(
       unsigned int value) {
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
bool configuration::host::_set_flap_detection_enabled(bool value) {
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
bool configuration::host::_set_flap_detection_options(
       std::string const& value) {
  unsigned short options(none);
  std::list<std::string> values;
  misc::split(value, values, ',');
  for (std::list<std::string>::iterator
         it(values.begin()), end(values.end());
       it != end;
       ++it) {
    misc::trim(*it);
    if (*it == "o" || *it == "up")
      options |= up;
    else if (*it == "d" || *it == "down")
      options |= down;
    else if (*it == "u" || *it == "unreachable")
      options |= unreachable;
    else if (*it == "n" || *it == "none")
      options = none;
    else if (*it == "a" || *it == "all")
      options = up | down | unreachable;
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
bool configuration::host::_set_freshness_threshold(unsigned int value) {
  _freshness_threshold = value;
  return (true);
}

/**
 *  Set gd2_image value.
 *
 *  @param[in] value The new gd2_image value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_gd2_image(std::string const& value) {
  _gd2_image = value;
  return (true);
}

/**
 *  Set high_flap_threshold value.
 *
 *  @param[in] value The new high_flap_threshold value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_high_flap_threshold(unsigned int value) {
  _high_flap_threshold = value;
  return (true);
}

/**
 *  Set host_name value.
 *
 *  @param[in] value The new host_name value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_host_name(std::string const& value) {
  _host_name = value;
  _id = _hash(value);
  return (true);
}

/**
 *  Set hostgroups value.
 *
 *  @param[in] value The new hostgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_hostgroups(std::string const& value) {
  _hostgroups.set(value);
  return (true);
}

/**
 *  Set icon_image value.
 *
 *  @param[in] value The new icon_image value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_icon_image(std::string const& value) {
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
bool configuration::host::_set_icon_image_alt(
       std::string const& value) {
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
bool configuration::host::_set_initial_state(std::string const& value) {
  std::string data(value);
  misc::trim(data);
  if (data == "o" || data == "up")
    _initial_state = HOST_UP;
  else if (data == "d" || data == "down")
    _initial_state = HOST_DOWN;
  else if (data == "u" || data == "unreachable")
    _initial_state = HOST_UNREACHABLE;
  else
    return (false);
  return (true);
}

/**
 *  Set low_flap_threshold value.
 *
 *  @param[in] value The new low_flap_threshold value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_low_flap_threshold(unsigned int value) {
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
bool configuration::host::_set_max_check_attempts(unsigned int value) {
  if (!value)
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
bool configuration::host::_set_notes(std::string const& value) {
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
bool configuration::host::_set_notes_url(std::string const& value) {
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
bool configuration::host::_set_notifications_enabled(bool value) {
  _notifications_enabled = value;
  return (true);
}

/**
 *  Set notification_interval value.
 *
 *  @param[in] value The new notification_interval value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_notification_interval(
       unsigned int value) {
  _notification_interval = value;
  return (true);
}

/**
 *  Set notification_options value.
 *
 *  @param[in] value The new notification_options value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_notification_options(
       std::string const& value) {
  unsigned short options(none);
  std::list<std::string> values;
  misc::split(value, values, ',');
  for (std::list<std::string>::iterator
         it(values.begin()), end(values.end());
       it != end;
       ++it) {
    misc::trim(*it);
    if (*it == "d" || *it == "down")
      options |= down;
    else if (*it == "u" || *it == "unreachable")
      options |= unreachable;
    else if (*it == "r" || *it == "recovery")
      options |= recovery;
    else if (*it == "f" || *it == "flapping")
      options |= flapping;
    else if (*it == "s" || *it == "downtime")
      options |= downtime;
    else if (*it == "n" || *it == "none")
      options = none;
    else if (*it == "a" || *it == "all")
      options = down | unreachable | recovery | flapping | downtime;
    else
      return (false);
  }
  _notification_options = options;
  return (true);
}

/**
 *  Set notification_period value.
 *
 *  @param[in] value The new notification_period value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_notification_period(
       std::string const& value) {
  _notification_period = value;
  return (true);
}

/**
 *  Set obsess_over_host value.
 *
 *  @param[in] value The new obsess_over_host value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_obsess_over_host(bool value) {
  _obsess_over_host = value;
  return (true);
}

/**
 *  Set parents value.
 *
 *  @param[in] value The new parents value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_parents(std::string const& value) {
  _parents.set(value);
  return (true);
}

/**
 *  Set process_perf_data value.
 *
 *  @param[in] value The new process_perf_data value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_process_perf_data(bool value) {
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
bool configuration::host::_set_retain_nonstatus_information(
       bool value) {
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
bool configuration::host::_set_retain_status_information(bool value) {
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
bool configuration::host::_set_retry_interval(unsigned int value) {
  _retry_interval = value;
  return (true);
}

/**
 *  Set stalking_options value.
 *
 *  @param[in] value The new stalking_options value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_stalking_options(
       std::string const& value) {
  unsigned short options(none);
  std::list<std::string> values;
  misc::split(value, values, ',');
  for (std::list<std::string>::iterator
         it(values.begin()), end(values.end());
       it != end;
       ++it) {
    misc::trim(*it);
    if (*it == "o" || *it == "up")
      options |= up;
    else if (*it == "d" || *it == "down")
      options |= down;
    else if (*it == "u" || *it == "unreachable")
      options |= unreachable;
    else if (*it == "n" || *it == "none")
      options = none;
    else if (*it == "a" || *it == "all")
      options = up | down | unreachable;
    else
      return (false);
  }
  _stalking_options = options;
  return (true);
}

/**
 *  Set statusmap_image value.
 *
 *  @param[in] value The new statusmap_image value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_statusmap_image(
       std::string const& value) {
  _statusmap_image = value;
  return (true);
}

/**
 *  Set vrml_image value.
 *
 *  @param[in] value The new vrml_image value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::host::_set_vrml_image(std::string const& value) {
  _vrml_image = value;
  return (true);
}
