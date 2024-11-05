/**
 * Copyright 2011-2019,2022-2024 Centreon
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

#include "com/centreon/engine/configuration/applier/host.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/scheduler.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/severity.hh"
#ifdef LEGACY_CONF
#include "common/engine_legacy_conf/host.hh"
#else
#include "common/engine_conf/severity_helper.hh"
#include "common/engine_conf/state.pb.h"
#endif

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

#ifdef LEGACY_CONF
/**
 *  Add new host.
 *
 *  @param[in] obj  The new host to add into the monitoring engine.
 */
void applier::host::add_object(const configuration::host& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new host '" << obj.host_name() << "'.";
  config_logger->debug("Creating new host '{}'.", obj.host_name());

  // Add host to the global configuration set.
  config->hosts().insert(obj);

  // Create host.
  auto h = std::make_shared<com::centreon::engine::host>(
      obj.host_id(), obj.host_name(), obj.display_name(), obj.alias(),
      obj.address(), obj.check_period(), obj.check_interval(),
      obj.retry_interval(), obj.max_check_attempts(),
      static_cast<bool>(obj.notification_options() & configuration::host::up),
      static_cast<bool>(obj.notification_options() & configuration::host::down),
      static_cast<bool>(obj.notification_options() &
                        configuration::host::unreachable),
      static_cast<bool>(obj.notification_options() &
                        configuration::host::flapping),
      static_cast<bool>(obj.notification_options() &
                        configuration::host::downtime),
      obj.notification_interval(), obj.first_notification_delay(),
      obj.recovery_notification_delay(), obj.notification_period(),
      obj.notifications_enabled(), obj.check_command(), obj.checks_active(),
      obj.checks_passive(), obj.event_handler(), obj.event_handler_enabled(),
      obj.flap_detection_enabled(), obj.low_flap_threshold(),
      obj.high_flap_threshold(),
      static_cast<bool>(obj.flap_detection_options() & configuration::host::up),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::host::down),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::host::unreachable),
      static_cast<bool>(obj.stalking_options() & configuration::host::up),
      static_cast<bool>(obj.stalking_options() & configuration::host::down),
      static_cast<bool>(obj.stalking_options() &
                        configuration::host::unreachable),
      obj.process_perf_data(), obj.check_freshness(), obj.freshness_threshold(),
      obj.notes(), obj.notes_url(), obj.action_url(), obj.icon_image(),
      obj.icon_image_alt(), obj.vrml_image(), obj.statusmap_image(),
      obj.coords_2d().x(), obj.coords_2d().y(), obj.have_coords_2d(),
      obj.coords_3d().x(), obj.coords_3d().y(), obj.coords_3d().z(),
      obj.have_coords_3d(),
      true,  // should_be_drawn, enabled by Nagios
      obj.retain_status_information(), obj.retain_nonstatus_information(),
      obj.obsess_over_host(), obj.timezone(), obj.icon_id());

  engine::host::hosts.insert({h->name(), h});
  engine::host::hosts_by_id.insert({obj.host_id(), h});

  h->set_initial_notif_time(0);
  h->set_should_reschedule_current_check(false);
  h->set_host_id(obj.host_id());
  h->set_acknowledgement_timeout(obj.acknowledgement_timeout() *
                                 config->interval_length());
  h->set_last_acknowledgement(0);

  // Contacts
  for (set_string::const_iterator it(obj.contacts().begin()),
       end(obj.contacts().end());
       it != end; ++it)
    h->mut_contacts().insert({*it, nullptr});

  // Contact groups.
  for (set_string::const_iterator it(obj.contactgroups().begin()),
       end(obj.contactgroups().end());
       it != end; ++it)
    h->get_contactgroups().insert({*it, nullptr});

  // Custom variables.
  for (auto it = obj.customvariables().begin(),
            end = obj.customvariables().end();
       it != end; ++it) {
    h->custom_variables[it->first] =
        engine::customvariable(it->second.value(), it->second.is_sent());

    if (it->second.is_sent()) {
      timeval tv(get_broker_timestamp(nullptr));
      broker_custom_variable(NEBTYPE_HOSTCUSTOMVARIABLE_ADD, h.get(), it->first,
                             it->second.value(), &tv);
    }
  }

  // add tags
  for (std::set<std::pair<uint64_t, uint16_t>>::iterator
           it = obj.tags().begin(),
           end = obj.tags().end();
       it != end; ++it) {
    tag_map::iterator it_tag{engine::tag::tags.find(*it)};
    if (it_tag == engine::tag::tags.end())
      throw engine_error() << "Could not find tag '" << it->first
                           << "' on which to apply host (" << obj.host_id()
                           << ")";
    else
      h->mut_tags().emplace_front(it_tag->second);
  }

  // Parents.
  for (set_string::const_iterator it(obj.parents().begin()),
       end(obj.parents().end());
       it != end; ++it)
    h->add_parent_host(*it);

  // Add severity.
  if (obj.severity_id()) {
    configuration::severity::key_type k = {obj.severity_id(),
                                           configuration::severity::host};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << "Could not add the severity (" << k.first << ", "
                           << k.second << ") to the host '" << obj.host_name()
                           << "'";
    h->set_severity(sv->second);
  }

  // Notify event broker.
  broker_adaptive_host_data(NEBTYPE_HOST_ADD, NEBFLAG_NONE, NEBATTR_NONE,
                            h.get(), MODATTR_ALL);
}
#else
/**
 *  Add new host.
 *
 *  @param[in] obj  The new host to add into the monitoring engine.
 */
