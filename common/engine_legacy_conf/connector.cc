/**
 * Copyright 2011-2013 Merethis
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
#include "connector.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<connector, type, &connector::method>::generic

std::unordered_map<std::string, connector::setter_func> const
    connector::_setters{
        {"connector_line", SETTER(std::string const&, _set_connector_line)},
        {"connector_name", SETTER(std::string const&, _set_connector_name)}};

/**
 *  Constructor.
 *
 *  @param[in] key The object key.
 */
connector::connector(key_type const& key)
    : object(object::connector), _connector_name(key) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The connector to copy.
 */
connector::connector(connector const& right) : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
connector::~connector() throw() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The connector to copy.
 *
 *  @return This connector.
 */
connector& connector::operator=(connector const& right) {
  if (this != &right) {
    object::operator=(right);
    _connector_line = right._connector_line;
    _connector_name = right._connector_name;
  }
  return *this;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The connector to compare.
 *
 *  @return True if is the same connector, otherwise false.
 */
bool connector::operator==(connector const& right) const throw() {
  return (object::operator==(right) &&
          _connector_line == right._connector_line &&
          _connector_name == right._connector_name);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The connector to compare.
 *
 *  @return True if is not the same connector, otherwise false.
 */
bool connector::operator!=(connector const& right) const throw() {
  return !operator==(right);
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is strictly less than right.
 */
bool connector::operator<(connector const& right) const throw() {
  if (_connector_name != right._connector_name)
    return _connector_name < right._connector_name;
  return _connector_line < right._connector_line;
}

/**
 *  @brief Check if the object is valid.
 *
 *  If the object is not valid, an exception is thrown.
 */
void connector::check_validity(error_cnt& err [[maybe_unused]]) const {
  if (_connector_name.empty())
    throw msg_fmt("Connector has no name (property 'connector_name')");
  if (_connector_line.empty())
    throw msg_fmt(
        "Connector '{}' has no command line (property 'connector_line')",
        _connector_name);
}

/**
 *  Get the connector key.
 *
 *  @return The connector name.
 */
connector::key_type const& connector::key() const throw() {
  return _connector_name;
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void connector::merge(object const& obj) {
  if (obj.type() != _type)
    throw msg_fmt("Cannot merge connector with '{}'",
                  static_cast<uint32_t>(obj.type()));
  connector const& tmpl(static_cast<connector const&>(obj));

  MRG_DEFAULT(_connector_line);
}

/**
 *  Parse and set the connector property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool connector::parse(char const* key, char const* value) {
  std::unordered_map<std::string, connector::setter_func>::const_iterator it{
      _setters.find(key)};
  if (it != _setters.end())
    return (it->second)(*this, value);
  return false;
}

/**
 *  Get connector_line.
 *
 *  @return The connector_line.
 */
std::string const& connector::connector_line() const throw() {
  return _connector_line;
}

/**
 *  Get connector_name.
 *
 *  @return The connector_name.
 */
std::string const& connector::connector_name() const throw() {
  return _connector_name;
}

/**
 *  Set connector_line value.
 *
 *  @param[in] value The new connector_line value.
 *
 *  @return True on success, otherwise false.
 */
bool connector::_set_connector_line(std::string const& value) {
  _connector_line = value;
  return true;
}

/**
 *  Set connector_name value.
 *
 *  @param[in] value The new connector_name value.
 *
 *  @return True on success, otherwise false.
 */
bool connector::_set_connector_name(std::string const& value) {
  _connector_name = value;
  return true;
}
