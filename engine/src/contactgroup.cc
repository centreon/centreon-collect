/**
 * Copyright 2011-2019,2024 Centreon
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
#ifdef LEGACY_CONF
#include "common/engine_legacy_conf/contactgroup.hh"
#endif
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/contactgroup.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

contactgroup_map contactgroup::contactgroups;

#ifdef LEGACY_CONF
/**
 *  Constructor from a configuration contactgroup
 *
 * @param obj Configuration contactgroup
 */
contactgroup::contactgroup(configuration::contactgroup const& obj)
    : _alias(obj.alias().empty() ? obj.contactgroup_name() : obj.alias()),
      _name(obj.contactgroup_name()) {
  // Make sure we have the data we need.
  if (_name.empty())
    throw engine_error() << "contactgroup: Contact group name is empty";

  // Notify event broker.
  broker_group(NEBTYPE_CONTACTGROUP_ADD, this);
}
#else
/**
 *  Constructor from a protobuf configuration contactgroup
 *
 * @param obj Configuration contactgroup
 */
contactgroup::contactgroup(const configuration::Contactgroup& obj)
    : _alias{obj.alias().empty() ? obj.contactgroup_name() : obj.alias()},
      _name{obj.contactgroup_name()} {
  assert(!_name.empty());
  // Notify event broker.
  broker_group(NEBTYPE_CONTACTGROUP_ADD, this);
}
#endif

/**
 * Assignment operator.
 *
 * @param[in] other Object to copy.
 *
 * @return This object
 */
contactgroup& contactgroup::operator=(contactgroup const& other) {
  _alias = other._alias;
  _members = other._members;
  _name = other._name;
  return *this;
}

/**
 * Destructor.
 */
contactgroup::~contactgroup() {
  _members.clear();
}

std::string const& contactgroup::get_name() const {
  return _name;
}

void contactgroup::clear_members() {
  _members.clear();
}

contact_map& contactgroup::get_members() {
  return _members;
}

const contact_map& contactgroup::get_members() const {
  return _members;
}

std::string const& contactgroup::get_alias() const {
  return _alias;
}

void contactgroup::set_alias(std::string const& alias) {
  _alias = alias;
}

std::ostream& operator<<(std::ostream& os, const contactgroup_map& obj) {
  for (contactgroup_map::const_iterator it = obj.begin(), end = obj.end();
       it != end; ++it) {
    os << it->first;
    if (std::next(it) != end)
      os << ", ";
    else
      os << "";
  }
  return os;
}

void contactgroup::resolve(uint32_t& w __attribute__((unused)), uint32_t& e) {
  uint32_t errors = 0;

  for (contact_map::iterator it = _members.begin(), end = _members.end();
       it != end; ++it) {
    /* Check members */
    if (!it->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Contact '" << it->first << "' specified in contact group '"
          << _name << "' is not defined anywhere!";
      config_logger->error(
          "Error: Contact '{}' specified in contact group '{}' is not defined "
          "anywhere!",
          it->first, _name);
      errors++;
    } else
      it->second->get_parent_groups()[_name] = this;
  }

  /* Check for illegal characters in contact group name. */
  if (contains_illegal_object_chars(const_cast<char*>(_name.c_str()))) {
    engine_logger(log_verification_error, basic)
        << "Error: The name of contact group '" << _name
        << "' contains one or more illegal characters.";
    config_logger->error(
        "Error: The name of contact group '{}' contains one or more illegal "
        "characters.",
        _name);
    errors++;
  }

  e += errors;
  if (errors)
    throw engine_error() << "Error: Cannot resolve contact group " << _name
                         << "'";
}