void applier::host::add_object(const configuration::Host& obj) {
  // Logging.
  config_logger->debug("Creating new host '{}'.", obj.host_name());

  // Add host to the global configuration set.
  auto* cfg_obj = pb_config.add_hosts();
  cfg_obj->CopyFrom(obj);

  // Create host.
  auto h = std::make_shared<com::centreon::engine::host>(
      obj.host_id(), obj.host_name(), obj.display_name(), obj.alias(),
      obj.address(), obj.check_period(), obj.check_interval(),
      obj.retry_interval(), obj.max_check_attempts(),
      static_cast<bool>(obj.notification_options() & action_hst_up),
      static_cast<bool>(obj.notification_options() & action_hst_down),
      static_cast<bool>(obj.notification_options() & action_hst_unreachable),
      static_cast<bool>(obj.notification_options() & action_hst_flapping),
      static_cast<bool>(obj.notification_options() & action_hst_downtime),
      obj.notification_interval(), obj.first_notification_delay(),
      obj.recovery_notification_delay(), obj.notification_period(),
      obj.notifications_enabled(), obj.check_command(), obj.checks_active(),
      obj.checks_passive(), obj.event_handler(), obj.event_handler_enabled(),
      obj.flap_detection_enabled(), obj.low_flap_threshold(),
      obj.high_flap_threshold(),
      static_cast<bool>(obj.flap_detection_options() & action_hst_up),
      static_cast<bool>(obj.flap_detection_options() & action_hst_down),
      static_cast<bool>(obj.flap_detection_options() & action_hst_unreachable),
      static_cast<bool>(obj.stalking_options() & action_hst_up),
      static_cast<bool>(obj.stalking_options() & action_hst_down),
      static_cast<bool>(obj.stalking_options() & action_hst_unreachable),
      obj.process_perf_data(), obj.check_freshness(), obj.freshness_threshold(),
      obj.notes(), obj.notes_url(), obj.action_url(), obj.icon_image(),
      obj.icon_image_alt(), obj.vrml_image(), obj.statusmap_image(),
      obj.coords_2d().x(), obj.coords_2d().y(), obj.has_coords_2d(),
      obj.coords_3d().x(), obj.coords_3d().y(), obj.coords_3d().z(),
      obj.has_coords_3d(),
      true,  // should_be_drawn, enabled by Nagios
      obj.retain_status_information(), obj.retain_nonstatus_information(),
      obj.obsess_over_host(), obj.timezone(), obj.icon_id());

  engine::host::hosts.insert({h->name(), h});
  engine::host::hosts_by_id.insert({obj.host_id(), h});

  h->set_initial_notif_time(0);
  h->set_should_reschedule_current_check(false);
  h->set_host_id(obj.host_id());
  h->set_acknowledgement_timeout(obj.acknowledgement_timeout() *
                                 pb_config.interval_length());
  h->set_last_acknowledgement(0);

  // Contacts
  for (auto& c : obj.contacts().data())
    h->mut_contacts().insert({c, nullptr});

  // Contact groups.
  for (auto& cg : obj.contactgroups().data())
    h->get_contactgroups().insert({cg, nullptr});

  // Custom variables.
  for (auto& cv : obj.customvariables()) {
    h->custom_variables[cv.name()] =
        engine::customvariable(cv.value(), cv.is_sent());

    if (cv.is_sent()) {
      timeval tv(get_broker_timestamp(nullptr));
      broker_custom_variable(NEBTYPE_HOSTCUSTOMVARIABLE_ADD, h.get(), cv.name(),
                             cv.value(), &tv);
    }
  }

  // add tags
  for (auto& t : obj.tags()) {
    auto p = std::make_pair(t.first(), t.second());
    tag_map::iterator it_tag{engine::tag::tags.find(p)};
    if (it_tag == engine::tag::tags.end())
      throw engine_error() << "Could not find tag '" << t.first()
                           << "' on which to apply host (" << obj.host_id()
                           << ")";
    else
      h->mut_tags().emplace_front(it_tag->second);
  }

  // Parents.
  for (auto& p : obj.parents().data())
    h->add_parent_host(p);

  // Add severity.
  if (obj.severity_id()) {
    configuration::severity_helper::key_type k = {obj.severity_id(),
                                                  SeverityType::host};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << "Could not add the severity (" << k.first << ", "
                           << k.second << ") to the host '" << obj.host_name()
                           << "'";
    h->set_severity(sv->second);
  }

  // Notify event broker.
  broker_adaptive_host_data(NEBTYPE_HOST_ADD, NEBFLAG_NONE, NEBATTR_NONE,
                            h.get(), MODATTR_ALL);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Modified host.
 *
 *  @param[in] obj  The new host to modify into the monitoring engine.
 */
void applier::host::modify_object(configuration::host const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Modifying host '" << obj.host_name() << "'.";
  config_logger->debug("Modifying host '{}'.", obj.host_name());

  // Find the configuration object.
  set_host::iterator it_cfg(config->hosts_find(obj.key()));
  if (it_cfg == config->hosts().end())
    throw engine_error() << "Cannot modify non-existing host '"
                         << obj.host_name() << "'";

  // Find host object.
  host_id_map::iterator it_obj(engine::host::hosts_by_id.find(obj.key()));
  if (it_obj == engine::host::hosts_by_id.end())
    throw engine_error() << "Could not modify non-existing "
                         << "host object '" << obj.host_name() << "'";

  // Update the global configuration set.
  configuration::host obj_old(*it_cfg);
  config->hosts().erase(it_cfg);
  config->hosts().insert(obj);

  // Modify properties.
  if (it_obj->second->name() != obj.host_name()) {
    engine::host::hosts.erase(it_obj->second->name());
    engine::host::hosts.insert({obj.host_name(), it_obj->second});
  }

  it_obj->second->set_name(obj.host_name());
  it_obj->second->set_display_name(obj.display_name());
  if (!obj.alias().empty())
    it_obj->second->set_alias(obj.alias());
  else
    it_obj->second->set_alias(obj.host_name());
  it_obj->second->set_address(obj.address());
  if (!obj.check_period().empty())
    it_obj->second->set_check_period(obj.check_period());
  it_obj->second->set_check_interval(static_cast<double>(obj.check_interval()));
  it_obj->second->set_retry_interval(static_cast<double>(obj.retry_interval()));
  it_obj->second->set_max_attempts(static_cast<int>(obj.max_check_attempts()));
  it_obj->second->set_notify_on(
      (obj.notification_options() & configuration::host::up ? notifier::up
                                                            : notifier::none) |
      (obj.notification_options() & configuration::host::down
           ? notifier::down
           : notifier::none) |
      (obj.notification_options() & configuration::host::unreachable
           ? notifier::unreachable
           : notifier::none) |
      (obj.notification_options() & configuration::host::flapping
           ? (notifier::flappingstart | notifier::flappingstop |
              notifier::flappingdisabled)
           : notifier::none) |
      (obj.notification_options() & configuration::host::downtime
           ? notifier::downtime
           : notifier::none));
  it_obj->second->set_notification_interval(
      static_cast<double>(obj.notification_interval()));
  it_obj->second->set_first_notification_delay(
      static_cast<double>(obj.first_notification_delay()));
  it_obj->second->set_notification_period(obj.notification_period());
  it_obj->second->set_notifications_enabled(
      static_cast<int>(obj.notifications_enabled()));
  it_obj->second->set_check_command(obj.check_command());
  it_obj->second->set_checks_enabled(static_cast<int>(obj.checks_active()));
  it_obj->second->set_accept_passive_checks(
      static_cast<int>(obj.checks_passive()));
  it_obj->second->set_event_handler(obj.event_handler());
  it_obj->second->set_event_handler_enabled(
      static_cast<int>(obj.event_handler_enabled()));
  it_obj->second->set_flap_detection_enabled(obj.flap_detection_enabled());
  it_obj->second->set_low_flap_threshold(obj.low_flap_threshold());
  it_obj->second->set_high_flap_threshold(obj.high_flap_threshold());
  it_obj->second->set_flap_detection_on(notifier::none);
  it_obj->second->add_flap_detection_on(
      obj.flap_detection_options() & configuration::host::up ? notifier::up
                                                             : notifier::none);
  it_obj->second->add_flap_detection_on(obj.flap_detection_options() &
                                                configuration::host::down
                                            ? notifier::down
                                            : notifier::none);
  it_obj->second->add_flap_detection_on(obj.flap_detection_options() &
                                                configuration::host::unreachable
                                            ? notifier::unreachable
                                            : notifier::none);
  it_obj->second->set_stalk_on(notifier::none);
  it_obj->second->add_stalk_on(obj.stalking_options() & configuration::host::up
                                   ? notifier::up
                                   : notifier::none);
  it_obj->second->add_stalk_on(
      obj.stalking_options() & configuration::host::down ? notifier::down
                                                         : notifier::none);
  it_obj->second->add_stalk_on(obj.stalking_options() &
                                       configuration::host::unreachable
                                   ? notifier::unreachable
                                   : notifier::none);
  it_obj->second->set_process_performance_data(
      static_cast<int>(obj.process_perf_data()));
  it_obj->second->set_check_freshness(static_cast<int>(obj.check_freshness()));
  it_obj->second->set_freshness_threshold(
      static_cast<int>(obj.freshness_threshold()));
  it_obj->second->set_notes(obj.notes());
  it_obj->second->set_notes_url(obj.notes_url());
  it_obj->second->set_action_url(obj.action_url());
  it_obj->second->set_icon_image(obj.icon_image());
  it_obj->second->set_icon_image_alt(obj.icon_image_alt());
  it_obj->second->set_vrml_image(obj.vrml_image());
  it_obj->second->set_statusmap_image(obj.statusmap_image());
  it_obj->second->set_x_2d(obj.coords_2d().x());
  it_obj->second->set_y_2d(obj.coords_2d().y());
  it_obj->second->set_have_2d_coords(static_cast<int>(obj.have_coords_2d()));
  it_obj->second->set_x_3d(obj.coords_3d().x());
  it_obj->second->set_y_3d(obj.coords_3d().y());
  it_obj->second->set_z_3d(obj.coords_3d().z());
  it_obj->second->set_have_3d_coords(static_cast<int>(obj.have_coords_3d()));
  it_obj->second->set_retain_status_information(
      static_cast<int>(obj.retain_status_information()));
  it_obj->second->set_retain_nonstatus_information(
      static_cast<int>(obj.retain_nonstatus_information()));
  it_obj->second->set_obsess_over(obj.obsess_over_host());
  it_obj->second->set_timezone(obj.timezone());
  it_obj->second->set_host_id(obj.host_id());
  it_obj->second->set_acknowledgement_timeout(obj.acknowledgement_timeout() *
                                              config->interval_length());
  it_obj->second->set_recovery_notification_delay(
      obj.recovery_notification_delay());
  it_obj->second->set_icon_id(obj.icon_id());

  // Contacts.
  if (obj.contacts() != obj_old.contacts()) {
    // Delete old contacts.
    it_obj->second->mut_contacts().clear();

    // Add contacts to host.
    for (set_string::const_iterator it(obj.contacts().begin()),
         end(obj.contacts().end());
         it != end; ++it)
      it_obj->second->mut_contacts().insert({*it, nullptr});
  }

  // Contact groups.
  if (obj.contactgroups() != obj_old.contactgroups()) {
    // Delete old contact groups.
    it_obj->second->get_contactgroups().clear();

    // Add contact groups to host.
    for (set_string::const_iterator it(obj.contactgroups().begin()),
         end(obj.contactgroups().end());
         it != end; ++it)
      it_obj->second->get_contactgroups().insert({*it, nullptr});
  }

  // Custom variables.
  if (obj.customvariables() != obj_old.customvariables()) {
    for (auto& c : it_obj->second->custom_variables) {
      if (c.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_HOSTCUSTOMVARIABLE_DELETE,
                               it_obj->second.get(), c.first, c.second.value(),
                               &tv);
      }
    }
    it_obj->second->custom_variables.clear();

    for (auto& c : obj.customvariables()) {
      it_obj->second->custom_variables[c.first] =
          engine::customvariable(c.second.value(), c.second.is_sent());

      if (c.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_HOSTCUSTOMVARIABLE_ADD,
                               it_obj->second.get(), c.first, c.second.value(),
                               &tv);
      }
    }
  }

  // add tags
  if (obj.tags() != obj_old.tags()) {
    it_obj->second->mut_tags().clear();
    for (std::set<std::pair<uint64_t, uint16_t>>::iterator
             it = obj.tags().begin(),
             end = obj.tags().end();
         it != end; ++it) {
      tag_map::iterator it_tag{engine::tag::tags.find(*it)};
      if (it_tag == engine::tag::tags.end())
        throw engine_error()
            << "Could not find tag '" << it->first
            << "' on which to apply host (" << obj.host_id() << ")";
      else
        it_obj->second->mut_tags().emplace_front(it_tag->second);
    }
  }

  // Parents.
  if (obj.parents() != obj_old.parents()) {
    // Delete old parents.
    {
      for (const auto& [_, sptr_host] : it_obj->second->parent_hosts)
        broker_relation_data(NEBTYPE_PARENT_DELETE, sptr_host.get(), nullptr,
                             it_obj->second.get(), nullptr);
    }
    it_obj->second->parent_hosts.clear();

    // Create parents.
    for (set_string::const_iterator it(obj.parents().begin()),
         end(obj.parents().end());
         it != end; ++it)
      it_obj->second->add_parent_host(*it);
  }

  // Severity.
  if (obj.severity_id()) {
    configuration::severity::key_type k = {obj.severity_id(),
                                           configuration::severity::host};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << "Could not update the severity (" << k.first
                           << ", " << k.second << ") to the host '"
                           << obj.host_name() << "'";
    it_obj->second->set_severity(sv->second);
  } else
    it_obj->second->set_severity(nullptr);

  // Notify event broker.
  broker_adaptive_host_data(NEBTYPE_HOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE,
                            it_obj->second.get(), MODATTR_ALL);
}
#else
/**
 *  Modified host.
 *
 *  @param[in] obj  The new host to modify into the monitoring engine.
 */
