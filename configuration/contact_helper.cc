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

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

/**
 * @brief Constructor from a Contact object.
 *
 * @param obj The Contact object on which this helper works. The helper is not
 * the owner of this object.
 */
contact_helper::contact_helper(Contact* obj)
    : message_helper(object_type::contact,
                     obj,
                     {
                         {"contact_groups", "contactgroups"},
                     },
                     21) {
  _init();
}

/**
 * @brief For several keys, the parser of Contact objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool contact_helper::hook(const absl::string_view& key,
                          const absl::string_view& value) {
  Contact* obj = static_cast<Contact*>(mut_obj());

  if (key == "host_notification_options") {
    uint32_t options;
    if (fill_host_notification_options(&options, value)) {
      obj->set_host_notification_options(options);
      return true;
    } else
      return false;
  } else if (key == "service_notification_options") {
    uint32_t options;
    if (fill_service_notification_options(&options, value)) {
      obj->set_service_notification_options(options);
      return true;
    } else
      return false;
  } else if (key == "contactgroups") {
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

/**
 * @brief Check the validity of the Contact object.
 */
void contact_helper::check_validity() const {
  const Contact* o = static_cast<const Contact*>(obj());

  if (o->contact_name().empty())
    throw msg_fmt("Contact has no name (property 'contact_name')");
}

/**
 * @brief Initializer of the Contact object, in other words set its default
 * values.
 */
void contact_helper::_init() {
  Contact* obj = static_cast<Contact*>(mut_obj());
  obj->set_can_submit_commands(true);
  obj->set_host_notifications_enabled(true);
  obj->set_host_notification_options(action_hst_none);
  obj->set_retain_nonstatus_information(true);
  obj->set_retain_status_information(true);
  obj->set_service_notification_options(action_svc_none);
  obj->set_service_notifications_enabled(true);
}
}  // namespace configuration
}  // namespace engine
}  // namespace centreon

}  // namespace com