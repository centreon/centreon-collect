/**
 * Copyright 2011-2024 Centreon
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
#include "com/centreon/engine/configuration/applier/serviceescalation.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "common/engine_legacy_conf/serviceescalation.hh"

using namespace com::centreon::engine::configuration;

/**
 *  Add new service escalation.
 *
 *  @param[in] obj  The new service escalation to add into the
 *                  monitoring engine.
 */
void applier::serviceescalation::add_object(
    configuration::serviceescalation const& obj) {
  // Check service escalation.
  if ((obj.hosts().size() != 1) || !obj.hostgroups().empty() ||
      obj.service_description().size() != 1 || !obj.servicegroups().empty())
    throw engine_error() << "Could not create service escalation with multiple "
                            "hosts / host groups / services / service groups";

  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new escalation for service '"
      << obj.service_description().front() << "' of host '"
      << obj.hosts().front() << "'";
  config_logger->debug("Creating new escalation for service '{}' of host '{}'",
                       obj.service_description().front(), obj.hosts().front());

  // Add escalation to the global configuration set.
  config->serviceescalations().insert(obj);

  size_t key = configuration::serviceescalation_key(obj);

  // Create service escalation.
  auto se = std::make_shared<engine::serviceescalation>(
      obj.hosts().front(), obj.service_description().front(),
      obj.first_notification(), obj.last_notification(),
      obj.notification_interval(), obj.escalation_period(),
      ((obj.escalation_options() & configuration::serviceescalation::warning)
           ? notifier::warning
           : notifier::none) |
          ((obj.escalation_options() &
            configuration::serviceescalation::unknown)
               ? notifier::unknown
               : notifier::none) |
          ((obj.escalation_options() &
            configuration::serviceescalation::critical)
               ? notifier::critical
               : notifier::none) |
          ((obj.escalation_options() &
            configuration::serviceescalation::recovery)
               ? notifier::ok
               : notifier::none),
      key);

  // Add new items to the global list.
  engine::serviceescalation::serviceescalations.insert(
      {{se->get_hostname(), se->get_description()}, se});

  // Notify event broker.
  timeval tv(get_broker_timestamp(nullptr));
  broker_adaptive_escalation_data(NEBTYPE_SERVICEESCALATION_ADD, NEBFLAG_NONE,
                                  NEBATTR_NONE, se.get(), &tv);

  // Add contact groups to service escalation.
  for (set_string::const_iterator it(obj.contactgroups().begin()),
       end(obj.contactgroups().end());
       it != end; ++it)
    se->get_contactgroups().insert({*it, nullptr});
}

/**
 *  Expand all service escalations.
 *
 *  @param[in,out] s  Configuration being applied.
 */
void applier::serviceescalation::expand_objects(configuration::state& s) {
  // Browse all escalations.
  engine_logger(logging::dbg_config, logging::more)
      << "Expanding service escalations";
  config_logger->debug("Expanding service escalations");

  configuration::set_serviceescalation expanded;
  for (configuration::set_serviceescalation::const_iterator
           it_esc(s.serviceescalations().begin()),
       end_esc(s.serviceescalations().end());
       it_esc != end_esc; ++it_esc) {
    // Expanded services.
    std::set<std::pair<std::string, std::string>> expanded_services;
    _expand_services(it_esc->hosts(), it_esc->hostgroups(),
                     it_esc->service_description(), it_esc->servicegroups(), s,
                     expanded_services);

    // Browse all services.
    for (std::set<std::pair<std::string, std::string>>::const_iterator
             it(expanded_services.begin()),
         end(expanded_services.end());
         it != end; ++it) {
      configuration::serviceescalation sesc(*it_esc);
      sesc.hostgroups().clear();
      sesc.hosts().clear();
      sesc.hosts().push_back(it->first);
      sesc.servicegroups().clear();
      sesc.service_description().clear();
      sesc.service_description().push_back(it->second);

      // Insert new service escalation and expand it.
      _inherits_special_vars(sesc, s);
      expanded.insert(sesc);
    }
  }

  // Set expanded service escalations in configuration state.
  s.serviceescalations().swap(expanded);
}

