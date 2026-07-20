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
#include "common/engine_conf/contactgroup_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Contactgroup object.
 *
 * @param obj The Contactgroup object on which this helper works. The helper is
 * not the owner of this object.
 */
contactgroup_helper::contactgroup_helper(Contactgroup* obj)
    : message_helper(object_type::contactgroup,
                     obj,
                     {},
                     Contactgroup::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Contactgroup objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool contactgroup_helper::hook(std::string_view key, std::string_view value) {
  Contactgroup* obj = static_cast<Contactgroup*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);
  if (key == "contactgroup_members") {
    fill_string_group(obj->mutable_contactgroup_members(), value);
    return true;
  } else if (key == "members") {
    fill_string_group(obj->mutable_members(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Contactgroup object.
 *
 * @param err An error counter.
 */
void contactgroup_helper::check_validity(error_cnt& err) const {
  const Contactgroup* o = static_cast<const Contactgroup*>(obj());

  if (o->contactgroup_name().empty()) {
    err.config_errors++;
    throw msg_fmt("Contactgroup has no name (property 'contactgroup_name')");
  }
}

/**
 * @brief The initializer of the Contactgroup message.
 */
void contactgroup_helper::_init() {
  Contactgroup* obj = static_cast<Contactgroup*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
}

/**
 * @brief Expand the contactgroups.
 *
 * @param s The configuration state to expand.
 * @param err The error count object to update in case of errors.
 */
void contactgroup_helper::expand(
    configuration::State& s,
    configuration::error_cnt& err,
    const absl::flat_hash_map<std::string_view, configuration::Contactgroup*>&
        m_contactgroups) {
  absl::flat_hash_set<std::string_view> resolved;

  for (auto& cg : *s.mutable_contactgroups())
    _resolve_members(s, cg, resolved, err, m_contactgroups);
}

/**
 * @brief Resolves the members of a contact group by recursively processing its
 * member groups.
 *
 * This function ensures that all members of a contact group, including those in
 * nested groups, are resolved and added to the contact group's member list. It
 * also handles errors when a non-existing contact group member is encountered.
 *
 * @param s The current configuration state.
 * @param obj The contact group object whose members are to be resolved.
 * @param resolved A set of already resolved contact group names to avoid
 * circular dependencies.
 * @param err A structure to count configuration errors.
 * @param m_contactgroups A map of contact group names to their corresponding
 * contact group objects.
 *
 * @throws msg_fmt If a non-existing contact group member is encountered.
 */
void contactgroup_helper::_resolve_members(
    configuration::State& s,
    configuration::Contactgroup& obj,
    absl::flat_hash_set<std::string_view>& resolved,
    configuration::error_cnt& err,
    const absl::flat_hash_map<std::string_view, configuration::Contactgroup*>&
        m_contactgroups) {
  if (resolved.contains(obj.contactgroup_name()))
    return;

  resolved.emplace(obj.contactgroup_name());
  if (!obj.contactgroup_members().data().empty()) {
    for (auto& cg_name : obj.contactgroup_members().data()) {
      auto it = m_contactgroups.find(cg_name);

      if (it == m_contactgroups.end()) {
        err.config_errors++;
        throw msg_fmt(
            "Error: Could not add non-existing contact group member '{}' to "
            "contactgroup '{}'",
            cg_name, obj.contactgroup_name());
      }

      Contactgroup& inner_cg = *it->second;
      _resolve_members(s, inner_cg, resolved, err, m_contactgroups);
      for (auto& c_name : inner_cg.members().data())
        fill_string_group(obj.mutable_members(), c_name);
    }
    obj.mutable_contactgroup_members()->clear_data();
  }
}

}  // namespace com::centreon::engine::configuration
