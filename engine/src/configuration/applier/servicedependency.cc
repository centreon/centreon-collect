/**
 * Copyright 2011-2013,2017-2024 Centreon
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

#include "com/centreon/engine/configuration/applier/servicedependency.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#ifdef LEGACY_CONF
#include "common/engine_legacy_conf/object.hh"
#include "common/engine_legacy_conf/servicedependency.hh"
#else
#include "common/engine_conf/state.pb.h"
#endif

using namespace com::centreon::engine::configuration;

#ifdef LEGACY_CONF
/**
 *  Add new service dependency.
 *
 *  @param[in] obj  The new servicedependency to add into the monitoring
 *                  engine.
 */
void applier::servicedependency::add_object(
    configuration::servicedependency const& obj) {
  // Check service dependency.
  if (obj.hosts().size() != 1 || !obj.hostgroups().empty() ||
      obj.service_description().size() != 1 || !obj.servicegroups().empty() ||
      obj.dependent_hosts().size() != 1 ||
      !obj.dependent_hostgroups().empty() ||
      obj.dependent_service_description().size() != 1 ||
      !obj.dependent_servicegroups().empty())
    throw engine_error()
        << "Could not create service "
        << "dependency with multiple (dependent) hosts / host groups "
        << "/ services / service groups";

  if (obj.dependency_type() !=
          configuration::servicedependency::execution_dependency &&
      obj.dependency_type() !=
          configuration::servicedependency::notification_dependency)
    throw engine_error()
        << "Could not create unexpanded dependency of service '"
        << obj.dependent_service_description().front() << "' of host '"
        << obj.dependent_hosts().front() << "' on service '"
        << obj.service_description().front() << "' of host '"
        << obj.hosts().front() << "'";

  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new service dependency of service '"
      << obj.dependent_service_description().front() << "' of host '"
      << obj.dependent_hosts().front() << "' on service '"
      << obj.service_description().front() << "' of host '"
      << obj.hosts().front() << "'.";
  config_logger->debug(
      "Creating new service dependency of service '{}' of host '{}' on service "
      "'{}' of host '{}'.",
      obj.dependent_service_description().front(),
      obj.dependent_hosts().front(), obj.service_description().front(),
      obj.hosts().front());

  // Add dependency to the global configuration set.
  config->servicedependencies().insert(obj);

  std::shared_ptr<engine::servicedependency> sd;

  if (obj.dependency_type() ==
      configuration::servicedependency::execution_dependency)
    // Create execution dependency.
    sd = std::make_shared<engine::servicedependency>(
        configuration::servicedependency_key(obj),
        obj.dependent_hosts().front(),
        obj.dependent_service_description().front(), obj.hosts().front(),
        obj.service_description().front(), dependency::execution,
        obj.inherits_parent(),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::servicedependency::ok),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::servicedependency::warning),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::servicedependency::unknown),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::servicedependency::critical),
        static_cast<bool>(obj.execution_failure_options() &
                          configuration::servicedependency::pending),
        obj.dependency_period());
  else
    // Create notification dependency.
    sd = std::make_shared<engine::servicedependency>(
        configuration::servicedependency_key(obj),
        obj.dependent_hosts().front(),
        obj.dependent_service_description().front(), obj.hosts().front(),
        obj.service_description().front(), dependency::notification,
        obj.inherits_parent(),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::servicedependency::ok),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::servicedependency::warning),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::servicedependency::unknown),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::servicedependency::critical),
        static_cast<bool>(obj.notification_failure_options() &
                          configuration::servicedependency::pending),
        obj.dependency_period());

  // Add new items to the global list.
  engine::servicedependency::servicedependencies.insert(
      {{sd->get_dependent_hostname(), sd->get_dependent_service_description()},
       sd});

  // Notify event broker.
  broker_adaptive_dependency_data(NEBTYPE_SERVICEDEPENDENCY_ADD, sd.get());
}
#else
/**
 *  Add new service dependency.
 *
 *  @param[in] obj  The new servicedependency to add into the monitoring
 *                  engine.
 */
