/*
** Copyright 2011-2019,2022-2023 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/engine/configuration/applier/service.hh"
#include <absl/container/flat_hash_set.h>
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/scheduler.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/severity.hh"
#include "configuration/message_helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::configuration;

/**
 * @brief Add a new service.
 *
 * @param obj The new service protobuf configuration to add into the monitoring
 * engine.
 */
void applier::service::add_object(const configuration::Service& obj) {
  // Check service.
  if (obj.hosts().data().empty())
    throw engine_error() << fmt::format(
        "Could not create service '{}' with no host defined",
        obj.service_description());
  else if (obj.hosts().data().size() > 1)
    throw engine_error() << fmt::format(
        "Could not create service '{}' with multiple hosts defined",
        obj.service_description());
  else if (!obj.hostgroups().data().empty())
    throw engine_error() << fmt::format(
        "Could not create service '{}' with multiple host groups defined",
        obj.service_description());
  else if (obj.host_id() == 0)
    throw engine_error() << fmt::format(
        "No host_id available for the host '{}' - unable to create service "
        "'{}'",
        *obj.hosts().data().begin(), obj.service_description());

  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new service '" << obj.service_description() << "' of host '"
      << *obj.hosts().data().begin() << "'.";
  log_v2::config()->debug("Creating new service '{}' of host '{}'.",
                          obj.service_description(),
                          *obj.hosts().data().begin());

  // Add service to the global configuration set.
  auto* cfg_svc = pb_config.add_services();
  cfg_svc->CopyFrom(obj);

  // Create service.
  engine::service* svc{add_service(
      obj.host_id(), obj.service_id(), *obj.hosts().data().begin(),
      obj.service_description(), obj.display_name(), obj.check_period(),
      static_cast<engine::service::service_state>(obj.initial_state()),
      obj.max_check_attempts(), obj.check_interval(), obj.retry_interval(),
      obj.notification_interval(), obj.first_notification_delay(),
      obj.recovery_notification_delay(), obj.notification_period(),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::ok),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::unknown),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::warning),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::critical),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::flapping),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::downtime),
      obj.notifications_enabled(), obj.is_volatile(), obj.event_handler(),
      obj.event_handler_enabled(), obj.check_command(), obj.checks_active(),
      obj.checks_passive(), obj.flap_detection_enabled(),
      obj.low_flap_threshold(), obj.high_flap_threshold(),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::service::ok),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::service::warning),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::service::unknown),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::service::critical),
      static_cast<bool>(obj.stalking_options() & configuration::service::ok),
      static_cast<bool>(obj.stalking_options() &
                        configuration::service::warning),
      static_cast<bool>(obj.stalking_options() &
                        configuration::service::unknown),
      static_cast<bool>(obj.stalking_options() &
                        configuration::service::critical),
      obj.process_perf_data(), obj.check_freshness(), obj.freshness_threshold(),
      obj.notes(), obj.notes_url(), obj.action_url(), obj.icon_image(),
      obj.icon_image_alt(), obj.retain_status_information(),
      obj.retain_nonstatus_information(), obj.obsess_over_service(),
      obj.timezone(), obj.icon_id())};
  if (!svc)
    throw engine_error() << fmt::format(
        "Could not register service '{}' of host '{}'",
        obj.service_description(), *obj.hosts().data().begin());
  svc->set_initial_notif_time(0);
  engine::service::services[{*obj.hosts().data().begin(),
                             obj.service_description()}]
      ->set_host_id(obj.host_id());
  engine::service::services[{*obj.hosts().data().begin(),
                             obj.service_description()}]
      ->set_service_id(obj.service_id());
  svc->set_acknowledgement_timeout(obj.acknowledgement_timeout() *
                                   config->interval_length());
  svc->set_last_acknowledgement(0);

  // Add contacts.
  for (auto& c : obj.contacts().data())
    svc->mut_contacts().insert({c, nullptr});

  // Add contactgroups.
  for (auto& cg : obj.contactgroups().data())
    svc->get_contactgroups().insert({cg, nullptr});

  // Add custom variables.
  for (auto& cv : obj.customvariables()) {
    svc->custom_variables.emplace(cv.name(),
                                  customvariable(cv.value(), cv.is_sent()));

    if (cv.is_sent()) {
      timeval tv(get_broker_timestamp(nullptr));
      broker_custom_variable(NEBTYPE_SERVICECUSTOMVARIABLE_ADD, svc,
                             cv.name().c_str(), cv.value().c_str(), &tv);
    }
  }

  // Add severity.
  if (obj.severity_id()) {
    configuration::severity::key_type k = {obj.severity_id(),
                                           configuration::severity::service};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << fmt::format(
          "Could not add the severity ({}, {}) to the service '{}' of host "
          "'{}'",
          k.first, k.second, obj.service_description(),
          *obj.hosts().data().begin());
    svc->set_severity(sv->second);
  }

  // add tags
  for (auto& t : obj.tags()) {
    auto k = std::make_pair(t.first(), t.second());
    tag_map::iterator it_tag{engine::tag::tags.find(k)};
    if (it_tag == engine::tag::tags.end())
      throw engine_error() << fmt::format(
          "Could not find tag ({}, {}) on which to apply service ({}, {})",
          k.first, k.second, obj.host_id(), obj.service_id());
    else
      svc->mut_tags().emplace_front(it_tag->second);
  }

  // Notify event broker.
  broker_adaptive_service_data(NEBTYPE_SERVICE_ADD, NEBFLAG_NONE, NEBATTR_NONE,
                               svc, MODATTR_ALL);
}

