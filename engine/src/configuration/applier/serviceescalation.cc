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

using namespace com::centreon::engine::configuration;

/**
 *  Add new service escalation.
 *
 *  @param[in] obj  The new service escalation to add into the
 *                  monitoring engine.
 */
void applier::serviceescalation::add_object(
    const configuration::Serviceescalation& obj) {
  // Check service escalation.
  if (obj.hosts().data().size() != 1 || !obj.hostgroups().data().empty() ||
      obj.service_description().data().size() != 1 ||
      !obj.servicegroups().data().empty()) {
    throw engine_error() << "Could not create service escalation with multiple "
                            "hosts / host groups / services / service groups: "
                         << obj.DebugString();
  }

  // Logging.
  config_logger->debug("Creating new escalation for service '{}' of host '{}'",
                       obj.service_description().data()[0],
                       obj.hosts().data()[0]);

  // Add escalation to the global configuration set.
  auto* se_cfg = pb_indexed_config.state().add_serviceescalations();
  se_cfg->CopyFrom(obj);

  size_t key = configuration::serviceescalation_key(obj);

  // Create service escalation.
  auto se = std::make_shared<engine::serviceescalation>(
      obj.hosts().data()[0], obj.service_description().data()[0],
      obj.first_notification(), obj.last_notification(),
      obj.notification_interval(), obj.escalation_period(),
      ((obj.escalation_options() & action_se_warning) ? notifier::warning
                                                      : notifier::none) |
          ((obj.escalation_options() & action_se_unknown) ? notifier::unknown
                                                          : notifier::none) |
          ((obj.escalation_options() & action_se_critical) ? notifier::critical
                                                           : notifier::none) |
          ((obj.escalation_options() & action_se_recovery) ? notifier::ok
                                                           : notifier::none),
      key);

  // Add new items to the global list.
  engine::serviceescalation::serviceescalations.insert(
      {{se->get_hostname(), se->get_description()}, se});

  // Add contact groups to service escalation.
  for (auto& cg : obj.contactgroups().data()) {
    se->get_contactgroups().insert({cg, nullptr});
  }
}

/**
 *  Expand all service escalations.
 *
 *  @param[in,out] s  Configuration being applied.
 */
void applier::serviceescalation::expand_objects(configuration::State& s) {
  std::list<std::unique_ptr<Serviceescalation>> resolved;
  // Browse all escalations.
  config_logger->debug("Expanding service escalations");

  for (auto& se : *s.mutable_serviceescalations()) {
    /* A set of all the hosts related to this escalation */
    absl::flat_hash_set<std::string> host_names;
    for (auto& hname : se.hosts().data())
      host_names.insert(hname);
    if (se.hostgroups().data().size() > 0) {
      for (auto& hg_name : se.hostgroups().data()) {
        auto found_hg =
            std::find_if(s.hostgroups().begin(), s.hostgroups().end(),
                         [&hg_name](const Hostgroup& hg) {
                           return hg.hostgroup_name() == hg_name;
                         });
        if (found_hg != s.hostgroups().end()) {
          for (auto& h : found_hg->members().data())
            host_names.emplace(h);
        } else
          throw engine_error() << fmt::format(
              "Could not expand non-existing host group '{}'", hg_name);
      }
    }

    /* A set of all the pairs (hostname, service-description) impacted by this
     * escalation. */
    absl::flat_hash_set<std::pair<std::string, std::string>> expanded;
    for (auto& hn : host_names) {
      for (auto& sn : se.service_description().data())
        expanded.emplace(hn, sn);
    }

    for (auto& sg_name : se.servicegroups().data()) {
      auto found =
          std::find_if(s.servicegroups().begin(), s.servicegroups().end(),
                       [&sg_name](const Servicegroup& sg) {
                         return sg.servicegroup_name() == sg_name;
                       });
      if (found == s.servicegroups().end())
        throw engine_error()
            << fmt::format("Could not resolve service group '{}'", sg_name);

      for (auto& m : found->members().data())
        expanded.emplace(m.first(), m.second());
    }
    se.mutable_hostgroups()->clear_data();
    se.mutable_hosts()->clear_data();
    se.mutable_servicegroups()->clear_data();
    se.mutable_service_description()->clear_data();
    for (auto& p : expanded) {
      resolved.emplace_back(std::make_unique<Serviceescalation>());
      auto& e = resolved.back();
      e->CopyFrom(se);
      fill_string_group(e->mutable_hosts(), p.first);
      fill_string_group(e->mutable_service_description(), p.second);
    }
  }
  s.clear_serviceescalations();
  for (auto& e : resolved)
    s.mutable_serviceescalations()->AddAllocated(e.release());
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
    configuration::Serviceescalation* old_obj [[maybe_unused]],
    const configuration::Serviceescalation& new_obj [[maybe_unused]]) {
  throw engine_error()
      << "Could not modify a service escalation: service escalation objects "
         "can only be added or removed, this is likely a software bug that you "
         "should report to Centreon Engine developers";
}

