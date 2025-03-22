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
#include "common/engine_conf/state.pb.h"

using namespace com::centreon::engine::configuration;

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
  auto* new_obj = pb_indexed_config.state().add_servicedependencies();
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
  auto& obj = pb_indexed_config.state().servicedependencies(idx);
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
  pb_indexed_config.state().mutable_servicedependencies()->DeleteSubrange(idx,
                                                                          1);
}

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
