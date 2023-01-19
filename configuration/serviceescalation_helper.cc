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
#include "configuration/serviceescalation_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
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
                     12) {
  init_serviceescalation(static_cast<Serviceescalation*>(mut_obj()));
}

bool serviceescalation_helper::hook(const absl::string_view& key,
                                    const absl::string_view& value) {
  Serviceescalation* obj = static_cast<Serviceescalation*>(mut_obj());
  if (key == "contactgroups") {
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
void serviceescalation_helper::check_validity() const {
  const Serviceescalation* o = static_cast<const Serviceescalation*>(obj());
}
}  // namespace com::centreon::engine::configuration
