/**
 * Copyright 2011-2023 Centreon
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

#include <absl/hash/hash.h>

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/hostdependency.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/log_v2.hh"
#include "configuration/message_helper.hh"
#include "configuration/state-generated.pb.h"

using namespace com::centreon::engine::configuration;

namespace com {
namespace centreon {
namespace engine {
namespace configuration {
size_t hostdependency_key(const Hostdependency& hd) {
  assert(hd.hosts().data().size() == 1 && hd.hostgroups().data().empty() &&
         hd.dependent_hosts().data().size() == 1 &&
         hd.dependent_hostgroups().data().empty());
  return absl::HashOf(hd.dependency_period(), hd.dependency_type(),
                      hd.dependent_hosts().data(0), hd.hosts().data(0),
                      hd.inherits_parent(), hd.notification_failure_options());
}

size_t hostdependency_key_l(const hostdependency& hd) {
  assert(hd.hosts().size() == 1 && hd.hostgroups().empty() &&
         hd.dependent_hosts().size() == 1 && hd.dependent_hostgroups().empty());
  return absl::HashOf(hd.dependency_period(), hd.dependency_type(),
                      *hd.dependent_hosts().begin(), *hd.hosts().begin(),
                      hd.inherits_parent(), hd.notification_failure_options());
}
}  // namespace configuration
}  // namespace engine
}  // namespace centreon
}  // namespace com

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
  log_v2::config()->debug(
      "Creating new host dependency of host '{}' on host '{}'.",
      *obj.dependent_hosts().begin(), *obj.hosts().begin());

  // Add dependency to the global configuration set.
  config->hostdependencies().insert(obj);

  std::shared_ptr<engine::hostdependency> hd;

  if (obj.dependency_type() ==
      configuration::hostdependency::execution_dependency)
    // Create executon dependency.
    hd = std::make_shared<engine::hostdependency>(
        hostdependency_key_l(obj), *obj.dependent_hosts().begin(),
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
        hostdependency_key_l(obj), *obj.dependent_hosts().begin(),
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
  if (obj.dependency_type() !=
          configuration::hostdependency::execution_dependency &&
      obj.dependency_type() !=
          configuration::hostdependency::notification_dependency)
    throw engine_error() << fmt::format(
        "Could not create unexpanded host dependency of '{}' on '{}'",
        obj.dependent_hosts().data(0), obj.hosts().data(0));

  // Logging.
  log_v2::config()->debug(
      "Creating new host dependency of host '{}' on host '{}'.",
      obj.dependent_hosts().data(0), obj.hosts().data(0));

  // Add dependency to the global configuration set.
  auto* new_obj = pb_config.add_hostdependencies();
  new_obj->CopyFrom(obj);

  std::shared_ptr<engine::hostdependency> hd;

  if (obj.dependency_type() ==
      configuration::hostdependency::execution_dependency)
    // Create executon dependency.
    hd = std::make_shared<engine::hostdependency>(
        hostdependency_key(obj), obj.dependent_hosts().data(0),
        obj.hosts().data(0),
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
        hostdependency_key(obj), obj.dependent_hosts().data(0),
        obj.hosts().data(0),
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
      {obj.dependent_hosts().data(0), hd});

  broker_adaptive_dependency_data(NEBTYPE_HOSTDEPENDENCY_ADD, hd.get());
}

/**
 *  Expand host dependencies.
 *
 *  @param[in,out] s  Configuration being applied.
 */
void applier::hostdependency::expand_objects(configuration::State& s) {
  std::list<configuration::Hostdependency> lst;

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
          for (int i = 0; i < 2; i++) {
            lst.emplace_back();
            auto& new_hd = lst.back();
            fill_string_group(new_hd.mutable_hosts(), h);
            fill_string_group(new_hd.mutable_dependent_hosts(), h_dep);
            new_hd.set_dependency_type(
                i == 0 ? DependencyKind::execution_dependency
                       : DependencyKind::notification_dependency);
            if (i)
              new_hd.set_execution_failure_options(0);
            else
              new_hd.set_execution_failure_options(0);
          }
        }
      }
      s.mutable_hostdependencies()->DeleteSubrange(i, 1);
    }
  }
  for (auto& hd : lst) {
    auto* new_hd = s.add_hostdependencies();
    new_hd->CopyFrom(std::move(hd));
  }
}

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
      for (std::set<std::string>::const_iterator it1(depended_hosts.begin()),
           end1(depended_hosts.end());
           it1 != end1; ++it1)
        for (std::set<std::string>::const_iterator it2(dependent_hosts.begin()),
             end2(dependent_hosts.end());
             it2 != end2; ++it2)
          for (int i(0); i < 2; ++i) {
            // Create host dependency instance.
            configuration::hostdependency hdep(*it_dep);
            hdep.hostgroups().clear();
            hdep.hosts().clear();
            hdep.hosts().insert(*it1);
            hdep.dependent_hostgroups().clear();
            hdep.dependent_hosts().clear();
            hdep.dependent_hosts().insert(*it2);
            hdep.dependency_type(
                !i ? configuration::hostdependency::execution_dependency
                   : configuration::hostdependency::notification_dependency);
            if (i)
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

/**
 *  @brief Modify host dependency.
 *
 *  Host dependencies cannot be defined with anything else than their
 *  full content. Therefore no modification can occur.
 *
 *  @param[in] obj  Unused.
 */
void applier::hostdependency::modify_object(
    configuration::Hostdependency* old_obj [[_may_be_unused]],
    const configuration::Hostdependency& new_obj [[_may_be_unused]]) {
  throw engine_error()
      << "Could not modify a host dependency: Host dependency objects can only "
         "be added or removed, this is likely a software bug that you should "
         "report to Centreon Engine developers";
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
    configuration::hostdependency const& obj) {
  (void)obj;
  throw engine_error()
      << "Could not modify a host dependency: "
      << "Host dependency objects can only be added or removed, "
      << "this is likely a software bug that you should report to "
      << "Centreon Engine developers";
}

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
  log_v2::config()->debug("Removing a host dependency.");

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

/**
 *  Remove old host dependency.
 *
 *  @param[in] idx  The index of the host dependency configuration to remove
 * from engine.
 */
void applier::hostdependency::remove_object(ssize_t idx) {
  // Logging.
  log_v2::config()->debug("Removing a host dependency.");

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

/**
 *  Resolve a hostdependency.
 *
 *  @param[in] obj  Hostdependency object.
 */
void applier::hostdependency::resolve_object(
    configuration::hostdependency const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving a host dependency.";
  log_v2::config()->debug("Resolving a host dependency.");

  // Find host escalation
  hostdependency_mmap::iterator it{
      engine::hostdependency::hostdependencies_find(obj.key())};

  if (engine::hostdependency::hostdependencies.end() == it)
    throw engine_error() << "Cannot resolve non-existing host escalation";

  // Resolve host dependency.
  it->second->resolve(config_warnings, config_errors);
}

/**
 *  Resolve a hostdependency.
 *
 *  @param[in] obj  Hostdependency object.
 */
void applier::hostdependency::resolve_object(
    const configuration::Hostdependency& obj) {
  // Logging.
  log_v2::config()->debug("Resolving a host dependency.");

  // Find host escalation
  auto k = hostdependency_key(obj);

  auto it = engine::hostdependency::hostdependencies_find(
      {obj.dependent_hosts().data(0), k});

  if (engine::hostdependency::hostdependencies.end() == it)
    throw engine_error() << "Cannot resolve non-existing host escalation";

  // Resolve host dependency.
  it->second->resolve(config_warnings, config_errors);
}

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
