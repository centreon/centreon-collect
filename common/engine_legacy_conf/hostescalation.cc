/**
 * Copyright 2011-2013,2015 Merethis
 * Copyright 2017-2024 Centreon
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
#include "hostescalation.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<hostescalation, type, &hostescalation::method>::generic

namespace com::centreon::engine::configuration {

size_t hostescalation_key(const hostescalation& he) {
  return absl::HashOf(*he.hosts().begin(), he.contactgroups(),
                      he.escalation_options(), he.escalation_period(),
                      he.first_notification(), he.last_notification(),
                      he.notification_interval());
}

}  // namespace com::centreon::engine::configuration

std::unordered_map<std::string,
                   hostescalation::setter_func> const hostescalation::_setters{
    {"hostgroup", SETTER(std::string const&, _set_hostgroups)},
    {"hostgroups", SETTER(std::string const&, _set_hostgroups)},
    {"hostgroup_name", SETTER(std::string const&, _set_hostgroups)},
    {"host", SETTER(std::string const&, _set_hosts)},
    {"host_name", SETTER(std::string const&, _set_hosts)},
    {"contact_groups", SETTER(std::string const&, _set_contactgroups)},
    {"escalation_options", SETTER(std::string const&, _set_escalation_options)},
    {"escalation_period", SETTER(std::string const&, _set_escalation_period)},
    {"first_notification", SETTER(uint32_t, _set_first_notification)},
    {"last_notification", SETTER(uint32_t, _set_last_notification)},
    {"notification_interval", SETTER(uint32_t, _set_notification_interval)}};

// Default values.
static unsigned short const default_escalation_options(hostescalation::none);
static unsigned int const default_first_notification(-2);
static unsigned int const default_last_notification(-2);
static unsigned int const default_notification_interval(0);

/**
 *  Default constructor.
 */
hostescalation::hostescalation()
    : object(object::hostescalation),
      _escalation_options(default_escalation_options),
      _first_notification(default_first_notification),
      _last_notification(default_last_notification),
      _notification_interval(default_notification_interval) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The hostescalation to copy.
 */
hostescalation::hostescalation(hostescalation const& right) : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
hostescalation::~hostescalation() throw() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The hostescalation to copy.
 *
 *  @return This hostescalation.
 */