void applier::host::modify_object(configuration::Host* old_obj,
                                  const configuration::Host& new_obj) {
  // Logging.
  config_logger->debug("Modifying host '{}' (id {}).", new_obj.host_name(),
                       new_obj.host_id());

  // Find host object.
  host_id_map::iterator it_obj =
      engine::host::hosts_by_id.find(new_obj.host_id());
  if (it_obj == engine::host::hosts_by_id.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing host object '{}' (id {})",
        new_obj.host_name(), new_obj.host_id());
  std::shared_ptr<engine::host> h = it_obj->second;

  // Modify properties.
  if (h->name() != new_obj.host_name()) {
    engine::host::hosts.erase(h->name());
    engine::host::hosts.insert({new_obj.host_name(), h});
  }

  h->set_name(new_obj.host_name());
  h->set_display_name(new_obj.display_name());
  if (!new_obj.alias().empty())
    h->set_alias(new_obj.alias());
  else
    h->set_alias(new_obj.host_name());
  h->set_address(new_obj.address());
  if (!new_obj.check_period().empty())
    h->set_check_period(new_obj.check_period());
  h->set_check_interval(static_cast<double>(new_obj.check_interval()));
  h->set_retry_interval(static_cast<double>(new_obj.retry_interval()));
  h->set_max_attempts(static_cast<int>(new_obj.max_check_attempts()));
  h->set_notify_on(
      (new_obj.notification_options() & action_hst_up ? notifier::up
                                                      : notifier::none) |
      (new_obj.notification_options() & action_hst_down ? notifier::down
                                                        : notifier::none) |
      (new_obj.notification_options() & action_hst_unreachable
           ? notifier::unreachable
           : notifier::none) |
      (new_obj.notification_options() & action_hst_flapping
           ? (notifier::flappingstart | notifier::flappingstop |
              notifier::flappingdisabled)
           : notifier::none) |
      (new_obj.notification_options() & action_hst_downtime ? notifier::downtime
                                                            : notifier::none));
  h->set_notification_interval(
      static_cast<double>(new_obj.notification_interval()));
  h->set_first_notification_delay(
      static_cast<double>(new_obj.first_notification_delay()));
  h->set_notification_period(new_obj.notification_period());
  h->set_notifications_enabled(
      static_cast<int>(new_obj.notifications_enabled()));
  h->set_check_command(new_obj.check_command());
  h->set_checks_enabled(static_cast<int>(new_obj.checks_active()));
  h->set_accept_passive_checks(static_cast<int>(new_obj.checks_passive()));
  h->set_event_handler(new_obj.event_handler());
  h->set_event_handler_enabled(
      static_cast<int>(new_obj.event_handler_enabled()));
  h->set_flap_detection_enabled(new_obj.flap_detection_enabled());
  h->set_low_flap_threshold(new_obj.low_flap_threshold());
  h->set_high_flap_threshold(new_obj.high_flap_threshold());
  h->set_flap_detection_on(notifier::none);
  h->add_flap_detection_on(new_obj.flap_detection_options() & action_hst_up
                               ? notifier::up
                               : notifier::none);
  h->add_flap_detection_on(new_obj.flap_detection_options() & action_hst_down
                               ? notifier::down
                               : notifier::none);
  h->add_flap_detection_on(new_obj.flap_detection_options() &
                                   action_hst_unreachable
                               ? notifier::unreachable
                               : notifier::none);
  h->set_stalk_on(notifier::none);
  h->add_stalk_on(new_obj.stalking_options() & action_hst_up ? notifier::up
                                                             : notifier::none);
  h->add_stalk_on(new_obj.stalking_options() & action_hst_down
                      ? notifier::down
                      : notifier::none);
  h->add_stalk_on(new_obj.stalking_options() & action_hst_unreachable
                      ? notifier::unreachable
                      : notifier::none);
  h->set_process_performance_data(
      static_cast<int>(new_obj.process_perf_data()));
  h->set_check_freshness(static_cast<int>(new_obj.check_freshness()));
  h->set_freshness_threshold(static_cast<int>(new_obj.freshness_threshold()));
  h->set_notes(new_obj.notes());
  h->set_notes_url(new_obj.notes_url());
  h->set_action_url(new_obj.action_url());
  h->set_icon_image(new_obj.icon_image());
  h->set_icon_image_alt(new_obj.icon_image_alt());
  h->set_vrml_image(new_obj.vrml_image());
  h->set_statusmap_image(new_obj.statusmap_image());
  h->set_x_2d(new_obj.coords_2d().x());
  h->set_y_2d(new_obj.coords_2d().y());
  h->set_have_2d_coords(static_cast<int>(new_obj.has_coords_2d()));
  h->set_x_3d(new_obj.coords_3d().x());
  h->set_y_3d(new_obj.coords_3d().y());
  h->set_z_3d(new_obj.coords_3d().z());
  h->set_have_3d_coords(static_cast<int>(new_obj.has_coords_3d()));
  h->set_retain_status_information(
      static_cast<int>(new_obj.retain_status_information()));
  h->set_retain_nonstatus_information(
      static_cast<int>(new_obj.retain_nonstatus_information()));
  h->set_obsess_over(new_obj.obsess_over_host());
  h->set_timezone(new_obj.timezone());
  h->set_host_id(new_obj.host_id());
  h->set_acknowledgement_timeout(new_obj.acknowledgement_timeout() *
                                 pb_config.interval_length());
  h->set_recovery_notification_delay(new_obj.recovery_notification_delay());
  h->set_icon_id(new_obj.icon_id());

  // Contacts.
  if (!MessageDifferencer::Equals(new_obj.contacts(), old_obj->contacts())) {
    // Delete old contacts.
    h->mut_contacts().clear();

    // Add contacts to host.
    for (auto& c : new_obj.contacts().data())
      h->mut_contacts().insert({c, nullptr});
  }

  // Contact groups.
  if (!MessageDifferencer::Equals(new_obj.contactgroups(),
                                  old_obj->contactgroups())) {
    // Delete old contact groups.
    h->get_contactgroups().clear();

    // Add contact groups to host.
    for (auto& cg : new_obj.contactgroups().data())
      h->get_contactgroups().insert({cg, nullptr});
  }

  // Custom variables.
  if (!std::equal(
          new_obj.customvariables().begin(), new_obj.customvariables().end(),
          old_obj->customvariables().begin(), old_obj->customvariables().end(),
          MessageDifferencer::Equals)) {
    for (auto& cv : h->custom_variables) {
      if (cv.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_HOSTCUSTOMVARIABLE_DELETE, h.get(),
                               cv.first, cv.second.value(), &tv);
      }
    }
    h->custom_variables.clear();

    for (auto& c : new_obj.customvariables()) {
      h->custom_variables[c.name()] = c.value();

      if (c.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_HOSTCUSTOMVARIABLE_ADD, h.get(),
                               c.name(), c.value(), &tv);
      }
    }
  }

  // add tags
  bool tags_changed = false;
  if (old_obj->tags().size() == new_obj.tags().size()) {
    for (auto new_it = new_obj.tags().begin(), old_it = old_obj->tags().begin();
         old_it != old_obj->tags().end() && new_it != new_obj.tags().end();
         ++old_it, ++new_it) {
      if (new_it->first() != old_it->first() ||
          new_it->second() != old_it->second()) {
        tags_changed = true;
        break;
      }
    }
  } else
    tags_changed = true;

  if (tags_changed) {
    h->mut_tags().clear();
    old_obj->mutable_tags()->CopyFrom(new_obj.tags());
    for (auto& t : new_obj.tags()) {
      tag_map::iterator it_tag =
          engine::tag::tags.find({t.first(), t.second()});
      if (it_tag == engine::tag::tags.end())
        throw engine_error()
            << fmt::format("Could not find tag '{}' on which to apply host {}",
                           t.first(), new_obj.host_id());
      else
        h->mut_tags().emplace_front(it_tag->second);
    }
  }

  // Parents.
  bool parents_changed = false;
  if (old_obj->parents().data().size() == new_obj.parents().data().size()) {
    for (auto new_it = new_obj.parents().data().begin(),
              old_it = old_obj->parents().data().begin();
         old_it != old_obj->parents().data().end() &&
         new_it != new_obj.parents().data().end();
         ++old_it, ++new_it) {
      if (*new_it != *old_it) {
        parents_changed = true;
        break;
      }
    }
  } else
    parents_changed = true;

  if (parents_changed) {
    // Delete old parents.
    for (const auto& [_, sptr_host] : h->parent_hosts)
      broker_relation_data(NEBTYPE_PARENT_DELETE, sptr_host.get(), nullptr,
                           h.get(), nullptr);
    h->parent_hosts.clear();

    // Create parents.
    for (auto& parent_name : new_obj.parents().data())
      h->add_parent_host(parent_name);
  }

  // Severity.
  if (new_obj.severity_id()) {
    configuration::severity_helper::key_type k = {new_obj.severity_id(),
                                                  SeverityType::host};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << "Could not update the severity (" << k.first
                           << ", " << k.second << ") to the host '"
                           << new_obj.host_name() << "'";
    h->set_severity(sv->second);
  } else
    h->set_severity(nullptr);

  // Notify event broker.
  broker_adaptive_host_data(NEBTYPE_HOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE,
                            it_obj->second.get(), MODATTR_ALL);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Remove old host.
 *
 *  @param[in] obj The new host to remove from the monitoring engine.
 */