/**
 *  Add new service.
 *
 *  @param[in] obj  The new service to add into the monitoring engine.
 */
void applier::service::add_object(configuration::service const& obj) {
  // Check service.
  if (obj.host_name().empty())
    throw engine_error() << "Could not create service '"
                         << obj.service_description()
                         << "' with no host defined";
  else if (!obj.host_id())
    throw engine_error() << "No host_id available for the host '"
                         << obj.host_name() << "' - unable to create service '"
                         << obj.service_description() << "'";

  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new service '" << obj.service_description() << "' of host '"
      << obj.host_name() << "'.";
  log_v2::config()->debug("Creating new service '{}' of host '{}'.",
                          obj.service_description(), obj.host_name());

  // Add service to the global configuration set.
  config->services().insert(obj);

  // Create service.
  engine::service* svc{add_service(
      obj.host_id(), obj.service_id(), obj.host_name(),
      obj.service_description(), obj.display_name(), obj.check_period(),
      static_cast<engine::service::service_state>(obj.initial_state()),
      obj.max_check_attempts(), obj.check_interval(), obj.retry_interval(),
      obj.notification_interval(), obj.first_notification_delay(),
      obj.recovery_notification_delay(), obj.notification_period(),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::ok),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::unknown),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::warning),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::critical),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::flapping),
      static_cast<bool>(obj.notification_options() &
                        configuration::service::downtime),
      obj.notifications_enabled(), obj.is_volatile(), obj.event_handler(),
      obj.event_handler_enabled(), obj.check_command(), obj.checks_active(),
      obj.checks_passive(), obj.flap_detection_enabled(),
      obj.low_flap_threshold(), obj.high_flap_threshold(),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::service::ok),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::service::warning),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::service::unknown),
      static_cast<bool>(obj.flap_detection_options() &
                        configuration::service::critical),
      static_cast<bool>(obj.stalking_options() & configuration::service::ok),
      static_cast<bool>(obj.stalking_options() &
                        configuration::service::warning),
      static_cast<bool>(obj.stalking_options() &
                        configuration::service::unknown),
      static_cast<bool>(obj.stalking_options() &
                        configuration::service::critical),
      obj.process_perf_data(), obj.check_freshness(), obj.freshness_threshold(),
      obj.notes(), obj.notes_url(), obj.action_url(), obj.icon_image(),
      obj.icon_image_alt(), obj.retain_status_information(),
      obj.retain_nonstatus_information(), obj.obsess_over_service(),
      obj.timezone(), obj.icon_id())};
  if (!svc)
    throw engine_error() << "Could not register service '"
                         << obj.service_description() << "' of host '"
                         << obj.host_name() << "'";
  svc->set_initial_notif_time(0);
  engine::service::services[{obj.host_name(), obj.service_description()}]
      ->set_host_id(obj.host_id());
  engine::service::services[{obj.host_name(), obj.service_description()}]
      ->set_service_id(obj.service_id());
  svc->set_acknowledgement_timeout(obj.acknowledgement_timeout() *
                                   config->interval_length());
  svc->set_last_acknowledgement(0);

  // Add contacts.
  for (set_string::const_iterator it(obj.contacts().begin()),
       end(obj.contacts().end());
       it != end; ++it)
    svc->mut_contacts().insert({*it, nullptr});

  // Add contactgroups.
  for (set_string::const_iterator it(obj.contactgroups().begin()),
       end(obj.contactgroups().end());
       it != end; ++it)
    svc->get_contactgroups().insert({*it, nullptr});

  // Add custom variables.
  for (map_customvar::const_iterator it(obj.customvariables().begin()),
       end(obj.customvariables().end());
       it != end; ++it) {
    svc->custom_variables[it->first] = it->second;

    if (it->second.is_sent()) {
      timeval tv(get_broker_timestamp(nullptr));
      broker_custom_variable(NEBTYPE_SERVICECUSTOMVARIABLE_ADD, svc,
                             it->first.c_str(), it->second.value().c_str(),
                             &tv);
    }
  }

  // Add severity.
  if (obj.severity_id()) {
    configuration::severity::key_type k = {obj.severity_id(),
                                           configuration::severity::service};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << "Could not add the severity (" << k.first << ", "
                           << k.second << ") to the service '"
                           << obj.service_description() << "' of host '"
                           << obj.host_name() << "'";
    svc->set_severity(sv->second);
  }

  // add tags
  for (std::set<std::pair<uint64_t, uint16_t>>::iterator
           it = obj.tags().begin(),
           end = obj.tags().end();
       it != end; ++it) {
    tag_map::iterator it_tag{engine::tag::tags.find(*it)};
    if (it_tag == engine::tag::tags.end())
      throw engine_error() << "Could not find tag (" << it->first << ", "
                           << it->second << ") on which to apply service ("
                           << obj.host_id() << ", " << obj.service_id() << ")";
    else
      svc->mut_tags().emplace_front(it_tag->second);
  }

  // Notify event broker.
  broker_adaptive_service_data(NEBTYPE_SERVICE_ADD, NEBFLAG_NONE, NEBATTR_NONE,
                               svc, MODATTR_ALL);
}

