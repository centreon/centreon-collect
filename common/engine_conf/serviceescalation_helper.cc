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
#include "common/engine_conf/serviceescalation_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/engine_conf/state.pb.h"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

size_t serviceescalation_key(const Serviceescalation& se) {
  return absl::HashOf(se.hosts().data(0), se.service_description().data(0),
                      // se.contactgroups(),
                      se.escalation_options(), se.escalation_period(),
                      se.first_notification(), se.last_notification(),
                      se.notification_interval());
}

/**
 * @brief Constructor from a Serviceescalation object.
 *
 * @param obj The Serviceescalation object on which this helper works. The
 * helper is not the owner of this object.
 */
serviceescalation_helper::serviceescalation_helper(Serviceescalation* obj)
    : message_helper(object_type::serviceescalation,
                     obj,
                     {
                         {"host", "hosts"},
                         {"host_name", "hosts"},
                         {"description", "service_description"},
                         {"servicegroup", "servicegroups"},
                         {"servicegroup_name", "servicegroups"},
                         {"hostgroup", "hostgroups"},
                         {"hostgroup_name", "hostgroups"},
                         {"contact_groups", "contactgroups"},
                     },
                     Serviceescalation::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Serviceescalation objects has a
 * particular behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool serviceescalation_helper::hook(std::string_view key,
                                    const std::string_view& value) {
  Serviceescalation* obj = static_cast<Serviceescalation*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);

  if (key == "escalation_options") {
    uint32_t options = action_he_none;
    auto arr = absl::StrSplit(value, ',');
    for (auto& v : arr) {
      std::string_view vv = absl::StripAsciiWhitespace(v);
      if (vv == "w" || vv == "warning")
        options |= action_se_warning;
      else if (vv == "u" || vv == "unknown")
        options |= action_se_unknown;
      else if (vv == "c" || vv == "critical")
        options |= action_se_critical;
      else if (vv == "r" || vv == "recovery")
        options |= action_se_recovery;
      else if (vv == "n" || vv == "none")
        options = action_se_none;
      else if (vv == "a" || vv == "all")
        options = action_se_warning | action_se_unknown | action_se_critical |
                  action_se_recovery;
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
 * @brief Check the validity of the Serviceescalation object.
 *
 * @param err An error counter.
 */
void serviceescalation_helper::check_validity(error_cnt& err) const {
  const Serviceescalation* o = static_cast<const Serviceescalation*>(obj());

  if (o->servicegroups().data().empty()) {
    if (o->service_description().data().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Service escalation is not attached to "
          "any service or service group (properties "
          "'service_description' and 'servicegroup_name', "
          "respectively)");
    } else if (o->hosts().data().empty() && o->hostgroups().data().empty()) {
      err.config_errors++;
      throw msg_fmt(
          "Service escalation is not attached to "
          "any host or host group (properties 'host_name' or "
          "'hostgroup_name', respectively)");
    }
  }
}

/**
 * @brief Initializer of the Serviceescalation object, in other words set its
 * default values.
 */
void serviceescalation_helper::_init() {
  Serviceescalation* obj = static_cast<Serviceescalation*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->set_escalation_options(action_se_none);
  obj->set_first_notification(-2);
  obj->set_last_notification(-2);
  obj->set_notification_interval(0);
}
}  // namespace com::centreon::engine::configuration
