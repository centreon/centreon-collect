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

/**
 * @brief Validate a Contact against the rest of the configuration.
 *
 * This is the configuration-level equivalent of the former
 * `engine::contact::resolve()`: it only accumulates warnings/errors into @a err
 * (it never throws) and performs no runtime wiring. It expects to run on a
 * post-`expand` State, and that `check_validity` has already rejected contacts
 * with no name. Checks performed: the host/service notification commands are
 * defined and exist, the host/service notification timeperiods exist, and the
 * recovery notification options are consistent.
 *
 * @param c The Contact to validate.
 * @param commands Names of every command defined in the configuration.
 * @param timeperiods Names of every timeperiod defined in the configuration.
 * @param illegal_chars Characters forbidden in object names (State's
 * illegal_object_chars).
 * @param err Warning/error counters, incremented in place.
 * @param logger Logger receiving the human-readable diagnostics.
 */
void contact_helper::resolve(
    const configuration::Contact& c,
    const absl::flat_hash_set<std::string_view>& commands,
    const absl::flat_hash_set<std::string_view>& timeperiods,
    std::string_view illegal_chars,
    configuration::error_cnt& err,
    const std::shared_ptr<spdlog::logger>& logger) {
  /* Some checks are already done by contact_helper::check_validity */

  /* check service notification commands */
  if (c.service_notification_commands().data_size() == 0) {
    logger->error(
        "Error: Contact '{}' has no service notification commands defined!",
        c.contact_name());
    err.config_errors++;
  } else {
    for (auto& cmd : c.service_notification_commands().data()) {
      if (!commands.contains(cmd)) {
        logger->error(
            "Error: Service notification command '{}' specified for contact "
            "'{}' is not defined anywhere!",
            cmd, c.contact_name());
        err.config_errors++;
      }
    }
  }

  /* check host notification commands */
  if (c.host_notification_commands().data_size() == 0) {
    logger->error(
        "Error: Contact '{}' has no host notification commands defined!",
        c.contact_name());
    err.config_errors++;
  } else {
    for (auto& cmd : c.host_notification_commands().data()) {
      if (!commands.contains(cmd)) {
        logger->error(
            "Error: Host notification command '{}' specified for contact '{}' "
            "is not defined anywhere!",
            cmd, c.contact_name());
        err.config_errors++;
      }
    }
  }

  /* check service notification timeperiod */
  if (c.service_notification_period().empty()) {
    logger->warn(
        "Warning: Contact '{}' has no service notification time period "
        "defined!",
        c.contact_name());
    err.config_warnings++;
  } else {
    if (!timeperiods.contains(c.service_notification_period())) {
      logger->error(
          "Error: Service notification period '{}' specified for contact '{}' "
          "is not defined anywhere!",
          c.service_notification_period(), c.contact_name());
      err.config_errors++;
    }
  }

  /* check host notification timeperiod */
  if (c.host_notification_period().empty()) {
    logger->warn(
        "Warning: Contact '{}' has no host notification time period defined!",
        c.contact_name());
    err.config_warnings++;
  } else {
    if (!timeperiods.contains(c.host_notification_period())) {
      logger->error(
          "Error: Host notification period '{}' specified for contact '{}' is "
          "not defined anywhere!",
          c.host_notification_period(), c.contact_name());
      err.config_errors++;
    }
  }

  /* check for sane host recovery options */
  if ((c.host_notification_options() & action_hst_up) &&
      !(c.host_notification_options() &
        (action_hst_down | action_hst_unreachable))) {
    logger->warn(
        "Warning: Host recovery notification option for contact '{}' doesn't "
        "make any sense - specify down "
        "and/or unreachable options as well",
        c.contact_name());
    err.config_warnings++;
  }

  /* check for sane service recovery options */
  if ((c.service_notification_options() & action_svc_ok) &&
      !(c.service_notification_options() &
        (action_svc_critical | action_svc_warning))) {
    logger->warn(
        "Warning: Service recovery notification option for contact '{}' "
        "doesn't make any sense - specify critical and/or warning options as "
        "well",
        c.contact_name());
    err.config_warnings++;
  }

  /* check for illegal characters in contact name */
  if (name_contains_illegal_chars(c.contact_name(), illegal_chars)) {
    logger->error(
        "Error: The name of contact '{}' contains one or more illegal "
        "characters.",
        c.contact_name());
    err.config_errors++;
  }
}

}  // namespace com::centreon::engine::configuration
