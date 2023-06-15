/**
* Copyright 2011-2013,2015,2017,2023 Centreon
*
* This file is part of Centreon Engine.
*
* Centreon Engine is free software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* as published by the Free Software Foundation.
*
* Centreon Engine is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Centreon Engine. If not, see
* <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/engine/configuration/applier/servicegroup.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "configuration/state-generated.pb.h"

using namespace com::centreon::engine::configuration;

/**
 *  Add new servicegroup.
 *
 *  @param[in] obj  The new servicegroup to add into the monitoring
 *                  engine.
 */
void applier::servicegroup::add_object(configuration::servicegroup const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new servicegroup '" << obj.servicegroup_name() << "'";
  log_v2::config()->debug("Creating new servicegroup '{}'",
                          obj.servicegroup_name());

  // Add service group to the global configuration set.
  config->servicegroups().insert(obj);

  // Create servicegroup.
  auto sg = std::make_shared<engine::servicegroup>(
      obj.servicegroup_id(), obj.servicegroup_name(), obj.alias(), obj.notes(),
      obj.notes_url(), obj.action_url());

  // Add  new items to the list.
  engine::servicegroup::servicegroups.insert({sg->get_group_name(), sg});

  // Add servicegroup id to the other props.
  sg->set_id(obj.servicegroup_id());

  // Apply resolved services on servicegroup.
  for (set_pair_string::const_iterator it(obj.members().begin()),
       end(obj.members().end());
       it != end; ++it)
    sg->members[{it->first, it->second}] = nullptr;

  // Notify event broker.
  broker_group(NEBTYPE_SERVICEGROUP_ADD, sg.get());
}

/**
 * @brief Add a new Service group given as a Protobuf object.
 *
 * @param obj The new service group to add into the monitoring engine.
 */
void applier::servicegroup::add_object(const configuration::Servicegroup& obj) {
  // Logging.
  log_v2::config()->debug("Creating new servicegroup '{}'",
                          obj.servicegroup_name());

  // Add service group to the global configuration set.
  auto* new_obj = pb_config.add_servicegroups();
  new_obj->CopyFrom(obj);

  // Create servicegroup.
  auto sg = std::make_shared<engine::servicegroup>(
      obj.servicegroup_id(), obj.servicegroup_name(), obj.alias(), obj.notes(),
      obj.notes_url(), obj.action_url());

  // Add  new items to the list.
  engine::servicegroup::servicegroups.insert({sg->get_group_name(), sg});

  // Add servicegroup id to the other props.
  sg->set_id(obj.servicegroup_id());

  // Notify event broker.
  broker_group(NEBTYPE_SERVICEGROUP_ADD, sg.get());

  // Apply resolved services on servicegroup.
  for (auto& m : obj.members().data())
    sg->members[{m.first(), m.second()}] = nullptr;
}

/**
 * @brief Resolve the servicegroup sg_conf, so for each of its servicegroup
 * member, we get all its service members and copy them into sg_conf.
 * Once this done, resolved is completed with the name of sg_conf.
 *
 * @param s The full configuration for this poller.
 * @param sg_conf The servicegroup configuration to resolve.
 * @param resolved The set of servicegroup configurations already resolved.
 * @param sg_by_name A const table of servicegroup configurations indexed by
 * their name.
 */
void applier::servicegroup::_resolve_members(
    configuration::State& s,
    configuration::Servicegroup* sg_conf,
    absl::flat_hash_set<absl::string_view>& resolved,
    const absl::flat_hash_map<absl::string_view, configuration::Servicegroup*>&
        sg_by_name) {
  for (auto& sgm : sg_conf->servicegroup_members().data()) {
    configuration::Servicegroup* sgm_conf =
        sg_by_name.at(absl::string_view(sgm));
    if (sgm_conf == nullptr)
      throw engine_error() << fmt::format(
          "Could not add non-existing service group member '{}' to service "
          "group '{}'",
          sgm, sg_conf->servicegroup_name());
    if (!resolved.contains(sgm_conf->servicegroup_name()))
      _resolve_members(s, sgm_conf, resolved, sg_by_name);

    for (auto& sm : sgm_conf->members().data()) {
      fill_pair_string_group(sg_conf->mutable_members(), sm.first(),
                             sm.second());
    }
  }
  sg_conf->clear_servicegroup_members();
  resolved.emplace(sg_conf->servicegroup_name());
}

/**
 *  Expand all service groups.
 *
 *  @param[in,out] s  State being applied.
 */
void applier::servicegroup::expand_objects(configuration::State& s) {
  // This set stores resolved service groups.
  absl::flat_hash_set<absl::string_view> resolved;

  // Here, we store each Servicegroup pointer by its name.
  absl::flat_hash_map<absl::string_view, configuration::Servicegroup*>
      sg_by_name;
  for (auto& sg_conf : *s.mutable_servicegroups())
    sg_by_name[sg_conf.servicegroup_name()] = &sg_conf;

  // Each servicegroup can contain servicegroups, that is to mean the services
  // in the sub servicegroups are also in our servicegroup.
  // So, we iterate through all the servicegroups defined in the configuration,
  // and for each one if it has servicegroup members, we fill its service
  // members with theirs and then we clear the servicegroup members. At that
  // step, a servicegroup is considered as resolved.
  for (auto& sg_conf : *s.mutable_servicegroups()) {
    if (!resolved.contains(sg_conf.servicegroup_name())) {
      _resolve_members(s, &sg_conf, resolved, sg_by_name);
    }
  }
}

