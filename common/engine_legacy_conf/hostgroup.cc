/**
 * Copyright 2011-2013,2017-2024 Centreon
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
#include "hostgroup.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<hostgroup, type, &hostgroup::method>::generic

std::unordered_map<std::string, hostgroup::setter_func> const
    hostgroup::_setters{
        {"hostgroup_id", SETTER(unsigned int, _set_hostgroup_id)},
        {"hostgroup_name", SETTER(std::string const&, _set_hostgroup_name)},
        {"alias", SETTER(std::string const&, _set_alias)},
        {"members", SETTER(std::string const&, _set_members)},
        {"notes", SETTER(std::string const&, _set_notes)},
        {"notes_url", SETTER(std::string const&, _set_notes_url)},
        {"action_url", SETTER(std::string const&, _set_action_url)}};

/**
 *  Constructor.
 *
 *  @param[in] key The object key.
 */
hostgroup::hostgroup(key_type const& key)
    : object(object::hostgroup), _hostgroup_id(0), _hostgroup_name(key) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The hostgroup to copy.
 */
hostgroup::hostgroup(hostgroup const& right) : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
hostgroup::~hostgroup() throw() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The hostgroup to copy.
 *
 *  @return This hostgroup.
 */
hostgroup& hostgroup::operator=(hostgroup const& right) {
  if (this != &right) {
    object::operator=(right);
    _action_url = right._action_url;
    _alias = right._alias;
    _hostgroup_id = right._hostgroup_id;
    _hostgroup_name = right._hostgroup_name;
    _members = right._members;
    _notes = right._notes;
    _notes_url = right._notes_url;
  }
  return *this;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The hostgroup to compare.
 *
 *  @return True if is the same hostgroup, otherwise false.
 */
bool hostgroup::operator==(hostgroup const& right) const throw() {
  if (!object::operator==(right)) {
    _logger->debug("configuration::hostgroup::equality => object don't match");
    return false;
  }
  if (_action_url != right._action_url) {
    _logger->debug(
        "configuration::hostgroup::equality => action url don't match");
    return false;
  }
  if (_alias != right._alias) {
    _logger->debug("configuration::hostgroup::equality => alias don't match");
    return false;
  }
  if (_hostgroup_id != right._hostgroup_id) {
    _logger->debug(
        "configuration::hostgroup::equality => hostgroup id don't match");
    return false;
  }
  if (_hostgroup_name != right._hostgroup_name) {
    _logger->debug(
        "configuration::hostgroup::equality => hostgroup name don't match");
    return false;
  }
  if (_members != right._members) {
    _logger->debug("configuration::hostgroup::equality => members don't match");
    return false;
  }
  if (_notes != right._notes) {
    _logger->debug("configuration::hostgroup::equality => notes don't match");
    return false;
  }
  if (_notes_url != right._notes_url) {
    _logger->debug(
        "configuration::hostgroup::equality => notes url don't match");
    return false;
  }
  return true;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The hostgroup to compare.
 *
 *  @return True if is not the same hostgroup, otherwise false.
 */
bool hostgroup::operator!=(hostgroup const& right) const throw() {
  return !operator==(right);
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is less than right.
 */
bool hostgroup::operator<(hostgroup const& right) const throw() {
  // hostgroup_name has to be first in this operator.
  // The configuration diff mechanism relies on this.
  if (_hostgroup_name != right._hostgroup_name)
    return _hostgroup_name < right._hostgroup_name;
  else if (_action_url != right._action_url)
    return _action_url < right._action_url;
  else if (_alias != right._alias)
    return _alias < right._alias;
  else if (_hostgroup_id != right._hostgroup_id)
    return _hostgroup_id < right._hostgroup_id;
  else if (_notes != right._notes)
    return _notes < right._notes;
  else if (_notes_url != right._notes_url)
    return _notes_url < right._notes_url;
  return _members < right._members;
}

/**
 *  @brief Check if the object is valid.
 *
 *  If the object is not valid, an exception is thrown.
 */
void hostgroup::check_validity(error_cnt& err [[maybe_unused]]) const {
  if (_hostgroup_name.empty())
    throw msg_fmt("Host group has no name (property 'hostgroup_name')");
}

/**
 *  Get the host group key.
 *
 *  @return The host group name.
 */
hostgroup::key_type const& hostgroup::key() const throw() {
  return _hostgroup_name;
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void hostgroup::merge(object const& obj) {
  if (obj.type() != _type)
    throw msg_fmt("Cannot merge host group with '{}'",
                  static_cast<uint32_t>(obj.type()));
  hostgroup const& tmpl(static_cast<hostgroup const&>(obj));
  MRG_DEFAULT(_action_url);
  MRG_DEFAULT(_alias);
  MRG_DEFAULT(_hostgroup_name);
  MRG_INHERIT(_members);
  MRG_DEFAULT(_notes);
  MRG_DEFAULT(_notes_url);
}

/**
 *  Parse and set the hostgroup property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool hostgroup::parse(char const* key, char const* value) {
  std::unordered_map<std::string, hostgroup::setter_func>::const_iterator it{
      _setters.find(key)};
  if (it != _setters.end())
    return (it->second)(*this, value);
  return false;
}

/**
 *  Get action_url.
 *
 *  @return The action_url.
 */
std::string const& hostgroup::action_url() const throw() {
  return _action_url;
}

/**
 *  Get alias.
 *
 *  @return The alias.
 */
std::string const& hostgroup::alias() const throw() {
  return _alias;
}

/**
 *  Get hostgroup id.
 *
 *  @return  The hostgroup id.
 */
unsigned int hostgroup::hostgroup_id() const throw() {
  return _hostgroup_id;
}

/**
 *  Get hostgroup_name.
 *
 *  @return The hostgroup_name.
 */
std::string const& hostgroup::hostgroup_name() const throw() {
  return _hostgroup_name;
}

/**
 *  Get members.
 *
 *  @return The members.
 */
set_string& hostgroup::members() throw() {
  return *_members;
}

/**
 *  Get members.
 *
 *  @return The members.
 */
set_string const& hostgroup::members() const throw() {
  return *_members;
}

/**
 *  Get notes.
 *
 *  @return The notes.
 */
std::string const& hostgroup::notes() const throw() {
  return _notes;
}

/**
 *  Get notes_url.
 *
 *  @return The notes_url.
 */
std::string const& hostgroup::notes_url() const throw() {
  return _notes_url;
}

/**
 *  Set action_url value.
 *
 *  @param[in] value The new action_url value.
 *
 *  @return True on success, otherwise false.
 */
bool hostgroup::_set_action_url(std::string const& value) {
  _action_url = value;
  return true;
}

/**
 *  Set alias value.
 *
 *  @param[in] value The new alias value.
 *
 *  @return True on success, otherwise false.
 */
bool hostgroup::_set_alias(std::string const& value) {
  _alias = value;
  return true;
}

/**
 *  Set hostgroup_id value.
 *
 *  @param[in] value  The new hostgroup_id value.
 *
 *  @return True on success, otherwise false.
 */
bool hostgroup::_set_hostgroup_id(unsigned int value) {
  _hostgroup_id = value;
  return true;
}

/**
 *  Set hostgroup_name value.
 *
 *  @param[in] value The new hostgroup_name value.
 *
 *  @return True on success, otherwise false.
 */
bool hostgroup::_set_hostgroup_name(std::string const& value) {
  _hostgroup_name = value;
  return true;
}

/**
 *  Set members value.
 *
 *  @param[in] value The new members value.
 *
 *  @return True on success, otherwise false.
 */
bool hostgroup::_set_members(std::string const& value) {
  _members = value;
  return true;
}

/**
 *  Set notes value.
 *
 *  @param[in] value The new notes value.
 *
 *  @return True on success, otherwise false.
 */
bool hostgroup::_set_notes(std::string const& value) {
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
bool hostgroup::_set_notes_url(std::string const& value) {
  _notes_url = value;
  return true;
}
