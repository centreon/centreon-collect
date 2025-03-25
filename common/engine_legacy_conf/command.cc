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
#include "command.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<command, type, &command::method>::generic

std::unordered_map<std::string, command::setter_func> const command::_setters{
    {"command_line", SETTER(std::string const&, _set_command_line)},
    {"command_name", SETTER(std::string const&, _set_command_name)},
    {"connector", SETTER(std::string const&, _set_connector)}};

/**
 *  Constructor.
 *
 *  @param[in] key The object key.
 */
command::command(key_type const& key)
    : object(object::command), _command_name(key) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The command to copy.
 */
command::command(command const& right) : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
command::~command() throw() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The command to copy.
 *
 *  @return This command.
 */
command& command::operator=(command const& right) {
  if (this != &right) {
    object::operator=(right);
    _command_line = right._command_line;
    _command_name = right._command_name;
    _connector = right._connector;
  }
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The command to compare.
 *
 *  @return True if is the same command, otherwise false.
 */
bool command::operator==(command const& right) const throw() {
  return (object::operator==(right) && _command_line == right._command_line &&
          _command_name == right._command_name &&
          _connector == right._connector);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The command to compare.
 *
 *  @return True if is not the same command, otherwise false.
 */
bool command::operator!=(command const& right) const throw() {
  return (!operator==(right));
}

/**
 *  Less-than operator.
 *
 *  @param[in] right Object to compare to.
 *
 *  @return True if this object is strictly less than right.
 */
bool command::operator<(command const& right) const throw() {
  if (_command_name != right._command_name)
    return (_command_name < right._command_name);
  return (_command_line < right._command_line);
}

/**
 *  @brief Check if the object is valid.
 *
 *  If the object is not valid, an exception is thrown.
 */
void command::check_validity(error_cnt& err [[maybe_unused]]) const {
  if (_command_name.empty())
    throw msg_fmt("Command has no name (property 'command_name')");
  if (_command_line.empty())
    throw msg_fmt("Command '{}' has no command line (property 'command_line')",
                  _command_name);
}

/**
 *  Get the command key.
 *
 *  @return The command name.
 */
command::key_type const& command::key() const throw() {
  return _command_name;
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void command::merge(object const& obj) {
  if (obj.type() != _type)
    throw msg_fmt("Cannot merge command with '{}'",
                  static_cast<uint32_t>(obj.type()));
  command const& tmpl(static_cast<command const&>(obj));

  MRG_DEFAULT(_command_line);
  MRG_DEFAULT(_command_name);
  MRG_DEFAULT(_connector);
}

/**
 *  Parse and set the command property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool command::parse(char const* key, char const* value) {
  std::unordered_map<std::string, command::setter_func>::const_iterator it{
      _setters.find(key)};
  if (it != _setters.end())
    return (it->second)(*this, value);
  return false;
}

/**
 *  Get command_line.
 *
 *  @return The command_line.
 */
std::string const& command::command_line() const throw() {
  return (_command_line);
}

/**
 *  Get command_name.
 *
 *  @return The command_name.
 */
std::string const& command::command_name() const throw() {
  return (_command_name);
}

/**
 *  Get connector.
 *
 *  @return The connector.
 */
std::string const& command::connector() const throw() {
  return (_connector);
}

/**
 *  Set command_line value.
 *
 *  @param[in] value The new command_line value.
 *
 *  @return True on success, otherwise false.
 */
bool command::_set_command_line(std::string const& value) {
  _command_line = value;
  return (true);
}

/**
 *  Set command_name value.
 *
 *  @param[in] value The new command_name value.
 *
 *  @return True on success, otherwise false.
 */
bool command::_set_command_name(std::string const& value) {
  _command_name = value;
  return (true);
}

/**
 *  Set connector value.
 *
 *  @param[in] value The new connector value.
 *
 *  @return True on success, otherwise false.
 */
bool command::_set_connector(std::string const& value) {
  _connector = value;
  return (true);
}
