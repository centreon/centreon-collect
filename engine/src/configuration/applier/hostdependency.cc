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
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine::configuration;

#ifdef LEGACY_CONF
/**
 *  Add new hostdependency.
 *
 *  @param[in] obj  The new host dependency to add into the monitoring
 *                  engine.
 */
void applier::hostdependency::add_object(
    configuration::hostdependency const& obj) {
  // Check host dependency.
  if ((obj.hosts().size() != 1) || !obj.hostgroups().empty() ||
      (obj.dependent_hosts().size() != 1) ||
      !obj.dependent_hostgroups().empty())
    throw engine_error() << "Could not create host dependency "
                            "with multiple (dependent) host / host groups";
  if ((obj.dependency_type() !=
       configuration::hostdependency::execution_dependency) &&
      (obj.dependency_type() !=
       configuration::hostdependency::notification_dependency))
    throw engine_error() << "Could not create unexpanded "
                         << "host dependency of '"
                         << *obj.dependent_hosts().begin() << "' on '"
                         << *obj.hosts().begin() << "'";

  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new host dependency of host '"
      << *obj.dependent_hosts().begin() << "' on host '" << *obj.hosts().begin()
      << "'.";
  config_logger->debug(
      "Creating new host dependency of host '{}' on host '{}'.",
      *obj.dependent_hosts().begin(), *obj.hosts().begin());

  // Add dependency to the global configuration set.
  config->hostdependencies().insert(obj);

  std::shared_ptr<engine::hostdependency> hd;

  if (obj.dependency_type() ==
      configuration::hostdependency::execution_dependency)
    // Create executon dependency.
    hd = std::make_shared<engine::hostdependency>(
        hostdependency_key(obj), *obj.dependent_hosts().begin(),
        *obj.hosts().begin(),
        static_cast<engine::hostdependency::types>(obj.dependency_type()),
        obj.inherits_parent(),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::hostdependency::up),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::hostdependency::down),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::hostdependency::unreachable),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::hostdependency::pending),
        obj.dependency_period());
  else
    // Create notification dependency.
    hd = std::make_shared<engine::hostdependency>(
        hostdependency_key(obj), *obj.dependent_hosts().begin(),
        *obj.hosts().begin(),
        static_cast<engine::hostdependency::types>(obj.dependency_type()),
        obj.inherits_parent(),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::hostdependency::up),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::hostdependency::down),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::hostdependency::unreachable),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::hostdependency::pending),
        obj.dependency_period());

  engine::hostdependency::hostdependencies.insert(
      {*obj.dependent_hosts().begin(), hd});

  broker_adaptive_dependency_data(NEBTYPE_HOSTDEPENDENCY_ADD, hd.get());
}
#else
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

  // Add dependency to the global configuration set.
  auto* new_obj = pb_config.add_hostdependencies();
  new_obj->CopyFrom(obj);

  std::shared_ptr<engine::hostdependency> hd;

  if (obj.dependency_type() == DependencyKind::execution_dependency)
    // Create executon dependency.
    hd = std::make_shared<engine::hostdependency>(
        configuration::hostdependency_key(obj), obj.dependent_hosts().data(0),
        obj.hosts().data(0),
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
        hostdependency_key(obj), obj.dependent_hosts().data(0),
        obj.hosts().data(0),
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

  broker_adaptive_dependency_data(NEBTYPE_HOSTDEPENDENCY_ADD, hd.get());
}
#endif

#ifdef LEGACY_CONF
/**
 *  Expand host dependencies.
 *
 *  @param[in,out] s  Configuration being applied.
 */
