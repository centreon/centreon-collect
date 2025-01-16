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
#include "common/engine_conf/servicegroup_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Servicegroup object.
 *
 * @param obj The Servicegroup object on which this helper works. The helper is
 * not the owner of this object.
 */
servicegroup_helper::servicegroup_helper(Servicegroup* obj)
    : message_helper(object_type::servicegroup,
                     obj,
                     {},
                     Servicegroup::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Servicegroup objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool servicegroup_helper::hook(std::string_view key, std::string_view value) {
  Servicegroup* obj = static_cast<Servicegroup*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);
  if (key == "members") {
    fill_pair_string_group(obj->mutable_members(), value);
    return true;
  } else if (key == "servicegroup_members") {
    fill_string_group(obj->mutable_servicegroup_members(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Servicegroup object.
 *
 * @param err An error counter.
 */
void servicegroup_helper::check_validity(error_cnt& err) const {
  const Servicegroup* o = static_cast<const Servicegroup*>(obj());

  if (o->servicegroup_name().empty()) {
    err.config_errors++;
    throw msg_fmt("Service group has no name (property 'servicegroup_name')");
  }
}

void servicegroup_helper::_init() {
  Servicegroup* obj = static_cast<Servicegroup*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
}

/**
 * @brief Expand the Servicegroup object.
 *
 * @param s The configuration state to expand.
 * @param err The error count object to update in case of errors.
 */
void servicegroup_helper::expand(
    configuration::State& s,
    configuration::error_cnt& err,
    absl::flat_hash_map<std::string, configuration::Servicegroup*>&
        m_servicegroups) {
  // This set stores resolved service groups.
  absl::flat_hash_set<std::string_view> resolved;

  // Each servicegroup can contain servicegroups, that is to mean the services
  // in the sub servicegroups are also in our servicegroup.
  // So, we iterate through all the servicegroups defined in the configuration,
  // and for each one if it has servicegroup members, we fill its service
  // members with theirs and then we clear the servicegroup members. At that
  // step, a servicegroup is considered as resolved.
  for (auto& sg_conf : *s.mutable_servicegroups()) {
    if (!resolved.contains(sg_conf.servicegroup_name())) {
      _resolve_members(s, &sg_conf, resolved, m_servicegroups, err);
    }
  }
}

/**
 * @brief Resolve the members of a service group.
 *
 * @param s The configuration state.
 * @param sg_conf The service group to resolve.
 * @param resolved The set of resolved service groups.
 * @param sg_by_name The map of service groups by name.
 * @param err The error counter.
 */
void servicegroup_helper::_resolve_members(
    configuration::State& s,
    configuration::Servicegroup* sg_conf,
    absl::flat_hash_set<std::string_view>& resolved,
    const absl::flat_hash_map<std::string, configuration::Servicegroup*>&
        sg_by_name,
    configuration::error_cnt& err) {
  for (auto& sgm : sg_conf->servicegroup_members().data()) {
    std::cout << "Resolving service group member sgm " << sgm << std::endl;
    auto sgm_conf = sg_by_name.find(sgm);
    if (sgm_conf == sg_by_name.end()) {
      err.config_errors++;
      throw msg_fmt(
          "Could not add non-existing service group member '{}' to service "
          "group '{}'\n",
          sgm, sg_conf->servicegroup_name());
    }
    if (!resolved.contains(sgm_conf->second->servicegroup_name()))
      _resolve_members(s, sgm_conf->second, resolved, sg_by_name, err);

    for (auto& sm : sgm_conf->second->members().data()) {
      fill_pair_string_group(sg_conf->mutable_members(), sm.first(),
                             sm.second());
    }
  }
  sg_conf->clear_servicegroup_members();
  resolved.emplace(sg_conf->servicegroup_name());
}
}  // namespace com::centreon::engine::configuration