/**
 *  @brief Modify service escalation.
 *
 *  Service escalations cannot be defined with anything else than their
 *  full content. Therefore no modification can occur.
 *
 *  @param[in] obj  Unused.
 */
void applier::serviceescalation::modify_object(
    configuration::serviceescalation const& obj) {
  (void)obj;
  throw engine_error()
      << "Could not modify a service "
      << "escalation: service escalation objects can only be added "
      << "or removed, this is likely a software bug that you should "
      << "report to Centreon Engine developers";
}

/**
 *  Remove old service escalation.
 *
 *  @param[in] obj  The service escalation to remove from the monitoring
 *                  engine.
 */
void applier::serviceescalation::remove_object(
    configuration::serviceescalation const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing a service escalation.";
  config_logger->debug("Removing a service escalation.");

  // Find service escalation.
  std::string const& host_name{obj.hosts().front()};
  std::string const& description{obj.service_description().front()};
  /* Let's get a range of escalations for the concerned service */
  std::pair<serviceescalation_mmap::iterator, serviceescalation_mmap::iterator>
      range{engine::serviceescalation::serviceescalations.equal_range(
          {host_name, description})};
  bool service_exists;

  /* Let's get the service... */
  service_map::iterator sit{
      engine::service::services.find({host_name, description})};
  /* ... and its escalations */
  if (sit == engine::service::services.end()) {
    engine_logger(logging::dbg_config, logging::more)
        << "Cannot find service '" << host_name << "/" << description
        << "' - already removed.";
    config_logger->debug("Cannot find service '{}/{}' - already removed.",
                         host_name, description);
    service_exists = false;
  } else
    service_exists = true;

  size_t key = serviceescalation_key(obj);
  for (serviceescalation_mmap::iterator it{range.first}, end{range.second};
       it != end; ++it) {
    if (it->second->internal_key() == key) {
      // We have the serviceescalation to remove.

      // Notify event broker.
      timeval tv(get_broker_timestamp(nullptr));
      broker_adaptive_escalation_data(NEBTYPE_SERVICEESCALATION_DELETE,
                                      NEBFLAG_NONE, NEBATTR_NONE,
                                      it->second.get(), &tv);

      if (service_exists) {
        engine_logger(logging::dbg_config, logging::more)
            << "Service '" << host_name << "/" << description
            << "' found - removing escalation from it.";
        config_logger->debug(
            "Service '{}/{}' found - removing escalation from it.", host_name,
            description);
        std::list<escalation*>& srv_escalations(sit->second->get_escalations());
        /* We need also to remove the escalation from the service */
        for (std::list<engine::escalation*>::iterator
                 eit{srv_escalations.begin()},
             eend{srv_escalations.end()};
             eit != eend; ++eit) {
          if (*eit == it->second.get()) {
            srv_escalations.erase(eit);
            break;
          }
        }
      }

      // Remove escalation from the global configuration set.
      engine::serviceescalation::serviceescalations.erase(it);
      break;
    }
  }

  /* And we clear the configuration */
  config->serviceescalations().erase(obj);
}

/**
 *  Resolve a serviceescalation.
 *
 *  @param[in] obj  Serviceescalation object.
 */
