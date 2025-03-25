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

#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/scheduler.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/severity.hh"
#include "common/engine_conf/severity_helper.hh"

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
  if (obj.host_name().empty())
    throw engine_error() << fmt::format(
        "Could not create service '{}' with no host defined",
        obj.service_description());
  else if (obj.host_id() == 0)
    throw engine_error() << fmt::format(
        "No host_id available for the host '{}' - unable to create service "
        "'{}'",
        obj.host_name(), obj.service_description());

  // Logging.
  config_logger->debug("Creating new service '{}' of host '{}'.",
                       obj.service_description(), obj.host_name());

  // Add service to the global configuration set.
  auto* cfg_svc = pb_indexed_config.mut_state().add_services();
  cfg_svc->CopyFrom(obj);

  // Create service.
  engine::service* svc{add_service(
      obj.host_id(), obj.service_id(), obj.host_name(),
      obj.service_description(), obj.display_name(), obj.check_period(),
      obj.max_check_attempts(), obj.check_interval(), obj.retry_interval(),
      obj.notification_interval(), obj.first_notification_delay(),
      obj.recovery_notification_delay(), obj.notification_period(),
      static_cast<bool>(obj.notification_options() & action_svc_ok),
      static_cast<bool>(obj.notification_options() & action_svc_unknown),
      static_cast<bool>(obj.notification_options() & action_svc_warning),
      static_cast<bool>(obj.notification_options() & action_svc_critical),
      static_cast<bool>(obj.notification_options() & action_svc_flapping),
      static_cast<bool>(obj.notification_options() & action_svc_downtime),
      obj.notifications_enabled(), obj.is_volatile(), obj.event_handler(),
      obj.event_handler_enabled(), obj.check_command(), obj.checks_active(),
      obj.checks_passive(), obj.flap_detection_enabled(),
      obj.low_flap_threshold(), obj.high_flap_threshold(),
      static_cast<bool>(obj.flap_detection_options() & action_svc_ok),
      static_cast<bool>(obj.flap_detection_options() & action_svc_warning),
      static_cast<bool>(obj.flap_detection_options() & action_svc_unknown),
      static_cast<bool>(obj.flap_detection_options() & action_svc_critical),
      static_cast<bool>(obj.stalking_options() & action_svc_ok),
      static_cast<bool>(obj.stalking_options() & action_svc_warning),
      static_cast<bool>(obj.stalking_options() & action_svc_unknown),
      static_cast<bool>(obj.stalking_options() & action_svc_critical),
      obj.process_perf_data(), obj.check_freshness(), obj.freshness_threshold(),
      obj.notes(), obj.notes_url(), obj.action_url(), obj.icon_image(),
      obj.icon_image_alt(), obj.retain_status_information(),
      obj.retain_nonstatus_information(), obj.obsess_over_service(),
      obj.timezone(), obj.icon_id())};
  if (!svc)
    throw engine_error() << fmt::format(
        "Could not register service '{}' of host '{}'",
        obj.service_description(), obj.host_name());
  svc->set_initial_notif_time(0);
  engine::service::services[{obj.host_name(), obj.service_description()}]
      ->set_host_id(obj.host_id());
  engine::service::services[{obj.host_name(), obj.service_description()}]
      ->set_service_id(obj.service_id());
  svc->set_acknowledgement_timeout(obj.acknowledgement_timeout() *
                                   pb_indexed_config.state().interval_length());
  svc->set_last_acknowledgement(0);

  // Add contacts.
  for (auto& c : obj.contacts().data())
    svc->mut_contacts().insert({c, nullptr});

  // Add contactgroups.
  for (auto& cg : obj.contactgroups().data())
    svc->get_contactgroups().insert({cg, nullptr});

  // Add custom variables.
  for (auto& cv : obj.customvariables()) {
    svc->custom_variables.emplace(
        cv.name(), engine::customvariable(cv.value(), cv.is_sent()));

    if (cv.is_sent()) {
      timeval tv(get_broker_timestamp(nullptr));
      broker_custom_variable(NEBTYPE_SERVICECUSTOMVARIABLE_ADD, svc, cv.name(),
                             cv.value(), &tv);
    }
  }

  // Add severity.
  if (obj.severity_id()) {
    configuration::severity_helper::key_type k = {obj.severity_id(),
                                                  SeverityType::service};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << fmt::format(
          "Could not add the severity ({}, {}) to the service '{}' of host "
          "'{}'",
          k.first, k.second, obj.service_description(), obj.host_name());
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
  broker_adaptive_service_data(NEBTYPE_SERVICE_ADD, NEBFLAG_NONE, svc,
                               MODATTR_ALL);
}

