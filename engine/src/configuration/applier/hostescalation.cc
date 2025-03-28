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

#include "com/centreon/engine/configuration/applier/hostescalation.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine::configuration;

/**
 *  Add new host escalation.
 *
 *  @param[in] obj  The new host escalation to add into the monitoring
 *                  engine.
 */
void applier::hostescalation::add_object(
    const configuration::Hostescalation& obj) {
  // Check host escalation.
  if (obj.hosts().data().size() != 1 || !obj.hostgroups().data().empty())
    throw engine_error()
        << "Could not create host escalation with multiple hosts / host groups";

  // Logging.
  config_logger->debug("Creating new escalation for host '{}'.",
                       obj.hosts().data(0));

  // Add escalation to the global configuration set.
  auto* new_obj = pb_indexed_config.mut_state().add_hostescalations();
  new_obj->CopyFrom(obj);

  size_t key = hostescalation_key(obj);

  // Create host escalation.
  auto he = std::make_shared<engine::hostescalation>(
      obj.hosts().data(0), obj.first_notification(), obj.last_notification(),
      obj.notification_interval(), obj.escalation_period(),
      ((obj.escalation_options() & action_he_down) ? notifier::down
                                                   : notifier::none) |
          ((obj.escalation_options() & action_he_unreachable)
               ? notifier::unreachable
               : notifier::none) |
          ((obj.escalation_options() & action_he_recovery) ? notifier::up
                                                           : notifier::none),
      key);

  // Add new items to the configuration state.
  engine::hostescalation::hostescalations.insert({he->get_hostname(), he});

  // Add contact groups to host escalation.
  for (auto& g : obj.contactgroups().data())
    he->get_contactgroups().insert({g, nullptr});
}

/**
 *  @brief Modify host escalation.
 *
 *  Host escalations cannot be defined with anything else than their
 *  full content. Therefore no modification can occur.
 *
 *  @param[in] obj  Unused.
 */
void applier::hostescalation::modify_object(
    configuration::Hostescalation* old_obj [[maybe_unused]],
    const configuration::Hostescalation& new_obj [[maybe_unused]]) {
  throw engine_error()
      << "Could not modify a host escalation: host escalation objects can only "
         "be added or removed, this is likely a software bug that you should "
         "report to Centreon Engine developers";
}

/**
 *  Remove old hostescalation.
 *
 *  @param[in] obj  The new hostescalation to remove from the monitoring
 *                  engine.
 */
template <>
void applier::hostescalation::remove_object<size_t>(
    const std::pair<ssize_t, size_t>& p) {
  configuration::Hostescalation obj =
      pb_indexed_config.state().hostescalations(p.first);
  // Logging.
  config_logger->debug("Removing a host escalation.");

  // Find host escalation.
  const std::string& host_name{obj.hosts().data(0)};
  std::pair<hostescalation_mmap::iterator, hostescalation_mmap::iterator> range{
      engine::hostescalation::hostescalations.equal_range(host_name)};
  bool host_exists;

  /* Let's get the host... */
  host_map::iterator hit{engine::host::hosts.find(host_name)};
  /* ... and its escalations */
  if (hit == engine::host::hosts.end()) {
    config_logger->debug("Cannot find host '{}' - already removed.", host_name);
    host_exists = false;
  } else
    host_exists = true;

  for (hostescalation_mmap::iterator it{range.first}, end{range.second};
       it != end; ++it) {
    /* It's a pity but for now we don't have any possibility or key to verify
     * if the hostescalation is the good one. */
    if (it->second->get_first_notification() == obj.first_notification() &&
        it->second->get_last_notification() == obj.last_notification() &&
        it->second->get_notification_interval() ==
            obj.notification_interval() &&
        it->second->get_escalation_period() == obj.escalation_period() &&
        it->second->get_escalate_on(notifier::down) ==
            static_cast<bool>(obj.escalation_options() & action_he_down) &&
        it->second->get_escalate_on(notifier::unreachable) ==
            static_cast<bool>(obj.escalation_options() &
                              action_he_unreachable) &&
        it->second->get_escalate_on(notifier::up) ==
            static_cast<bool>(obj.escalation_options() & action_he_recovery)) {
      // We have the hostescalation to remove.

      if (host_exists) {
        config_logger->debug("Host '{}' found - removing escalation from it.",
                             host_name);
        std::list<escalation*>& escalations(hit->second->get_escalations());
        /* We need also to remove the escalation from the host */
        for (std::list<engine::escalation*>::iterator heit{escalations.begin()},
             heend{escalations.end()};
             heit != heend; ++heit) {
          if (*heit == it->second.get()) {
            escalations.erase(heit);
            break;
          }
        }
      }
      // Remove host escalation from the global configuration set.
      engine::hostescalation::hostescalations.erase(it);
      break;
    }
  }

  /* And we clear the configuration */
  pb_indexed_config.mut_state().mutable_hostescalations()->DeleteSubrange(
      p.first, 1);
}

/**
 *  Resolve a hostescalation.
 *
 *  @param[in] obj  Hostescalation object.
 */
void applier::hostescalation::resolve_object(
    const configuration::Hostescalation& obj,
    error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving a host escalation.");

  // Find host escalation
  bool found = false;
  const std::string& hostname{obj.hosts().data(0)};
  auto p = engine::hostescalation::hostescalations.equal_range(hostname);

  if (p.first == p.second)
    throw engine_error() << "Cannot find host escalations concerning host '"
                         << hostname << "'";

  size_t key = hostescalation_key(obj);
  for (hostescalation_mmap::iterator it{p.first}; it != p.second; ++it) {
    /* It's a pity but for now we don't have any idea or key to verify if
     * the hostescalation is the good one. */
    if (it->second->internal_key() == key) {
      found = true;
      // Resolve host escalation.
      it->second->resolve(err.config_warnings, err.config_errors);
      break;
    }
  }
  if (!found)
    throw engine_error() << "Cannot resolve non-existing host escalation";
}
