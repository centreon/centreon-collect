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
#include "configuration/severity_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
severity_helper::severity_helper(Severity* obj)
    : message_helper(object_type::severity,
                     obj,
                     {
                         {"severity_name", "name"},
                         {"severity_id", "id"},
                         {"severity_level", "level"},
                         {"severity_icon_id", "icon_id"},
                     },
                     6) {
  init_severity(static_cast<Severity*>(mut_obj()));
}

bool severity_helper::hook(const absl::string_view& key,
                           const absl::string_view& value) {
  Severity* obj = static_cast<Severity*>(mut_obj());
  return false;
}
void severity_helper::check_validity() const {
  const Severity* o = static_cast<const Severity*>(obj());
}
}  // namespace com::centreon::engine::configuration
