/**
 * Copyright 2020,2023-2024 Centreon
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
#include "com/centreon/engine/configuration/applier/anomalydetection.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/scheduler.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::configuration;

/**
 * @brief Add new anomalydetection.
 *
 * @param obj The new anomalydetection protobuf configuration to add into the
 * monitoring engine.
 */
void applier::anomalydetection::add_object(
    const configuration::Anomalydetection& obj) {
  // Check anomalydetection.
  if (!obj.host_id())
    throw engine_error() << fmt::format(
        "No host_id available for the host '{} - unable to create "
        "anomalydetection '{}'",
        obj.host_name(), obj.service_description());

  // Logging.
  SPDLOG_LOGGER_DEBUG(config_logger,
                      "Creating new anomalydetection '{}' of host '{}'.",
                      obj.service_description(), obj.host_name());

  // Add anomalydetection to the global configuration set.
  pb_indexed_config.mut_anomalydetections().emplace(
      std::make_pair(obj.host_id(), obj.service_id()),
      std::make_unique<Anomalydetection>(obj));

  // Create anomalydetection.
  engine::anomalydetection* ad{add_anomalydetection(
      obj.host_id(), obj.service_id(), obj.host_name(),
      obj.service_description(), obj.display_name(), obj.internal_id(),
      obj.dependent_service_id(), obj.metric_name(), obj.thresholds_file(),
      obj.status_change(), obj.max_check_attempts(), obj.check_interval(),
      obj.retry_interval(), obj.notification_interval(),
      obj.first_notification_delay(), obj.recovery_notification_delay(),
      obj.notification_period(),
      static_cast<bool>(obj.notification_options() & action_svc_ok),
      static_cast<bool>(obj.notification_options() & action_svc_unknown),
      static_cast<bool>(obj.notification_options() & action_svc_warning),
      static_cast<bool>(obj.notification_options() & action_svc_critical),
      static_cast<bool>(obj.notification_options() & action_svc_flapping),
      static_cast<bool>(obj.notification_options() & action_svc_downtime),
      obj.notifications_enabled(), obj.is_volatile(), obj.event_handler(),
      obj.event_handler_enabled(), obj.checks_active(), obj.checks_passive(),
      obj.flap_detection_enabled(), obj.low_flap_threshold(),
      obj.high_flap_threshold(),
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
      obj.timezone(), obj.icon_id(), obj.sensitivity())};
  if (!ad)
    throw engine_error() << "Could not register anomalydetection '"
                         << obj.service_description() << "' of host '"
                         << obj.host_name() << "'";
  ad->set_initial_notif_time(0);
  engine::anomalydetection::services[{obj.host_name(),
                                      obj.service_description()}]
      ->set_host_id(obj.host_id());
  engine::anomalydetection::services[{obj.host_name(),
                                      obj.service_description()}]
      ->set_service_id(obj.service_id());
  ad->set_acknowledgement_timeout(obj.acknowledgement_timeout() *
                                  pb_indexed_config.state().interval_length());
  ad->set_last_acknowledgement(0);

  // Add contacts.
  for (auto& c : obj.contacts().data())
    ad->mut_contacts().insert({c, nullptr});

  // Add contactgroups.
  for (auto& cg : obj.contactgroups().data())
    ad->get_contactgroups().insert({cg, nullptr});

  // Add custom variables.
  for (auto& cv : obj.customvariables()) {
    engine::customvariable& c = ad->custom_variables[cv.name()];
    c.set_value(cv.value());
    c.set_sent(cv.is_sent());

    if (c.is_sent()) {
      timeval tv(get_broker_timestamp(nullptr));
      broker_custom_variable<engine::service>(NEBTYPE_SERVICECUSTOMVARIABLE_ADD,
                                              ad, cv.name(), cv.value(), &tv);
    }
  }

  // Notify event broker.
  broker_adaptive_service_data(NEBTYPE_SERVICE_ADD, NEBFLAG_NONE, ad,
                               MODATTR_ALL);
}

/**
 *  Modified anomalydetection.
 *
 *  @param[in] obj  The new anomalydetection to modify into the monitoring
 *                  engine.
 */