void applier::servicedependency::add_object(
    const configuration::Servicedependency& obj) {
  // Check service dependency.
  if (obj.hosts().data().size() != 1 || !obj.hostgroups().data().empty() ||
      obj.service_description().data().size() != 1 ||
      !obj.servicegroups().data().empty() ||
      obj.dependent_hosts().data().size() != 1 ||
      !obj.dependent_hostgroups().data().empty() ||
      obj.dependent_service_description().data().size() != 1 ||
      !obj.dependent_servicegroups().data().empty())
    throw engine_error()
        << "Could not create service "
        << "dependency with multiple (dependent) hosts / host groups "
        << "/ services / service groups";

  if (obj.dependency_type() != execution_dependency &&
      obj.dependency_type() != notification_dependency)
    throw engine_error()
        << "Could not create unexpanded dependency of service '"
        << obj.dependent_service_description().data()[0] << "' of host '"
        << obj.dependent_hosts().data()[0] << "' on service '"
        << obj.service_description().data()[0] << "' of host '"
        << obj.hosts().data()[0] << "'";

  // Logging.
  config_logger->debug(
      "Creating new service dependency of service '{}' of host '{}' on service "
      "'{}' of host '{}'.",
      obj.dependent_service_description().data()[0],
      obj.dependent_hosts().data()[0], obj.service_description().data()[0],
      obj.hosts().data()[0]);

  // Add dependency to the global configuration set.
  auto* new_obj = pb_config.add_servicedependencies();
  new_obj->CopyFrom(obj);

  std::shared_ptr<engine::servicedependency> sd;

  if (obj.dependency_type() == execution_dependency)
    // Create execution dependency.
    sd = std::make_shared<engine::servicedependency>(
        configuration::servicedependency_key(obj),
        obj.dependent_hosts().data()[0],
        obj.dependent_service_description().data()[0], obj.hosts().data()[0],
        obj.service_description().data()[0], dependency::execution,
        obj.inherits_parent(),
        static_cast<bool>(obj.execution_failure_options() & action_sd_ok),
        static_cast<bool>(obj.execution_failure_options() & action_sd_warning),
        static_cast<bool>(obj.execution_failure_options() & action_sd_unknown),
        static_cast<bool>(obj.execution_failure_options() & action_sd_critical),
        static_cast<bool>(obj.execution_failure_options() & action_sd_pending),
        obj.dependency_period());
  else
    // Create notification dependency.
    sd = std::make_shared<engine::servicedependency>(
        servicedependency_key(obj), obj.dependent_hosts().data()[0],
        obj.dependent_service_description().data()[0], obj.hosts().data()[0],
        obj.service_description().data()[0], dependency::notification,
        obj.inherits_parent(),
        static_cast<bool>(obj.notification_failure_options() & action_sd_ok),
        static_cast<bool>(obj.notification_failure_options() &
                          action_sd_warning),
        static_cast<bool>(obj.notification_failure_options() &
                          action_sd_unknown),
        static_cast<bool>(obj.notification_failure_options() &
                          action_sd_critical),
        static_cast<bool>(obj.notification_failure_options() &
                          action_sd_pending),
        obj.dependency_period());

  // Add new items to the global list.
  engine::servicedependency::servicedependencies.insert(
      {{sd->get_dependent_hostname(), sd->get_dependent_service_description()},
       sd});
}
#endif

#ifdef LEGACY_CONF
/**
 *  Expand service dependencies.
 *
 *  @param[in,out] s  Configuration being applied.
 */
