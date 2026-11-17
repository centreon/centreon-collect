/**
 * Copyright 2011-2024 Centreon
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
#include "com/centreon/engine/hostgroup.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

hostgroup_map com::centreon::engine::hostgroup::hostgroups;

/*
 *  Constructor
 *
 *  @param[in] id         Host group id.
 *  @param[in] name       Host group name.
 *  @param[in] alias      Host group alias.
 *  @param[in] notes      Notes.
 *  @param[in] notes_url  URL.
 *  @param[in] action_url Action URL.
 */
hostgroup::hostgroup(uint64_t id,
                     std::string const& name,
                     std::string const& alias,
                     std::string const& notes,
                     std::string const& notes_url,
                     std::string const& action_url)
    : _id{id},
      _group_name{name},
      _alias{alias},
      _notes{notes},
      _notes_url{notes_url},
      _action_url{action_url} {
  // Make sure we have the data we need.
  if (name.empty()) {
    engine_logger(log_config_error, basic) << "Error: Hostgroup name is NULL";
    config_logger->error("Error: Hostgroup name is NULL");
    throw(engine_error() << "Could not register host group '" << name << "'");
  }

  // Check if the host group already exist.
  hostgroup_map::const_iterator found(hostgroup::hostgroups.find(name));
  if (found != hostgroup::hostgroups.end()) {
    engine_logger(log_config_error, basic)
        << "Error: Hostgroup '" << name << "' has already been defined";
    config_logger->error("Error: Hostgroup '{}' has already been defined",
                         name);
    throw(engine_error() << "Could not register host group '" << name << "'");
  }
}

uint64_t hostgroup::get_id() const {
  return _id;
}

void hostgroup::set_id(uint64_t id) {
  _id = id;
}

std::string const& hostgroup::get_group_name() const {
  return _group_name;
}

std::string const& hostgroup::get_alias() const {
  return _alias;
}

std::string const& hostgroup::get_notes() const {
  return _notes;
}

std::string const& hostgroup::get_notes_url() const {
  return _notes_url;
}

std::string const& hostgroup::get_action_url() const {
  return _action_url;
}

void hostgroup::set_group_name(std::string const& group_name) {
  _group_name = group_name;
}

void hostgroup::set_alias(std::string const& alias) {
  _alias = alias;
}

void hostgroup::set_notes(std::string const& notes) {
  _notes = notes;
}

void hostgroup::set_notes_url(std::string const& notes_url) {
  _notes_url = notes_url;
}

void hostgroup::set_action_url(std::string const& action_url) {
  _action_url = action_url;
}

/**
 *  Dump hostgroup content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The hostgroup to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::hostgroup const& obj) {
  os << "hostgroup {\n"
        "  group_name: "
     << obj.get_group_name()
     << "\n"
        "  alias:      "
     << obj.get_alias()
     << "\n"
        "  members:    "
     << obj.members
     << "\n"
        "  notes:      "
     << obj.get_notes()
     << "\n"
        "  notes_url:  "
     << obj.get_notes_url()
     << "\n"
        "  action_url: "
     << obj.get_action_url()
     << "\n"
        "}\n";
  return (os);
}

void hostgroup::resolve(uint32_t& w [[maybe_unused]], uint32_t& e) {
  uint32_t errors = 0;

  // Check all group members.
  for (host_map_unsafe::iterator it = members.begin(), end = members.end();
       it != end; ++it) {
    host_map::const_iterator it_host{host::hosts.find(it->first)};
    if (it_host == host::hosts.end() || !it_host->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Host '" << it->first << "' specified in host group '"
          << get_group_name() << "' is not defined anywhere!";
      config_logger->error(
          "Error: Host '{}' specified in host group '{}' is not defined "
          "anywhere!",
          it->first, get_group_name());
      it->second = nullptr;
      errors++;
    }
    // Save a pointer to this hostgroup for faster host/group
    // membership lookups later.
    else {
      // Update or add of group for name
      if (it_host->second.get() != it->second) {
        // Notify event broker.
        broker_group_member(NEBTYPE_HOSTGROUPMEMBER_ADD, it_host->second.get(),
                            this);
      }
      it_host->second->get_parent_groups().push_back(this);
      // Save host pointer for later.
      it->second = it_host->second.get();
    }
  }

  // Check for illegal characters in hostgroup name.
  if (contains_illegal_object_chars(get_group_name().c_str())) {
    engine_logger(log_verification_error, basic)
        << "Error: The name of hostgroup '" << get_group_name()
        << "' contains one or more illegal characters.";
    config_logger->error(
        "Error: The name of hostgroup '{}' contains one or more illegal "
        "characters.",
        get_group_name());
    errors++;
  }

  if (errors) {
    e += errors;
    throw engine_error() << "Cannot resolve host group '" << get_group_name()
                         << "'";
  }
}
