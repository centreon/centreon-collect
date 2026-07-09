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

using namespace com::centreon::engine;
using namespace com::centreon::common::timeperiods;
namespace notifications = com::centreon::common::notifications;

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

void escalation::add_escalate_on(notifications::notification_flag type) {
  _escalate_on |= type;
}

void escalation::remove_escalate_on(notifications::notification_flag type) {
  _escalate_on &= ~type;
}

uint32_t escalation::get_escalate_on() const {
  return _escalate_on;
}

void escalation::set_escalate_on(uint32_t escalate_on) {
  _escalate_on = escalate_on;
}

bool escalation::get_escalate_on(notifications::notification_flag type) const {
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
  if (!get_escalation_period().empty() && escalation_period_ptr &&
      !escalation_period_ptr->check_time_against_period(current_time))
    return false;

  if (notification_number < _first_notification ||
      (notification_number > _last_notification && _last_notification != 0))
    return false;
  return true;
}

size_t escalation::internal_key() const {
  return _internal_key;
}