/**
 *  Expand all service groups.
 *
 *  @param[in,out] s  State being applied.
 */
void applier::servicegroup::expand_objects(configuration::state& s) {
  // Resolve groups.
  _resolved.clear();
  for (configuration::set_servicegroup::const_iterator
           it(s.servicegroups().begin()),
       end(s.servicegroups().end());
       it != end; ++it)
    _resolve_members(*it, s);

  // Save resolved groups in the configuration set.
  s.servicegroups().clear();
  for (resolved_set::const_iterator it(_resolved.begin()), end(_resolved.end());
       it != end; ++it)
    s.servicegroups().insert(it->second);
}

/**
 *  Modify servicegroup.
 *
 *  @param[in] obj  The new servicegroup to modify into the monitoring
 *                  engine.
 */
void applier::servicegroup::modify_object(
    configuration::Servicegroup* to_modify,
    const configuration::Servicegroup& new_object) {
  // Logging.
  log_v2::config()->debug("Modifying servicegroup '{}'",
                          to_modify->servicegroup_name());

  // Find service group object.
  servicegroup_map::iterator it_obj =
      engine::servicegroup::servicegroups.find(to_modify->servicegroup_name());

  if (it_obj == engine::servicegroup::servicegroups.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing service group object '{}'",
        to_modify->servicegroup_name());
  engine::servicegroup* sg = it_obj->second.get();

  // Modify properties.
  sg->set_id(new_object.servicegroup_id());
  sg->set_action_url(new_object.action_url());
  sg->set_alias(new_object.alias().empty() ? new_object.servicegroup_name()
                                           : new_object.alias());
  sg->set_notes(new_object.notes());
  sg->set_notes_url(new_object.notes_url());

  // Were members modified ?
  if (!MessageDifferencer::Equals(new_object.members(), to_modify->members())) {
    // Delete all old service group members.
    for (service_map_unsafe::iterator it = it_obj->second->members.begin(),
                                      end = it_obj->second->members.end();
         it != end; ++it) {
      broker_group_member(NEBTYPE_SERVICEGROUPMEMBER_DELETE, it->second, sg);
    }
    it_obj->second->members.clear();

    // Create new service group members.
    for (auto& m : new_object.members().data())
      sg->members[{m.first(), m.second()}] = nullptr;
  }

  // Update the global configuration set.
  to_modify->CopyFrom(new_object);

  // Notify event broker.
  broker_group(NEBTYPE_SERVICEGROUP_UPDATE, sg);
}

/**
 *  Modify servicegroup.
 *
 *  @param[in] obj  The new servicegroup to modify into the monitoring
 *                  engine.
 */
void applier::servicegroup::modify_object(
    configuration::servicegroup const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Modifying servicegroup '" << obj.servicegroup_name() << "'";
  log_v2::config()->debug("Modifying servicegroup '{}'",
                          obj.servicegroup_name());

  // Find old configuration.
  set_servicegroup::iterator it_cfg(config->servicegroups_find(obj.key()));
  if (it_cfg == config->servicegroups().end())
    throw(engine_error() << "Could not modify non-existing "
                         << "service group '" << obj.servicegroup_name()
                         << "'");

  // Find service group object.
  servicegroup_map::iterator it_obj{
      engine::servicegroup::servicegroups.find(obj.key())};

  if (it_obj == engine::servicegroup::servicegroups.end())
    throw(engine_error() << "Could not modify non-existing "
                         << "service group object '" << obj.servicegroup_name()
                         << "'");
  engine::servicegroup* sg(it_obj->second.get());

  // Update the global configuration set.
  configuration::servicegroup old_cfg(*it_cfg);
  config->servicegroups().erase(it_cfg);
  config->servicegroups().insert(obj);

  // Modify properties.
  sg->set_id(obj.servicegroup_id());
  sg->set_action_url(obj.action_url());
  sg->set_alias((obj.alias().empty() ? obj.servicegroup_name() : obj.alias()));
  sg->set_notes(obj.notes());
  sg->set_notes_url(obj.notes_url());

  // Were members modified ?
  if (obj.members() != old_cfg.members()) {
    // Delete all old service group members.
    for (service_map_unsafe::iterator it(it_obj->second->members.begin()),
         end(it_obj->second->members.end());
         it != end; ++it) {
      broker_group_member(NEBTYPE_SERVICEGROUPMEMBER_DELETE, it->second, sg);
    }
    it_obj->second->members.clear();

    // Create new service group members.
    for (set_pair_string::const_iterator it(obj.members().begin()),
         end(obj.members().end());
         it != end; ++it)
      sg->members[{it->first, it->second}] = nullptr;
  }

  // Notify event broker.
  broker_group(NEBTYPE_SERVICEGROUP_UPDATE, sg);
}

