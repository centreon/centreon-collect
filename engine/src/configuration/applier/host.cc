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
#include "common/engine_conf/severity_helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

/**
 *  Add new host.
 *
 *  @param[in] obj  The new host to add into the monitoring engine.
 */
void applier::host::add_object(const configuration::Host& obj) {
  // Logging.
  config_logger->debug("Creating new host '{}'.", obj.host_name());

  // Add host to the global configuration set.
  auto new_host_config = std::make_unique<Host>();
  new_host_config->CopyFrom(obj);
  pb_indexed_config.mut_hosts().emplace(obj.host_id(),
                                        std::move(new_host_config));

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
                                 pb_indexed_config.state().interval_length());
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
  broker_adaptive_host_data(NEBTYPE_HOST_ADD, NEBFLAG_NONE, h.get(),
                            MODATTR_ALL);
}

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
                                 pb_indexed_config.state().interval_length());
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
  broker_adaptive_host_data(NEBTYPE_HOST_UPDATE, NEBFLAG_NONE,
                            it_obj->second.get(), MODATTR_ALL);
}

/**
 *  Remove old host.
 *
 *  @param[in] obj The new host to remove from the monitoring engine.
 */
void applier::host::remove_object(uint64_t host_id) {
  const Host& obj = *pb_indexed_config.hosts().at(host_id);
  // Logging.
  config_logger->debug("Removing host '{}'.", obj.host_name());

  // Find host.
  host_id_map::iterator it = engine::host::hosts_by_id.find(obj.host_id());
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
                                   it_s->second, MODATTR_ALL);

    broker_adaptive_host_data(NEBTYPE_HOST_DELETE, NEBFLAG_NONE,
                              it->second.get(), MODATTR_ALL);

    // Erase host object (will effectively delete the object).
    engine::host::hosts.erase(it->second->name());
    engine::host::hosts_by_id.erase(it);
  }

  // Remove host from the global configuration set.
  pb_indexed_config.mut_hosts().erase(host_id);
}

/**
 * @brief Resolve a host.
 *
 * @param obj Host protobuf configuration object.
 */
void applier::host::resolve_object(const configuration::Host& obj,
                                   error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving host '{}'.", obj.host_name());

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
