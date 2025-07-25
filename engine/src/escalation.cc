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
#include "com/centreon/engine/escalation.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/timeperiod.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

escalation::escalation(uint32_t first_notification,
                       uint32_t last_notification,
                       double notification_interval,
                       std::string const& escalation_period,
                       uint32_t escalate_on,
                       const size_t key)
    : _first_notification{first_notification},
      _last_notification{last_notification},
      _notification_interval{
          (notification_interval < 0) ? 0 : notification_interval},
      _escalation_period{escalation_period},
      _escalate_on{escalate_on},
      _internal_key{key},
      notifier_ptr{nullptr},
      escalation_period_ptr{nullptr} {}

std::string const& escalation::get_escalation_period() const {
  return _escalation_period;
}

uint32_t escalation::get_first_notification() const {
  return _first_notification;
}

uint32_t escalation::get_last_notification() const {
  return _last_notification;
}

double escalation::get_notification_interval() const {
  return _notification_interval;
}

void escalation::set_notification_interval(double notification_interval) {
  _notification_interval = notification_interval;
}

void escalation::add_escalate_on(notifier::notification_flag type) {
  _escalate_on |= type;
}

void escalation::remove_escalate_on(notifier::notification_flag type) {
  _escalate_on &= ~type;
}

uint32_t escalation::get_escalate_on() const {
  return _escalate_on;
}

void escalation::set_escalate_on(uint32_t escalate_on) {
  _escalate_on = escalate_on;
}

bool escalation::get_escalate_on(notifier::notification_flag type) const {
  return _escalate_on & type;
}

const contactgroup_map& escalation::get_contactgroups() const {
  return _contact_groups;
}

contactgroup_map& escalation::get_contactgroups() {
  return _contact_groups;
}

/**
 *  This method is called by a notifier to know if this escalation is touched
 *  by the notification to send.
 *
 * @param state The current notifier state.
 * @param notification_number The current notifier notification number.
 *
 * @return A boolean.
 */
bool escalation::is_viable(int state __attribute__((unused)),
                           uint32_t notification_number) const {
  std::time_t current_time;
  std::time(&current_time);

  /* Skip this escalation if current_time is outside its timeperiod */
  if (!get_escalation_period().empty() &&
      !check_time_against_period(current_time, escalation_period_ptr))
    return false;

  if (notification_number < _first_notification ||
      (notification_number > _last_notification && _last_notification != 0))
    return false;
  return true;
}

void escalation::resolve(uint32_t& w [[maybe_unused]], uint32_t& e) {
  uint32_t errors = 0;
  // Find the timeperiod.
  if (!get_escalation_period().empty()) {
    timeperiod_map::const_iterator it{
        timeperiod::timeperiods.find(get_escalation_period())};

    if (it == timeperiod::timeperiods.end() || !it->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Escalation period '" << get_escalation_period()
          << "' specified in escalation is not defined anywhere!";
      config_logger->error(
          "Error: Escalation period '{}' specified in escalation is not "
          "defined anywhere!",
          get_escalation_period());
      errors++;
    } else
      // Save the timeperiod pointer for later.
      escalation_period_ptr = it->second.get();
  }

  // Check all contact groups.
  for (contactgroup_map::iterator it = _contact_groups.begin(),
       end = _contact_groups.end();
       it != end; ++it) {
    // Find the contact group.
    contactgroup_map::iterator it_cg{
        contactgroup::contactgroups.find(it->first)};

    if (it_cg == contactgroup::contactgroups.end() || !it_cg->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Contact group '" << it->first
          << "' specified in escalation for this notifier is not defined "
             "anywhere!";
      config_logger->error(
          "Error: Contact group '{}' specified in escalation for this notifier "
          "is not defined "
          "anywhere!",
          it->first);
      errors++;
    } else {
      // Save the contactgroup pointer for later.
      it->second = it_cg->second;
    }
  }

  if (errors) {
    e += errors;
    throw engine_error() << "Cannot resolve notifier escalation";
  }
}

size_t escalation::internal_key() const {
  return _internal_key;
}