void applier::host::remove_object(configuration::host const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing host '" << obj.host_name() << "'.";
  config_logger->debug("Removing host '{}'.", obj.host_name());

  // Find host.
  host_id_map::iterator it(engine::host::hosts_by_id.find(obj.key()));
  if (it != engine::host::hosts_by_id.end()) {
    // Remove host comments.
    comment::delete_host_comments(obj.host_id());

    // Remove host downtimes.
    downtimes::downtime_manager::instance()
        .delete_downtime_by_hostname_service_description_start_time_comment(
            obj.host_name(), "", {false, (time_t)0}, "");

    // Remove events related to this host.
    applier::scheduler::instance().remove_host(obj.key());

    // remove host from hostgroup->members
    for (auto& it_h : it->second->get_parent_groups())
      it_h->members.erase(it->second->name());

    // remove any relations
    for (const auto& [_, sptr_host] : it->second->parent_hosts)
      broker_relation_data(NEBTYPE_PARENT_DELETE, sptr_host.get(), nullptr,
                           it->second.get(), nullptr);

    // Notify event broker.
    for (auto it_s = it->second->services.begin();
         it_s != it->second->services.end(); ++it_s)
      broker_adaptive_service_data(NEBTYPE_SERVICE_DELETE, NEBFLAG_NONE,
                                   NEBATTR_NONE, it_s->second, MODATTR_ALL);

    broker_adaptive_host_data(NEBTYPE_HOST_DELETE, NEBFLAG_NONE, NEBATTR_NONE,
                              it->second.get(), MODATTR_ALL);

    // Erase host object (will effectively delete the object).
    engine::host::hosts.erase(it->second->name());
    engine::host::hosts_by_id.erase(it);
  }

  // Remove host from the global configuration set.
  config->hosts().erase(obj);
}
#else
/**
 *  Remove old host.
 *
 *  @param[in] obj The new host to remove from the monitoring engine.
 */
