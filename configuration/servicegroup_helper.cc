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
#include "configuration/servicegroup_helper.hh"

namespace com::centreon::engine::configuration {
servicegroup_helper::servicegroup_helper(Servicegroup* obj)
    : message_helper(object_type::servicegroup, obj, {}, 10) {
  init_servicegroup(static_cast<Servicegroup*>(mut_obj()));
}

bool servicegroup_helper::hook(const absl::string_view& key,
                               const absl::string_view& value) {
  Message* obj = mut_obj();
  if (key == "members") {
    fill_pair_string_group(obj->mutable_members(), value);
    return true;
  } else if (key == "servicegroup_members") {
    fill_string_group(obj->mutable_servicegroup_members(), value);
    return true;
  }
  return false;
}
}  // namespace com::centreon::engine::configuration