/**
 *  Expand a service object.
 *
 *  @param[in,out] s  State being applied.
 */
void applier::service::expand_objects(configuration::State& s) {
  std::list<std::unique_ptr<Service>> expanded;
  // Let's consider all the macros defined in s.
  absl::flat_hash_set<absl::string_view> cvs;
  for (auto& cv : s.macros_filter().data())
    cvs.emplace(cv);

  absl::flat_hash_map<absl::string_view, configuration::Hostgroup*> hgs;
  for (auto& hg : *s.mutable_hostgroups())
    hgs.emplace(hg.hostgroup_name(), &hg);

  // Browse all services.
  for (auto& service_cfg : *s.mutable_services()) {
    absl::flat_hash_set<absl::string_view> target_hosts;
    target_hosts.insert(service_cfg.hosts().data().begin(),
                        service_cfg.hosts().data().end());

    // Should custom variables be sent to broker ?
    for (auto& cv : *service_cfg.mutable_customvariables()) {
      if (!s.enable_macros_filter() || cvs.contains(cv.name()))
        cv.set_is_sent(true);
    }

    for (auto& grp_name : service_cfg.hostgroups().data()) {
      // Find host group.
      auto it = hgs.find(grp_name);
      if (it != hgs.end()) {
        if (it->second->members().data().empty() &&
            !s.allow_empty_hostgroup_assignment())
          throw engine_error() << fmt::format(
              "Could not expand host group '{}' specified in service '{}'",
              grp_name, service_cfg.service_description());
        target_hosts.insert(it->second->members().data().begin(),
                            it->second->members().data().end());
      } else
        throw engine_error() << fmt::format(
            "Could not find host group '{}' on which to apply service '{}'",
            grp_name, service_cfg.service_description());
    }

    // Browse all target hosts.
    for (auto& h : target_hosts) {
      auto svc = std::make_unique<configuration::Service>();
      svc->CopyFrom(service_cfg);
      svc->clear_hostgroups();
      svc->clear_hosts();
      svc->mutable_hosts()->add_data(h.data(), h.size());

      // Expand membershipts.
      _expand_service_memberships(*svc, s);

      // Inherits special vars.
      _inherits_special_vars(*svc, s);

      // Insert object.
      expanded.push_back(std::move(svc));
    }
  }

  // Set expanded services in configuration state.
  s.clear_services();
  for (auto& sv : expanded) {
    s.mutable_services()->AddAllocated(sv.release());
  }
}