void applier::hostdependency::expand_objects(configuration::state& s) {
  // Browse all dependencies.
  configuration::set_hostdependency expanded;
  for (configuration::set_hostdependency::const_iterator
           it_dep(s.hostdependencies().begin()),
       end_dep(s.hostdependencies().end());
       it_dep != end_dep; ++it_dep) {
    // Expand host dependency instances.
    if ((it_dep->hosts().size() != 1) || !it_dep->hostgroups().empty() ||
        (it_dep->dependent_hosts().size() != 1) ||
        !it_dep->dependent_hostgroups().empty() ||
        (it_dep->dependency_type() == configuration::hostdependency::unknown)) {
      // Expanded depended hosts.
      set_string depended_hosts;
      _expand_hosts(it_dep->hosts(), it_dep->hostgroups(), s, depended_hosts);

      // Expanded dependent hosts.
      std::set<std::string> dependent_hosts;
      _expand_hosts(it_dep->dependent_hosts(), it_dep->dependent_hostgroups(),
                    s, dependent_hosts);

      // Browse all depended and dependent hosts.
      for (std::set<std::string>::const_iterator it1 = depended_hosts.begin(),
                                                 end1 = depended_hosts.end();
           it1 != end1; ++it1)
        for (std::set<std::string>::const_iterator
                 it2 = dependent_hosts.begin(),
                 end2 = dependent_hosts.end();
             it2 != end2; ++it2)
          for (int i = 0; i < 2; ++i) {
            // Create host dependency instance.
            configuration::hostdependency hdep(*it_dep);
            hdep.hostgroups().clear();
            hdep.hosts().clear();
            hdep.hosts().insert(*it1);
            hdep.dependent_hostgroups().clear();
            hdep.dependent_hosts().clear();
            hdep.dependent_hosts().insert(*it2);
            hdep.dependency_type(
                i == 0
                    ? configuration::hostdependency::execution_dependency
                    : configuration::hostdependency::notification_dependency);
            if (i == 1)
              hdep.execution_failure_options(0);
            else
              hdep.notification_failure_options(0);

            // Insert new host dependency. We do not need to expand it
            // because no expansion is made on 1->1 dependency.
            expanded.insert(hdep);
          }
    }
    // Insert dependency if already good to go.
    else
      expanded.insert(*it_dep);
  }

  // Set expanded host dependencies in configuration state.
  s.hostdependencies().swap(expanded);
}
#else
/**
 *  Expand host dependencies.
 *
 *  @param[in,out] s  Configuration being applied.
 */
void applier::hostdependency::expand_objects(configuration::State& s) {
  std::list<std::unique_ptr<configuration::Hostdependency> > lst;

  config_logger->debug("Expanding host dependencies");

  for (int i = s.hostdependencies_size() - 1; i >= 0; --i) {
    auto* hd_conf = s.mutable_hostdependencies(i);
    if (hd_conf->hosts().data().size() > 1 ||
        !hd_conf->hostgroups().data().empty() ||
        hd_conf->dependent_hosts().data().size() > 1 ||
        !hd_conf->dependent_hostgroups().data().empty() ||
        hd_conf->dependency_type() == unknown) {
      for (auto& hg_name : hd_conf->dependent_hostgroups().data()) {
        auto found =
            std::find_if(s.hostgroups().begin(), s.hostgroups().end(),
                         [&hg_name](const configuration::Hostgroup& hg) {
                           return hg.hostgroup_name() == hg_name;
                         });
        if (found != s.hostgroups().end()) {
          auto& hg_conf = *found;
          for (auto& h : hg_conf.members().data())
            fill_string_group(hd_conf->mutable_dependent_hosts(), h);
        }
      }
      for (auto& hg_name : hd_conf->hostgroups().data()) {
        auto found =
            std::find_if(s.hostgroups().begin(), s.hostgroups().end(),
                         [&hg_name](const configuration::Hostgroup& hg) {
                           return hg.hostgroup_name() == hg_name;
                         });
        if (found != s.hostgroups().end()) {
          auto& hg_conf = *found;
          for (auto& h : hg_conf.members().data())
            fill_string_group(hd_conf->mutable_hosts(), h);
        }
      }
      for (auto& h : hd_conf->hosts().data()) {
        for (auto& h_dep : hd_conf->dependent_hosts().data()) {
          for (int ii = 1; ii <= 2; ii++) {
            if (hd_conf->dependency_type() == DependencyKind::unknown ||
                static_cast<int32_t>(hd_conf->dependency_type()) == ii) {
              lst.emplace_back(std::make_unique<Hostdependency>());
              auto& new_hd = lst.back();
              new_hd->set_dependency_period(hd_conf->dependency_period());
              new_hd->set_inherits_parent(hd_conf->inherits_parent());
              fill_string_group(new_hd->mutable_hosts(), h);
              fill_string_group(new_hd->mutable_dependent_hosts(), h_dep);
              if (ii == 2) {
                new_hd->set_dependency_type(
                    DependencyKind::execution_dependency);
                new_hd->set_execution_failure_options(
                    hd_conf->execution_failure_options());
              } else {
                new_hd->set_dependency_type(
                    DependencyKind::notification_dependency);
                new_hd->set_notification_failure_options(
                    hd_conf->notification_failure_options());
              }
            }
          }
        }
      }
      s.mutable_hostdependencies()->DeleteSubrange(i, 1);
    }
  }
  for (auto& hd : lst)
    s.mutable_hostdependencies()->AddAllocated(hd.release());
}
#endif

