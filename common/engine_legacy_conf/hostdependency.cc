/**
 * Copyright 2011-2013,2015 Merethis
 * Copyright 2017 - 2024 Centreon (https://www.centreon.com/)
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

#include "com/centreon/exceptions/msg_fmt.hh"
#include "state.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<hostdependency, type, &hostdependency::method>::generic

namespace com::centreon::engine::configuration {
size_t hostdependency_key(const hostdependency& hd) {
  assert(hd.hosts().size() == 1 && hd.hostgroups().empty() &&
         hd.dependent_hosts().size() == 1 && hd.dependent_hostgroups().empty());
  return absl::HashOf(hd.dependency_period(), hd.dependency_type(),
                      *hd.dependent_hosts().begin(), *hd.hosts().begin(),
                      hd.inherits_parent(), hd.notification_failure_options());
}
}  // namespace com::centreon::engine::configuration

std::unordered_map<std::string,
                   hostdependency::setter_func> const hostdependency::_setters{
    {"hostgroup", SETTER(std::string const&, _set_hostgroups)},
    {"hostgroups", SETTER(std::string const&, _set_hostgroups)},
    {"hostgroup_name", SETTER(std::string const&, _set_hostgroups)},
    {"host", SETTER(std::string const&, _set_hosts)},
    {"host_name", SETTER(std::string const&, _set_hosts)},
    {"master_host", SETTER(std::string const&, _set_hosts)},
    {"master_host_name", SETTER(std::string const&, _set_hosts)},
    {"dependent_hostgroup",
     SETTER(std::string const&, _set_dependent_hostgroups)},
    {"dependent_hostgroups",
     SETTER(std::string const&, _set_dependent_hostgroups)},
    {"dependent_hostgroup_name",
     SETTER(std::string const&, _set_dependent_hostgroups)},
    {"dependent_host", SETTER(std::string const&, _set_dependent_hosts)},
    {"dependent_host_name", SETTER(std::string const&, _set_dependent_hosts)},
    {"dependency_period", SETTER(std::string const&, _set_dependency_period)},
    {"inherits_parent", SETTER(bool, _set_inherits_parent)},
    {"notification_failure_options",
     SETTER(std::string const&, _set_notification_failure_options)},
    {"notification_failure_criteria",
     SETTER(std::string const&, _set_notification_failure_options)},
    {"execution_failure_options",
     SETTER(std::string const&, _set_execution_failure_options)},
    {"execution_failure_criteria",
     SETTER(std::string const&, _set_execution_failure_options)}};

// Default values.
static unsigned short const default_execution_failure_options(
    hostdependency::none);
static bool const default_inherits_parent(false);
static unsigned short const default_notification_failure_options(
    hostdependency::none);

/**
 *  Default constructor.
 */
hostdependency::hostdependency()
    : object(object::hostdependency),
      _dependency_type(unknown),
      _execution_failure_options(default_execution_failure_options),
      _inherits_parent(default_inherits_parent),
      _notification_failure_options(default_notification_failure_options) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The hostdependency to copy.
 */
hostdependency::hostdependency(hostdependency const& right) : object(right) {
  operator=(right);
}

/**
 *  Copy constructor.
 *
 *  @param[in] right The hostdependency to copy.
 *
 *  @return This hostdependency.
 */
