/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/servicedependency_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

size_t servicedependency_key(const Servicedependency& sd) {
  return absl::HashOf(sd.dependency_period(), sd.dependency_type(),
                      sd.hosts().data(0), sd.service_description().data(0),
                      sd.dependent_hosts().data(0),
                      sd.dependent_service_description().data(0),
                      sd.execution_failure_options(), sd.inherits_parent(),
                      sd.notification_failure_options());
}

/**
 * @brief Constructor from a Servicedependency object.
 *
 * @param obj The Servicedependency object on which this helper works. The
 * helper is not the owner of this object.
 */
servicedependency_helper::servicedependency_helper(Servicedependency* obj)
    : message_helper(
          object_type::servicedependency,
          obj,
          {
              {"servicegroup", "servicegroups"},
              {"servicegroup_name", "servicegroups"},
              {"hostgroup", "hostgroups"},
              {"hostgroup_name", "hostgroups"},
              {"host", "hosts"},
              {"host_name", "hosts"},
              {"master_host", "hosts"},
              {"master_host_name", "hosts"},
              {"description", "service_description"},
              {"master_description", "service_description"},
              {"master_service_description", "service_description"},
              {"dependent_servicegroup", "dependent_servicegroups"},
              {"dependent_servicegroup_name", "dependent_servicegroups"},
              {"dependent_hostgroup", "dependent_hostgroups"},
              {"dependent_hostgroup_name", "dependent_hostgroups"},
              {"dependent_host", "dependent_hosts"},
              {"dependent_host_name", "dependent_hosts"},
              {"dependent_description", "dependent_service_description"},
              {"execution_failure_criteria", "execution_failure_options"},
              {"notification_failure_criteria", "notification_failure_options"},
          },
          Servicedependency::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Servicedependency objects has a
 * particular behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool servicedependency_helper::hook(std::string_view key,
                                    std::string_view value) {
  Servicedependency* obj = static_cast<Servicedependency*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);

  if (key == "execution_failure_options" ||
      key == "notification_failure_options") {
    uint32_t options = action_sd_none;
    auto arr = absl::StrSplit(value, ',');
    for (auto& v : arr) {
      std::string_view vv = absl::StripAsciiWhitespace(v);
      if (vv == "o" || vv == "ok")
        options |= action_sd_ok;
      else if (vv == "u" || vv == "unknown")
        options |= action_sd_unknown;
      else if (vv == "w" || vv == "warning")
        options |= action_sd_warning;
      else if (vv == "c" || vv == "critical")
        options |= action_sd_critical;
      else if (vv == "p" || vv == "pending")
        options |= action_sd_pending;
      else if (vv == "n" || vv == "none")
        options = action_sd_none;
      else if (vv == "a" || vv == "all")
        options = action_sd_ok | action_sd_warning | action_sd_critical |
                  action_sd_pending;
      else
        return false;
    }
    if (key[0] == 'e')
      obj->set_execution_failure_options(options);
    else
      obj->set_notification_failure_options(options);
    return true;
  } else if (key == "dependent_hostgroups") {
    fill_string_group(obj->mutable_dependent_hostgroups(), value);
    return true;
  } else if (key == "dependent_hosts") {
    fill_string_group(obj->mutable_dependent_hosts(), value);
    return true;
  } else if (key == "dependent_servicegroups") {
    fill_string_group(obj->mutable_dependent_servicegroups(), value);
    return true;
  } else if (key == "dependent_service_description") {
    fill_string_group(obj->mutable_dependent_service_description(), value);
    return true;
  } else if (key == "hostgroups") {
    fill_string_group(obj->mutable_hostgroups(), value);
    return true;
  } else if (key == "hosts") {
    fill_string_group(obj->mutable_hosts(), value);
    return true;
  } else if (key == "servicegroups") {
    fill_string_group(obj->mutable_servicegroups(), value);
    return true;
  } else if (key == "service_description") {
    fill_string_group(obj->mutable_service_description(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Servicedependency object.
 *
 * @param err An error counter.
 */
void servicedependency_helper::check_validity(error_cnt& err) const {
  const Servicedependency* o = static_cast<const Servicedependency*>(obj());

  /* Check base service(s). */
  if (o->servicegroups().data().empty()) {
    if (o->service_description().data().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Service dependency is not attached to any service or service group "
          "(properties 'service_description' or 'servicegroup_name', "
          "respectively)");
    } else if (o->hosts().data().empty() && o->hostgroups().data().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Service dependency is not attached to any host or host group "
          "(properties 'host_name' or 'hostgroup_name', respectively)");
    }
  }

  /* Check dependent service(s). */
  if (o->dependent_servicegroups().data().empty()) {
    if (o->dependent_service_description().data().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Service dependency is not attached to "
          "any dependent service or dependent service group "
          "(properties 'dependent_service_description' or "
          "'dependent_servicegroup_name', respectively)");
    } else if (o->dependent_hosts().data().empty() &&
               o->dependent_hostgroups().data().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Service dependency is not attached to "
          "any dependent host or dependent host group (properties "
          "'dependent_host_name' or 'dependent_hostgroup_name', "
          "respectively)");
    }
  }
}