/**
 *  Expand a service object.
 *
 *  @param[in,out] s  State being applied.
 */
void applier::service::expand_objects(configuration::state& s) {
  // Browse all services.
  configuration::set_service expanded;
  for (configuration::service& svc : s.services()) {
    // Should custom variables be sent to broker ?
    for (map_customvar::iterator
             it = const_cast<map_customvar&>(svc.customvariables()).begin(),
             end = const_cast<map_customvar&>(svc.customvariables()).end();
         it != end; ++it) {
      if (!s.enable_macros_filter() ||
          s.macros_filter().find(it->first) != s.macros_filter().end()) {
        it->second.set_sent(true);
      }
    }

    // Expand memberships.
    _expand_service_memberships(svc, s);

    // Inherits special vars.
    _inherits_special_vars(svc, s);

    // Insert object.
    expanded.insert(svc);
  }

  // Set expanded services in configuration state.
  s.services().swap(expanded);
}

/**
 *  Modified service.
 *
 *  @param[in] obj  The new service to modify into the monitoring
 *                  engine.
 */
void applier::service::modify_object(configuration::service const& obj) {
  std::string const& host_name(obj.host_name());
  std::string const& service_description(obj.service_description());

  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Modifying new service '" << service_description << "' of host '"
      << host_name << "'.";
  log_v2::config()->debug("Modifying new service '{}' of host '{}'.",
                          service_description, host_name);

  // Find the configuration object.
  set_service::iterator it_cfg(config->services_find(obj.key()));
  if (it_cfg == config->services().end())
    throw engine_error() << "Cannot modify non-existing "
                            "service '"
                         << service_description << "' of host '" << host_name
                         << "'";

  // Find service object.
  service_id_map::iterator it_obj(
      engine::service::services_by_id.find(obj.key()));
  if (it_obj == engine::service::services_by_id.end())
    throw engine_error() << "Could not modify non-existing "
                         << "service object '" << service_description
                         << "' of host '" << host_name << "'";
  std::shared_ptr<engine::service> s{it_obj->second};

  // Update the global configuration set.
  configuration::service obj_old(*it_cfg);
  config->services().erase(it_cfg);
  config->services().insert(obj);

  // Modify properties.
  if (it_obj->second->get_hostname() != obj.host_name() ||
      it_obj->second->description() != obj.service_description()) {
    engine::service::services.erase(
        {it_obj->second->get_hostname(), it_obj->second->description()});
    engine::service::services.insert(
        {{obj.host_name(), obj.service_description()}, it_obj->second});
  }

  s->set_hostname(obj.host_name());
  s->set_description(obj.service_description());
  s->set_display_name(obj.display_name()),
      s->set_check_command(obj.check_command());
  s->set_event_handler(obj.event_handler());
  s->set_event_handler_enabled(obj.event_handler_enabled());
  s->set_initial_state(
      static_cast<engine::service::service_state>(obj.initial_state()));
  s->set_check_interval(obj.check_interval());
  s->set_retry_interval(obj.retry_interval());
  s->set_max_attempts(obj.max_check_attempts());

  s->set_notify_on(
      (obj.notification_options() & configuration::service::unknown
           ? notifier::unknown
           : notifier::none) |
      (obj.notification_options() & configuration::service::warning
           ? notifier::warning
           : notifier::none) |
      (obj.notification_options() & configuration::service::critical
           ? notifier::critical
           : notifier::none) |
      (obj.notification_options() & configuration::service::ok
           ? notifier::ok
           : notifier::none) |
      (obj.notification_options() & configuration::service::flapping
           ? (notifier::flappingstart | notifier::flappingstop |
              notifier::flappingdisabled)
           : notifier::none) |
      (obj.notification_options() & configuration::service::downtime
           ? notifier::downtime
           : notifier::none));

  s->set_notification_interval(
      static_cast<double>(obj.notification_interval()));
  s->set_first_notification_delay(
      static_cast<double>(obj.first_notification_delay()));

  s->add_stalk_on(obj.stalking_options() & configuration::service::ok
                      ? notifier::ok
                      : notifier::none);
  s->add_stalk_on(obj.stalking_options() & configuration::service::warning
                      ? notifier::warning
                      : notifier::none);
  s->add_stalk_on(obj.stalking_options() & configuration::service::unknown
                      ? notifier::unknown
                      : notifier::none);
  s->add_stalk_on(obj.stalking_options() & configuration::service::critical
                      ? notifier::critical
                      : notifier::none);

  s->set_notification_period(obj.notification_period());
  s->set_check_period(obj.check_period());
  s->set_flap_detection_enabled(obj.flap_detection_enabled());
  s->set_low_flap_threshold(obj.low_flap_threshold());
  s->set_high_flap_threshold(obj.high_flap_threshold());

  s->set_flap_detection_on(notifier::none);
  s->add_flap_detection_on(obj.flap_detection_options() &
                                   configuration::service::ok
                               ? notifier::ok
                               : notifier::none);
  s->add_flap_detection_on(obj.flap_detection_options() &
                                   configuration::service::warning
                               ? notifier::warning
                               : notifier::none);
  s->add_flap_detection_on(obj.flap_detection_options() &
                                   configuration::service::unknown
                               ? notifier::unknown
                               : notifier::none);
  s->add_flap_detection_on(obj.flap_detection_options() &
                                   configuration::service::critical
                               ? notifier::critical
                               : notifier::none);

  s->set_process_performance_data(static_cast<int>(obj.process_perf_data()));
  s->set_check_freshness(obj.check_freshness());
  s->set_freshness_threshold(obj.freshness_threshold());
  s->set_accept_passive_checks(obj.checks_passive());
  s->set_event_handler(obj.event_handler());
  s->set_checks_enabled(obj.checks_active());
  s->set_retain_status_information(
      static_cast<bool>(obj.retain_status_information()));
  s->set_retain_nonstatus_information(
      static_cast<bool>(obj.retain_nonstatus_information()));
  s->set_notifications_enabled(obj.notifications_enabled());
  s->set_obsess_over(obj.obsess_over_service());
  s->set_notes(obj.notes());
  s->set_notes_url(obj.notes_url());
  s->set_action_url(obj.action_url());
  s->set_icon_image(obj.icon_image());
  s->set_icon_image_alt(obj.icon_image_alt());
  s->set_is_volatile(obj.is_volatile());
  s->set_timezone(obj.timezone());
  s->set_host_id(obj.host_id());
  s->set_service_id(obj.service_id());
  s->set_acknowledgement_timeout(obj.acknowledgement_timeout() *
                                 config->interval_length());
  s->set_recovery_notification_delay(obj.recovery_notification_delay());

  // Contacts.
  if (obj.contacts() != obj_old.contacts()) {
    // Delete old contacts.
    s->mut_contacts().clear();

    // Add contacts to host.
    for (set_string::const_iterator it(obj.contacts().begin()),
         end(obj.contacts().end());
         it != end; ++it)
      s->mut_contacts().insert({*it, nullptr});
  }

  // Contact groups.
  if (obj.contactgroups() != obj_old.contactgroups()) {
    // Delete old contact groups.
    s->get_contactgroups().clear();

    // Add contact groups to host.
    for (set_string::const_iterator it(obj.contactgroups().begin()),
         end(obj.contactgroups().end());
         it != end; ++it)
      s->get_contactgroups().insert({*it, nullptr});
  }

  // Custom variables.
  if (obj.customvariables() != obj_old.customvariables()) {
    for (auto& c : s->custom_variables) {
      if (c.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_SERVICECUSTOMVARIABLE_DELETE, s.get(),
                               c.first.c_str(), c.second.value().c_str(), &tv);
      }
    }
    s->custom_variables.clear();

    for (auto& c : obj.customvariables()) {
      s->custom_variables[c.first] = c.second;

      if (c.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_SERVICECUSTOMVARIABLE_ADD, s.get(),
                               c.first.c_str(), c.second.value().c_str(), &tv);
      }
    }
  }

  // Severity.
  if (obj.severity_id()) {
    configuration::severity::key_type k = {obj.severity_id(),
                                           configuration::severity::service};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << "Could not update the severity (" << k.first
                           << ", " << k.second << ") to the service '"
                           << obj.service_description() << "' of host '"
                           << obj.host_name() << "'";
    s->set_severity(sv->second);
  } else
    s->set_severity(nullptr);

  // add tags
  if (obj.tags() != obj_old.tags()) {
    s->mut_tags().clear();
    for (auto& t : obj.tags()) {
      tag_map::iterator it_tag{engine::tag::tags.find(t)};
      if (it_tag == engine::tag::tags.end())
        throw engine_error() << "Could not find tag '" << t.first
                             << "' on which to apply service (" << obj.host_id()
                             << ", " << obj.service_id() << ")";
      else
        s->mut_tags().emplace_front(it_tag->second);
    }
  }
  // Notify event broker.
  broker_adaptive_service_data(NEBTYPE_SERVICE_UPDATE, NEBFLAG_NONE,
                               NEBATTR_NONE, s.get(), MODATTR_ALL);
}