hostescalation& hostescalation::operator=(hostescalation const& right) {
  if (this != &right) {
    object::operator=(right);
    _contactgroups = right._contactgroups;
    _escalation_options = right._escalation_options;
    _escalation_period = right._escalation_period;
    _first_notification = right._first_notification;
    _hostgroups = right._hostgroups;
    _hosts = right._hosts;
    _last_notification = right._last_notification;
    _notification_interval = right._notification_interval;
  }
  return *this;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The hostescalation to compare.
 *
 *  @return True if is the same hostescalation, otherwise false.
 */
bool hostescalation::operator==(hostescalation const& right) const throw() {
  return (object::operator==(right) && _contactgroups == right._contactgroups &&
          _escalation_options == right._escalation_options &&
          _escalation_period == right._escalation_period &&
          _first_notification == right._first_notification &&
          _hostgroups == right._hostgroups && _hosts == right._hosts &&
          _last_notification == right._last_notification &&
          _notification_interval == right._notification_interval);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The hostescalation to compare.
 *
 *  @return True if is not the same hostescalation, otherwise false.
 */
bool hostescalation::operator!=(hostescalation const& right) const throw() {
  return !operator==(right);
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool hostescalation::operator<(hostescalation const& right) const {
  if (_hosts != right._hosts)
    return _hosts < right._hosts;
  else if (_hostgroups != right._hostgroups)
    return _hostgroups < right._hostgroups;
  else if (_contactgroups != right._contactgroups)
    return _contactgroups < right._contactgroups;
  else if (_escalation_options != right._escalation_options)
    return _escalation_options < right._escalation_options;
  else if (_escalation_period != right._escalation_period)
    return _escalation_period < right._escalation_period;
  else if (_first_notification != right._first_notification)
    return _first_notification < right._first_notification;
  else if (_last_notification != right._last_notification)
    return _last_notification < right._last_notification;
  return _notification_interval < right._notification_interval;
}

/**
 *  @brief Check if the object is valid.
 *
 *  If the object is not valid, an exception is thrown.
 */
void hostescalation::check_validity(error_cnt& err [[maybe_unused]]) const {
  if (_hosts->empty() && _hostgroups->empty())
    throw msg_fmt(
        "Host escalation is not attached to any host or host group (properties "
        "'host_name' or 'hostgroup_name', respectively)");
}

/**
 *  Get host escalation key.
 *
 *  @return This object.
 */
hostescalation::key_type const& hostescalation::key() const throw() {
  return *this;
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void hostescalation::merge(object const& obj) {
  if (obj.type() != _type)
    throw msg_fmt("Cannot merge host escalation with '{}'",
                  static_cast<uint32_t>(obj.type()));
  hostescalation const& tmpl(static_cast<hostescalation const&>(obj));

  MRG_INHERIT(_contactgroups);
  MRG_OPTION(_escalation_options);
  MRG_OPTION(_escalation_period);
  MRG_OPTION(_first_notification);
  MRG_INHERIT(_hostgroups);
  MRG_INHERIT(_hosts);
  MRG_OPTION(_last_notification);
  MRG_OPTION(_notification_interval);
}

/**
 *  Parse and set the hostescalation property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::parse(char const* key, char const* value) {
  std::unordered_map<std::string, hostescalation::setter_func>::const_iterator
      it{_setters.find(key)};
  if (it != _setters.end())
    return (it->second)(*this, value);
  return false;
}

/**
 *  Get contact groups.
 *
 *  @return The contact groups.
 */
set_string& hostescalation::contactgroups() throw() {
  return *_contactgroups;
}

/**
 *  Get contactgroups.
 *
 *  @return The contactgroups.
 */
set_string const& hostescalation::contactgroups() const throw() {
  return *_contactgroups;
}

/**
 *  Check if contact groups were defined.
 *
 *  @return True if contact groups were defined.
 */
bool hostescalation::contactgroups_defined() const throw() {
  return _contactgroups.is_set();
}

/**
 *  Set escalation options.
 *
 *  @param[in] options New escalation options.
 */
void hostescalation::escalation_options(unsigned short options) throw() {
  _escalation_options = options;
}

/**
 *  Get escalation_options.
 *
 *  @return The escalation_options.
 */
unsigned short hostescalation::escalation_options() const throw() {
  return _escalation_options;
}

/**
 *  Set the escalation period.
 *
 *  @param[in] period New escalation period.
 */
void hostescalation::escalation_period(std::string const& period) {
  _escalation_period = period;
}

/**
 *  Get escalation_period.
 *
 *  @return The escalation_period.
 */
std::string const& hostescalation::escalation_period() const throw() {
  return _escalation_period;
}

/**
 *  Check if escalation period was defined.
 *
 *  @return True if escalation period was defined.
 */
bool hostescalation::escalation_period_defined() const throw() {
  return _escalation_period.is_set();
}

/**
 *  Set the first notification.
 *
 *  @param[in] n New first notification number.
 */
void hostescalation::first_notification(unsigned int n) throw() {
  _first_notification = n;
}

/**
 *  Get first_notification.
 *
 *  @return The first_notification.
 */
unsigned int hostescalation::first_notification() const throw() {
  return _first_notification;
}

/**
 *  Get host groups.
 *
 *  @return The host groups.
 */
set_string& hostescalation::hostgroups() throw() {
  return *_hostgroups;
}

/**
 *  Get hostgroups.
 *
 *  @return The hostgroups.
 */
set_string const& hostescalation::hostgroups() const throw() {
  return *_hostgroups;
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
set_string& hostescalation::hosts() throw() {
  return *_hosts;
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
set_string const& hostescalation::hosts() const throw() {
  return *_hosts;
}

/**
 *  Set the last notification.
 *
 *  @param[in] n New last notification number.
 */
void hostescalation::last_notification(unsigned int n) throw() {
  _last_notification = n;
}

/**
 *  Get last_notification.
 *
 *  @return The last_notification.
 */
unsigned int hostescalation::last_notification() const throw() {
  return _last_notification;
}

/**
 *  Set the notification interval.
 *
 *  @param[in] interval New notification interval.
 */
void hostescalation::notification_interval(unsigned int interval) {
  _notification_interval = interval;
}

/**
 *  Get notification_interval.
 *
 *  @return The notification_interval.
 */
unsigned int hostescalation::notification_interval() const throw() {
  return _notification_interval;
}

/**
 *  Check if the notification interval was defined.
 *
 *  @return True if the notification interval was defined.
 */
bool hostescalation::notification_interval_defined() const throw() {
  return _notification_interval.is_set();
}

/**
 *  Set contactgroups value.
 *
 *  @param[in] value The new contactgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::_set_contactgroups(std::string const& value) {
  _contactgroups = value;
  return true;
}

/**
 *  Set escalation_options value.
 *
 *  @param[in] value The new escalation_options value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::_set_escalation_options(std::string const& value) {
  unsigned short options(none);
  auto values = absl::StrSplit(value, ',');
  for (auto& val : values) {
    auto v = absl::StripAsciiWhitespace(val);
    if (v == "d" || v == "down")
      options |= down;
    else if (v == "u" || v == "unreachable")
      options |= unreachable;
    else if (v == "r" || v == "recovery")
      options |= recovery;
    else if (v == "n" || v == "none")
      options = none;
    else if (v == "a" || v == "all")
      options = down | unreachable | recovery;
    else
      return false;
  }
  _escalation_options = options;
  return true;
}

/**
 *  Set escalation_period value.
 *
 *  @param[in] value The new escalation_period value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::_set_escalation_period(std::string const& value) {
  _escalation_period = value;
  return true;
}

/**
 *  Set first_notification value.
 *
 *  @param[in] value The new first_notification value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::_set_first_notification(unsigned int value) {
  _first_notification = value;
  return true;
}

/**
 *  Set hostgroups value.
 *
 *  @param[in] value The new hostgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::_set_hostgroups(std::string const& value) {
  _hostgroups = value;
  return true;
}

/**
 *  Set hosts value.
 *
 *  @param[in] value The new hosts value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::_set_hosts(std::string const& value) {
  _hosts = value;
  return true;
}

/**
 *  Set last_notification value.
 *
 *  @param[in] value The new last_notification value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::_set_last_notification(unsigned int value) {
  _last_notification = value;
  return true;
}

/**
 *  Set notification_interval value.
 *
 *  @param[in] value The new notification_interval value.
 *
 *  @return True on success, otherwise false.
 */
bool hostescalation::_set_notification_interval(unsigned int value) {
  _notification_interval = value;
  return true;
}
