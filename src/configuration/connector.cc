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

#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/configuration/connector.hh"
#include "com/centreon/engine/error.hh"

using namespace com::centreon::engine;

#define SETTER(type, method) \
  &configuration::object::setter< \
     configuration::connector, \
     type, \
     &configuration::connector::method>::generic

static struct {
  std::string const name;
  bool (*func)(configuration::connector&, std::string const&);
} gl_setters[] = {
  { "connector_line", SETTER(std::string const&, _set_connector_line) },
  { "connector_name", SETTER(std::string const&, _set_connector_name) }
};

/**
 *  Default constructor.
 */
configuration::connector::connector()
  : object(object::connector, "connector") {

}

/**
 *  Copy constructor.
 *
 *  @param[in] right The connector to copy.
 */
configuration::connector::connector(connector const& right)
  : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
configuration::connector::~connector() throw () {

}

/**
 *  Copy constructor.
 *
 *  @param[in] right The connector to copy.
 *
 *  @return This connector.
 */
configuration::connector& configuration::connector::operator=(
                            connector const& right) {
  if (this != &right) {
    object::operator=(right);
    _connector_line = right._connector_line;
    _connector_name = right._connector_name;
  }
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The connector to compare.
 *
 *  @return True if is the same connector, otherwise false.
 */
bool configuration::connector::operator==(
       connector const& right) const throw () {
  return (object::operator==(right)
          && _connector_line == right._connector_line
          && _connector_name == right._connector_name);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The connector to compare.
 *
 *  @return True if is not the same connector, otherwise false.
 */
bool configuration::connector::operator!=(
       connector const& right) const throw () {
  return (!operator==(right));
}

/**
 *  Create new connector.
 *
 *  @return The new connector.
 */
commands::connector* configuration::connector::create() const {
  char* command_line(NULL);
  nagios_macros* macros(get_global_macros());

  process_macros_r(
    macros,
    _connector_line.c_str(),
    &command_line,
    0);
  std::string processed_cmd(command_line);
  delete[] command_line;

  return (new commands::connector(
                _connector_name,
                processed_cmd,
                &checks::checker::instance()));
}

/**
 *  Get the unique object id.
 *
 *  @return The object id.
 */
std::size_t configuration::connector::id() const throw () {
  return (_id);
}

/**
 *  Check if the object is valid.
 *
 *  @return True if is a valid object, otherwise false.
 */
bool configuration::connector::is_valid() const throw () {
  return (!_connector_name.empty()
          && !_connector_line.empty());
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void configuration::connector::merge(object const& obj) {
  if (obj.type() != _type)
    throw (engine_error() << "merge failed: invalid object type");
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
bool configuration::connector::parse(
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
 *  Set connector_line value.
 *
 *  @param[in] value The new connector_line value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::connector::_set_connector_line(
       std::string const& value) {
  _connector_line = value;
  return (true);
}

/**
 *  Set connector_name value.
 *
 *  @param[in] value The new connector_name value.
 *
 *  @return True on success, otherwise false.
 */
bool configuration::connector::_set_connector_name(
       std::string const& value) {
  _connector_name = value;
  _id = _hash(value);
  return (true);
}