void applier::serviceescalation::resolve_object(
    configuration::serviceescalation const& obj,
    error_cnt& err) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving a service escalation.";
  config_logger->debug("Resolving a service escalation.");

  // Find service escalation
  bool found{false};
  std::string const& hostname{*obj.hosts().begin()};
  std::string const& desc{obj.service_description().front()};
  auto p(engine::serviceescalation::serviceescalations.equal_range(
      {hostname, desc}));
  if (p.first == p.second)
    throw engine_error() << "Cannot find service escalations "
                         << "concerning host '" << hostname << "' and service '"
                         << desc << "'";
  size_t key = serviceescalation_key(obj);
  for (serviceescalation_mmap::iterator it = p.first; it != p.second; ++it) {
    if (it->second->internal_key() == key && it->second->matches(obj)) {
      found = true;
      // Resolve service escalation.
      it->second->resolve(err.config_warnings, err.config_errors);
      break;
    }
  }
  if (!found)
    throw engine_error() << "Cannot resolve non-existing service escalation";
}

/**
 *  Expand services.
 *
 *  @param[in]     hst      Hosts.
 *  @param[in]     hg       Host groups.
 *  @param[in]     svc      Service descriptions.
 *  @param[in]     sg       Service groups.
 *  @param[in,out] s        Configuration state.
 *  @param[out]    expanded Expanded services.
 */
void applier::serviceescalation::_expand_services(
    std::list<std::string> const& hst,
    std::list<std::string> const& hg,
    std::list<std::string> const& svc,
    std::list<std::string> const& sg,
    configuration::state& s,
    std::set<std::pair<std::string, std::string>>& expanded) {
  // Expanded hosts.
  std::set<std::string> all_hosts;

  // Base hosts.
  all_hosts.insert(hst.begin(), hst.end());

  // Host groups.
  for (std::list<std::string>::const_iterator it(hg.begin()), end(hg.end());
       it != end; ++it) {
    // Find host group.
    configuration::set_hostgroup::iterator it_group(s.hostgroups_find(*it));
    if (it_group == s.hostgroups().end())
      throw engine_error() << "Could not resolve host group '" << *it << "'";

    // Add host group members.
    all_hosts.insert(it_group->members().begin(), it_group->members().end());
  }

  // Hosts * services.
  for (std::set<std::string>::const_iterator it_host(all_hosts.begin()),
       end_host(all_hosts.end());
       it_host != end_host; ++it_host)
    for (std::list<std::string>::const_iterator it_service(svc.begin()),
         end_service(svc.end());
         it_service != end_service; ++it_service)
      expanded.insert({*it_host, *it_service});

  // Service groups.
  for (std::list<std::string>::const_iterator it(sg.begin()), end(sg.end());
       it != end; ++it) {
    // Find service group.
    configuration::set_servicegroup::iterator it_group(
        s.servicegroups_find(*it));
    if (it_group == s.servicegroups().end())
      throw engine_error() << "Could not resolve service group '" << *it << "'";

    // Add service group members.
    for (set_pair_string::const_iterator it_member(it_group->members().begin()),
         end_member(it_group->members().end());
         it_member != end_member; ++it_member)
      expanded.insert(*it_member);
  }
}

/**
 *  Inherits special variables from the service.
 *
 *  @param[in,out] obj Service escalation object.
 *  @param[in]     s   Configuration state.
 */
void applier::serviceescalation::_inherits_special_vars(
    configuration::serviceescalation& obj,
    configuration::state const& s) {
  // Detect if any special variables has not been defined.
  if (!obj.contactgroups_defined() || !obj.notification_interval_defined() ||
      !obj.escalation_period_defined()) {
    // Find service.
    configuration::set_service::const_iterator it{s.services_find(
        obj.hosts().front(), obj.service_description().front())};
    if (it == s.services().end())
      throw engine_error() << "Could not inherit special "
                           << "variables from service '"
                           << obj.service_description().front() << "' of host '"
                           << obj.hosts().front()
                           << "': service does not exist";

    // Inherits variables.
    if (!obj.contactgroups_defined())
      obj.contactgroups() = it->contactgroups();
    if (!obj.notification_interval_defined())
      obj.notification_interval(it->notification_interval());
    if (!obj.escalation_period_defined())
      obj.escalation_period(it->notification_period());
  }
}
