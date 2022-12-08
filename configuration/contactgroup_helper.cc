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
#include "configuration/contactgroup_helper.hh"

namespace com::centreon::engine::configuration {
contactgroup_helper::contactgroup_helper(Contactgroup* obj)
    : message_helper(object_type::contactgroup, obj, {}, 6) {
  init_contactgroup(static_cast<Contactgroup*>(mut_obj()));
}

bool contactgroup_helper::hook(const absl::string_view& k,
                               const absl::string_view& value) {
  Contactgroup* obj = static_cast<Contactgroup*>(mut_obj());
  absl::string_view key;
  {
    auto it = correspondence().find(k);
    if (it != correspondence().end())
      key = it->second;
    else
      key = k;
  }
  if (key == "contactgroup_members") {
    fill_string_group(obj->mutable_contactgroup_members(), value);
    return true;
  } else if (key == "members") {
    fill_string_group(obj->mutable_members(), value);
    return true;
  }
  return false;
}
}  // namespace com::centreon::engine::configuration