/**
 *  Remove old servicegroup.
 *
 *  @param[in] idw  Index of the servicegroup to remove in the configuration.
 */
void applier::servicegroup::remove_object(ssize_t idx) {
  // Logging.
  auto obj = pb_config.servicegroups(idx);
  log_v2::config()->debug("Removing servicegroup '{}'",
                          obj.servicegroup_name());

  // Find service group.
  servicegroup_map::iterator it =
      engine::servicegroup::servicegroups.find(obj.servicegroup_name());
  if (it != engine::servicegroup::servicegroups.end()) {
    // Notify event broker.
    broker_group(NEBTYPE_SERVICEGROUP_DELETE, it->second.get());

    // Remove service dependency from its list.
    engine::servicegroup::servicegroups.erase(it);
  }

  // Remove service group from the global configuration state.
  pb_config.mutable_servicegroups()->DeleteSubrange(idx, 1);
}

/**
 *  Remove old servicegroup.
 *
 *  @param[in] obj  The new servicegroup to remove from the monitoring
 *                  engine.
 */
void applier::servicegroup::remove_object(
    configuration::servicegroup const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing servicegroup '" << obj.servicegroup_name() << "'";
  log_v2::config()->debug("Removing servicegroup '{}'",
                          obj.servicegroup_name());

  // Find service group.
  servicegroup_map::iterator it{
      engine::servicegroup::servicegroups.find(obj.key())};
  if (it != engine::servicegroup::servicegroups.end()) {
    // Notify event broker.
    broker_group(NEBTYPE_SERVICEGROUP_DELETE, it->second.get());

    // Remove service dependency from its list.
    engine::servicegroup::servicegroups.erase(it);
  }

  // Remove service group from the global configuration state.
  config->servicegroups().erase(obj);
}

/**
 *  Resolve a servicegroup.
 *
 *  @param[in,out] obj  Servicegroup object.
 */
void applier::servicegroup::resolve_object(
    configuration::servicegroup const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing service group '" << obj.servicegroup_name() << "'";
  log_v2::config()->debug("Removing service group '{}'",
                          obj.servicegroup_name());

  // Find service group.
  servicegroup_map::const_iterator it{
      engine::servicegroup::servicegroups.find(obj.key())};
  if (it == engine::servicegroup::servicegroups.end())
    throw engine_error() << "Cannot resolve non-existing "
                         << "service group '" << obj.servicegroup_name() << "'";

  // Resolve service group.
  it->second->resolve(config_warnings, config_errors);
}

void applier::servicegroup::resolve_object(
    const configuration::Servicegroup& obj) {
  // Logging.
  log_v2::config()->debug("Removing service group '{}'",
                          obj.servicegroup_name());

  // Find service group.
  servicegroup_map::const_iterator it =
      engine::servicegroup::servicegroups.find(obj.servicegroup_name());
  if (it == engine::servicegroup::servicegroups.end())
    throw engine_error() << fmt::format(
        "Cannot resolve non-existing service group '{}'",
        obj.servicegroup_name());

  // Resolve service group.
  it->second->resolve(config_warnings, config_errors);
}

/**
 *  Resolve members of a service group.
 *
 *  @param[in,out] obj  Service group object.
 *  @param[in]     s    Configuration being applied.
 */
void applier::servicegroup::_resolve_members(
    configuration::servicegroup const& obj,
    configuration::state const& s) {
  // Only process if servicegroup has not been resolved already.
  if (_resolved.find(obj.key()) == _resolved.end()) {
    // Logging.
    engine_logger(logging::dbg_config, logging::more)
        << "Resolving members of service group '" << obj.servicegroup_name()
        << "'";
    log_v2::config()->debug("Resolving members of service group '{}'",
                            obj.servicegroup_name());

    // Mark object as resolved.
    configuration::servicegroup& resolved_obj(_resolved[obj.key()]);

    // Insert base members.
    resolved_obj = obj;
    resolved_obj.servicegroup_members().clear();

    // Add servicegroup members.
    for (set_string::const_iterator it(obj.servicegroup_members().begin()),
         end(obj.servicegroup_members().end());
         it != end; ++it) {
      // Find servicegroup entry.
      set_servicegroup::iterator it2(s.servicegroups_find(*it));
      if (it2 == s.servicegroups().end())
        throw(engine_error()
              << "Could not add non-existing service group member '" << *it
              << "' to service group '" << obj.servicegroup_name() << "'");

      // Resolve servicegroup member.
      _resolve_members(*it2, s);

      // Add servicegroup member members to members.
      for (set_pair_string::const_iterator it3(it2->members().begin()),
           end3(it2->members().end());
           it3 != end3; ++it3)
        resolved_obj.members().insert(*it3);
    }
  }
}
