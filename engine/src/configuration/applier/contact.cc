/**
 * Copyright 2011-2015,2017-2024 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/configuration/applier/contact.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/deleter/listmember.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

#ifdef LEGACY_CONF
/**
 *  Add new contact.
 *
 *  @param[in] obj  The new contact to add into the monitoring engine.
 */
void applier::contact::add_object(configuration::contact const& obj) {
  // Make sure we have the data we need.
  if (obj.contact_name().empty())
    throw engine_error() << "Could not register contact with an empty name";

  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new contact '" << obj.contact_name() << "'.";
  config_logger->debug("Creating new contact '{}'.", obj.contact_name());

  // Add contact to the global configuration set.
  config->contacts().insert(obj);

  // Create address list.
  std::vector<std::string> addresses;
  std::copy(obj.address().begin(), obj.address().end(),
            std::back_inserter(addresses));

  // Create contact.
  std::shared_ptr<com::centreon::engine::contact> c(add_contact(
      obj.contact_name(), obj.alias(), obj.email(), obj.pager(),
      std::move(addresses), obj.service_notification_period(),
      obj.host_notification_period(),
      static_cast<bool>(obj.service_notification_options() & service::ok),
      static_cast<bool>(obj.service_notification_options() & service::critical),
      static_cast<bool>(obj.service_notification_options() & service::warning),
      static_cast<bool>(obj.service_notification_options() & service::unknown),
      static_cast<bool>(obj.service_notification_options() & service::flapping),
      static_cast<bool>(obj.service_notification_options() & service::downtime),
      static_cast<bool>(obj.host_notification_options() & host::up),
      static_cast<bool>(obj.host_notification_options() & host::down),
      static_cast<bool>(obj.host_notification_options() & host::unreachable),
      static_cast<bool>(obj.host_notification_options() & host::flapping),
      static_cast<bool>(obj.host_notification_options() & host::downtime),
      obj.host_notifications_enabled(), obj.service_notifications_enabled(),
      obj.can_submit_commands(), obj.retain_status_information(),
      obj.retain_nonstatus_information()));
  if (!c)
    throw(engine_error() << "Could not register contact '" << obj.contact_name()
                         << "'");
  c->set_timezone(obj.timezone());

  // Add new items to the configuration state.
  engine::contact::contacts.insert({c->get_name(), c});

  // Add all custom variables.
  for (auto it = obj.customvariables().begin(),
            end = obj.customvariables().end();
       it != end; ++it) {
    auto& cv = c->get_custom_variables()[it->first];
    cv.set_value(it->second.value());
    cv.set_sent(it->second.is_sent());

    if (it->second.is_sent()) {
      timeval tv(get_broker_timestamp(nullptr));
      broker_custom_variable(NEBTYPE_CONTACTCUSTOMVARIABLE_ADD, c.get(),
                             it->first, it->second.value(), &tv);
    }
  }
}
#else
/**
 *  Add new contact.
 *
 *  @param[in] obj  The new contact to add into the monitoring engine.
 */
