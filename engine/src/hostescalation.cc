/**
 * Copyright 2011-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */
#include "com/centreon/engine/hostescalation.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"
#include "common/engine_conf/state.pb.h"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

hostescalation_mmap hostescalation::hostescalations;

/**
 *  Create a new host escalation.
 *
 *  @param[in] host_name               Host name.
 *  @param[in] first_notification      First notification.
 *  @param[in] last_notification       Last notification.
 *  @param[in] notification_interval   Notification interval.
 *  @param[in] escalation_period       Escalation timeperiod name.
 *  @param[in] escalate_on_down        Escalate on down ?
 *  @param[in] escalate_on_unreachable Escalate on unreachable ?
 *  @param[in] escalate_on_recovery    Escalate on recovery ?
 */
hostescalation::hostescalation(std::string const& host_name,
                               uint32_t first_notification,
                               uint32_t last_notification,
                               double notification_interval,
                               std::string const& escalation_period,
                               uint32_t escalate_on,
                               const size_t key)
    : escalation{first_notification, last_notification, notification_interval,
                 escalation_period,  escalate_on,       key},
      _hostname{host_name} {
  if (host_name.empty())
    throw engine_error() << "Could not create escalation "
                         << "on host '" << host_name << "'";
}

std::string const& hostescalation::get_hostname() const {
  return _hostname;
}

/**
 *  This method is called by a notifier to know if this escalation is touched
 *  by the notification to send.
 *
 * @param state The notifier state.
 * @param notification_number The current notifier notification number.
 *
 * @return A boolean.
 */
bool hostescalation::is_viable(int state, uint32_t notification_number) const {
  engine_logger(dbg_functions, basic) << "serviceescalation::is_viable()";
  functions_logger->trace("serviceescalation::is_viable()");

  bool retval{escalation::is_viable(state, notification_number)};
  if (retval) {
    std::array<notifier::notification_flag, 3> nt = {
        notifier::up,
        notifier::down,
        notifier::unreachable,
    };

    if (!get_escalate_on(nt[state]))
      return false;
    return true;
  } else
    return retval;
}

void hostescalation::resolve(uint32_t& w [[maybe_unused]], uint32_t& e) {
  uint32_t errors = 0;

  // Find the host.
  host_map::const_iterator found(host::hosts.find(this->get_hostname()));
  if (found == host::hosts.end() || !found->second) {
    engine_logger(log_verification_error, basic)
        << "Error: Host '" << this->get_hostname()
        << "' specified in host escalation is not defined anywhere!";
    config_logger->error(
        "Error: Host '{}' specified in host escalation is not defined "
        "anywhere!",
        this->get_hostname());
    errors++;
    notifier_ptr = nullptr;
  } else {
    notifier_ptr = found->second.get();
    notifier_ptr->get_escalations().push_back(this);
  }

  try {
    escalation::resolve(w, errors);
  } catch (std::exception const& ee) {
    engine_logger(log_verification_error, basic)
        << "Error: Notifier escalation error: " << ee.what();
    config_logger->error("Error: Notifier escalation error: {}", ee.what());
  }

  // Add errors.
  if (errors) {
    e += errors;
    throw engine_error() << "Cannot resolve host escalation";
  }
}

/**
 * @brief Checks that this hostescalation corresponds to the Configuration
 * object obj. This function doesn't check contactgroups as it is usually used
 * to modify them.
 *
 * @param obj A host escalation configuration object.
 *
 * @return A boolean that is True if they match.
 */
bool hostescalation::matches(const configuration::Hostescalation& obj) const {
  uint32_t escalate_on =
      ((obj.escalation_options() & configuration::action_he_down)
           ? notifier::down
           : notifier::none) |
      ((obj.escalation_options() & configuration::action_he_unreachable)
           ? notifier::unreachable
           : notifier::none) |
      ((obj.escalation_options() & configuration::action_he_recovery)
           ? notifier::up
           : notifier::none);
  if (_hostname != *obj.hosts().data().begin() ||
      get_first_notification() != obj.first_notification() ||
      get_last_notification() != obj.last_notification() ||
      get_notification_interval() != obj.notification_interval() ||
      get_escalation_period() != obj.escalation_period() ||
      get_escalate_on() != escalate_on)
    return false;

  return true;
}
