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
#include "configuration/hostescalation_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
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
                     10) {
  init_hostescalation(static_cast<Hostescalation*>(mut_obj()));
}

bool hostescalation_helper::hook(const absl::string_view& key,
                                 const absl::string_view& value) {
  Hostescalation* obj = static_cast<Hostescalation*>(mut_obj());
  if (key == "contactgroups") {
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
void hostescalation_helper::check_validity() const {
  const Hostescalation* o = static_cast<const Hostescalation*>(obj());

  if (o->obj().register_()) {
    if (o->hosts().data().empty() && o->hostgroups().data().empty())
      throw msg_fmt(
          "Hostescalation must contain at least one of the fields 'hosts' or "
          "'hostgroups' not empty");
  }
}
}  // namespace com::centreon::engine::configuration