void applier::servicedependency::expand_objects(configuration::state& s) {
  // Browse all dependencies.
  configuration::set_servicedependency expanded;
  for (configuration::set_servicedependency::const_iterator
           it_dep(s.servicedependencies().begin()),
       end_dep(s.servicedependencies().end());
       it_dep != end_dep; ++it_dep) {
    // Expand service dependency instances.
    if (it_dep->hosts().size() != 1 || !it_dep->hostgroups().empty() ||
        it_dep->service_description().size() != 1 ||
        !it_dep->servicegroups().empty() ||
        it_dep->dependent_hosts().size() != 1 ||
        !it_dep->dependent_hostgroups().empty() ||
        it_dep->dependent_service_description().size() != 1 ||
        !it_dep->dependent_servicegroups().empty() ||
        it_dep->dependency_type() ==
            configuration::servicedependency::unknown_type) {
      // Expand depended services.
      std::set<std::pair<std::string, std::string>> depended_services;
      _expand_services(it_dep->hosts(), it_dep->hostgroups(),
                       it_dep->service_description(), it_dep->servicegroups(),
                       s, depended_services);

      // Expand dependent services.
      std::set<std::pair<std::string, std::string>> dependent_services;
      _expand_services(
          it_dep->dependent_hosts(), it_dep->dependent_hostgroups(),
          it_dep->dependent_service_description(),
          it_dep->dependent_servicegroups(), s, dependent_services);

      // Browse all depended and dependent services.
      for (std::set<std::pair<std::string, std::string>>::const_iterator
               it1(depended_services.begin()),
           end1(depended_services.end());
           it1 != end1; ++it1)
        for (std::set<std::pair<std::string, std::string>>::const_iterator
                 it2(dependent_services.begin()),
             end2(dependent_services.end());
             it2 != end2; ++it2)
          for (int i(0); i < 2; ++i) {
            // Create service dependency instance.
            configuration::servicedependency sdep(*it_dep);
            sdep.hostgroups().clear();
            sdep.hosts().clear();
            sdep.hosts().push_back(it1->first);
            sdep.servicegroups().clear();
            sdep.service_description().clear();
            sdep.service_description().push_back(it1->second);
            sdep.dependent_hostgroups().clear();
            sdep.dependent_hosts().clear();
            sdep.dependent_hosts().push_back(it2->first);
            sdep.dependent_servicegroups().clear();
            sdep.dependent_service_description().clear();
            sdep.dependent_service_description().push_back(it2->second);
            if (i == 0) {
              sdep.dependency_type(
                  configuration::servicedependency::execution_dependency);
              sdep.notification_failure_options(0);
            } else {
              sdep.dependency_type(
                  configuration::servicedependency::notification_dependency);
              sdep.execution_failure_options(0);
            }

            // Insert new service dependency. We do not need to expand it
            // because no expansion is made on 1->1 dependency.
            expanded.insert(sdep);
          }
    }
    // Insert dependency if already good to go.
    else
      expanded.insert(*it_dep);
  }

  // Set expanded service dependencies in configuration state.
  s.servicedependencies().swap(expanded);
}
#else
/**
 *  Expand service dependencies.
 *
 *  @param[in,out] s  Configuration being applied.
 */
void applier::servicedependency::expand_objects(configuration::State& s) {
  // Browse all dependencies.
  std::list<std::unique_ptr<Servicedependency>> expanded;
  for (auto& dep : s.servicedependencies()) {
    // Expand service dependency instances.
    if (dep.hosts().data().size() != 1 || !dep.hostgroups().data().empty() ||
        dep.service_description().data().size() != 1 ||
        !dep.servicegroups().data().empty() ||
        dep.dependent_hosts().data().size() != 1 ||
        !dep.dependent_hostgroups().data().empty() ||
        dep.dependent_service_description().data().size() != 1 ||
        !dep.dependent_servicegroups().data().empty() ||
        dep.dependency_type() == DependencyKind::unknown) {
      // Expand depended services.
      absl::flat_hash_set<std::pair<std::string, std::string>>
          depended_services;
      _expand_services(dep.hosts().data(), dep.hostgroups().data(),
                       dep.service_description().data(),
                       dep.servicegroups().data(), s, depended_services);

      // Expand dependent services.
      absl::flat_hash_set<std::pair<std::string, std::string>>
          dependent_services;
      _expand_services(
          dep.dependent_hosts().data(), dep.dependent_hostgroups().data(),
          dep.dependent_service_description().data(),
          dep.dependent_servicegroups().data(), s, dependent_services);

      // Browse all depended and dependent services.
      for (auto& p1 : depended_services)
        for (auto& p2 : dependent_services) {
          // Create service dependency instance.
          for (int32_t i = 1; i <= 2; i++) {
            if (dep.dependency_type() == DependencyKind::unknown ||
                static_cast<int32_t>(dep.dependency_type()) == i) {
              auto sdep = std::make_unique<Servicedependency>();
              sdep->CopyFrom(dep);
              sdep->clear_hostgroups();
              sdep->clear_hosts();
              sdep->mutable_hosts()->add_data(p1.first);
              sdep->clear_servicegroups();
              sdep->clear_service_description();
              sdep->mutable_service_description()->add_data(p1.second);
              sdep->clear_dependent_hostgroups();
              sdep->clear_dependent_hosts();
              sdep->mutable_dependent_hosts()->add_data(p2.first);
              sdep->clear_dependent_servicegroups();
              sdep->clear_dependent_service_description();
              sdep->mutable_dependent_service_description()->add_data(
                  p2.second);
              if (i == 2) {
                sdep->set_dependency_type(DependencyKind::execution_dependency);
                sdep->set_notification_failure_options(0);
              } else {
                sdep->set_dependency_type(
                    DependencyKind::notification_dependency);
                sdep->set_execution_failure_options(0);
              }
              expanded.push_back(std::move(sdep));
            }
          }
        }
    }
  }

  // Set expanded service dependencies in configuration state.
  s.clear_servicedependencies();
  for (auto& e : expanded)
    s.mutable_servicedependencies()->AddAllocated(e.release());
}
#endif