/**
 *  Remove old service.
 *
 *  @param[in] obj  The new service to remove from the monitoring
 *                  engine.
 */
void applier::service::remove_object(configuration::service const& obj) {
  std::string const& host_name(obj.host_name());
  std::string const& service_description(obj.service_description());

  assert(obj.key().first);
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing service '" << service_description << "' of host '"
      << host_name << "'.";
  log_v2::config()->debug("Removing service '{}' of host '{}'.",
                          service_description, host_name);

  // Find anomaly detections depending on this service
  set_anomalydetection sad = config->anomalydetections();
  for (auto cad : sad) {
    if (cad.host_id() == obj.key().first &&
        cad.dependent_service_id() == obj.key().second) {
      auto ad = engine::service::services_by_id.find(cad.key());
      std::static_pointer_cast<engine::anomalydetection>(ad->second)
          ->set_dependent_service(nullptr);
    }
  }
  // Find service.
  auto it = engine::service::services_by_id.find(obj.key());
  if (it != engine::service::services_by_id.end()) {
    std::shared_ptr<engine::service> svc(it->second);

    // Remove service comments.
    comment::delete_service_comments(obj.key().first, obj.key().second);

    // Remove service downtimes.
    downtime_manager::instance()
        .delete_downtime_by_hostname_service_description_start_time_comment(
            host_name, service_description, {false, (time_t)0}, "");

    // Remove events related to this service.
    applier::scheduler::instance().remove_service(obj.host_id(),
                                                  obj.service_id());

    // remove service from servicegroup->members
    for (auto& it_s : it->second->get_parent_groups())
      it_s->members.erase({host_name, service_description});

    // Notify event broker.
    broker_adaptive_service_data(NEBTYPE_SERVICE_DELETE, NEBFLAG_NONE,
                                 NEBATTR_NONE, svc.get(), MODATTR_ALL);

    // Unregister service.
    engine::service::services.erase({host_name, service_description});
    engine::service::services_by_id.erase(it);
  }

  // Remove service from the global configuration set.
  config->services().erase(obj);
}