/**
 * @brief Initializer of the Servicedependency object, in other words set its
 * default values.
 */
void servicedependency_helper::_init() {
  Servicedependency* obj = static_cast<Servicedependency*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->set_execution_failure_options(action_sd_none);
  obj->set_inherits_parent(false);
  obj->set_notification_failure_options(action_sd_none);
}

/**
 * @brief Expand service dependencies.
 *
 * @param s The configuration state to expand.
 * @param err The error count object to update in case of errors.
 */
void servicedependency_helper::expand(
    State& s,
    error_cnt& err [[maybe_unused]],
    const absl::flat_hash_map<std::string_view, configuration::Hostgroup*>&
        hostgroups,
    const absl::flat_hash_map<std::string_view, configuration::Servicegroup*>&
        servicegroups) {
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
                       dep.servicegroups().data(), depended_services,
                       hostgroups, servicegroups);

      // Expand dependent services.
      absl::flat_hash_set<std::pair<std::string, std::string>>
          dependent_services;
      _expand_services(dep.dependent_hosts().data(),
                       dep.dependent_hostgroups().data(),
                       dep.dependent_service_description().data(),
                       dep.dependent_servicegroups().data(), dependent_services,
                       hostgroups, servicegroups);

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
 * @brief Expand services.
 *
 * @param hst Hosts.
 * @param hg Host groups.
 * @param svc Service descriptions.
 * @param sg Service groups.
 * @param s Configuration state.
 * @param expanded Expanded services.
 */
void servicedependency_helper::_expand_services(
    const ::google::protobuf::RepeatedPtrField<std::string>& hst,
    const ::google::protobuf::RepeatedPtrField<std::string>& hg,
    const ::google::protobuf::RepeatedPtrField<std::string>& svc,
    const ::google::protobuf::RepeatedPtrField<std::string>& sg,
    absl::flat_hash_set<std::pair<std::string, std::string>>& expanded,
    const absl::flat_hash_map<std::string_view, configuration::Hostgroup*>&
        hostgroups,
    const absl::flat_hash_map<std::string_view, configuration::Servicegroup*>&
        servicegroups) {
  // Expanded hosts.
  absl::flat_hash_set<std::string> all_hosts;

  // Base hosts.
  all_hosts.insert(hst.begin(), hst.end());

  // Host groups.
  for (auto& hgn : hg) {
    // Find host group
    auto found = hostgroups.find(hgn);
    if (found == hostgroups.end())
      throw msg_fmt("Could not resolve host group '{}'", hgn);
    // Add host group members.
    all_hosts.insert(found->second->members().data().begin(),
                     found->second->members().data().end());
  }

  // Hosts * services.
  for (auto& h : all_hosts)
    for (auto& s : svc)
      expanded.insert({h, s});

  // Service groups.
  for (auto& sgn : sg) {
    // Find service group.
    auto found = servicegroups.find(sgn);
    ;
    if (found == servicegroups.end())
      throw msg_fmt("Coulx not resolve service group '{}'", sgn);

    // Add service group members.
    for (auto& m : found->second->members().data())
      expanded.insert({m.first(), m.second()});
  }
}

}  // namespace com::centreon::engine::configuration