#ifdef LEGACY_CONF
/**
 *  @brief Modify service dependency.
 *
 *  Service dependencies cannot be defined with anything else than their
 *  full content. Therefore no modification can occur.
 *
 *  @param[in] obj  Unused.
 */
void applier::servicedependency::modify_object(
    configuration::servicedependency const& obj) {
  (void)obj;
  throw engine_error()
      << "Could not modify a service "
      << "dependency: service dependency objects can only be added "
      << "or removed, this is likely a software bug that you should "
      << "report to Centreon Engine developers";
}
#else
/**
 *  @brief Modify service dependency.
 *
 *  Service dependencies cannot be defined with anything else than their
 *  full content. Therefore no modification can occur.
 *
 *  @param[in] obj  Unused.
 */
void applier::servicedependency::modify_object(
    configuration::Servicedependency* old_obj [[maybe_unused]],
    const configuration::Servicedependency& new_obj [[maybe_unused]]) {
  throw engine_error()
      << "Could not modify a service dependency: service dependency objects "
         "can only be added or removed, this is likely a software bug that "
         "you should report to Centreon Engine developers";
}
#endif

#ifdef LEGACY_CONF
/**
 *  Remove old service dependency.
 *
 *  @param[in] obj  The service dependency to remove from the monitoring
 *                  engine.
 */
void applier::servicedependency::remove_object(
    configuration::servicedependency const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing a service dependency.";
  config_logger->debug("Removing a service dependency.");

  // Find service dependency.
  servicedependency_mmap::iterator it(
      engine::servicedependency::servicedependencies_find(obj.key()));
  if (it != engine::servicedependency::servicedependencies.end()) {
    // Notify event broker.
    broker_adaptive_dependency_data(NEBTYPE_SERVICEDEPENDENCY_DELETE,
                                    it->second.get());

    // Remove service dependency from its list.
    engine::servicedependency::servicedependencies.erase(it);
  }

  // Remove dependency from the global configuration set.
  config->servicedependencies().erase(obj);
}
#else
/**
 *  Remove old service dependency.
 *
 *  @param[in] obj  The service dependency to remove from the monitoring
 *                  engine.
 */