/**
 *  Expand a service object.
 *
 *  @param[in,out] s  State being applied.
 */
void applier::service::expand_objects(configuration::indexed_state& s) {
  std::list<std::unique_ptr<Service>> expanded;
  // Let's consider all the macros defined in s.
  absl::flat_hash_set<std::string_view> cvs;
  for (auto& cv : s.state().macros_filter().data())
    cvs.emplace(cv);

  absl::flat_hash_map<std::string_view, configuration::Hostgroup*> hgs;
  for (auto& hg : *s.mut_state().mutable_hostgroups())
    hgs.emplace(hg.hostgroup_name(), &hg);

  // Browse all services.
  for (auto& service_cfg : *s.mut_state().mutable_services()) {
    // Should custom variables be sent to broker ?
    for (auto& cv : *service_cfg.mutable_customvariables()) {
      if (!s.state().enable_macros_filter() || cvs.contains(cv.name()))
        cv.set_is_sent(true);
    }

    // Expand membershipts.
    _expand_service_memberships(service_cfg, s);

    // Inherits special vars.
    _inherits_special_vars(service_cfg, s);
  }
}

/**
 *  @brief Modify a service configuration and the real time associated service.
 *
 *  @param[in] old_obj  The service to modify into the monitoring
 *                      engine.
 *  @param[in] new_obj  The new service to apply.
 */