void applier::contact::add_object(const configuration::Contact& obj) {
  // Make sure we have the data we need.
  if (obj.contact_name().empty())
    throw engine_error() << "Could not register contact with an empty name";

  // Logging.
  config_logger->debug("Creating new contact '{}'.", obj.contact_name());

  // Add contact to the global configuration set.
  configuration::Contact* ct_cfg = pb_config.add_contacts();
  ct_cfg->CopyFrom(obj);

  // Create address list.
  std::vector<std::string> addresses;
  std::copy(obj.address().begin(), obj.address().end(),
            std::back_inserter(addresses));

  // Create contact.
  std::shared_ptr<com::centreon::engine::contact> c(add_contact(
      obj.contact_name(), obj.alias(), obj.email(), obj.pager(),
      std::move(addresses), obj.service_notification_period(),
      obj.host_notification_period(),
      static_cast<bool>(obj.service_notification_options() & action_svc_ok),
      static_cast<bool>(obj.service_notification_options() &
                        action_svc_critical),
      static_cast<bool>(obj.service_notification_options() &
                        action_svc_warning),
      static_cast<bool>(obj.service_notification_options() &
                        action_svc_unknown),
      static_cast<bool>(obj.service_notification_options() &
                        action_svc_flapping),
      static_cast<bool>(obj.service_notification_options() &
                        action_svc_downtime),
      static_cast<bool>(obj.host_notification_options() & action_hst_up),
      static_cast<bool>(obj.host_notification_options() & action_hst_down),
      static_cast<bool>(obj.host_notification_options() &
                        action_hst_unreachable),
      static_cast<bool>(obj.host_notification_options() & action_hst_flapping),
      static_cast<bool>(obj.host_notification_options() & action_hst_downtime),
      obj.host_notifications_enabled(), obj.service_notifications_enabled(),
      obj.can_submit_commands(), obj.retain_status_information(),
      obj.retain_nonstatus_information()));
  if (!c)
    throw engine_error() << "Could not register contact '" << obj.contact_name()
                         << "'";
  c->set_timezone(obj.timezone());

  // Add new items to the configuration state.
  engine::contact::contacts.insert({c->get_name(), c});

  // Add all custom variables.
  for (const configuration::CustomVariable& cv : obj.customvariables()) {
    c->get_custom_variables()[cv.name()] =
        engine::customvariable(cv.value(), cv.is_sent());

    if (cv.is_sent()) {
      timeval tv(get_broker_timestamp(nullptr));
      broker_custom_variable(NEBTYPE_CONTACTCUSTOMVARIABLE_ADD, c.get(),
                             cv.name(), cv.value(), &tv);
    }
  }
}
#endif

#ifdef LEGACY_CONF
/**
 *  Modified contact.
 *
 *  @param[in] obj  The new contact to modify into the monitoring engine.
 */
