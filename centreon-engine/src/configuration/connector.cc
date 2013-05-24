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

#include "com/centreon/engine/configuration/connector.hh"
#include "com/centreon/engine/error.hh"

using namespace com::centreon::engine::configuration;

#define SETTER(type, method) \
  &object::setter<connector, type, &connector::method>::generic

static struct {
  std::string const name;
  bool (*func)(connector&, std::string const&);
} gl_setters[] = {
  { "connector_line", SETTER(std::string const&, _set_connector_line) },
  { "connector_name", SETTER(std::string const&, _set_connector_name) }
};

/**
 *  Default constructor.
 */
connector::connector()
  : object("connector") {

}

/**
 *  Copy constructor.
 *
 *  @param[in] right The connector to copy.
 */
connector::connector(connector const& right)
  : object(right) {
  operator=(right);
}

/**
 *  Destructor.
 */
connector::~connector() throw () {

}

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
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right The connector to compare.
 *
 *  @return True if is the same connector, otherwise false.
 */
bool connector::operator==(connector const& right) const throw () {
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
bool connector::operator!=(connector const& right) const throw () {
  return (!operator==(right));
}

/**
 *  Merge object.
 *
 *  @param[in] obj The object to merge.
 */
void connector::merge(object const& obj) {
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
bool connector::parse(
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
bool connector::_set_connector_line(std::string const& value) {
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
bool connector::_set_connector_name(std::string const& value) {
  _connector_name = value;
  return (true);
}
