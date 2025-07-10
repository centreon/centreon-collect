/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/contact_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

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
                     Contact::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of Contact objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool contact_helper::hook(std::string_view key, std::string_view value) {
  Contact* obj = static_cast<Contact*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);
  if (key == "contact_name") {
    obj->set_contact_name(std::string(value));
    set_changed(obj->descriptor()->FindFieldByName("contact_name")->index());
    if (obj->alias().empty()) {
      obj->set_alias(obj->contact_name());
      set_changed(obj->descriptor()->FindFieldByName("alias")->index());
    }
    return true;
  } else if (key == "host_notification_options") {
    uint16_t options = action_hst_none;
    if (fill_host_notification_options(&options, value)) {
      obj->set_host_notification_options(options);
      set_changed(obj->descriptor()
                      ->FindFieldByName("host_notification_options")
                      ->index());
      return true;
    } else
      return false;
  } else if (key == "service_notification_options") {
    uint16_t options = action_svc_none;
    if (fill_service_notification_options(&options, value)) {
      obj->set_service_notification_options(options);
      set_changed(obj->descriptor()
                      ->FindFieldByName("service_notification_options")
                      ->index());
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
  } else if (key.compare(0, 7, "address") == 0) {
    obj->add_address(value.data(), value.size());
    set_changed(obj->descriptor()->FindFieldByName("address")->index());
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the Contact object.
 *
 * @param err An error counter.
 */
void contact_helper::check_validity(error_cnt& err) const {
  const Contact* o = static_cast<const Contact*>(obj());

  if (o->contact_name().empty()) {
    err.config_errors++;
    throw msg_fmt("Contact has no name (property 'contact_name')");
  }
}

/**
 * @brief Initializer of the Contact object, in other words set its default
 * values.
 */
void contact_helper::_init() {
  Contact* obj = static_cast<Contact*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
  obj->set_can_submit_commands(true);
  obj->set_host_notifications_enabled(true);
  obj->set_host_notification_options(action_hst_none);
  obj->set_retain_nonstatus_information(true);
  obj->set_retain_status_information(true);
  obj->set_service_notification_options(action_svc_none);
  obj->set_service_notifications_enabled(true);
}

/**
 * @brief If the provided key/value have their parsing to fail previously,
 * it is possible they are a customvariable. A customvariable name has its
 * name starting with an underscore. This method checks the possibility to
 * store a customvariable in the given object and stores it if possible.
 *
 * @param key   The name of the customvariable.
 * @param value Its value as a string.
 *
 * @return True if the customvariable has been well stored.
 */
bool contact_helper::insert_customvariable(std::string_view key,
                                           std::string_view value) {
  if (key[0] != '_')
    return false;

  key.remove_prefix(1);

  Contact* obj = static_cast<Contact*>(mut_obj());
  auto* cvs = obj->mutable_customvariables();
  for (auto& c : *cvs) {
    if (c.name() == key) {
      c.set_value(value.data(), value.size());
      return true;
    }
  }
  auto new_cv = cvs->Add();
  new_cv->set_name(key.data(), key.size());
  new_cv->set_value(value.data(), value.size());
  return true;
}

/**
 * @brief Expand the Contact object.
 *
 * @param s The configuration::State object.
 * @param err An error counter.
 */
void contact_helper::expand(
    configuration::State& s,
    configuration::error_cnt& err,
    const absl::flat_hash_map<std::string_view, configuration::Contactgroup*>&
        m_contactgroups) {
  // Browse all contacts.
  for (auto& c : *s.mutable_contacts()) {
    // Browse current contact's groups.
    for (auto& cg : *c.mutable_contactgroups()->mutable_data()) {
      // Find contact group.
      auto found_cg = m_contactgroups.find(cg);
      if (found_cg == m_contactgroups.end()) {
        err.config_errors++;
        throw msg_fmt(
            "Could not add contact '{}' to non-existing contact group '{}'",
            c.contact_name(), cg);
      }
      fill_string_group(found_cg->second->mutable_members(), c.contact_name());
    }
  }
}
}  // namespace com::centreon::engine::configuration
