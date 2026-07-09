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

using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine;
namespace notifications = com::centreon::common::notifications;

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
  functions_logger->trace("serviceescalation::is_viable()");

  bool retval{escalation::is_viable(state, notification_number)};
  if (retval) {
    std::array<notifications::notification_flag, 4> nt = {
        notifications::ok,
        notifications::warning,
        notifications::critical,
        notifications::unknown,
    };

    if (!get_escalate_on(nt[state]))
      return false;
    return true;
  } else
    return retval;
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
           ? notifications::warning
           : notifications::none) |
      ((obj.escalation_options() & configuration::action_se_unknown)
           ? notifications::unknown
           : notifications::none) |
      ((obj.escalation_options() & configuration::action_se_critical)
           ? notifications::critical
           : notifications::none) |
      ((obj.escalation_options() & configuration::action_se_recovery)
           ? notifications::ok
           : notifications::none);
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
