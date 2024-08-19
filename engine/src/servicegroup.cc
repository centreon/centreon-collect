/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
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
#include "com/centreon/engine/servicegroup.hh"
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

servicegroup_map servicegroup::servicegroups;

/**
 *  Create a new service group
 *
 *  @param[in] id         Group id.
 *  @param[in] name       Group name.
 *  @param[in] alias      Group alias.
 *  @param[in] notes      Notes.
 *  @param[in] notes_url  URL.
 *  @param[in] action_url Action URL.
 *
 */
servicegroup::servicegroup(uint64_t id,
                           std::string const& group_name,
                           std::string const& alias,
                           std::string const& notes,
                           std::string const& notes_url,
                           std::string const& action_url)
    : _id{id},
      _group_name{group_name},
      _alias{alias.empty() ? group_name : alias},
      _notes{notes},
      _notes_url{notes_url},
      _action_url{action_url} {
  // Check if the service group already exist.
  if (is_servicegroup_exist(group_name)) {
    engine_logger(log_config_error, basic)
        << "Error: Servicegroup '" << group_name
        << "' has already been defined";
    config_logger->error("Error: Servicegroup '{}' has already been defined",
                         group_name);
    throw engine_error() << "Could not register service group '" << group_name
                         << "'";
  }
}

uint64_t servicegroup::get_id() const {
  return _id;
}

void servicegroup::set_id(uint64_t id) {
  _id = id;
}

std::string const& servicegroup::get_group_name() const {
  return _group_name;
}

void servicegroup::set_group_name(std::string const& group_name) {
  _group_name = group_name;
}

std::string const& servicegroup::get_alias() const {
  return _alias;
}

void servicegroup::set_alias(std::string const& alias) {
  _alias = alias;
}

std::string const& servicegroup::get_notes() const {
  return _notes;
}

void servicegroup::set_notes(std::string const& notes) {
  _notes = notes;
}

std::string const& servicegroup::get_notes_url() const {
  return _notes_url;
}

void servicegroup::set_notes_url(std::string const& notes_url) {
  _notes_url = notes_url;
}

std::string const& servicegroup::get_action_url() const {
  return _action_url;
}

void servicegroup::set_action_url(std::string const& action_url) {
  _action_url = action_url;
}

/**
 *  Dump servicegroup content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The servicegroup to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, servicegroup const& obj) {
  os << "servicegroup {\n"
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
  return os;
}

/**
 *  Get if servicegroup exist.
 *
 *  @param[in] name The servicegroup name.
 *
 *  @return True if the servicegroup is found, otherwise false.
 */
bool engine::is_servicegroup_exist(std::string const& name) throw() {
  servicegroup_map::const_iterator it{servicegroup::servicegroups.find(name)};
  return it != servicegroup::servicegroups.end();
}

void servicegroup::resolve(uint32_t& w [[maybe_unused]], uint32_t& e) {
  uint32_t errors = 0;

  // Check all group members.
  for (service_map_unsafe::iterator it(members.begin()), end(members.end());
       it != end; ++it) {
    service_map::const_iterator found(service::services.find(it->first));

    if (found == service::services.end() || !found->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Service '" << it->first.second << "' on host '"
          << it->first.first << "' specified in service group '" << _group_name
          << "' is not defined anywhere!";
      config_logger->error(
          "Error: Service '{}' on host '{}' specified in service group '{}' is "
          "not defined anywhere!",
          it->first.second, it->first.first, _group_name);
      errors++;
    }
    // Save a pointer to this servicegroup for faster service/group
    // membership lookups later.
    else {
      // Update or add of group for name
      if (found->second.get() != it->second) {
        // Notify event broker.
        broker_group_member(NEBTYPE_SERVICEGROUPMEMBER_ADD, found->second.get(),
                            this);
      }
      found->second->get_parent_groups().push_back(this);
      // Save service pointer for later.
      it->second = found->second.get();
    }
  }

  // Check for illegal characters in servicegroup name.
  if (contains_illegal_object_chars(_group_name.c_str())) {
    engine_logger(log_verification_error, basic)
        << "Error: The name of servicegroup '" << _group_name
        << "' contains one or more illegal characters.";
    config_logger->error(
        "Error: The name of servicegroup '{}' contains one or more illegal "
        "characters.",
        _group_name);
    errors++;
  }

  if (errors) {
    e += errors;
    throw engine_error() << "Cannot resolve servicegroup " << _group_name;
  }
}
