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

#include "com/centreon/engine/configuration/applier/hostdependency.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"

using namespace com::centreon::engine::configuration;

/**
 *  Add new hostdependency.
 *
 *  @param[in] obj  The new host dependency to add into the monitoring
 *                  engine.
 */
void applier::hostdependency::add_object(
    const configuration::Hostdependency& obj) {
  // Check host dependency.
  if (obj.hosts().data().size() != 1 || !obj.hostgroups().data().empty() ||
      obj.dependent_hosts().data().size() != 1 ||
      !obj.dependent_hostgroups().data().empty())
    throw engine_error() << "Could not create host dependency "
                            "with multiple (dependent) host / host groups";
  if (obj.dependency_type() != DependencyKind::execution_dependency &&
      obj.dependency_type() != DependencyKind::notification_dependency)
    throw engine_error() << fmt::format(
        "Could not create unexpanded host dependency of '{}' on '{}'",
        obj.dependent_hosts().data(0), obj.hosts().data(0));

  // Logging.
  config_logger->debug(
      "Creating new host dependency of host '{}' on host '{}'.",
      obj.dependent_hosts().data(0), obj.hosts().data(0));

  uint64_t hash_key = hostdependency_key(obj);
  // Add dependency to the global configuration set.
  pb_indexed_config.mut_hostdependencies().emplace(
      hash_key, std::make_unique<Hostdependency>(obj));

  std::shared_ptr<engine::hostdependency> hd;

  if (obj.dependency_type() == DependencyKind::execution_dependency)
    // Create executon dependency.
    hd = std::make_shared<engine::hostdependency>(
        hash_key, obj.dependent_hosts().data(0), obj.hosts().data(0),
        static_cast<engine::hostdependency::types>(obj.dependency_type()),
        obj.inherits_parent(),
        static_cast<bool>(obj.execution_failure_options() & action_hd_up),
        static_cast<bool>(obj.execution_failure_options() & action_hd_down),
        static_cast<bool>(obj.execution_failure_options() &
                          action_hd_unreachable),
        static_cast<bool>(obj.execution_failure_options() & action_hd_pending),
        obj.dependency_period());
  else
    // Create notification dependency.
    hd = std::make_shared<engine::hostdependency>(
        hash_key, obj.dependent_hosts().data(0), obj.hosts().data(0),
        static_cast<engine::hostdependency::types>(obj.dependency_type()),
        obj.inherits_parent(),
        static_cast<bool>(obj.notification_failure_options() & action_hd_up),
        static_cast<bool>(obj.notification_failure_options() & action_hd_down),
        static_cast<bool>(obj.notification_failure_options() &
                          action_hd_unreachable),
        static_cast<bool>(obj.notification_failure_options() &
                          action_hd_pending),
        obj.dependency_period());

  engine::hostdependency::hostdependencies.insert(
      {obj.dependent_hosts().data(0), hd});
}

/**
 *  @brief Modify host dependency.
 *
 *  Host dependencies cannot be defined with anything else than their
 *  full content. Therefore no modification can occur.
 *
 *  @param[in] obj  Unused.
 */
void applier::hostdependency::modify_object(
    configuration::Hostdependency* old_obj [[maybe_unused]],
    const configuration::Hostdependency& new_obj [[maybe_unused]]) {
  throw engine_error()
      << "Could not modify a host dependency: Host dependency objects can "
         "only "
         "be added or removed, this is likely a software bug that you should "
         "report to Centreon Engine developers";
}

/**
 *  Remove old host dependency.
 *
 *  @param[in] idx  The index of the host dependency configuration to remove
 * from engine.
 */
void applier::hostdependency::remove_object(uint64_t hash_key) {
  // Logging.
  config_logger->debug("Removing a host dependency.");

  // Find host dependency.
  auto& obj = *pb_indexed_config.hostdependencies().at(hash_key);

  hostdependency_mmap::iterator it =
      engine::hostdependency::hostdependencies_find(
          {obj.dependent_hosts().data(0), hash_key});
  if (it != engine::hostdependency::hostdependencies.end()) {
    // Remove host dependency from its list.
    engine::hostdependency::hostdependencies.erase(it);
  }

  // Remove dependency from the global configuration set.
  pb_indexed_config.mut_hostdependencies().erase(hash_key);
}

/**
 *  Resolve a hostdependency.
 *
 *  @param[in] obj  Hostdependency object.
 */
void applier::hostdependency::resolve_object(
    const configuration::Hostdependency& obj) {
  // Logging.
  config_logger->debug("Resolving a host dependency.");

  // Find host dependency
  auto k = hostdependency_key(obj);

  auto it = engine::hostdependency::hostdependencies_find(
      {obj.dependent_hosts().data(0), k});

  if (engine::hostdependency::hostdependencies.end() == it)
    throw engine_error() << "Cannot resolve non-existing host dependency";

  // This is pure wiring: the existence of the dependent host, the master host
  // and the dependency period is validated by state_helper::resolve (single
  // home). Here we only wire the objects that exist and leave the rest unwired.
  host_map::const_iterator hit =
      host::hosts.find(obj.dependent_hosts().data(0));
  it->second->dependent_host_ptr =
      hit != host::hosts.end() ? hit->second.get() : nullptr;

  hit = host::hosts.find(obj.hosts().data(0));
  it->second->master_host_ptr =
      hit != host::hosts.end() ? hit->second.get() : nullptr;

  if (obj.dependency_period().empty())
    it->second->dependency_period_ptr = nullptr;
  else {
    timeperiod_map::const_iterator pit{
        ::timeperiods.find(obj.dependency_period())};
    it->second->dependency_period_ptr =
        pit != ::timeperiods.end() ? pit->second.get() : nullptr;
  }
}
