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
#include "configuration/hostdependency_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
hostdependency_helper::hostdependency_helper(Hostdependency* obj)
    : message_helper(
          object_type::hostdependency,
          obj,
          {
              {"hostgroup", "hostgroups"},
              {"hostgroup_name", "hostgroups"},
              {"host", "hosts"},
              {"host_name", "hosts"},
              {"master_host", "hosts"},
              {"master_host_name", "hosts"},
              {"dependent_hostgroup", "dependent_hostgroups"},
              {"dependent_hostgroup_name", "dependent_hostgroups"},
              {"dependent_host", "dependent_hosts"},
              {"dependent_host_name", "dependent_hosts"},
              {"notification_failure_criteria", "notification_failure_options"},
              {"execution_failure_criteria", "execution_failure_options"},
          },
          11) {
  init_hostdependency(static_cast<Hostdependency*>(mut_obj()));
}

bool hostdependency_helper::hook(const absl::string_view& key,
                                 const absl::string_view& value) {
  Hostdependency* obj = static_cast<Hostdependency*>(mut_obj());
  if (key == "dependent_hostgroups") {
    fill_string_group(obj->mutable_dependent_hostgroups(), value);
    return true;
  } else if (key == "dependent_hosts") {
    fill_string_group(obj->mutable_dependent_hosts(), value);
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
void hostdependency_helper::check_validity() const {
  const Hostdependency* o = static_cast<const Hostdependency*>(obj());

  if (o->hosts().data().empty() && o->hostgroups().data().empty())
    throw msg_fmt(
        "Host dependency is not attached to any host or host group (properties "
        "'hosts' or 'hostgroups', respectively)");

  if (o->dependent_hosts().data().empty() &&
      o->dependent_hostgroups().data().empty())
    throw msg_fmt(
        "Host dependency is not attached to any "
        "dependent host or dependent host group (properties "
        "'dependent_hosts' or 'dependent_hostgroups', "
        "respectively)");
}
}  // namespace com::centreon::engine::configuration
