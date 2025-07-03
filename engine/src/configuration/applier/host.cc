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
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/severity.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

/**
 *  Default constructor.
 */
applier::host::host() {}

/**
 *  Destructor.
 */
applier::host::~host() throw() {}

/**
 *  Add new host.
 *
 *  @param[in] obj  The new host to add into the monitoring engine.
 */
void applier::host::add_object(configuration::host const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new host '" << obj.host_name() << "'.";
  config_logger->debug("Creating new host '{}'.", obj.host_name());

  // Add host to the global configuration set.
  config->hosts().insert(obj);

  // Create host.
  auto h = std::make_shared<com::centreon::engine::host>(
      obj.host_id(), obj.host_name(), obj.display_name(), obj.alias(),
      obj.address(), obj.check_period(),
      static_cast<engine::host::host_state>(obj.initial_state()),
      obj.check_interval(), obj.retry_interval(), obj.max_check_attempts(),
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
      broker_custom_variable(NEBTYPE_HOSTCUSTOMVARIABLE_ADD, h.get(),
                             it->first.c_str(), it->second.value().c_str(),
                             &tv);
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
    throw engine_error() << "Could not modify non-existing " << "host object '"
                         << obj.host_name() << "'";

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
  if (obj.check_period().empty())
    it_obj->second->set_check_period(obj.check_period());
  it_obj->second->set_initial_state(
      static_cast<engine::host::host_state>(obj.initial_state()));
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
  it_obj->second->set_icon_id(obj.icon_id());
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
                               it_obj->second.get(), c.first.c_str(),
                               c.second.value().c_str(), &tv);
      }
    }
    it_obj->second->custom_variables.clear();

    for (auto& c : obj.customvariables()) {
      it_obj->second->custom_variables[c.first] =
          engine::customvariable(c.second.value(), c.second.is_sent());

      if (c.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_HOSTCUSTOMVARIABLE_ADD,
                               it_obj->second.get(), c.first.c_str(),
                               c.second.value().c_str(), &tv);
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

/**
 *  Resolve a host.
 *
 *  @param[in] obj  Host object.
 */
void applier::host::resolve_object(configuration::host const& obj) {
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
  it->second->resolve(config_warnings, config_errors);
}

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
