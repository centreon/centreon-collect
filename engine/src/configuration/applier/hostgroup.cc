/**
 * Copyright 2011-2013,2015,2017-2024 Centreon
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

#include "com/centreon/engine/configuration/applier/hostgroup.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/deleter/listmember.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine::configuration;

#ifdef LEGACY_CONF
/**
 *  Add new hostgroup.
 *
 *  @param[in] obj  The new hostgroup to add into the monitoring engine.
 */
void applier::hostgroup::add_object(configuration::hostgroup const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new hostgroup '" << obj.hostgroup_name() << "'.";
  config_logger->debug("Creating new hostgroup '{}'.", obj.hostgroup_name());

  // Add host group to the global configuration state.
  config->hostgroups().insert(obj);

  // Create host group.
  auto hg = std::make_shared<com::centreon::engine::hostgroup>(
      obj.hostgroup_id(), obj.hostgroup_name(), obj.alias(), obj.notes(),
      obj.notes_url(), obj.action_url());

  // Add new items to the configuration state.
  engine::hostgroup::hostgroups.insert({hg->get_group_name(), hg});

  // Apply resolved hosts on hostgroup.
  for (set_string::const_iterator it(obj.members().begin()),
       end(obj.members().end());
       it != end; ++it)
    hg->members.insert({*it, nullptr});

  // Notify event broker.
  broker_group(NEBTYPE_HOSTGROUP_ADD, hg.get());
}
#else
/**
 *  Add new hostgroup.
 *
 *  @param[in] obj  The new hostgroup to add into the monitoring engine.
 */
void applier::hostgroup::add_object(const configuration::Hostgroup& obj) {
  // Logging.
  config_logger->debug("Creating new hostgroup '{}'.", obj.hostgroup_name());

  // Add host group to the global configuration state.
  auto* new_obj = pb_config.add_hostgroups();
  new_obj->CopyFrom(obj);

  // Create host group.
  auto hg = std::make_shared<com::centreon::engine::hostgroup>(
      obj.hostgroup_id(), obj.hostgroup_name(), obj.alias(), obj.notes(),
      obj.notes_url(), obj.action_url());

  // Add new items to the configuration state.
  engine::hostgroup::hostgroups.insert({hg->get_group_name(), hg});

  // Notify event broker.
  broker_group(NEBTYPE_HOSTGROUP_ADD, hg.get());

  // Apply resolved hosts on hostgroup.
  for (auto& h : obj.members().data())
    hg->members.insert({h, nullptr});
}
#endif

#ifdef LEGACY_CONF
/**
 *  Expand all host groups.
 *
 *  @param[in,out] s  State being applied.
 */
void applier::hostgroup::expand_objects(configuration::state& s) {
  // Resolve groups.
  _resolved.clear();
  for (configuration::set_hostgroup::const_iterator it(s.hostgroups().begin()),
       end(s.hostgroups().end());
       it != end; ++it)
    _resolve_members(s, *it);

  // Save resolved groups in the configuration set.
  s.hostgroups().clear();
  for (resolved_set::const_iterator it(_resolved.begin()), end(_resolved.end());
       it != end; ++it)
    s.hostgroups().insert(it->second);
}
#else
/**
 *  Expand all host groups.
 *
 *  @param[in,out] s  State being applied.
 */
void applier::hostgroup::expand_objects(configuration::State& s
                                        [[maybe_unused]]) {}
#endif

#ifdef LEGACY_CONF
/**
 *  Modified hostgroup.
 *
 *  @param[in] obj  The new hostgroup to modify into the monitoring
 *                  engine.
 */
void applier::hostgroup::modify_object(configuration::hostgroup const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Modifying hostgroup '" << obj.hostgroup_name() << "'";
  config_logger->debug("Modifying hostgroup '{}'", obj.hostgroup_name());

  // Find old configuration.
  set_hostgroup::iterator it_cfg(config->hostgroups_find(obj.key()));
  if (it_cfg == config->hostgroups().end())
    throw engine_error() << "Could not modify non-existing "
                         << "host group '" << obj.hostgroup_name() << "'";

  // Find host group object.
  hostgroup_map::iterator it_obj(engine::hostgroup::hostgroups.find(obj.key()));
  if (it_obj == engine::hostgroup::hostgroups.end())
    throw engine_error() << "Could not modify non-existing "
                         << "host group object '" << obj.hostgroup_name()
                         << "'";

  // Update the global configuration set.
  configuration::hostgroup old_cfg(*it_cfg);
  config->hostgroups().erase(it_cfg);
  config->hostgroups().insert(obj);

  it_obj->second->set_action_url(obj.action_url());
  it_obj->second->set_alias(obj.alias());
  it_obj->second->set_notes(obj.notes());
  it_obj->second->set_notes_url(obj.notes_url());
  it_obj->second->set_id(obj.hostgroup_id());

  // Were members modified ?
  if (obj.members() != old_cfg.members()) {
    // Delete all old host group members.
    for (host_map_unsafe::iterator it(it_obj->second->members.begin()),
         end(it_obj->second->members.end());
         it != end; ++it) {
      broker_group_member(NEBTYPE_HOSTGROUPMEMBER_DELETE, it->second,
                          it_obj->second.get());
    }
    it_obj->second->members.clear();

    for (set_string::const_iterator it(obj.members().begin()),
         end(obj.members().end());
         it != end; ++it)
      it_obj->second->members.insert({*it, nullptr});
  }

  // Notify event broker.
  broker_group(NEBTYPE_HOSTGROUP_UPDATE, it_obj->second.get());
}
#else
/**
 *  Modified hostgroup.
 *
 *  @param[in] obj  The new hostgroup to modify into the monitoring
 *                  engine.
 */