void applier::service::modify_object(configuration::Service* old_obj,
                                     const configuration::Service& new_obj) {
  const std::string& host_name(old_obj->host_name());
  const std::string& service_description(old_obj->service_description());

  // Logging.
  config_logger->debug("Modifying service '{}' of host '{}'.",
                       service_description, host_name);

  // Find service object.
  service_id_map::iterator it_obj = engine::service::services_by_id.find(
      {old_obj->host_id(), old_obj->service_id()});
  if (it_obj == engine::service::services_by_id.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing service object '{}' of host '{}'",
        service_description, host_name);
  std::shared_ptr<engine::service> s = it_obj->second;

  // Modify properties.
  if (it_obj->second->get_hostname() != new_obj.host_name() ||
      it_obj->second->description() != new_obj.service_description()) {
    engine::service::services.erase(
        {it_obj->second->get_hostname(), it_obj->second->description()});
    engine::service::services.insert(
        {{new_obj.host_name(), new_obj.service_description()}, it_obj->second});
  }

  s->set_hostname(new_obj.host_name());
  s->set_description(new_obj.service_description());
  s->set_display_name(new_obj.display_name());
  s->set_check_command(new_obj.check_command());
  s->set_event_handler(new_obj.event_handler());
  s->set_event_handler_enabled(new_obj.event_handler_enabled());
  s->set_check_interval(new_obj.check_interval());
  s->set_retry_interval(new_obj.retry_interval());
  s->set_max_attempts(new_obj.max_check_attempts());

  s->set_notify_on(
      (new_obj.notification_options() & action_svc_unknown ? notifier::unknown
                                                           : notifier::none) |
      (new_obj.notification_options() & action_svc_warning ? notifier::warning
                                                           : notifier::none) |
      (new_obj.notification_options() & action_svc_critical ? notifier::critical
                                                            : notifier::none) |
      (new_obj.notification_options() & action_svc_ok ? notifier::ok
                                                      : notifier::none) |
      (new_obj.notification_options() & action_svc_flapping
           ? (notifier::flappingstart | notifier::flappingstop |
              notifier::flappingdisabled)
           : notifier::none) |
      (new_obj.notification_options() & action_svc_downtime ? notifier::downtime
                                                            : notifier::none));

  s->set_notification_interval(
      static_cast<double>(new_obj.notification_interval()));
  s->set_first_notification_delay(
      static_cast<double>(new_obj.first_notification_delay()));
  s->set_stalk_on(notifier::none);
  s->add_stalk_on(new_obj.stalking_options() & action_svc_ok ? notifier::ok
                                                             : notifier::none);
  s->add_stalk_on(new_obj.stalking_options() & action_svc_warning
                      ? notifier::warning
                      : notifier::none);
  s->add_stalk_on(new_obj.stalking_options() & action_svc_unknown
                      ? notifier::unknown
                      : notifier::none);
  s->add_stalk_on(new_obj.stalking_options() & action_svc_critical
                      ? notifier::critical
                      : notifier::none);

  s->set_notification_period(new_obj.notification_period());
  s->set_check_period(new_obj.check_period());
  s->set_flap_detection_enabled(new_obj.flap_detection_enabled());
  s->set_low_flap_threshold(new_obj.low_flap_threshold());
  s->set_high_flap_threshold(new_obj.high_flap_threshold());

  s->set_flap_detection_on(notifier::none);
  s->add_flap_detection_on(new_obj.flap_detection_options() & action_svc_ok
                               ? notifier::ok
                               : notifier::none);
  s->add_flap_detection_on(new_obj.flap_detection_options() & action_svc_warning
                               ? notifier::warning
                               : notifier::none);
  s->add_flap_detection_on(new_obj.flap_detection_options() & action_svc_unknown
                               ? notifier::unknown
                               : notifier::none);
  s->add_flap_detection_on(new_obj.flap_detection_options() &
                                   action_svc_critical
                               ? notifier::critical
                               : notifier::none);

  s->set_process_performance_data(
      static_cast<int>(new_obj.process_perf_data()));
  s->set_check_freshness(new_obj.check_freshness());
  s->set_freshness_threshold(new_obj.freshness_threshold());
  s->set_accept_passive_checks(new_obj.checks_passive());
  s->set_event_handler(new_obj.event_handler());
  s->set_checks_enabled(new_obj.checks_active());
  s->set_retain_status_information(
      static_cast<bool>(new_obj.retain_status_information()));
  s->set_retain_nonstatus_information(
      static_cast<bool>(new_obj.retain_nonstatus_information()));
  s->set_notifications_enabled(new_obj.notifications_enabled());
  s->set_obsess_over(new_obj.obsess_over_service());
  s->set_notes(new_obj.notes());
  s->set_notes_url(new_obj.notes_url());
  s->set_action_url(new_obj.action_url());
  s->set_icon_image(new_obj.icon_image());
  s->set_icon_image_alt(new_obj.icon_image_alt());
  s->set_is_volatile(new_obj.is_volatile());
  s->set_timezone(new_obj.timezone());
  s->set_host_id(new_obj.host_id());
  s->set_service_id(new_obj.service_id());
  s->set_acknowledgement_timeout(new_obj.acknowledgement_timeout() *
                                 pb_indexed_config.state().interval_length());
  s->set_recovery_notification_delay(new_obj.recovery_notification_delay());
  s->set_icon_id(new_obj.icon_id());

  // Contacts.
  if (!MessageDifferencer::Equals(new_obj.contacts(), old_obj->contacts())) {
    // Delete old contacts.
    s->mut_contacts().clear();

    // Add contacts to host.
    for (auto& contact_name : new_obj.contacts().data())
      s->mut_contacts().insert({contact_name, nullptr});
  }

  // Contact groups.
  if (!MessageDifferencer::Equals(new_obj.contactgroups(),
                                  old_obj->contactgroups())) {
    // Delete old contact groups.
    s->get_contactgroups().clear();

    // Add contact groups to host.
    for (auto& cg_name : new_obj.contactgroups().data())
      s->get_contactgroups().insert({cg_name, nullptr});
  }

  // Custom variables.
  if (!std::equal(
          new_obj.customvariables().begin(), new_obj.customvariables().end(),
          old_obj->customvariables().begin(), old_obj->customvariables().end(),
          MessageDifferencer::Equals)) {
    for (auto& c : s->custom_variables) {
      if (c.second.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_SERVICECUSTOMVARIABLE_DELETE, s.get(),
                               c.first.c_str(), c.second.value().c_str(), &tv);
      }
    }
    s->custom_variables.clear();

    for (auto& c : new_obj.customvariables()) {
      s->custom_variables[c.name()] = c.value();

      if (c.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable(NEBTYPE_SERVICECUSTOMVARIABLE_ADD, s.get(),
                               c.name(), c.value(), &tv);
      }
    }
  }

  // Severity.
  if (new_obj.severity_id()) {
    configuration::severity_helper::key_type k = {new_obj.severity_id(),
                                                  SeverityType::service};
    auto sv = engine::severity::severities.find(k);
    if (sv == engine::severity::severities.end())
      throw engine_error() << "Could not update the severity (" << k.first
                           << ", " << k.second << ") to the service '"
                           << new_obj.service_description() << "' of host '"
                           << new_obj.host_name() << "'";
    s->set_severity(sv->second);
  } else
    s->set_severity(nullptr);

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
    s->mut_tags().clear();
    old_obj->mutable_tags()->CopyFrom(new_obj.tags());
    for (auto& t : new_obj.tags()) {
      tag_map::iterator it_tag =
          engine::tag::tags.find({t.first(), t.second()});
      if (it_tag == engine::tag::tags.end())
        throw engine_error() << fmt::format(
            "Could not find tag '{}' on which to apply service ({}, {})",
            t.first(), new_obj.host_id(), new_obj.service_id());
      else
        s->mut_tags().emplace_front(it_tag->second);
    }
  }
  // Notify event broker.
  broker_adaptive_service_data(NEBTYPE_SERVICE_UPDATE, NEBFLAG_NONE, s.get(),
                               MODATTR_ALL);
}

