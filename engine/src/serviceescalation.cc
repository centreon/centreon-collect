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
#include "com/centreon/engine/serviceescalation.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine;

serviceescalation_mmap serviceescalation::serviceescalations;

serviceescalation::serviceescalation(std::string const& hostname,
                                     std::string const& description,
                                     uint32_t first_notification,
                                     uint32_t last_notification,
                                     double notification_interval,
                                     std::string const& escalation_period,
                                     uint32_t escalate_on,
                                     const size_t key)
    : escalation{first_notification, last_notification, notification_interval,
                 escalation_period,  escalate_on,       key},
      _hostname{hostname},
      _description{description} {
  if (hostname.empty())
    throw engine_error() << "Could not create escalation "
                         << "on a host without name";
  if (description.empty())
    throw engine_error() << "Could not create escalation "
                         << "on a service without description";
}

std::string const& serviceescalation::get_hostname() const {
  return _hostname;
}

std::string const& serviceescalation::get_description() const {
  return _description;
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
bool serviceescalation::is_viable(int state,
                                  uint32_t notification_number) const {
  engine_logger(dbg_functions, basic) << "serviceescalation::is_viable()";
  functions_logger->trace("serviceescalation::is_viable()");

  bool retval{escalation::is_viable(state, notification_number)};
  if (retval) {
    std::array<notifier::notification_flag, 4> nt = {
        notifier::ok,
        notifier::warning,
        notifier::critical,
        notifier::unknown,
    };

    if (!get_escalate_on(nt[state]))
      return false;
    return true;
  } else
    return retval;
}

void serviceescalation::resolve(uint32_t& w [[maybe_unused]], uint32_t& e) {
  uint32_t errors = 0;

  // Find the service.
  service_map::const_iterator found{
      service::services.find({get_hostname(), get_description()})};
  if (found == service::services.end() || !found->second) {
    engine_logger(log_verification_error, basic)
        << "Error: Service '" << get_description() << "' on host '"
        << get_hostname()
        << "' specified in service escalation is not defined anywhere!";
    config_logger->error(
        "Error: Service '{}' on host '{}' specified in service escalation is "
        "not defined anywhere!",
        get_description(), get_hostname());
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
    throw engine_error() << "Cannot resolve service escalation";
  }
}

/**
 * @brief Checks that this serviceescalation corresponds to the Configuration
 * object obj. This function doesn't check contactgroups as it is usually used
 * to modify them.
 *
 * @param obj A service escalation configuration object.
 *
 * @return A boolean that is True if they match.
 */
bool serviceescalation::matches(
    const configuration::Serviceescalation& obj) const {
  uint32_t escalate_on =
      ((obj.escalation_options() & configuration::action_se_warning)
           ? notifier::warning
           : notifier::none) |
      ((obj.escalation_options() & configuration::action_se_unknown)
           ? notifier::unknown
           : notifier::none) |
      ((obj.escalation_options() & configuration::action_se_critical)
           ? notifier::critical
           : notifier::none) |
      ((obj.escalation_options() & configuration::action_se_recovery)
           ? notifier::ok
           : notifier::none);
  if (_hostname != obj.hosts().data(0) ||
      _description != obj.service_description().data(0) ||
      get_first_notification() != obj.first_notification() ||
      get_last_notification() != obj.last_notification() ||
      get_notification_interval() != obj.notification_interval() ||
      get_escalation_period() != obj.escalation_period() ||
      get_escalate_on() != escalate_on)
    return false;

  return true;
}