void applier::hostgroup::modify_object(
    configuration::Hostgroup* old_obj,
    const configuration::Hostgroup& new_obj) {
  // Logging.
  config_logger->debug("Modifying hostgroup '{}'", old_obj->hostgroup_name());

  // Find host group object.
  hostgroup_map::iterator it_obj =
      engine::hostgroup::hostgroups.find(old_obj->hostgroup_name());
  if (it_obj == engine::hostgroup::hostgroups.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing host group object '{}'",
        old_obj->hostgroup_name());

  it_obj->second->set_action_url(new_obj.action_url());
  it_obj->second->set_alias(new_obj.alias());
  it_obj->second->set_notes(new_obj.notes());
  it_obj->second->set_notes_url(new_obj.notes_url());
  it_obj->second->set_id(new_obj.hostgroup_id());

  // Were members modified ?
  if (!MessageDifferencer::Equals(new_obj.members(), old_obj->members())) {
    // Delete all old host group members.
    for (host_map_unsafe::iterator it(it_obj->second->members.begin()),
         end(it_obj->second->members.end());
         it != end; ++it) {
      broker_group_member(NEBTYPE_HOSTGROUPMEMBER_DELETE, it->second,
                          it_obj->second.get());
    }
    it_obj->second->members.clear();

    for (auto it = new_obj.members().data().begin(),
              end = new_obj.members().data().end();
         it != end; ++it)
      it_obj->second->members.insert({*it, nullptr});
  }

  old_obj->CopyFrom(new_obj);
  // Notify event broker.
  broker_group(NEBTYPE_HOSTGROUP_UPDATE, it_obj->second.get());
}
#endif

#ifdef LEGACY_CONF
/**
 *  Remove old hostgroup.
 *
 *  @param[in] obj  The new hostgroup to remove from the monitoring
 *                  engine.
 */
void applier::hostgroup::remove_object(configuration::hostgroup const& obj) {
  // Logging.
  config_logger->debug("Removing host group '{}'", obj.hostgroup_name());

  // Find host group.
  hostgroup_map::iterator it{engine::hostgroup::hostgroups.find(obj.key())};
  if (it != engine::hostgroup::hostgroups.end()) {
    engine::hostgroup* grp(it->second.get());

    // Notify event broker.
    broker_group(NEBTYPE_HOSTGROUP_DELETE, grp);

    // Erase host group object (will effectively delete the object).
    engine::hostgroup::hostgroups.erase(it);
  }

  // Remove host group from the global configuration set.
  config->hostgroups().erase(obj);
}
#else
/**
 *  Remove old hostgroup.
 *
 *  @param[in] obj  The new hostgroup to remove from the monitoring
 *                  engine.
 */
void applier::hostgroup::remove_object(ssize_t idx) {
  const Hostgroup& obj = pb_config.hostgroups(idx);
  // Logging.
  config_logger->debug("Removing host group '{}'", obj.hostgroup_name());

  // Find host group.
  hostgroup_map::iterator it =
      engine::hostgroup::hostgroups.find(obj.hostgroup_name());
  if (it != engine::hostgroup::hostgroups.end()) {
    engine::hostgroup* grp(it->second.get());

    // Notify event broker.
    broker_group(NEBTYPE_HOSTGROUP_DELETE, grp);

    // Erase host group object (will effectively delete the object).
    engine::hostgroup::hostgroups.erase(it);
  }

  // Remove host group from the global configuration set.
  pb_config.mutable_hostgroups()->DeleteSubrange(idx, 1);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Resolve a host group.
 *
 *  @param[in] obj  Object to resolved.
 */
void applier::hostgroup::resolve_object(configuration::hostgroup const& obj,
                                        error_cnt& err) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving host group '" << obj.hostgroup_name() << "'";
  config_logger->debug("Resolving host group '{}'", obj.hostgroup_name());

  // Find host group.
  hostgroup_map::iterator it{engine::hostgroup::hostgroups.find(obj.key())};
  if (it == engine::hostgroup::hostgroups.end())
    throw engine_error() << "Cannot resolve non-existing "
                         << "host group '" << obj.hostgroup_name() << "'";

  // Resolve host group.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#else
/**
 *  Resolve a host group.
 *
 *  @param[in] obj  Object to resolved.
 */
void applier::hostgroup::resolve_object(const configuration::Hostgroup& obj,
                                        error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving host group '{}'", obj.hostgroup_name());

  // Find host group.
  hostgroup_map::iterator it =
      engine::hostgroup::hostgroups.find(obj.hostgroup_name());
  if (it == engine::hostgroup::hostgroups.end())
    throw engine_error() << fmt::format(
        "Cannot resolve non-existing host group '{}'", obj.hostgroup_name());

  // Resolve host group.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Resolve members of a host group.
 *
 *  @param[in]     s    Configuration being applied.
 *  @param[in,out] obj  Hostgroup object.
 */
void applier::hostgroup::_resolve_members(configuration::state& s
                                          __attribute__((unused)),
                                          configuration::hostgroup const& obj) {
  // Only process if hostgroup has not been resolved already.
  if (_resolved.find(obj.key()) == _resolved.end()) {
    // Logging.
    engine_logger(logging::dbg_config, logging::more)
        << "Resolving members of host group '" << obj.hostgroup_name() << "'";
    config_logger->debug("Resolving members of host group '{}'",
                         obj.hostgroup_name());

    // Mark object as resolved.
    configuration::hostgroup& resolved_obj(_resolved[obj.key()]);

    // Insert base members.
    resolved_obj = obj;
  }
}
#endif