void applier::anomalydetection::modify_object(
    configuration::Anomalydetection* old_obj,
    const configuration::Anomalydetection& new_obj) {
  const std::string& host_name(old_obj->host_name());
  const std::string& service_description(old_obj->service_description());

  // Logging.
  SPDLOG_LOGGER_DEBUG(config_logger,
                      "Modifying new anomalydetection '{}' of host '{}'.",
                      service_description, host_name);

  // Find anomalydetection object.
  service_id_map::iterator it_obj =
      engine::anomalydetection::services_by_id.find(
          {old_obj->host_id(), old_obj->service_id()});
  if (it_obj == engine::anomalydetection::services_by_id.end())
    throw engine_error() << fmt::format(
        "Could not modify non-existing anomalydetection object '{}' of host "
        "'{}'",
        service_description, host_name);
  std::shared_ptr<engine::anomalydetection> s =
      std::static_pointer_cast<engine::anomalydetection>(it_obj->second);

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
  s->set_metric_name(new_obj.metric_name());
  s->set_thresholds_file(new_obj.thresholds_file());
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
        broker_custom_variable<engine::service>(
            NEBTYPE_SERVICECUSTOMVARIABLE_DELETE, s.get(), c.first.c_str(),
            c.second.value().c_str(), &tv);
      }
    }
    s->custom_variables.clear();

    for (auto& c : new_obj.customvariables()) {
      s->custom_variables[c.name()] = c.value();

      if (c.is_sent()) {
        timeval tv(get_broker_timestamp(nullptr));
        broker_custom_variable<engine::service>(
            NEBTYPE_SERVICECUSTOMVARIABLE_ADD, s.get(), c.name(), c.value(),
            &tv);
      }
    }
  }

  // Notify event broker.
  broker_adaptive_service_data(NEBTYPE_SERVICE_UPDATE, NEBFLAG_NONE, s.get(),
                               MODATTR_ALL);
}

void applier::anomalydetection::remove_object(
    const std::pair<uint64_t, uint64_t>& key) {
  Anomalydetection& obj = *pb_indexed_config.mut_anomalydetections().at(key);
  const std::string& host_name(obj.host_name());
  const std::string& service_description(obj.service_description());

  // Logging.
  SPDLOG_LOGGER_DEBUG(config_logger,
                      "Removing anomalydetection '{}' of host '{}'.",
                      service_description, host_name);

  // Find anomalydetection.
  auto it =
      engine::service::services_by_id.find({obj.host_id(), obj.service_id()});
  if (it != engine::service::services_by_id.end()) {
    std::shared_ptr<engine::anomalydetection> ad(
        std::static_pointer_cast<engine::anomalydetection>(it->second));

    // Remove anomalydetection comments.
    comment::delete_service_comments(obj.host_id(), obj.service_id());

    // Remove anomalydetection downtimes.
    downtime_manager::instance()
        .delete_downtime_by_hostname_service_description_start_time_comment(
            host_name, service_description, {false, (time_t)0}, "");

    // Remove events related to this anomalydetection.
    applier::scheduler::instance().remove_service(obj.host_id(),
                                                  obj.service_id());

    // remove anomalydetection from servicegroup->members
    for (auto& it_s : it->second->get_parent_groups())
      it_s->members.erase({host_name, service_description});

    // Notify event broker.
    broker_adaptive_service_data(NEBTYPE_SERVICE_DELETE, NEBFLAG_NONE, ad.get(),
                                 MODATTR_ALL);

    // Unregister anomalydetection.
    engine::anomalydetection::services.erase({host_name, service_description});
    engine::anomalydetection::services_by_id.erase(it);
  }

  // Remove anomalydetection from the global configuration set.
  pb_indexed_config.mut_anomalydetections().erase(key);
}

/**
 *  Resolve a anomalydetection.
 *
 *  @param[in] obj  Service object.
 */
void applier::anomalydetection::resolve_object(
    const configuration::Anomalydetection& obj,
    error_cnt& err) {
  // Logging.
  SPDLOG_LOGGER_DEBUG(config_logger,
                      "Resolving anomalydetection '{}' of host '{}'.",
                      obj.service_description(), obj.host_name());

  // Find anomalydetection.
  service_id_map::iterator it = engine::anomalydetection::services_by_id.find(
      {obj.host_id(), obj.service_id()});
  if (engine::anomalydetection::services_by_id.end() == it)
    throw engine_error() << "Cannot resolve non-existing anomalydetection '"
                         << obj.service_description() << "' of host '"
                         << obj.host_name() << "'";

  // Remove anomalydetection group links.
  it->second->get_parent_groups().clear();

  // Find host and adjust its counters.
  host_id_map::iterator hst(engine::host::hosts_by_id.find(it->first.first));
  if (hst != engine::host::hosts_by_id.end()) {
    hst->second->set_total_services(hst->second->get_total_services() + 1);
    hst->second->set_total_service_check_interval(
        hst->second->get_total_service_check_interval() +
        static_cast<uint64_t>(it->second->check_interval()));
  }

  // Resolve anomalydetection.
  it->second->resolve(err.config_warnings, err.config_errors);
}