void applier::servicedependency::remove_object(ssize_t idx) {
  // Logging.
  config_logger->debug("Removing a service dependency.");

  // Find service dependency.
  auto& obj = pb_config.servicedependencies(idx);
  size_t key = servicedependency_key(obj);

  servicedependency_mmap::iterator it =
      engine::servicedependency::servicedependencies_find(
          std::make_tuple(obj.dependent_hosts().data(0),
                          obj.dependent_service_description().data(0), key));
  if (it != engine::servicedependency::servicedependencies.end()) {
    // Remove service dependency from its list.
    engine::servicedependency::servicedependencies.erase(it);
  }

  // Remove dependency from the global configuration set.
  pb_config.mutable_servicedependencies()->DeleteSubrange(idx, 1);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Resolve a servicedependency.
 *
 *  @param[in] obj  Servicedependency object.
 */
void applier::servicedependency::resolve_object(
    const configuration::servicedependency& obj,
    error_cnt& err) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving a service dependency.";
  config_logger->debug("Resolving a service dependency.");

  // Find service dependency.
  servicedependency_mmap::iterator it(
      engine::servicedependency::servicedependencies_find(obj.key()));
  if (engine::servicedependency::servicedependencies.end() == it)
    throw engine_error() << "Cannot resolve non-existing service dependency";

  // Resolve service dependency.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#else
/**
 *  Resolve a servicedependency.
 *
 *  @param[in] obj  Servicedependency object.
 */
void applier::servicedependency::resolve_object(
    const configuration::Servicedependency& obj,
    error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving a service dependency.");

  // Find service dependency.
  size_t key = configuration::servicedependency_key(obj);
  servicedependency_mmap::iterator it =
      engine::servicedependency::servicedependencies_find(
          {obj.dependent_hosts().data(0),
           obj.dependent_service_description().data(0), key});
  if (engine::servicedependency::servicedependencies.end() == it)
    throw engine_error() << "Cannot resolve non-existing service dependency";

  // Resolve service dependency.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Expand services.
 *
 *  @param[in]     hst      Hosts.
 *  @param[in]     hg       Host groups.
 *  @param[in]     svc      Service descriptions.
 *  @param[in]     sg       Service groups.
 *  @param[in,out] s        Configuration state.
 *  @param[out]    expanded Expanded services.
 */
void applier::servicedependency::_expand_services(
    std::list<std::string> const& hst,
    std::list<std::string> const& hg,
    std::list<std::string> const& svc,
    std::list<std::string> const& sg,
    configuration::state& s,
    std::set<std::pair<std::string, std::string>>& expanded) {
  // Expanded hosts.
  std::set<std::string> all_hosts;

  // Base hosts.
  all_hosts.insert(hst.begin(), hst.end());

  // Host groups.
  for (std::list<std::string>::const_iterator it(hg.begin()), end(hg.end());
       it != end; ++it) {
    // Find host group.
    configuration::set_hostgroup::iterator it_group(s.hostgroups_find(*it));
    if (it_group == s.hostgroups().end())
      throw engine_error() << "Could not resolve host group '" << *it << "'";

    // Add host group members.
    all_hosts.insert(it_group->members().begin(), it_group->members().end());
  }

  // Hosts * services.
  for (std::set<std::string>::const_iterator it_host(all_hosts.begin()),
       end_host(all_hosts.end());
       it_host != end_host; ++it_host)
    for (std::list<std::string>::const_iterator it_service(svc.begin()),
         end_service(svc.end());
         it_service != end_service; ++it_service)
      expanded.insert({*it_host, *it_service});

  // Service groups.
  for (std::list<std::string>::const_iterator it(sg.begin()), end(sg.end());
       it != end; ++it) {
    // Find service group.
    configuration::set_servicegroup::iterator it_group(
        s.servicegroups_find(*it));
    if (it_group == s.servicegroups().end())
      throw(engine_error() << "Could not resolve service group '" << *it
                           << "'");

    // Add service group members.
    for (set_pair_string::const_iterator it_member(it_group->members().begin()),
         end_member(it_group->members().end());
         it_member != end_member; ++it_member)
      expanded.insert(*it_member);
  }
}
#else
/**
 *  Expand services.
 *
 *  @param[in]     hst      Hosts.
 *  @param[in]     hg       Host groups.
 *  @param[in]     svc      Service descriptions.
 *  @param[in]     sg       Service groups.
 *  @param[in,out] s        Configuration state.
 *  @param[out]    expanded Expanded services.
 */
void applier::servicedependency::_expand_services(
    const ::google::protobuf::RepeatedPtrField<std::string>& hst,
    const ::google::protobuf::RepeatedPtrField<std::string>& hg,
    const ::google::protobuf::RepeatedPtrField<std::string>& svc,
    const ::google::protobuf::RepeatedPtrField<std::string>& sg,
    configuration::State& s,
    absl::flat_hash_set<std::pair<std::string, std::string>>& expanded) {
  // Expanded hosts.
  absl::flat_hash_set<std::string> all_hosts;

  // Base hosts.
  all_hosts.insert(hst.begin(), hst.end());

  // Host groups.
  for (auto& hgn : hg) {
    // Find host group
    auto found = std::find_if(
        s.hostgroups().begin(), s.hostgroups().end(),
        [&hgn](const Hostgroup& hgg) { return hgg.hostgroup_name() == hgn; });
    if (found == s.hostgroups().end())
      throw engine_error() << fmt::format("Could not resolve host group '{}'",
                                          hgn);
    // Add host group members.
    all_hosts.insert(found->members().data().begin(),
                     found->members().data().end());
  }

  // Hosts * services.
  for (auto& h : all_hosts)
    for (auto& s : svc)
      expanded.insert({h, s});

  // Service groups.
  for (auto& sgn : sg) {
    // Find service group.
    auto found =
        std::find_if(s.servicegroups().begin(), s.servicegroups().end(),
                     [&sgn](const Servicegroup& sgg) {
                       return sgg.servicegroup_name() == sgn;
                     });
    if (found == s.servicegroups().end())
      throw engine_error() << fmt::format(
          "Coulx not resolve service group '{}'", sgn);

    // Add service group members.
    for (auto& m : found->members().data())
      expanded.insert({m.first(), m.second()});
  }
}
#endif