void applier::contact::modify_object(configuration::contact const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Modifying contact '" << obj.contact_name() << "'.";
  config_logger->debug("Modifying contact '{}'.", obj.contact_name());

  // Find old configuration.
  set_contact::iterator it_cfg(config->contacts_find(obj.key()));
  if (it_cfg == config->contacts().end())
    throw engine_error() << "Cannot modify non-existing contact '"
                         << obj.contact_name() << "'";

  // Find contact object.
  contact_map::iterator it_obj(engine::contact::contacts.find(obj.key()));
  if (it_obj == engine::contact::contacts.end())
    throw engine_error() << "Could not modify non-existing "
                         << "contact object '" << obj.contact_name() << "'";
  engine::contact* c(it_obj->second.get());

  // Update the global configuration set.
  configuration::contact old_cfg(*it_cfg);
  config->contacts().erase(it_cfg);
  config->contacts().insert(obj);

  // Modify contact.
  std::string const& tmp(obj.alias().empty() ? obj.contact_name()
                                             : obj.alias());
  if (c->get_alias() != tmp)
    c->set_alias(tmp);
  if (c->get_email() != obj.email())
    c->set_email(obj.email());
  if (c->get_pager() != obj.pager())
    c->set_pager(obj.pager());
  if (c->get_addresses() != obj.address())
    c->set_addresses(obj.address());
  c->set_notify_on(
      notifier::service_notification,
      (obj.service_notification_options() & service::unknown ? notifier::unknown
                                                             : notifier::none) |
          (obj.service_notification_options() & service::warning
               ? notifier::warning
               : notifier::none) |
          (obj.service_notification_options() & service::critical
               ? notifier::critical
               : notifier::none) |
          (obj.service_notification_options() & service::ok ? notifier::ok
                                                            : notifier::none) |
          (obj.service_notification_options() & service::flapping
               ? (notifier::flappingstart | notifier::flappingstop |
                  notifier::flappingdisabled)
               : notifier::none) |
          (obj.service_notification_options() & service::downtime
               ? notifier::downtime
               : notifier::none));
  c->set_notify_on(
      notifier::host_notification,
      (obj.host_notification_options() & host::down ? notifier::down
                                                    : notifier::none) |
          (obj.host_notification_options() & host::unreachable
               ? notifier::unreachable
               : notifier::none) |
          (obj.host_notification_options() & host::up ? notifier::up
                                                      : notifier::none) |
          (obj.host_notification_options() & host::flapping
               ? (notifier::flappingstart | notifier::flappingstop |
                  notifier::flappingdisabled)
               : notifier::none) |
          (obj.host_notification_options() & host::downtime ? notifier::downtime
                                                            : notifier::none));
  if (c->get_host_notification_period() != obj.host_notification_period())
    c->set_host_notification_period(obj.host_notification_period());
  if (c->get_service_notification_period() != obj.service_notification_period())
    c->set_service_notification_period(obj.service_notification_period());
  if (c->get_host_notifications_enabled() != obj.host_notifications_enabled())
    c->set_host_notifications_enabled(obj.host_notifications_enabled());
  if (c->get_service_notifications_enabled() !=
      obj.service_notifications_enabled())
    c->set_service_notifications_enabled(obj.service_notifications_enabled());
  if (c->get_can_submit_commands() != obj.can_submit_commands())
    c->set_can_submit_commands(obj.can_submit_commands());
  if (c->get_retain_status_information() != obj.retain_status_information())
    c->set_retain_status_information(obj.retain_status_information());
  if (c->get_retain_nonstatus_information() !=
      obj.retain_nonstatus_information())
    c->set_retain_nonstatus_information(obj.retain_nonstatus_information());
  c->set_timezone(obj.timezone());

  // Host notification commands.
  if (obj.host_notification_commands() !=
      old_cfg.host_notification_commands()) {
    c->get_host_notification_commands().clear();

    for (list_string::const_iterator
             it(obj.host_notification_commands().begin()),
         end(obj.host_notification_commands().end());
         it != end; ++it) {
      command_map::const_iterator itt(commands::command::commands.find(*it));
      if (itt != commands::command::commands.end())
        c->get_host_notification_commands().push_back(itt->second);
      else
        throw(engine_error() << "Could not add host notification command '"
                             << *it << "' to contact '" << obj.contact_name()
                             << "': the command does not exist");
    }
  }

  // Service notification commands.
  if (obj.service_notification_commands() !=
      old_cfg.service_notification_commands()) {
    c->get_service_notification_commands().clear();

    for (list_string::const_iterator
             it(obj.service_notification_commands().begin()),
         end(obj.service_notification_commands().end());
         it != end; ++it) {
      command_map::const_iterator itt(commands::command::commands.find(*it));
      if (itt != commands::command::commands.end())
        c->get_service_notification_commands().push_back(itt->second);
      else
        throw(engine_error() << "Could not add service notification command '"
                             << *it << "' to contact '" << obj.contact_name()
                             << "': the command does not exist");
    }
  }

  // Custom variables.
  if (std::operator!=(obj.customvariables(), old_cfg.customvariables())) {
    for (auto& cus : c->get_custom_variables()) {
      if (cus.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_CONTACTCUSTOMVARIABLE_DELETE, c,
                               cus.first, cus.second.value(), &tv);
      }
    }
    c->get_custom_variables().clear();

    for (auto& cus : obj.customvariables()) {
      auto& cv = c->get_custom_variables()[cus.first];
      cv.set_value(cus.second.value());
      cv.set_sent(cus.second.is_sent());

      if (cus.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_CONTACTCUSTOMVARIABLE_ADD, c, cus.first,
                               cus.second.value(), &tv);
      }
    }
  }

  // Notify event broker.
  timeval tv(get_broker_timestamp(NULL));
  broker_adaptive_contact_data(NEBTYPE_CONTACT_UPDATE, NEBFLAG_NONE,
                               NEBATTR_NONE, c, CMD_NONE, MODATTR_ALL,
                               MODATTR_ALL, MODATTR_ALL, MODATTR_ALL,
                               MODATTR_ALL, MODATTR_ALL, &tv);
}
#else
/**
 * @brief Modify a contact from a protobuf contact configuration.
 *
 * @param obj The new contact to modify into the monitoring engine.
 */
