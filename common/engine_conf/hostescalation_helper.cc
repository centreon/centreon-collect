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
#include "common/engine_conf/hostescalation_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/engine_conf/state.pb.h"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Builds a key from a Hostescalation message. This is useful to check
 * modifications in hostescalations.
 *
 * @param hd The Hostescalation object to use to build the key.
 *
 * @return A number of type size_t.
 */
size_t hostescalation_key(const Hostescalation& he) {
  return absl::HashOf(he.hosts().data(0),
                      // he.contactgroups().data(),
                      he.escalation_options(), he.escalation_period(),
                      he.first_notification(), he.last_notification(),
                      he.notification_interval());
}

/**
 * @brief Constructor from a Hostescalation object.
 *
 * @param obj The Hostescalation object on which this helper works. The helper
 * is not the owner of this object.
 */
hostescalation_helper::hostescalation_helper(Hostescalation* obj)
    : message_helper(object_type::hostescalation,
                     obj,
                     {
                         {"hostgroup", "hostgroups"},
                         {"hostgroup_name", "hostgroups"},
                         {"host", "hosts"},
                         {"host_name", "hosts"},
                         {"contact_groups", "contactgroups"},
                     },
                     Hostescalation::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Hostescalation objects has a
 * particular behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool hostescalation_helper::hook(std::string_view key,
                                 const std::string_view& value) {
  Hostescalation* obj = static_cast<Hostescalation*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);

  if (key == "escalation_options") {
    uint32_t options = action_he_none;
    auto arr = absl::StrSplit(value, ',');
    for (auto& v : arr) {
      std::string_view vv = absl::StripAsciiWhitespace(v);
      if (vv == "d" || vv == "down")
        options |= action_he_down;
      else if (vv == "u" || vv == "unreachable")
        options |= action_he_unreachable;
      else if (vv == "r" || vv == "recovery")
        options |= action_he_recovery;
      else if (vv == "n" || vv == "none")
        options = action_he_none;
      else if (vv == "a" || vv == "all")
        options = action_he_down | action_he_unreachable | action_he_recovery;
      else
        return false;
    }
    obj->set_escalation_options(options);
    set_changed(
        obj->descriptor()->FindFieldByName("escalation_options")->index());
    return true;
  } else if (key == "contactgroups") {
    fill_string_group(obj->mutable_contactgroups(), value);
    return true;
  } else if (key == "hostgroups") {
    fill_string_group(obj->mutable_hostgroups(), value);
    return true;
  } else if (key == "hosts") {
    fill_string_group(obj->mutable_hosts(), value);
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Hostescalation object.
 *
 * @param err An error counter.
 */
void hostescalation_helper::check_validity(error_cnt& err) const {
  const Hostescalation* o = static_cast<const Hostescalation*>(obj());

  if (o->hosts().data().empty() && o->hostgroups().data().empty()) {
    err.config_errors++;
    throw msg_fmt(
        "Host escalation is not attached to any host or host group (properties "
        "'hosts' or 'hostgroups', respectively)");
  }
}

/**
 * @brief Initializer of the Hostescalation object, in other words set its
 * default values.
 */
void hostescalation_helper::_init() {
  Hostescalation* obj = static_cast<Hostescalation*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->set_escalation_options(action_he_none);
  obj->set_first_notification(-2);
  obj->set_last_notification(-2);
  obj->set_notification_interval(0);
}
}  // namespace com::centreon::engine::configuration