void applier::host::remove_object(ssize_t idx) {
  const Host& obj = pb_config.hosts()[idx];
  // Logging.
  config_logger->debug("Removing host '{}'.", obj.host_name());

  // Find host.
  host_id_map::iterator it(engine::host::hosts_by_id.find(obj.host_id()));
  if (it != engine::host::hosts_by_id.end()) {
    // Remove host comments.
    comment::delete_host_comments(obj.host_id());

    // Remove host downtimes.
    downtimes::downtime_manager::instance()
        .delete_downtime_by_hostname_service_description_start_time_comment(
            obj.host_name(), "", {false, (time_t)0}, "");

    // Remove events related to this host.
    applier::scheduler::instance().remove_host(obj.host_id());

    // remove host from hostgroup->members
    for (auto& it_h : it->second->get_parent_groups())
      it_h->members.erase(it->second->name());

    // remove any relations
    for (const auto& [_, sptr_host] : it->second->parent_hosts)
      broker_relation_data(NEBTYPE_PARENT_DELETE, sptr_host.get(), nullptr,
                           it->second.get(), nullptr);

    // Notify event broker.
    for (auto it_s = it->second->services.begin();
         it_s != it->second->services.end(); ++it_s)
      broker_adaptive_service_data(NEBTYPE_SERVICE_DELETE, NEBFLAG_NONE,
                                   NEBATTR_NONE, it_s->second, MODATTR_ALL);

    broker_adaptive_host_data(NEBTYPE_HOST_DELETE, NEBFLAG_NONE, NEBATTR_NONE,
                              it->second.get(), MODATTR_ALL);

    // Erase host object (will effectively delete the object).
    engine::host::hosts.erase(it->second->name());
    engine::host::hosts_by_id.erase(it);
  }

  // Remove host from the global configuration set.
  pb_config.mutable_hosts()->DeleteSubrange(idx, 1);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Resolve a host.
 *
 *  @param[in] obj  Host object.
 */
void applier::host::resolve_object(const configuration::host& obj,
                                   error_cnt& err) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving host '" << obj.host_name() << "'.";
  config_logger->debug("Resolving host '{}'.", obj.host_name());

  // If it is the very first host to be resolved,
  // remove all the child backlinks of all the hosts.
  // It is necessary to do it only once to prevent the removal
  // of valid child backlinks.
  if (obj == *config->hosts().begin()) {
    for (const auto& [_, sptr_host] : engine::host::hosts)
      sptr_host->child_hosts.clear();
  }

  // Find host.
  host_id_map::iterator it(engine::host::hosts_by_id.find(obj.key()));
  if (engine::host::hosts_by_id.end() == it)
    throw(engine_error() << "Cannot resolve non-existing host '"
                         << obj.host_name() << "'");

  // Remove service backlinks.
  it->second->services.clear();

  // Remove host group links.
  it->second->get_parent_groups().clear();

  // Reset host counters.
  it->second->set_total_services(0);
  it->second->set_total_service_check_interval(0);

  // Resolve host.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#else
/**
 * @brief Resolve a host.
 *
 * @param obj Host protobuf configuration object.
 */
void applier::host::resolve_object(const configuration::Host& obj,
                                   error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving host '{}'.", obj.host_name());

  // If it is the very first host to be resolved,
  // remove all the child backlinks of all the hosts.
  // It is necessary to do it only once to prevent the removal
  // of valid child backlinks.
  if (&obj == &(*pb_config.hosts().begin())) {
    for (const auto& [_, sptr_host] : engine::host::hosts)
      sptr_host->child_hosts.clear();
  }

  // Find host.
  host_id_map::iterator it = engine::host::hosts_by_id.find(obj.host_id());
  if (engine::host::hosts_by_id.end() == it)
    throw engine_error() << fmt::format("Cannot resolve non-existing host '{}'",
                                        obj.host_name());

  // Remove service backlinks.
  it->second->services.clear();

  // Remove host group links.
  it->second->get_parent_groups().clear();

  // Reset host counters.
  it->second->set_total_services(0);
  it->second->set_total_service_check_interval(0);

  // Resolve host.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#endif

#ifdef LEGACY_CONF
/**
 *  @brief Expand a host.
 *
 *  During expansion, the host will be added to its host groups. These
 *  will be modified in the state.
 *
 *  @param[int,out] s   Configuration state.
 */
void applier::host::expand_objects(configuration::state& s) {
  // Browse all hosts.
  set_host new_hosts;
  for (auto host_cfg : s.hosts()) {
    // Should custom variables be sent to broker ?
    for (auto it = host_cfg.mut_customvariables().begin(),
              end = host_cfg.mut_customvariables().end();
         it != end; ++it) {
      if (!s.enable_macros_filter() ||
          s.macros_filter().find(it->first) != s.macros_filter().end()) {
        it->second.set_sent(true);
      }
    }

    // Browse current host's groups.
    for (set_string::const_iterator it_group(host_cfg.hostgroups().begin()),
         end_group(host_cfg.hostgroups().end());
         it_group != end_group; ++it_group) {
      // Find host group.
      configuration::set_hostgroup::iterator group(
          s.hostgroups_find(*it_group));
      if (group == s.hostgroups().end())
        throw(engine_error()
              << "Could not add host '" << host_cfg.host_name()
              << "' to non-existing host group '" << *it_group << "'");

      // Remove host group from state.
      configuration::hostgroup backup(*group);
      s.hostgroups().erase(group);

      // Add host to group members.
      backup.members().insert(host_cfg.host_name());

      // Reinsert host group.
      s.hostgroups().insert(backup);
    }
    new_hosts.insert(host_cfg);
  }
  s.hosts() = std::move(new_hosts);
}
#else
/**
 *  @brief Expand a host.
 *
 *  During expansion, the host will be added to its host groups. These
 *  will be modified in the state.
 *
 *  @param[int,out] s   Configuration state.
 */
void applier::host::expand_objects(configuration::State& s) {
  // Let's consider all the macros defined in s.
  absl::flat_hash_set<std::string_view> cvs;
  for (auto& cv : s.macros_filter().data())
    cvs.emplace(cv);

  absl::flat_hash_map<std::string_view, configuration::Hostgroup*> hgs;
  for (auto& hg : *s.mutable_hostgroups())
    hgs.emplace(hg.hostgroup_name(), &hg);

  // Browse all hosts.
  for (auto& host_cfg : *s.mutable_hosts()) {
    // Should custom variables be sent to broker ?
    for (auto& cv : *host_cfg.mutable_customvariables()) {
      if (!s.enable_macros_filter() || cvs.contains(cv.name()))
        cv.set_is_sent(true);
    }

    for (auto& grp : host_cfg.hostgroups().data()) {
      auto it = hgs.find(grp);
      if (it != hgs.end()) {
        fill_string_group(it->second->mutable_members(), host_cfg.host_name());
      } else
        throw engine_error() << fmt::format(
            "Could not add host '{}' to non-existing host group '{}'",
            host_cfg.host_name(), grp);
    }
  }
}
#endif
