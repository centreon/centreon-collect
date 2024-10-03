/**
 * Copyright 2011-2015,2017-2024 Centreon
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

#include "serviceescalation.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<serviceescalation, type, &serviceescalation::method>::generic

namespace com::centreon::engine::configuration {
size_t serviceescalation_key(const serviceescalation& se) {
  return absl::HashOf(*se.hosts().begin(), *se.service_description().begin(),
                      se.contactgroups(), se.escalation_options(),
                      se.escalation_period(), se.first_notification(),
                      se.last_notification(), se.notification_interval());
}
}  // namespace com::centreon::engine::configuration

std::unordered_map<std::string, serviceescalation::setter_func> const
    serviceescalation::_setters{
        {"host", SETTER(std::string const&, _set_hosts)},
        {"host_name", SETTER(std::string const&, _set_hosts)},
        {"description", SETTER(std::string const&, _set_service_description)},
        {"service_description",
         SETTER(std::string const&, _set_service_description)},
        {"servicegroup", SETTER(std::string const&, _set_servicegroups)},
        {"servicegroups", SETTER(std::string const&, _set_servicegroups)},
        {"servicegroup_name", SETTER(std::string const&, _set_servicegroups)},
        {"hostgroup", SETTER(std::string const&, _set_hostgroups)},
        {"hostgroups", SETTER(std::string const&, _set_hostgroups)},
        {"hostgroup_name", SETTER(std::string const&, _set_hostgroups)},
        {"contact_groups", SETTER(std::string const&, _set_contactgroups)},
        {"escalation_options",
         SETTER(std::string const&, _set_escalation_options)},
        {"escalation_period",
         SETTER(std::string const&, _set_escalation_period)},
        {"first_notification", SETTER(unsigned int, _set_first_notification)},
        {"last_notification", SETTER(unsigned int, _set_last_notification)},
        {"notification_interval",
         SETTER(unsigned int, _set_notification_interval)}};

// Default values.
static unsigned short const default_escalation_options(serviceescalation::none);
static unsigned int const default_first_notification(-2);
static unsigned int const default_last_notification(-2);
static unsigned int const default_notification_interval(0);

/**
 *  Default constructor.
 */
serviceescalation::serviceescalation()
    : object(object::serviceescalation),
      _escalation_options(default_escalation_options),
      _first_notification(default_first_notification),
      _last_notification(default_last_notification),
      _notification_interval(default_notification_interval) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The serviceescalation to copy.
 */
serviceescalation::serviceescalation(serviceescalation const& right)
    : object(right) {
  operator=(right);
}

/**
 *  Copy constructor.
 *
 *  @param[in] right The serviceescalation to copy.
 *
 *  @return This serviceescalation.
 */
serviceescalation& serviceescalation::operator=(
    serviceescalation const& right) {
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
    _servicegroups = right._servicegroups;
    _service_description = right._service_description;
  }
  return *this;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The serviceescalation to compare.
 *
 *  @return True if is the same serviceescalation, otherwise false.
 */