/**
 *  Remove old service escalation.
 *
 *  @param[in] obj  The service escalation to remove from the monitoring
 *                  engine.
 */
void applier::serviceescalation::remove_object(ssize_t idx) {
  // Logging.
  config_logger->debug("Removing a service escalation.");

  configuration::Serviceescalation& obj =
      pb_indexed_config.state().mutable_serviceescalations()->at(idx);
  // Find service escalation.
  const std::string& host_name{obj.hosts().data()[0]};
  const std::string& description{obj.service_description().data()[0]};
  /* Let's get a range of escalations for the concerned service */
  auto range{engine::serviceescalation::serviceescalations.equal_range(
      {host_name, description})};
  bool service_exists;

  /* Let's get the service... */
  service_map::iterator sit{
      engine::service::services.find({host_name, description})};
  /* ... and its escalations */
  if (sit == engine::service::services.end()) {
    config_logger->debug("Cannot find service '{}/{}' - already removed.",
                         host_name, description);
    service_exists = false;
  } else
    service_exists = true;

  size_t key = serviceescalation_key(obj);
  for (serviceescalation_mmap::iterator it = range.first, end = range.second;
       it != end; ++it) {
    if (it->second->internal_key() == key) {
      // We have the serviceescalation to remove.

      if (service_exists) {
        config_logger->debug(
            "Service '{}/{}' found - removing escalation from it.", host_name,
            description);
        std::list<escalation*>& srv_escalations =
            sit->second->get_escalations();
        /* We need also to remove the escalation from the service */
        srv_escalations.remove_if(
            [my_escal = it->second.get()](const escalation* e) {
              return e == my_escal;
            });
      }

      // Remove escalation from the global configuration set.
      engine::serviceescalation::serviceescalations.erase(it);
      break;
    }
  }

  /* And we clear the configuration */
  pb_indexed_config.state().mutable_serviceescalations()->DeleteSubrange(idx,
                                                                         1);
}

/**
 *  Resolve a serviceescalation.
 *
 *  @param[in] obj  Serviceescalation object.
 */
void applier::serviceescalation::resolve_object(
    const configuration::Serviceescalation& obj,
    error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving a service escalation.");

  // Find service escalation
  bool found = false;
  const std::string& hostname{obj.hosts().data(0)};
  const std::string& desc{obj.service_description().data(0)};
  auto p = engine::serviceescalation::serviceescalations.equal_range(
      {hostname, desc});
  if (p.first == p.second)
    throw engine_error() << "Cannot find service escalations concerning host '"
                         << hostname << "' and service '" << desc << "'";
  size_t key = configuration::serviceescalation_key(obj);
  for (serviceescalation_mmap::iterator it = p.first; it != p.second; ++it) {
    if (it->second->internal_key() == key) {
      found = true;
      // Resolve service escalation.
      it->second->resolve(err.config_warnings, err.config_errors);
      break;
    }
  }
  if (!found)
    throw engine_error() << "Cannot resolve non-existing service escalation";
}
