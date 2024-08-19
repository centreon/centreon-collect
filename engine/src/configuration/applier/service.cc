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
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/scheduler.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/severity.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::configuration;

/**
 *  Check if the service group name matches the configuration object.
 */
class servicegroup_name_comparator {
 public:
  servicegroup_name_comparator(std::string const& servicegroup_name) {
    _servicegroup_name = servicegroup_name;
  }

  bool operator()(std::shared_ptr<configuration::servicegroup> sg) {
    return _servicegroup_name == sg->servicegroup_name();
  }

 private:
  std::string _servicegroup_name;
};

/**
 *  Default constructor.
 */
applier::service::service() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
applier::service::service(applier::service const& right) {
  (void)right;
}

/**
 *  Destructor.
 */
applier::service::~service() {}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
applier::service& applier::service::operator=(applier::service const& right) {
  (void)right;
  return *this;
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
  config_logger->debug("Creating new service '{}' of host '{}'.",
                       obj.service_description(), obj.host_name());

  // Add service to the global configuration set.
  config->mut_services().insert(obj);

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
  for (auto it = obj.customvariables().begin(),
            end = obj.customvariables().end();
       it != end; ++it) {
    svc->custom_variables[it->first] =
        engine::customvariable(it->second.value(), it->second.is_sent());

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
void applier::service::expand_objects(configuration::state& s) {
  // Browse all services.
  configuration::set_service expanded;
  for (auto svc : s.services()) {
    // Should custom variables be sent to broker ?
    for (auto it = svc.mut_customvariables().begin(),
              end = svc.mut_customvariables().end();
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
    expanded.insert(std::move(svc));
  }

  // Set expanded services in configuration state.
  s.mut_services() = std::move(expanded);
}

/**
 *  Modified service.
 *
 *  @param[in] obj  The new service to modify into the monitoring
 *                  engine.
 */
void applier::service::modify_object(configuration::service const& obj) {
  const std::string& host_name = obj.host_name();
  std::string const& service_description(obj.service_description());

  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Modifying new service '" << service_description << "' of host '"
      << host_name << "'.";
  config_logger->debug("Modifying new service '{}' of host '{}'.",
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
  config->mut_services().erase(it_cfg);
  config->mut_services().insert(obj);

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
      s->custom_variables[c.first] =
          engine::customvariable(c.second.value(), c.second.is_sent());

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
  config_logger->debug("Removing service '{}' of host '{}'.",
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
  config->mut_services().erase(obj);
}

/**
 *  Resolve a service.
 *
 *  @param[in] obj  Service object.
 */
void applier::service::resolve_object(configuration::service const& obj,
                                      error_cnt& err) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving service '" << obj.service_description() << "' of host '"
      << obj.host_name() << "'.";
  config_logger->debug("Resolving service '{}' of host '{}'.",
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
  it->second->resolve(err.config_warnings, err.config_errors);
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
        s.hosts_find(obj.host_name().c_str()));
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