/**
 *  Remove old service.
 *
 *  @param[in] obj  The new service to remove from the monitoring
 *                  engine.
 */
void applier::service::remove_object(ssize_t idx) {
  Service& obj = pb_config.mutable_services()->at(idx);
  const std::string& host_name = obj.hosts().data()[0];
  const std::string& service_description = obj.service_description();

  // Logging.
  log_v2::config()->debug("Removing service '{}' of host '{}'.",
                          service_description, host_name);

  // Find anomaly detections depending on this service
  for (auto cad : pb_config.anomalydetections()) {
    if (cad.host_id() == obj.host_id() &&
        cad.dependent_service_id() == obj.service_id()) {
      auto ad = engine::service::services_by_id.find(
          {cad.host_id(), cad.service_id()});
      if (ad != engine::service::services_by_id.end())
        std::static_pointer_cast<engine::anomalydetection>(ad->second)
            ->set_dependent_service(nullptr);
    }
  }
  // Find service.
  auto it =
      engine::service::services_by_id.find({obj.host_id(), obj.service_id()});
  if (it != engine::service::services_by_id.end()) {
    auto svc = it->second;

    // Remove service comments.
    comment::delete_service_comments(obj.host_id(), obj.service_id());

    // Remove service downtimes.
    downtime_manager::instance()
        .delete_downtime_by_hostname_service_description_start_time_comment(
            host_name, service_description, {false, (time_t)0}, "");

    // Remove events related to this service.
    applier::scheduler::instance().remove_service(obj.host_id(),
                                                  obj.service_id());

    // remove service from servicegroup->members
    for (auto& it_s : svc->get_parent_groups())
      it_s->members.erase({host_name, service_description});

    // Notify event broker.
    broker_adaptive_service_data(NEBTYPE_SERVICE_DELETE, NEBFLAG_NONE,
                                 NEBATTR_NONE, svc.get(), MODATTR_ALL);

    // Unregister service.
    engine::service::services.erase({host_name, service_description});
    engine::service::services_by_id.erase(it);
  }

  // Remove service from the global configuration set.
  pb_config.mutable_services()->DeleteSubrange(idx, 1);
}

