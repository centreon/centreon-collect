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
#include "configuration/contact_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {
contact_helper::contact_helper(Contact* obj)
    : message_helper(object_type::contact,
                     obj,
                     {
                         {"contact_groups", "contactgroups"},
                     },
                     21) {
  init_contact(static_cast<Contact*>(mut_obj()));
}

bool contact_helper::hook(const absl::string_view& key,
                          const absl::string_view& value) {
  Contact* obj = static_cast<Contact*>(mut_obj());
  if (key == "contactgroups") {
    fill_string_group(obj->mutable_contactgroups(), value);
    return true;
  } else if (key == "host_notification_commands") {
    fill_string_group(obj->mutable_host_notification_commands(), value);
    return true;
  } else if (key == "service_notification_commands") {
    fill_string_group(obj->mutable_service_notification_commands(), value);
    return true;
  }
  return false;
}
void contact_helper::check_validity() const {
  const Contact* o = static_cast<const Contact*>(obj());

  if (o->contact_name().empty())
    throw msg_fmt("Contact has no name (property 'contact_name')");
}
}  // namespace com::centreon::engine::configuration
