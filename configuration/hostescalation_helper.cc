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
}  // namespace com::centreon::engine::configuration