#ifdef LEGACY_CONF
/**
 *  @brief Modify host dependency.
 *
 *  Host dependencies cannot be defined with anything else than their
 *  full content. Therefore no modification can occur.
 *
 *  @param[in] obj  Unused.
 */
void applier::hostdependency::modify_object(
    configuration::hostdependency const& obj) {
  (void)obj;
  throw engine_error()
      << "Could not modify a host dependency: "
      << "Host dependency objects can only be added or removed, "
      << "this is likely a software bug that you should report to "
      << "Centreon Engine developers";
}
#else
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
#endif

#ifdef LEGACY_CONF
/**
 *  Remove old host dependency.
 *
 *  @param[in] obj  The host dependency to remove from the monitoring
 *                  engine.
 */
void applier::hostdependency::remove_object(
    configuration::hostdependency const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing a host dependency.";
  config_logger->debug("Removing a host dependency.");

  // Find host dependency.
  hostdependency_mmap::iterator it(
      engine::hostdependency::hostdependencies_find(obj.key()));
  if (it != engine::hostdependency::hostdependencies.end()) {
    com::centreon::engine::hostdependency* dependency(it->second.get());

    // Notify event broker.
    broker_adaptive_dependency_data(NEBTYPE_HOSTDEPENDENCY_DELETE, dependency);

    // Remove host dependency from its list.
    engine::hostdependency::hostdependencies.erase(it);
  }

  // Remove dependency from the global configuration set.
  config->hostdependencies().erase(obj);
}
#else
/**
 *  Remove old host dependency.
 *
 *  @param[in] idx  The index of the host dependency configuration to remove
 * from engine.
 */
void applier::hostdependency::remove_object(ssize_t idx) {
  // Logging.
  config_logger->debug("Removing a host dependency.");

  // Find host dependency.
  auto& obj = pb_config.hostdependencies(0);
  size_t key = hostdependency_key(obj);

  hostdependency_mmap::iterator it =
      engine::hostdependency::hostdependencies_find(
          {obj.dependent_hosts().data(0), key});
  if (it != engine::hostdependency::hostdependencies.end()) {
    com::centreon::engine::hostdependency* dependency(it->second.get());

    // Notify event broker.
    broker_adaptive_dependency_data(NEBTYPE_HOSTDEPENDENCY_DELETE, dependency);

    // Remove host dependency from its list.
    engine::hostdependency::hostdependencies.erase(it);
  }

  // Remove dependency from the global configuration set.
  pb_config.mutable_hostdependencies()->DeleteSubrange(idx, 1);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Resolve a hostdependency.
 *
 *  @param[in] obj  Hostdependency object.
 */
void applier::hostdependency::resolve_object(
    configuration::hostdependency const& obj,
    error_cnt& err) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving a host dependency.";
  config_logger->debug("Resolving a host dependency.");

  // Find host escalation
  hostdependency_mmap::iterator it{
      engine::hostdependency::hostdependencies_find(obj.key())};

  if (engine::hostdependency::hostdependencies.end() == it)
    throw engine_error() << "Cannot resolve non-existing host escalation";

  // Resolve host dependency.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#else
/**
 *  Resolve a hostdependency.
 *
 *  @param[in] obj  Hostdependency object.
 */
void applier::hostdependency::resolve_object(
    const configuration::Hostdependency& obj,
    error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving a host dependency.");

  // Find host escalation
  auto k = hostdependency_key(obj);

  auto it = engine::hostdependency::hostdependencies_find(
      {obj.dependent_hosts().data(0), k});

  if (engine::hostdependency::hostdependencies.end() == it)
    throw engine_error() << "Cannot resolve non-existing host escalation";

  // Resolve host dependency.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Expand hosts.
 *
 *  @param[in]     hosts      Host list.
 *  @param[in]     hostgroups Host group list.
 *  @param[in,out] s          Configuration being applied.
 *  @param[out]    expanded   Expanded hosts.
 */
void applier::hostdependency::_expand_hosts(
    std::set<std::string> const& hosts,
    std::set<std::string> const& hostgroups,
    configuration::state& s,
    std::set<std::string>& expanded) {
  // Copy hosts.
  expanded = hosts;

  // Browse host groups.
  for (set_string::const_iterator it(hostgroups.begin()), end(hostgroups.end());
       it != end; ++it) {
    // Find host group.
    set_hostgroup::iterator it_group(s.hostgroups_find(*it));
    if (it_group == s.hostgroups().end())
      throw engine_error() << "Could not expand non-existing host group '"
                           << *it << "'";

    // Add host group members.
    expanded.insert(it_group->members().begin(), it_group->members().end());
  }
}
#endif