void applier::contact::modify_object(configuration::Contact* to_modify,
                                     const configuration::Contact& new_object) {
  // Logging.
  config_logger->debug("Modifying contact '{}'.", new_object.contact_name());

  // Find contact object.
  contact_map::iterator it_obj(
      engine::contact::contacts.find(to_modify->contact_name()));
  if (it_obj == engine::contact::contacts.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing contact object '{}'",
        to_modify->contact_name());
  engine::contact* c = it_obj->second.get();

  // Update the global configuration set.
  to_modify->CopyFrom(new_object);

  // Modify contact.
  const std::string& tmp(new_object.alias().empty() ? new_object.contact_name()
                                                    : new_object.alias());
  if (c->get_alias() != tmp)
    c->set_alias(tmp);
  if (c->get_email() != new_object.email())
    c->set_email(new_object.email());
  if (c->get_pager() != new_object.pager())
    c->set_pager(new_object.pager());

  std::vector<std::string> addr;
  addr.reserve(new_object.address().size());
  std::copy(new_object.address().begin(), new_object.address().end(),
            std::back_inserter(addr));
  c->set_addresses(std::move(addr));

  c->set_notify_on(
      notifier::service_notification,
      (new_object.service_notification_options() & action_svc_unknown
           ? notifier::unknown
           : notifier::none) |
          (new_object.service_notification_options() & action_svc_warning
               ? notifier::warning
               : notifier::none) |
          (new_object.service_notification_options() & action_svc_critical
               ? notifier::critical
               : notifier::none) |
          (new_object.service_notification_options() & action_svc_ok
               ? notifier::ok
               : notifier::none) |
          (new_object.service_notification_options() & action_svc_flapping
               ? (notifier::flappingstart | notifier::flappingstop |
                  notifier::flappingdisabled)
               : notifier::none) |
          (new_object.service_notification_options() & action_svc_downtime
               ? notifier::downtime
               : notifier::none));
  c->set_notify_on(
      notifier::host_notification,
      (new_object.host_notification_options() & action_hst_down
           ? notifier::down
           : notifier::none) |
          (new_object.host_notification_options() & action_hst_unreachable
               ? notifier::unreachable
               : notifier::none) |
          (new_object.host_notification_options() & action_hst_up
               ? notifier::up
               : notifier::none) |
          (new_object.host_notification_options() & action_hst_flapping
               ? (notifier::flappingstart | notifier::flappingstop |
                  notifier::flappingdisabled)
               : notifier::none) |
          (new_object.host_notification_options() & action_hst_downtime
               ? notifier::downtime
               : notifier::none));
  if (c->get_host_notification_period() !=
      new_object.host_notification_period())
    c->set_host_notification_period(new_object.host_notification_period());
  if (c->get_service_notification_period() !=
      new_object.service_notification_period())
    c->set_service_notification_period(
        new_object.service_notification_period());
  if (c->get_host_notifications_enabled() !=
      new_object.host_notifications_enabled())
    c->set_host_notifications_enabled(new_object.host_notifications_enabled());
  if (c->get_service_notifications_enabled() !=
      new_object.service_notifications_enabled())
    c->set_service_notifications_enabled(
        new_object.service_notifications_enabled());
  if (c->get_can_submit_commands() != new_object.can_submit_commands())
    c->set_can_submit_commands(new_object.can_submit_commands());
  if (c->get_retain_status_information() !=
      new_object.retain_status_information())
    c->set_retain_status_information(new_object.retain_status_information());
  if (c->get_retain_nonstatus_information() !=
      new_object.retain_nonstatus_information())
    c->set_retain_nonstatus_information(
        new_object.retain_nonstatus_information());
  c->set_timezone(new_object.timezone());

  // Host notification commands.
  if (!MessageDifferencer::Equals(new_object.host_notification_commands(),
                                  to_modify->host_notification_commands())) {
    c->get_host_notification_commands().clear();

    for (auto& cfg_c : new_object.host_notification_commands().data()) {
      command_map::const_iterator itt = commands::command::commands.find(cfg_c);
      if (itt != commands::command::commands.end())
        c->get_host_notification_commands().push_back(itt->second);
      else
        throw engine_error() << fmt::format(
            "Could not add host notification command '{}' to contact '{}': the "
            "command does not exist",
            cfg_c, new_object.contact_name());
    }
  }

  // Service notification commands.
  if (!MessageDifferencer::Equals(new_object.service_notification_commands(),
                                  to_modify->service_notification_commands())) {
    c->get_service_notification_commands().clear();

    for (auto& cfg_c : new_object.service_notification_commands().data()) {
      command_map::const_iterator itt = commands::command::commands.find(cfg_c);
      if (itt != commands::command::commands.end())
        c->get_service_notification_commands().push_back(itt->second);
      else
        throw engine_error() << fmt::format(
            "Could not add service notification command '{}' to contact '{}': "
            "the command does not exist",
            cfg_c, new_object.contact_name());
    }
  }

  // Custom variables.
  absl::flat_hash_set<std::string> keys;
  for (auto& cfg_cv : new_object.customvariables()) {
    keys.emplace(cfg_cv.name());
    auto found = c->get_custom_variables().find(cfg_cv.name());
    if (found != c->get_custom_variables().end()) {
      if (found->second.value() != cfg_cv.value() ||
          found->second.is_sent() != cfg_cv.is_sent() ||
          found->second.has_been_modified()) {
        found->second.set_value(cfg_cv.value());
        found->second.set_sent(cfg_cv.is_sent());
        if (found->second.is_sent()) {
          timeval tv(get_broker_timestamp(nullptr));
          broker_custom_variable(NEBTYPE_CONTACTCUSTOMVARIABLE_DELETE, c,
                                 found->first.c_str(),
                                 found->second.value().c_str(), &tv);
          broker_custom_variable(NEBTYPE_CONTACTCUSTOMVARIABLE_ADD, c,
                                 found->first.c_str(),
                                 found->second.value().c_str(), &tv);
        }
      }
    } else {
      c->get_custom_variables().emplace(
          cfg_cv.name(),
          engine::customvariable(cfg_cv.value(), cfg_cv.is_sent()));
      if (cfg_cv.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_CONTACTCUSTOMVARIABLE_ADD, c,
                               cfg_cv.name().c_str(), cfg_cv.value().c_str(),
                               &tv);
      }
    }
  }
  if (static_cast<uint32_t>(new_object.customvariables().size()) !=
      c->get_custom_variables().size()) {
    // There are custom variables to remove...
    for (auto it = c->get_custom_variables().begin();
         it != c->get_custom_variables().end();) {
      if (!keys.contains(it->first)) {
        if (it->second.is_sent()) {
          timeval tv(get_broker_timestamp(nullptr));
          broker_custom_variable(NEBTYPE_CONTACTCUSTOMVARIABLE_DELETE, c,
                                 it->first.c_str(), it->second.value().c_str(),
                                 &tv);
        }
        it = c->get_custom_variables().erase(it);
      } else
        ++it;
    }
  }
}
#endif

