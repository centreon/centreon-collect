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
#include "configuration/host_helper.hh"

namespace com::centreon::engine::configuration {
host_helper::host_helper(Host* obj)
    : message_helper(object_type::host,
                     obj,
                     {
                         {"_HOST_ID", "host_id"},
                         {"host_groups", "hostgroups"},
                         {"contact_groups", "contactgroups"},
                         {"gd2_image", "statusmap_image"},
                         {"normal_check_interval", "check_interval"},
                         {"retry_check_interval", "retry_interval"},
                         {"checks_enabled", "checks_active"},
                         {"active_checks_enabled", "checks_active"},
                         {"passive_checks_enabled", "checks_passive"},
                         {"2d_coords", "coords_2d"},
                         {"3d_coords", "coords_3d"},
                         {"severity", "severity_id"},
                     },
                     53) {
  init_host(static_cast<Host*>(mut_obj()));
}

bool host_helper::hook(const absl::string_view& key,
                       const absl::string_view& value) {
  Message* obj = mut_obj();
  if (key == "contactgroups") {
    fill_string_group(obj->mutable_contactgroups(), value);
    return true;
  } else if (key == "contacts") {
    fill_string_group(obj->mutable_contacts(), value);
    return true;
  } else if (key == "hostgroups") {
    fill_string_group(obj->mutable_hostgroups(), value);
    return true;
  } else if (key == "parents") {
    fill_string_group(obj->mutable_parents(), value);
    return true;
  }
  return false;
}
}  // namespace com::centreon::engine::configuration