hostdependency& hostdependency::operator=(hostdependency const& right) {
  if (this != &right) {
    object::operator=(right);
    _dependency_period = right._dependency_period;
    _dependency_type = right._dependency_type;
    _dependent_hostgroups = right._dependent_hostgroups;
    _dependent_hosts = right._dependent_hosts;
    _execution_failure_options = right._execution_failure_options;
    _hostgroups = right._hostgroups;
    _hosts = right._hosts;
    _inherits_parent = right._inherits_parent;
    _notification_failure_options = right._notification_failure_options;
  }
  return *this;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The hostdependency to compare.
 *
 *  @return True if is the same hostdependency, otherwise false.
 */
bool hostdependency::operator==(hostdependency const& right) const noexcept {
  return (object::operator==(right) &&
          _dependency_period == right._dependency_period &&
          _dependency_type == right._dependency_type &&
          _dependent_hostgroups == right._dependent_hostgroups &&
          _dependent_hosts == right._dependent_hosts &&
          _execution_failure_options == right._execution_failure_options &&
          _hostgroups == right._hostgroups && _hosts == right._hosts &&
          _inherits_parent == right._inherits_parent &&
          _notification_failure_options == right._notification_failure_options);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The hostdependency to compare.
 *
 *  @return True if is not the same hostdependency, otherwise false.
 */
bool hostdependency::operator!=(hostdependency const& right) const noexcept {
  return !operator==(right);
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool hostdependency::operator<(hostdependency const& right) const {
  if (_dependent_hosts != right._dependent_hosts)
    return _dependent_hosts < right._dependent_hosts;
  else if (_hosts != right._hosts)
    return _hosts < right._hosts;
  else if (_hostgroups != right._hostgroups)
    return _hostgroups < right._hostgroups;
  else if (_dependent_hostgroups != right._dependent_hostgroups)
    return _dependent_hostgroups < right._dependent_hostgroups;
  else if (_dependency_type != right._dependency_type)
    return _dependency_type < right._dependency_type;
  else if (_dependency_period != right._dependency_period)
    return _dependency_period < right._dependency_period;
  else if (_execution_failure_options != right._execution_failure_options)
    return (_execution_failure_options < right._execution_failure_options);
  else if (_inherits_parent != right._inherits_parent)
    return _inherits_parent < right._inherits_parent;
  return (_notification_failure_options < right._notification_failure_options);
}

/**
 *  @brief Check if the object is valid.
 *
 *  If the object is not valid, an exception is thrown.
 */
void hostdependency::check_validity(configuration::error_cnt& err) const {
  if (_hosts->empty() && _hostgroups->empty())
    throw msg_fmt(
        "Host dependency is not attached to any host or host group (properties "
        "'host_name' or 'hostgroup_name', respectively)");
  if (_dependent_hosts->empty() && _dependent_hostgroups->empty())
    throw msg_fmt(
        "Host dependency is not attached to any dependent host or dependent "
        "host group (properties 'dependent_host_name' or "
        "'dependent_hostgroup_name', respectively)");

  if (!_execution_failure_options && !_notification_failure_options) {
    ++err.config_warnings;
    std::string host_name(!_hosts->empty() ? *_hosts->begin()
                                           : *_hostgroups->begin());
    std::string dependend_host_name(!_dependent_hosts->empty()
                                        ? *_dependent_hosts->begin()
                                        : *_dependent_hostgroups->begin());
    _logger->warn(
        "Warning: Ignoring lame host dependency of '{}' on host/hostgroups "
        "'{}'.",
        dependend_host_name, host_name);
  }
}

/**
 *  Get host dependency key.
 *
 *  @return This object.
 */
hostdependency::key_type const& hostdependency::key() const noexcept {
  return *this;
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void hostdependency::merge(object const& obj) {
  if (obj.type() != _type)
    throw msg_fmt("Cannot merge host dependency with '{}'",
                  static_cast<uint32_t>(obj.type()));
  hostdependency const& tmpl(static_cast<hostdependency const&>(obj));

  MRG_DEFAULT(_dependency_period);
  MRG_INHERIT(_dependent_hostgroups);
  MRG_INHERIT(_dependent_hosts);
  MRG_OPTION(_execution_failure_options);
  MRG_INHERIT(_hostgroups);
  MRG_INHERIT(_hosts);
  MRG_OPTION(_inherits_parent);
  MRG_OPTION(_notification_failure_options);
}

/**
 *  Parse and set the hostdependency property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool hostdependency::parse(char const* key, char const* value) {
  std::unordered_map<std::string, hostdependency::setter_func>::const_iterator
      it{_setters.find(key)};
  if (it != _setters.end())
    return (it->second)(*this, value);
  return false;
}

/**
 *  Set the dependency period.
 *
 *  @param[in] period New dependency period.
 */
void hostdependency::dependency_period(std::string const& period) {
  _dependency_period = period;
}

/**
 *  Get dependency_period.
 *
 *  @return The dependency_period.
 */
std::string const& hostdependency::dependency_period() const noexcept {
  return _dependency_period;
}

/**
 *  Set the dependency type.
 *
 *  @param[in] type Dependency type.
 */
void hostdependency::dependency_type(
    hostdependency::dependency_kind type) noexcept {
  _dependency_type = type;
}

/**
 *  Get the dependency type.
 *
 *  @return Dependency type.
 */
hostdependency::dependency_kind hostdependency::dependency_type()
    const noexcept {
  return _dependency_type;
}

/**
 *  Get dependent host groups.
 *
 *  @return Dependent host groups.
 */
set_string& hostdependency::dependent_hostgroups() noexcept {
  return *_dependent_hostgroups;
}

/**
 *  Get dependent_hostgroups.
 *
 *  @return The dependent_hostgroups.
 */
set_string const& hostdependency::dependent_hostgroups() const noexcept {
  return *_dependent_hostgroups;
}

/**
 *  Get dependent hosts.
 *
 *  @return The dependent hosts.
 */
set_string& hostdependency::dependent_hosts() noexcept {
  return *_dependent_hosts;
}

/**
 *  Get dependent_hosts.
 *
 *  @return The dependent_hosts.
 */
set_string const& hostdependency::dependent_hosts() const noexcept {
  return *_dependent_hosts;
}

/**
 *  Set the execution failure options.
 *
 *  @param[in] options New options.
 */
void hostdependency::execution_failure_options(unsigned int options) noexcept {
  _execution_failure_options.set(options);
}

/**
 *  Get execution_failure_options.
 *
 *  @return The execution_failure_options.
 */
unsigned int hostdependency::execution_failure_options() const noexcept {
  return _execution_failure_options;
}

/**
 *  Get host groups.
 *
 *  @return The host groups.
 */
set_string& hostdependency::hostgroups() noexcept {
  return *_hostgroups;
}

/**
 *  Get hostgroups.
 *
 *  @return The hostgroups.
 */
set_string const& hostdependency::hostgroups() const noexcept {
  return *_hostgroups;
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
set_string& hostdependency::hosts() noexcept {
  return *_hosts;
}

/**
 *  Get hosts.
 *
 *  @return The hosts.
 */
set_string const& hostdependency::hosts() const noexcept {
  return *_hosts;
}

/**
 *  Set parent inheritance.
 *
 *  @param[in] inherit True if dependency inherits parent.
 */
void hostdependency::inherits_parent(bool inherit) noexcept {
  _inherits_parent = inherit;
}

/**
 *  Get inherits_parent.
 *
 *  @return The inherits_parent.
 */
bool hostdependency::inherits_parent() const noexcept {
  return _inherits_parent;
}

/**
 *  Set notification failure options.
 *
 *  @param[in] options New options.
 */
void hostdependency::notification_failure_options(
    unsigned int options) noexcept {
  _notification_failure_options.set(options);
}

/**
 *  Get notification_failure_options.
 *
 *  @return The notification_failure_options.
 */
unsigned int hostdependency::notification_failure_options() const noexcept {
  return _notification_failure_options;
}

/**
 *  Set dependency_period value.
 *
 *  @param[in] value The new dependency_period value.
 *
 *  @return True on success, otherwise false.
 */
bool hostdependency::_set_dependency_period(std::string const& value) {
  _dependency_period = value;
  return true;
}

/**
 *  Set dependent_hostgroups value.
 *
 *  @param[in] value The new dependent_hostgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool hostdependency::_set_dependent_hostgroups(std::string const& value) {
  _dependent_hostgroups = value;
  return true;
}

/**
 *  Set dependent_hosts value.
 *
 *  @param[in] value The new dependent_hosts value.
 *
 *  @return True on success, otherwise false.
 */
bool hostdependency::_set_dependent_hosts(std::string const& value) {
  _dependent_hosts = value;
  return true;
}

/**
 *  Set execution_failure_options value.
 *
 *  @param[in] value The new execution_failure_options value.
 *
 *  @return True on success, otherwise false.
 */
bool hostdependency::_set_execution_failure_options(std::string const& value) {
  unsigned short options(none);
  auto values = absl::StrSplit(value, ',');
  for (auto& val : values) {
    auto v = absl::StripAsciiWhitespace(val);
    if (v == "o" || v == "up")
      options |= up;
    else if (v == "d" || v == "down")
      options |= down;
    else if (v == "u" || v == "unreachable")
      options |= unreachable;
    else if (v == "p" || v == "pending")
      options |= pending;
    else if (v == "n" || v == "none")
      options = none;
    else if (v == "a" || v == "all")
      options = up | down | unreachable | pending;
    else
      return false;
  }
  _execution_failure_options = options;
  return true;
}

/**
 *  Set hostgroups value.
 *
 *  @param[in] value The new hostgroups value.
 *
 *  @return True on success, otherwise false.
 */
bool hostdependency::_set_hostgroups(std::string const& value) {
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
bool hostdependency::_set_hosts(std::string const& value) {
  _hosts = value;
  return true;
}

/**
 *  Set inherits_parent value.
 *
 *  @param[in] value The new inherits_parent value.
 *
 *  @return True on success, otherwise false.
 */
bool hostdependency::_set_inherits_parent(bool value) {
  _inherits_parent = value;
  return true;
}

/**
 *  Set notification_failure_options value.
 *
 *  @param[in] value The new notification_failure_options value.
 *
 *  @return True on success, otherwise false.
 */
bool hostdependency::_set_notification_failure_options(
    std::string const& value) {
  unsigned short options(none);
  auto values = absl::StrSplit(value, ',');
  for (auto& val : values) {
    auto v = absl::StripAsciiWhitespace(val);
    if (v == "o" || v == "up")
      options |= up;
    else if (v == "d" || v == "down")
      options |= down;
    else if (v == "u" || v == "unreachable")
      options |= unreachable;
    else if (v == "p" || v == "pending")
      options |= pending;
    else if (v == "n" || v == "none")
      options = none;
    else if (v == "a" || v == "all")
      options = up | down | unreachable | pending;
    else
      return false;
  }
  _notification_failure_options = options;
  return true;
}
