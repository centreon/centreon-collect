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

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/deleter/customvariablesmember.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/objects/customvariablesmember.hh"
#include "com/centreon/engine/objects/tool.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

/**
 *  Equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool operator==(
       customvariablesmember const& obj1,
       customvariablesmember const& obj2) throw () {
  if (is_equal(obj1.variable_name, obj2.variable_name)
      && is_equal(obj1.variable_value, obj2.variable_value)
      && obj1.has_been_modified == obj2.has_been_modified) {
    if (!obj1.next && !obj2.next)
      return (*obj1.next == *obj2.next);
    if (obj1.next == obj2.next)
      return (true);
  }
  return (false);
}

/**
 *  Not equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool operator!=(
       customvariablesmember const& obj1,
       customvariablesmember const& obj2) throw () {
  return (!operator==(obj1, obj2));
}

/**
 *  Dump customvariablesmember content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The customvariablesmember to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, customvariablesmember const& obj) {
  for (customvariablesmember const* m(&obj); m; m = m->next)
    os << "  " << chkstr(m->variable_name) << ": " << chkstr(m->variable_value) << "\n";
  return (os);
}

/**
 *  Adds a custom variable to a contact.
 *
 *  @param[in] cntct    Contact object.
 *  @param[in] varname  Custom variable name.
 *  @param[in] varvalue Custom variable value.
 *
 *  @return Contact custom variable.
 */
customvariablesmember* add_custom_variable_to_contact(
                         contact* cntct,
                         char const* varname,
                         char const* varvalue) {
  // Add custom variable to contact.
  customvariablesmember* retval(add_custom_variable_to_object(
                                  &cntct->custom_variables,
                                  varname,
                                  varvalue));

  // Notify event broker.
  timeval tv(get_broker_timestamp(NULL));
  broker_custom_variable(
    NEBTYPE_CONTACTCUSTOMVARIABLE_ADD,
    NEBFLAG_NONE,
    NEBATTR_NONE,
    cntct,
    retval,
    &tv);

  return (retval);
}

/**
 *  Adds a custom variable to a host
 *
 *  @param[in] hst      Host.
 *  @param[in] varname  Custom variable name.
 *  @param[in] varvalue Custom variable value.
 *
 *  @return New host custom variable.
 */
customvariablesmember* add_custom_variable_to_host(
                         host* hst,
                         char const* varname,
                         char const* varvalue) {
  // Add custom variable to host.
  customvariablesmember* retval(add_custom_variable_to_object(
                                  &hst->custom_variables,
                                  varname,
                                  varvalue));

  // Notify event broker.
  timeval tv(get_broker_timestamp(NULL));
  broker_custom_variable(
    NEBTYPE_HOSTCUSTOMVARIABLE_ADD,
    NEBFLAG_NONE,
    NEBATTR_NONE,
    hst,
    retval,
    &tv);

  return (retval);
}

/**
 *  Adds a custom variable to an object.
 *
 *  @param[in] object_ptr Object's custom variables.
 *  @param[in] varname    Custom variable name.
 *  @param[in] varvalue   Custom variable value.
 *
 *  @return New custom variable.
 */
customvariablesmember* add_custom_variable_to_object(
                         customvariablesmember** object_ptr,
                         char const* varname,
                         char const* varvalue) {
  // Make sure we have the data we need.
  if (!object_ptr) {
    logger(log_config_error, basic)
      << "Error: Custom variable object is NULL";
    return (NULL);
  }
  if (!varname || !varname[0]) {
    logger(log_config_error, basic)
      << "Error: Custom variable name is NULL";
    return (NULL);
  }

  // Allocate memory for a new member.
  customvariablesmember* obj(new customvariablesmember);
  memset(obj, 0, sizeof(*obj));

  try {
    obj->variable_name = string::dup(varname);
    if (varvalue)
      obj->variable_value = string::dup(varvalue);

    // Add the new member to the head of the member list.
    obj->next = *object_ptr;
    *object_ptr = obj;
  }
  catch (...) {
    deleter::customvariablesmember(obj);
    obj = NULL;
  }

  return (obj);
}

/**
 *  Adds a custom variable to a service.
 *
 *  @param[in] svc      Service.
 *  @param[in] varname  Custom variable name.
 *  @param[in] varvalue Custom variable value.
 *
 *  @return New custom variable.
 */
customvariablesmember* add_custom_variable_to_service(
                         service* svc,
                         char const* varname,
                         char const* varvalue) {
  // Add custom variable to service.
  customvariablesmember* retval(add_custom_variable_to_object(
                                  &svc->custom_variables,
                                  varname,
                                  varvalue));

  // Notify event broker.
  timeval tv(get_broker_timestamp(NULL));
  broker_custom_variable(
    NEBTYPE_SERVICECUSTOMVARIABLE_ADD,
    NEBFLAG_NONE,
    NEBATTR_NONE,
    svc,
    retval,
    &tv);

  return (retval);
}

/**
 *  Update the custom variable value.
 *
 *  @param[in,out] lst   The custom variables list.
 *  @param[in]     key   The key to find.
 *  @param[in]     value The new value to set.
 *
 *  @return True if the custom variable change.
 */
bool engine::update_customvariable(
       customvariablesmember* lst,
       std::string const& key,
       std::string const& value) {
  char const* cv_name(key.c_str());
  char const* cv_value(value.c_str());
  for (customvariablesmember* m(lst); m; m = m->next) {
    if (!strcmp(cv_name, m->variable_name)) {
      if (strcmp(cv_value, m->variable_value)) {
        string::setstr(m->variable_value, value);
        m->has_been_modified = true;
      }
      return (true);
    }
  }
  return (false);
}
