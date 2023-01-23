/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "configuration/servicedependency_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
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
          15) {
  _init();
}

/**
 * @brief For several keys, the parser of Servicedependency objects has a
 * particular behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool servicedependency_helper::hook(const absl::string_view& key,
                                    const absl::string_view& value) {
  Servicedependency* obj = static_cast<Servicedependency*>(mut_obj());
  if (key == "dependent_hostgroups") {
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
 */
void servicedependency_helper::check_validity() const {
  const Servicedependency* o = static_cast<const Servicedependency*>(obj());

  /* Check base service(s). */
  if (o->servicegroups().data().empty()) {
    if (o->service_description().data().empty())
      throw msg_fmt(
          "Service dependency is not attached to any service or service group "
          "(properties 'service_description' or 'servicegroup_name', "
          "respectively)");
    else if (o->hosts().data().empty() && o->hostgroups().data().empty())
      throw msg_fmt(
          "Service dependency is not attached to any host or host group "
          "(properties 'host_name' or 'hostgroup_name', respectively)");
  }

  /* Check dependent service(s). */
  if (o->dependent_servicegroups().data().empty()) {
    if (o->dependent_service_description().data().empty())
      throw msg_fmt(
          "Service dependency is not attached to "
          "any dependent service or dependent service group "
          "(properties 'dependent_service_description' or "
          "'dependent_servicegroup_name', respectively)");
    else if (o->dependent_hosts().data().empty() &&
             o->dependent_hostgroups().data().empty())
      throw msg_fmt(
          "Service dependency is not attached to "
          "any dependent host or dependent host group (properties "
          "'dependent_host_name' or 'dependent_hostgroup_name', "
          "respectively)");
  }
}
void servicedependency_helper::_init() {
  Servicedependency* obj = static_cast<Servicedependency*>(mut_obj());
  obj->set_execution_failure_options(action_sd_none);
  obj->set_inherits_parent(false);
  obj->set_notification_failure_options(action_sd_none);
}

}  // namespace com::centreon::engine::configuration