/**
 *  Resolve a service.
 *
 *  @param[in] obj  Service object.
 */
void applier::service::resolve_object(const configuration::Service& obj) {
  // Logging.
  log_v2::config()->debug("Resolving service '{}' of host '{}'.",
                          obj.service_description(), obj.hosts().data()[0]);

  // Find service.
  service_id_map::iterator it =
      engine::service::services_by_id.find({obj.host_id(), obj.service_id()});
  if (engine::service::services_by_id.end() == it)
    throw engine_error() << "Cannot resolve non-existing service '"
                         << obj.service_description() << "' of host '"
                         << obj.hosts().data()[0] << "'";

  // Remove service group links.
  it->second->get_parent_groups().clear();

  // Find host and adjust its counters.
  host_id_map::iterator hst(engine::host::hosts_by_id.find(it->first.first));
  if (hst != engine::host::hosts_by_id.end()) {
    hst->second->set_total_services(hst->second->get_total_services() + 1);
    hst->second->set_total_service_check_interval(
        hst->second->get_total_service_check_interval() +
        static_cast<uint64_t>(it->second->check_interval()));
  }

  // Resolve service.
  it->second->resolve(config_warnings, config_errors);
}

/**
 *  Resolve a service.
 *
 *  @param[in] obj  Service object.
 */
void applier::service::resolve_object(configuration::service const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving service '" << obj.service_description() << "' of host '"
      << obj.host_name() << "'.";
  log_v2::config()->debug("Resolving service '{}' of host '{}'.",
                          obj.service_description(), obj.host_name());

  // Find service.
  service_id_map::iterator it(engine::service::services_by_id.find(obj.key()));
  if (engine::service::services_by_id.end() == it)
    throw engine_error() << "Cannot resolve non-existing service '"
                         << obj.service_description() << "' of host '"
                         << obj.host_name() << "'";

  // Remove service group links.
  it->second->get_parent_groups().clear();

  // Find host and adjust its counters.
  host_id_map::iterator hst(engine::host::hosts_by_id.find(it->first.first));
  if (hst != engine::host::hosts_by_id.end()) {
    hst->second->set_total_services(hst->second->get_total_services() + 1);
    hst->second->set_total_service_check_interval(
        hst->second->get_total_service_check_interval() +
        static_cast<uint64_t>(it->second->check_interval()));
  }

  // Resolve service.
  it->second->resolve(config_warnings, config_errors);
}

/**
 *  Expand service instance memberships.
 *
 *  @param[in]  obj Target service.
 *  @param[out] s   Configuration state.
 */
void applier::service::_expand_service_memberships(configuration::Service& obj,
                                                   configuration::State& s) {
  absl::flat_hash_map<absl::string_view, Servicegroup*> sgs;
  for (auto& sg : *s.mutable_servicegroups())
    sgs[sg.servicegroup_name()] = &sg;

  // Browse service groups.
  for (auto& sg_name : obj.servicegroups().data()) {
    // Find service group.
    auto found = sgs.find(sg_name);
    if (found == sgs.end())
      throw engine_error() << fmt::format(
          "Could not add service '{}' of host '{}' to non-existing service "
          "group '{}'",
          obj.service_description(), obj.hosts().data()[0], sg_name);

    // Add service to service members
    fill_pair_string_group(found->second->mutable_members(),
                           obj.hosts().data()[0], obj.service_description());
  }
}

