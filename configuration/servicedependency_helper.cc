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
  init_servicedependency(static_cast<Servicedependency*>(mut_obj()));
}

bool servicedependency_helper::hook(const absl::string_view& key,
                                    const absl::string_view& value) {
  Message* obj = mut_obj();
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
}  // namespace com::centreon::engine::configuration