#ifdef LEGACY_CONF
/**
 *  Remove old contact.
 *
 *  @param[in] obj  The new contact to remove from the monitoring engine.
 */
void applier::contact::remove_object(configuration::contact const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing contact '" << obj.contact_name() << "'.";
  config_logger->debug("Removing contact '{}'.", obj.contact_name());

  // Find contact.
  contact_map::iterator it{engine::contact::contacts.find(obj.key())};
  if (it != engine::contact::contacts.end()) {
    engine::contact* cntct(it->second.get());

    for (auto& it_c : it->second->get_parent_groups())
      it_c.second->get_members().erase(obj.contact_name());

    // Notify event broker.
    timeval tv(get_broker_timestamp(nullptr));
    broker_adaptive_contact_data(NEBTYPE_CONTACT_DELETE, NEBFLAG_NONE,
                                 NEBATTR_NONE, cntct, CMD_NONE, MODATTR_ALL,
                                 MODATTR_ALL, MODATTR_ALL, MODATTR_ALL,
                                 MODATTR_ALL, MODATTR_ALL, &tv);

    // Erase contact object (this will effectively delete the object).
    engine::contact::contacts.erase(it);
  }

  // Remove contact from the global configuration set.
  config->contacts().erase(obj);
}
#else
/**
 *  Remove old contact.
 *
 *  @param[in] obj  The new contact to remove from the monitoring engine.
 */
