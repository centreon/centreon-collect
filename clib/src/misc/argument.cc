/**
* Copyright 2011-2013 Centreon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
*/

#include "com/centreon/misc/argument.hh"
#include <iostream>

using namespace com::centreon::misc;

/**
 *  Default constructor.
 *
 *  @param[in] long_name  Argument's long name.
 *  @param[in] name       Argument's name.
 *  @param[in] has_value  Argument need a value.
 *  @param[in] is_set     Argument is set by default.
 *  @param[in] value      The default value.
 */
argument::argument(std::string const& long_name,
                   char name,
                   std::string const& description,
                   bool has_value,
                   bool is_set,
                   std::string const& value)
    : _description(description),
      _is_set(is_set),
      _has_value(has_value),
      _long_name(long_name),
      _name(name),
      _value(value) {}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
argument::argument(argument const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
argument::~argument() throw() {}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
argument& argument::operator=(argument const& right) {
  return (_internal_copy(right));
}

/**
 *  Equal operator.
 *
 *  @param[in] right The object to compare.
 *
 *  @return Tue if objects are equal, otherwise false.
 */
bool argument::operator==(argument const& right) const throw() {
  return (_name == right._name && _long_name == right._long_name &&
          _value == right._value && _is_set == right._is_set &&
          _has_value == right._has_value && _description == right._description);
}

/**
 *  Not equal operator.
 *
 *  @param[in] right The object to compare.
 *
 *  @return True if objects are different, otherwise false.
 */
bool argument::operator!=(argument const& right) const throw() {
  return (!operator==(right));
}

/**
 *  Get the argument description.
 *
 *  @return The description.
 */
std::string const& argument::get_description() const throw() {
  return (_description);
}

/**
 *  Get if argument is set.
 *
 *  @return True if argument is set.
 */
bool argument::get_is_set() const throw() {
  return (_is_set);
}

/**
 *  Get if argument need to has value.
 *
 *  @return True if argument need a value, otherwise false.
 */
bool argument::get_has_value() const throw() {
  return (_has_value);
}

/**
 *  Get the long name of the argument.
 *
 *  @return The long name.
 */
std::string const& argument::get_long_name() const throw() {
  return (_long_name);
}

/**
 *  Get the name of the argument.
 *
 *  @return The name.
 */
char argument::get_name() const throw() {
  return (_name);
}

/**
 *  Get the value.
 *
 *  @return The value.
 */
std::string const& argument::get_value() const throw() {
  return (_value);
}

/**
 *  Set the argument description.
 *
 *  @param[in] description  The description.
 */
void argument::set_description(std::string const& description) {
  _description = description;
}

/**
 *  Set is the argument is set.
 *
 *  @param[in] val  True if the argument is set.
 */
void argument::set_is_set(bool val) throw() {
  _is_set = val;
}

/**
 *  Set is the argument need a value.
 *
 *  @param[in] val  True if the argument need a value.
 */
void argument::set_has_value(bool val) throw() {
  _has_value = val;
}

/**
 *  Set the long name of the argument.
 *
 *  @param[in] long_name  The long name.
 */
void argument::set_long_name(std::string const& long_name) {
  _long_name = long_name;
}

/**
 *  Set the name of the argument.
 *
 *  @param[in] name  The name.
 */
void argument::set_name(char name) {
  _name = name;
}

/**
 *  Set the value of the argument.
 *
 *  @param[in] value  The value.
 */
void argument::set_value(std::string const& value) {
  _value = value;
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
argument& argument::_internal_copy(argument const& right) {
  if (this != &right) {
    _description = right._description;
    _is_set = right._is_set;
    _has_value = right._has_value;
    _long_name = right._long_name;
    _name = right._name;
    _value = right._value;
  }
  return (*this);
}