/**
 *  Expand service instance memberships.
 *
 *  @param[in]  obj Target service.
 *  @param[out] s   Configuration state.
 */
void applier::service::_expand_service_memberships(configuration::service& obj,
                                                   configuration::state& s) {
  // Browse service groups.
  for (set_string::const_iterator it(obj.servicegroups().begin()),
       end(obj.servicegroups().end());
       it != end; ++it) {
    // Find service group.
    configuration::set_servicegroup::iterator it_group(
        s.servicegroups_find(*it));
    if (it_group == s.servicegroups().end())
      throw(engine_error() << "Could not add service '"
                           << obj.service_description() << "' of host '"
                           << obj.host_name()
                           << "' to non-existing service group '" << *it
                           << "'");

    // Remove service group from state.
    configuration::servicegroup backup(*it_group);
    s.servicegroups().erase(it_group);

    // Add service to service members.
    backup.members().insert(
        std::make_pair(obj.host_name(), obj.service_description()));

    // Reinsert service group.
    s.servicegroups().insert(backup);
  }
}

/**
 *  @brief Inherits special variables from host.
 *
 *  These special variables, if not defined are inherited from host.
 *  They are contact_groups, notification_interval and
 *  notification_period.
 *
 *  @param[in,out] obj Target service.
 *  @param[in]     s   Configuration state.
 */
void applier::service::_inherits_special_vars(configuration::service& obj,
                                              configuration::state const& s) {
  // Detect if any special variable has not been defined.
  if (!obj.host_id() || !obj.contacts_defined() ||
      !obj.contactgroups_defined() || !obj.notification_interval_defined() ||
      !obj.notification_period_defined() || !obj.timezone_defined()) {
    // Find host.
    configuration::set_host::const_iterator it(
        s.hosts_find(obj.hosts().begin()->c_str()));
    if (it == s.hosts().end())
      throw engine_error()
          << "Could not inherit special variables for service '"
          << obj.service_description() << "': host '" << obj.host_name()
          << "' does not exist";

    // Inherits variables.
    if (!obj.host_id())
      obj.set_host_id(it->host_id());
    if (!obj.contacts_defined() && !obj.contactgroups_defined()) {
      obj.contacts() = it->contacts();
      obj.contactgroups() = it->contactgroups();
    }
    if (!obj.notification_interval_defined())
      obj.notification_interval(it->notification_interval());
    if (!obj.notification_period_defined())
      obj.notification_period(it->notification_period());
    if (!obj.timezone_defined())
      obj.timezone(it->timezone());
  }
}

/**
 *  @brief Inherits special variables from host.
 *
 *  These special variables, if not defined are inherited from host.
 *  They are contact_groups, notification_interval and
 *  notification_period.
 *
 *  @param[in,out] obj Target service.
 *  @param[in]     s   Configuration state.
 */
void applier::service::_inherits_special_vars(configuration::Service& obj,
                                              const configuration::State& s) {
  // Detect if any special variable has not been defined.
  if (!obj.host_id() || obj.contacts().data().empty() ||
      obj.contactgroups().data().empty() || obj.notification_interval() == 0 ||
      obj.notification_period().empty() || obj.timezone().empty()) {
    // Find host.
    auto it = std::find_if(s.hosts().begin(), s.hosts().end(),
                           [name = obj.hosts().data()[0]](const Host& h) {
                             return h.host_name() == name;
                           });
    if (it == s.hosts().end())
      throw engine_error() << fmt::format(
          "Could not inherit special variables for service '{}': host '{}' "
          "does not exist",
          obj.service_description(), obj.hosts().data()[0]);

    // Inherits variables.
    if (!obj.host_id())
      obj.set_host_id(it->host_id());
    if (obj.contacts().data().empty() && obj.contactgroups().data().empty()) {
      obj.mutable_contacts()->CopyFrom(it->contacts());
      obj.mutable_contactgroups()->CopyFrom(it->contactgroups());
    }
    if (obj.notification_interval() == 0)
      obj.set_notification_interval(it->notification_interval());
    if (obj.notification_period().empty())
      obj.set_notification_period(it->notification_period());
    if (obj.timezone().empty())
      obj.set_timezone(it->timezone());
  }
}