void applier::contact::remove_object(ssize_t idx) {
  const configuration::Contact& obj = pb_config.contacts()[idx];

  // Logging.
  config_logger->debug("Removing contact '{}'.", obj.contact_name());

  // Find contact.
  contact_map::iterator it{engine::contact::contacts.find(obj.contact_name())};
  if (it != engine::contact::contacts.end()) {
    engine::contact* cntct(it->second.get());

    for (auto& it_c : cntct->get_parent_groups())
      it_c.second->get_members().erase(obj.contact_name());

    // Erase contact object (this will effectively delete the object).
    engine::contact::contacts.erase(it);
  }

  // Remove contact from the global configuration set.
  pb_config.mutable_contacts()->DeleteSubrange(idx, 1);
}
#endif

#ifdef LEGACY_CONF
/**
 *  @brief Expand a contact.
 *
 *  During expansion, the contact will be added to its contact groups.
 *  These will be modified in the state.
 *
 *  @param[in,out] s  Configuration state.
 */
void applier::contact::expand_objects(configuration::state& s) {
  // Browse all contacts.
  configuration::set_contact new_contacts;
  // We loop on s.contacts() but we make a copy of each element because the
  // container is a set and changing its element has an impact on the order
  // they are stored. So we copy each element, modify them and move the result
  // to a new set.
  for (auto contact : s.contacts()) {
    // Should custom variables be sent to broker ?
    auto& mcv = contact.mutable_customvariables();
    for (auto& cv : mcv) {
      if (!s.enable_macros_filter() ||
          s.macros_filter().find(cv.first) != s.macros_filter().end()) {
        cv.second.set_sent(true);
      }
    }

    // Browse current contact's groups.
    for (auto& g : contact.contactgroups()) {
      // Find contact group.
      configuration::set_contactgroup::iterator group(s.contactgroups_find(g));
      if (group == s.contactgroups().end())
        throw(engine_error()
              << "Could not add contact '" << contact.contact_name()
              << "' to non-existing contact group '" << g << "'");

      // Remove contact group from state.
      configuration::contactgroup backup(*group);
      s.contactgroups().erase(group);

      // Add contact to group members.
      backup.members().insert(contact.contact_name());

      // Reinsert contact group.
      s.contactgroups().insert(backup);
    }
    new_contacts.insert(std::move(contact));
  }
  s.contacts() = std::move(new_contacts);
}
#else
/**
 *  @brief Expand a contact.
 *
 *  During expansion, the contact will be added to its contact groups.
 *  These will be modified in the state.
 *
 *  @param[in,out] s  Configuration state.
 */
void applier::contact::expand_objects(configuration::State& s) {
  // Let's consider all the macros defined in s.
  absl::flat_hash_set<std::string_view> cvs;
  for (auto& cv : s.macros_filter().data())
    cvs.emplace(cv);

  // Browse all contacts.
  for (auto& c : *s.mutable_contacts()) {
    // Should custom variables be sent to broker ?
    for (auto& cv : *c.mutable_customvariables()) {
      if (!s.enable_macros_filter() || cvs.contains(cv.name()))
        cv.set_is_sent(true);
    }

    // Browse current contact's groups.
    for (auto& cg : *c.mutable_contactgroups()->mutable_data()) {
      // Find contact group.
      Contactgroup* found_cg = nullptr;
      for (auto& cgg : *s.mutable_contactgroups())
        if (cgg.contactgroup_name() == cg) {
          found_cg = &cgg;
          break;
        }
      if (found_cg == nullptr)
        throw engine_error() << fmt::format(
            "Could not add contact '{}' to non-existing contact group '{}'",
            c.contact_name(), cg);

      fill_string_group(found_cg->mutable_members(), c.contact_name());
    }
  }
}
#endif