/**
 *  Remove old service.
 *
 *  @param[in] obj  The new service to remove from the monitoring
 *                  engine.
 */
template <>
void applier::service::remove_object(
    const std::pair<ssize_t, std::pair<uint64_t, uint64_t>>& p) {
  Service& obj = pb_indexed_config.mut_state().mutable_services()->at(p.first);
  const std::string& host_name = obj.host_name();
  const std::string& service_description = obj.service_description();

  // Logging.
  config_logger->debug("Removing service '{}' of host '{}'.",
                       service_description, host_name);

  // Find anomaly detections depending on this service
  for (auto cad : pb_indexed_config.state().anomalydetections()) {
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
                                 svc.get(), MODATTR_ALL);

    // Unregister service.
    engine::service::services.erase({host_name, service_description});
    engine::service::services_by_id.erase(it);
  }

  // Remove service from the global configuration set.
  pb_indexed_config.mut_state().mutable_services()->DeleteSubrange(p.first, 1);
}

/**
 *  Resolve a service.
 *
 *  @param[in] obj  Service object.
 */
void applier::service::resolve_object(const configuration::Service& obj,
                                      error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving service '{}' of host '{}'.",
                       obj.service_description(), obj.host_name());

  // Find service.
  service_id_map::iterator it =
      engine::service::services_by_id.find({obj.host_id(), obj.service_id()});
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
  it->second->resolve(err.config_warnings, err.config_errors);
}

/**
 *  Expand service instance memberships.
 *
 *  @param[in]  obj Target service.
 *  @param[out] s   Configuration state.
 */
void applier::service::_expand_service_memberships(
    configuration::Service& obj,
    configuration::indexed_state& s) {
  absl::flat_hash_map<std::string_view, Servicegroup*> sgs;
  for (auto& sg : *s.mut_state().mutable_servicegroups())
    sgs[sg.servicegroup_name()] = &sg;

  // Browse service groups.
  for (auto& sg_name : obj.servicegroups().data()) {
    // Find service group.
    auto found = sgs.find(sg_name);
    if (found == sgs.end())
      throw engine_error() << fmt::format(
          "Could not add service '{}' of host '{}' to non-existing service "
          "group '{}'",
          obj.service_description(), obj.host_name(), sg_name);

    // Add service to service members
    fill_pair_string_group(found->second->mutable_members(), obj.host_name(),
                           obj.service_description());
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
void applier::service::_inherits_special_vars(
    configuration::Service& obj,
    const configuration::indexed_state& s) {
  // Detect if any special variable has not been defined.
  if (!obj.host_id() || obj.contacts().data().empty() ||
      obj.contactgroups().data().empty() || obj.notification_interval() == 0 ||
      obj.notification_period().empty() || obj.timezone().empty()) {
    config_logger->error("inherits_special_vars dans if");
    // Find host.
    auto it = std::find_if(s.hosts().begin(), s.hosts().end(),
                           [name = obj.host_name()](const auto& p) {
                             return p.second->host_name() == name;
                           });
    if (it == s.hosts().end()) {
      config_logger->error("inherits_special_vars dans throw");
      config_logger->error(
          "Could not inherit special variables for service '{}': host '{}' "
          "does not exist",
          obj.service_description(), obj.host_name());
      throw engine_error() << fmt::format(
          "Could not inherit special variables for service '{}': host '{}' "
          "does not exist",
          obj.service_description(), obj.host_name());
    }

    // Inherits variables.
    if (!obj.host_id())
      obj.set_host_id(it->second->host_id());
    if (obj.contacts().data().empty() && obj.contactgroups().data().empty()) {
      obj.mutable_contacts()->CopyFrom(it->second->contacts());
      obj.mutable_contactgroups()->CopyFrom(it->second->contactgroups());
    }
    if (obj.notification_interval() == 0)
      obj.set_notification_interval(it->second->notification_interval());
    if (obj.notification_period().empty())
      obj.set_notification_period(it->second->notification_period());
    if (obj.timezone().empty())
      obj.set_timezone(it->second->timezone());
  }
  config_logger->error("inherits_special_vars après if");
}
