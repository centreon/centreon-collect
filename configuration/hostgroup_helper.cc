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
#include "configuration/hostgroup_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
hostgroup_helper::hostgroup_helper(Hostgroup* obj)
    : message_helper(object_type::hostgroup, obj, {}, 9) {
  init_hostgroup(static_cast<Hostgroup*>(mut_obj()));
}

bool hostgroup_helper::hook(const absl::string_view& key,
                            const absl::string_view& value) {
  Hostgroup* obj = static_cast<Hostgroup*>(mut_obj());
  if (key == "members") {
    fill_string_group(obj->mutable_members(), value);
    return true;
  }
  return false;
}
void hostgroup_helper::check_validity() const {
  const Hostgroup* o = static_cast<const Hostgroup*>(obj());
}
}  // namespace com::centreon::engine::configuration