#ifdef LEGACY_CONF
/**
 *  Resolve a contact.
 *
 *  @param[in,out] obj  Object to resolve.
 */
void applier::contact::resolve_object(const configuration::contact& obj,
                                      error_cnt& err) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving contact '" << obj.contact_name() << "'.";
  config_logger->debug("Resolving contact '{}'.", obj.contact_name());

  // Find contact.
  contact_map::const_iterator ct_it{
      engine::contact::contacts.find(obj.contact_name())};
  if (ct_it == engine::contact::contacts.end() || !ct_it->second)
    throw(engine_error() << "Cannot resolve non-existing contact '"
                         << obj.contact_name() << "'");

  ct_it->second->get_host_notification_commands().clear();

  // Add all the host notification commands.
  for (list_string::const_iterator it(obj.host_notification_commands().begin()),
       end(obj.host_notification_commands().end());
       it != end; ++it) {
    command_map::const_iterator itt(commands::command::commands.find(*it));
    if (itt != commands::command::commands.end())
      ct_it->second->get_host_notification_commands().push_back(itt->second);
    else {
      ++err.config_errors;
      throw(engine_error() << "Could not add host notification command '" << *it
                           << "' to contact '" << obj.contact_name()
                           << "': the command does not exist");
    }
  }

  ct_it->second->get_service_notification_commands().clear();

  // Add all the service notification commands.
  for (list_string::const_iterator
           it(obj.service_notification_commands().begin()),
       end(obj.service_notification_commands().end());
       it != end; ++it) {
    command_map::const_iterator itt(commands::command::commands.find(*it));
    if (itt != commands::command::commands.end())
      ct_it->second->get_service_notification_commands().push_back(itt->second);
    else {
      ++err.config_errors;
      throw(engine_error() << "Could not add service notification command '"
                           << *it << "' to contact '" << obj.contact_name()
                           << "': the command does not exist");
    }
  }

  // Remove contact group links.
  ct_it->second->get_parent_groups().clear();

  // Resolve contact.
  ct_it->second->resolve(err.config_warnings, err.config_errors);
}
#else
/**
 *  Resolve a contact.
 *
 *  @param[in,out] obj  Object to resolve.
 */
void applier::contact::resolve_object(const configuration::Contact& obj,
                                      error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving contact '{}'.", obj.contact_name());

  // Find contact.
  contact_map::const_iterator ct_it{
      engine::contact::contacts.find(obj.contact_name())};
  if (ct_it == engine::contact::contacts.end() || !ct_it->second)
    throw engine_error() << fmt::format(
        "Cannot resolve non-existing contact '{}'", obj.contact_name());

  ct_it->second->get_host_notification_commands().clear();

  // Add all the host notification commands.
  for (auto& cmd : obj.host_notification_commands().data()) {
    command_map::const_iterator itt(commands::command::commands.find(cmd));
    if (itt != commands::command::commands.end())
      ct_it->second->get_host_notification_commands().push_back(itt->second);
    else {
      ++err.config_errors;
      throw engine_error() << fmt::format(
          "Could not add host notification command '{}' to contact '{}': the "
          "command does not exist",
          cmd, obj.contact_name());
    }
  }

  ct_it->second->get_service_notification_commands().clear();

  // Add all the service notification commands.
  for (auto& cmd : obj.service_notification_commands().data()) {
    command_map::const_iterator itt(commands::command::commands.find(cmd));
    if (itt != commands::command::commands.end())
      ct_it->second->get_service_notification_commands().push_back(itt->second);
    else {
      ++err.config_errors;
      throw engine_error() << fmt::format(
          "Could not add service notification command '{}' to contact '{}': "
          "the command does not exist",
          cmd, obj.contact_name());
    }
  }

  // Remove contact group links.
  ct_it->second->get_parent_groups().clear();

  // Resolve contact.
  ct_it->second->resolve(err.config_warnings, err.config_errors);
}
#endif
