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
#include "common/engine_conf/hostgroup_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Hostgroup object.
 *
 * @param obj The Hostgroup object on which this helper works. The helper is not
 * the owner of this object.
 */
hostgroup_helper::hostgroup_helper(Hostgroup* obj)
    : message_helper(object_type::hostgroup,
                     obj,
                     {},
                     Hostgroup::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Hostgroup objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool hostgroup_helper::hook(std::string_view key, std::string_view value) {
  Hostgroup* obj = static_cast<Hostgroup*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);
  if (key == "members") {
    fill_string_group(obj->mutable_members(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Hostgroup object.
 *
 * @param err An error counter.
 */
void hostgroup_helper::check_validity(error_cnt& err) const {
  const Hostgroup* o = static_cast<const Hostgroup*>(obj());

  if (o->obj().register_()) {
    if (o->hostgroup_name().empty()) {
      err.config_errors++;
      throw msg_fmt("Host group has no name (property 'hostgroup_name')");
    }
  }
}

/**
 * @brief Initializer of the Hostgroup object, in other words set its default
 * values.
 */
void hostgroup_helper::_init() {
  Hostgroup* obj = static_cast<Hostgroup*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
}

void hostgroup_helper::expand(configuration::State& s,
                              configuration::error_cnt& /*err*/) {
  // Browse all hostgroups.
  for (auto& hostgroup_cfg : *s.mutable_hostgroups()) {
    hostgroup_cfg.set_poller_id(s.poller_id());
  }
}

/**
 * @brief Validate a Hostgroup against the rest of the configuration.
 *
 * This is the configuration-level equivalent of the former
 * `engine::hostgroup::resolve()`: it only accumulates warnings/errors into
 * @a err (it never throws) and performs no runtime wiring. It expects to run on
 * a post-`expand` State (members are flattened to concrete host names).
 * Checks performed: every member host is defined, and the host group name has
 * no illegal character.
 *
 * @param hg The Hostgroup to validate.
 * @param hosts Names of every host defined in the configuration.
 * @param illegal_chars Characters forbidden in object names (State's
 * illegal_object_chars).
 * @param err Warning/error counters, incremented in place.
 * @param logger Logger receiving the human-readable diagnostics.
 */
void hostgroup_helper::resolve(
    const configuration::Hostgroup& hg,
    const absl::flat_hash_set<std::string_view>& hosts,
    std::string_view illegal_chars,
    configuration::error_cnt& err,
    const std::shared_ptr<spdlog::logger>& logger) {
  /* Check members: every host of the group must be defined somewhere. */
  for (auto& member : hg.members().data()) {
    if (!hosts.contains(member)) {
      logger->error(
          "Error: Host '{}' specified in host group '{}' is not defined "
          "anywhere!",
          member, hg.hostgroup_name());
      err.config_errors++;
    }
  }

  /* Check for illegal characters in hostgroup name. */
  if (name_contains_illegal_chars(hg.hostgroup_name(), illegal_chars)) {
    logger->error(
        "Error: The name of hostgroup '{}' contains one or more illegal "
        "characters.",
        hg.hostgroup_name());
    err.config_errors++;
  }
}

}  // namespace com::centreon::engine::configuration