bool serviceescalation::operator==(serviceescalation const& right) const
    throw() {
  /* No comparison is made on the UUID because it is used between the
   * configuration object and the object. Since this object is randomly
   * constructor in almost all cases, we can have two equal escalations
   * with different uuid.*/
  if (!object::operator==(right)) {
    _logger->debug(
        "configuration::serviceescalation::equality => object don't match");
    return false;
  }
  if (_contactgroups != right._contactgroups) {
    _logger->debug(
        "configuration::serviceescalation::"
        "equality => contact groups don't match");
    return false;
  }
  if (_escalation_options != right._escalation_options) {
    _logger->debug(
        "configuration::serviceescalation::equality => escalation options "
        "don't match");
    return false;
  }
  if (_escalation_period != right._escalation_period) {
    _logger->debug(
        "configuration::serviceescalation::equality => escalation periods "
        "don't match");
    return false;
  }
  if (_first_notification != right._first_notification) {
    _logger->debug(
        "configuration::serviceescalation::equality => first notifications "
        "don't match");
    return false;
  }
  if (_hostgroups != right._hostgroups) {
    _logger->debug(
        "configuration::serviceescalation::"
        "equality => host groups don't match");
    return false;
  }
  if (_hosts != right._hosts) {
    _logger->debug(
        "configuration::serviceescalation::equality => hosts don't match");
    return false;
  }
  if (_last_notification != right._last_notification) {
    _logger->debug(
        "configuration::serviceescalation::equality => last notification "
        "don't match");
    return false;
  }
  if (_notification_interval != right._notification_interval) {
    _logger->debug(
        "configuration::serviceescalation::equality => notification "
        "interval don't match");
    return false;
  }
  if (_servicegroups != right._servicegroups) {
    _logger->debug(
        "configuration::serviceescalation::"
        "equality => service groups don't match");
    return false;
  }
  if (_service_description != right._service_description) {
    _logger->debug(
        "configuration::serviceescalation::equality => service descriptions "
        "don't match");
    return false;
  }
  _logger->debug("configuration::serviceescalation::equality => OK");
  return true;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The serviceescalation to compare.
 *
 *  @return True if is not the same serviceescalation, otherwise false.
 */
bool serviceescalation::operator!=(
    serviceescalation const& right) const noexcept {
  return !operator==(right);
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool serviceescalation::operator<(serviceescalation const& right) const {
  if (_hosts != right._hosts)
    return _hosts < right._hosts;
  else if (_hostgroups != right._hostgroups)
    return _hostgroups < right._hostgroups;
  else if (_service_description != right._service_description)
    return _service_description < right._service_description;
  else if (_servicegroups != right._servicegroups)
    return _servicegroups < right._servicegroups;
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
void serviceescalation::check_validity(error_cnt& err [[maybe_unused]]) const {
  if (_servicegroups->empty()) {
    if (_service_description->empty())
      throw msg_fmt(
          "Service escalation is not attached to any service or service group "
          "(properties 'service_description' and 'servicegroup_name', "
          "respectively)");
    else if (_hosts->empty() && _hostgroups->empty())
      throw msg_fmt(
          "Service escalation is not attached to any host or host group "
          "(properties 'host_name' or 'hostgroup_name', respectively)");
  }
}

/**
 *  Get the service escalation key.
 *
 *  @return This object.
 */
serviceescalation::key_type const& serviceescalation::key() const noexcept {
  return *this;
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void serviceescalation::merge(object const& obj) {
  if (obj.type() != _type)
    throw msg_fmt("Cannot merge service escalation with '{}'",
                  static_cast<uint32_t>(obj.type()));
  serviceescalation const& tmpl(static_cast<serviceescalation const&>(obj));

  MRG_INHERIT(_contactgroups);
  MRG_OPTION(_escalation_options);
  MRG_OPTION(_escalation_period);
  MRG_OPTION(_first_notification);
  MRG_INHERIT(_hostgroups);
  MRG_INHERIT(_hosts);
  MRG_OPTION(_last_notification);
  MRG_OPTION(_notification_interval);
  MRG_INHERIT(_servicegroups);
  MRG_INHERIT(_service_description);
}

/**
 *  Parse and set the serviceescalation property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool serviceescalation::parse(char const* key, char const* value) {
  std::unordered_map<std::string,
                     serviceescalation::setter_func>::const_iterator it{
      _setters.find(key)};
  if (it != _setters.end())
    return (it->second)(*this, value);
  return false;
}

/**
 *  Get contact groups.
 *
 *  @return The contact groups.
 */
set_string& serviceescalation::contactgroups() noexcept {
  return *_contactgroups;
}

/**
 *  Get contactgroups.
 *
 *  @return The contactgroups.
 */
set_string const& serviceescalation::contactgroups() const noexcept {
  return *_contactgroups;
}

/**
 *  Check if contact groups were defined.
 *
 *  @return True if contact groups were defined.
 */
bool serviceescalation::contactgroups_defined() const noexcept {
  return _contactgroups.is_set();
}

/**
 *  Set escalation options.
 *
 *  @param[in] options New escalation options.
 */
void serviceescalation::escalation_options(unsigned int options) noexcept {
  _escalation_options = options;
}

/**
 *  Get escalation_options.
 *
 *  @return The escalation_options.
 */
unsigned short serviceescalation::escalation_options() const noexcept {
  return _escalation_options;
}

/**
 *  Set the escalation period.
 *
 *  @param[in] period New escalation period.
 */
void serviceescalation::escalation_period(std::string const& period) {
  _escalation_period = period;
}

/**
 *  Get escalation_period.
 *
 *  @return The escalation_period.
 */
std::string const& serviceescalation::escalation_period() const noexcept {
  return _escalation_period;
}

/**
 *  Check if escalation period was defined.
 *
 *  @return True if the escalation period was defined.
 */
bool serviceescalation::escalation_period_defined() const noexcept {
  return _escalation_period.is_set();
}

/**
 *  Set the first notification number.
 *
 *  @param[in] n First notification number.
 */
void serviceescalation::first_notification(unsigned int n) noexcept {
  _first_notification = n;
}

/**
 *  Get first_notification.
 *
 *  @return The first_notification.
 */
unsigned int serviceescalation::first_notification() const noexcept {
  return _first_notification;
}

/**
 *  Get host groups.
 *
 *  @return Host groups.
 */
list_string& serviceescalation::hostgroups() noexcept {
  return *_hostgroups;
}

/**
 *  Get hostgroups.
 *
 *  @return The hostgroups.
 */
list_string const& serviceescalation::hostgroups() const noexcept {
  return *_hostgroups;
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
list_string& serviceescalation::hosts() noexcept {
  return *_hosts;
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
list_string const& serviceescalation::hosts() const noexcept {
  return *_hosts;
}

/**
 *  Set the last notification number.
 *
 *  @param[in] n Last notification number.
 */
void serviceescalation::last_notification(unsigned int n) noexcept {
  _last_notification = n;
}

/**
 *  Get last_notification.
 *
 *  @return The last_notification.
 */
unsigned int serviceescalation::last_notification() const noexcept {
  return _last_notification;
}

/**
 *  Set the notification interval.
 *
 *  @param[in] interval New notification interval.
 */
void serviceescalation::notification_interval(unsigned int interval) noexcept {
  _notification_interval = interval;
}

/**
 *  Get notification_interval.
 *
 *  @return The notification_interval.
 */
unsigned int serviceescalation::notification_interval() const noexcept {
  return _notification_interval;
}

/**
 *  Check if notification interval was set.
 *
 *  @return True if the notification interval was set.
 */
bool serviceescalation::notification_interval_defined() const noexcept {
  return _notification_interval.is_set();
}

/**
 *  Get service groups.
 *
 *  @return The service groups.
 */
list_string& serviceescalation::servicegroups() noexcept {
  return *_servicegroups;
}

/**
 *  Get servicegroups.
 *
 *  @return The servicegroups.
 */
list_string const& serviceescalation::servicegroups() const noexcept {
  return *_servicegroups;
}

/**
 *  Get service description.
 *
 *  @return Service description.
 */
list_string& serviceescalation::service_description() noexcept {
  return *_service_description;
}

/**
 *  Get service_description.
 *
 *  @return The service_description.
 */
list_string const& serviceescalation::service_description() const noexcept {
  return *_service_description;
}

/**
 *  Set contactgroups value.
 *
 *  @param[in] value The new contactgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool serviceescalation::_set_contactgroups(std::string const& value) {
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
bool serviceescalation::_set_escalation_options(std::string const& value) {
  unsigned short options(none);
  auto values = absl::StrSplit(value, ',');
  for (auto& val : values) {
    auto v = absl::StripAsciiWhitespace(val);
    if (v == "w" || v == "warning")
      options |= warning;
    else if (v == "u" || v == "unknown")
      options |= unknown;
    else if (v == "c" || v == "critical")
      options |= critical;
    else if (v == "r" || v == "recovery")
      options |= recovery;
    else if (v == "n" || v == "none")
      options = none;
    else if (v == "a" || v == "all")
      options = warning | unknown | critical | recovery;
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
bool serviceescalation::_set_escalation_period(std::string const& value) {
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
bool serviceescalation::_set_first_notification(unsigned int value) {
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
bool serviceescalation::_set_hostgroups(std::string const& value) {
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
bool serviceescalation::_set_hosts(std::string const& value) {
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
bool serviceescalation::_set_last_notification(unsigned int value) {
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
bool serviceescalation::_set_notification_interval(unsigned int value) {
  _notification_interval = value;
  return true;
}

/**
 *  Set servicegroups value.
 *
 *  @param[in] value The new servicegroups value.
 *
 *  @return True on success, otherwise false.
 */
bool serviceescalation::_set_servicegroups(std::string const& value) {
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
bool serviceescalation::_set_service_description(std::string const& value) {
  _service_description = value;
  return true;
}
