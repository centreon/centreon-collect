/**
 * Copyright 2011-2014 Merethis
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
#include "object.hh"
#include <absl/strings/ascii.h>
#include "anomalydetection.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "command.hh"
#include "common/log_v2/log_v2.hh"
#include "connector.hh"
#include "contact.hh"
#include "contactgroup.hh"
#include "host.hh"
#include "hostdependency.hh"
#include "hostescalation.hh"
#include "hostgroup.hh"
#include "service.hh"
#include "servicedependency.hh"
#include "serviceescalation.hh"
#include "servicegroup.hh"
#include "severity.hh"
#include "tag.hh"
#include "timeperiod.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::common::log_v2::log_v2;
using com::centreon::exceptions::msg_fmt;

#define SETTER(type, method) \
  &object::setter<object, type, &object::method>::generic

object::setters const object::_setters[] = {
    {"use", SETTER(std::string const&, _set_templates)},
    {"name", SETTER(std::string const&, _set_name)},
    {"register", SETTER(bool, _set_should_register)}};

/**
 *  Constructor.
 *
 *  @param[in] type      The object type.
 */
object::object(object::object_type type)
    : _is_resolve(false),
      _should_register(true),
      _type(type),
      _logger{log_v2::instance().get(log_v2::CONFIG)} {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The object to copy.
 */
object::object(object const& right) {
  operator=(right);
}

/**
 *  Copy constructor.
 *
 *  @param[in] right The object to copy.
 *
 *  @return This object.
 */
object& object::operator=(object const& right) {
  if (this != &right) {
    _is_resolve = right._is_resolve;
    _name = right._name;
    _should_register = right._should_register;
    _templates = right._templates;
    _type = right._type;
    _logger = right._logger;
  }
  return *this;
}

/**
 *  Equal operator.
 *
 *  @param[in] right The object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool object::operator==(object const& right) const noexcept {
  return (_name == right._name && _type == right._type &&
          _is_resolve == right._is_resolve &&
          _should_register == right._should_register &&
          _templates == right._templates);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool object::operator!=(object const& right) const noexcept {
  return !operator==(right);
}

/**
 *  Create object with object type.
 *
 *  @param[in] type_name The object type name.
 *
 *  @return New object.
 */
object_ptr object::create(std::string const& type_name) {
  object_ptr obj;
  if (type_name == "service")
    obj = std::make_shared<configuration::service>();
  else if (type_name == "host")
    obj = std::make_shared<configuration::host>();
  else if (type_name == "contact")
    obj = std::make_shared<configuration::contact>();
  else if (type_name == "contactgroup")
    obj = std::make_shared<configuration::contactgroup>();
  else if (type_name == "servicegroup")
    obj = std::make_shared<configuration::servicegroup>();
  else if (type_name == "hostgroup")
    obj = std::make_shared<configuration::hostgroup>();
  else if (type_name == "servicedependency")
    obj = std::make_shared<configuration::servicedependency>();
  else if (type_name == "serviceescalation")
    obj = std::make_shared<configuration::serviceescalation>();
  else if (type_name == "hostdependency")
    obj = std::make_shared<configuration::hostdependency>();
  else if (type_name == "hostescalation")
    obj = std::make_shared<configuration::hostescalation>();
  else if (type_name == "command")
    obj = std::make_shared<configuration::command>();
  else if (type_name == "timeperiod")
    obj = std::make_shared<configuration::timeperiod>();
  else if (type_name == "connector")
    obj = std::make_shared<configuration::connector>();
  else if (type_name == "anomalydetection")
    obj = std::make_shared<configuration::anomalydetection>();
  else if (type_name == "severity")
    obj = std::make_shared<configuration::severity>();
  else if (type_name == "tag")
    obj = std::make_shared<configuration::tag>();
  return obj;
}

/**
 *  Get the object name.
 *
 *  @return The object name.
 */
std::string const& object::name() const noexcept {
  return _name;
}

/**
 *  Parse and set the object property.
 *
 *  @param[in] key   The property name.
 *  @param[in] value The property value.
 *
 *  @return True on success, otherwise false.
 */
bool object::parse(char const* key, char const* value) {
  for (unsigned int i(0); i < sizeof(_setters) / sizeof(_setters[0]); ++i)
    if (!strcmp(_setters[i].name, key))
      return (_setters[i].func)(*this, value);
  return false;
}

/**
 *  Parse and set the object property.
 *
 *  @param[in] line The configuration line.
 *
 *  @return True on success, otherwise false.
 */
bool object::parse(std::string const& line) {
  std::size_t pos(line.find_first_of(" \t\r", 0));
  std::string key;
  std::string value;
  if (pos == std::string::npos)
    key = line;
  else {
    key.assign(line, 0, pos);
    value.assign(line, pos + 1, std::string::npos);
  }
  value = absl::StripAsciiWhitespace(value);
  if (!parse(key.c_str(), value.c_str()))
    return object::parse(key.c_str(), value.c_str());
  return true;
}

/**
 *  Resolve template object.
 *
 *  @param[in, out] templates The template list.
 */
void object::resolve_template(map_object& templates) {
  if (_is_resolve)
    return;

  _is_resolve = true;
  for (std::string& s : _templates) {
    map_object::iterator tmpl = templates.find(s);
    if (tmpl == templates.end())
      throw msg_fmt("Cannot merge object of type '{}'", s);
    tmpl->second->resolve_template(templates);
    merge(*tmpl->second);
  }
}

/**
 *  Check if object should be registered.
 *
 *  @return True if object should be registered, false otherwise.
 */
bool object::should_register() const noexcept {
  return _should_register;
}

/**
 *  Get the object type.
 *
 *  @return The object type.
 */
object::object_type object::type() const noexcept {
  return _type;
}

/**
 *  Get the object type name.
 *
 *  @return The object type name.
 */
std::string const& object::type_name() const noexcept {
  static std::string const tab[] = {"command",
                                    "connector",
                                    "contact",
                                    "contactgroup",
                                    "host",
                                    "hostdependency",
                                    "hostescalation",
                                    "hostextinfo",
                                    "hostgroup",
                                    "service",
                                    "servicedependency",
                                    "serviceescalation",
                                    "serviceextinfo",
                                    "servicegroup",
                                    "timeperiod",
                                    "anomalydetection",
                                    "severity",
                                    "tag"};
  return tab[_type];
}

/**
 *  Set name value.
 *
 *  @param[in] value The new name value.
 *
 *  @return True on success, otherwise false.
 */
bool object::_set_name(std::string const& value) {
  _name = value;
  return true;
}

/**
 *  Set whether or not this object should be registered.
 *
 *  @param[in] value Registration flag.
 *
 *  @return True.
 */
bool object::_set_should_register(bool value) {
  _should_register = value;
  return true;
}

/**
 *  Set templates value.
 *
 *  @param[in] value The new templates value.
 *
 *  @return True on success, otherwise false.
 */
bool object::_set_templates(std::string const& value) {
  _templates.clear();
  _templates = absl::StrSplit(value, ',');
  return true;
}
