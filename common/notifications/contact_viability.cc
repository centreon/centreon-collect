/**
 * Copyright 2026 Centreon
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

#include "common/notifications/contact_viability.hh"

#include "common/log_v2/log_v2.hh"

namespace com::centreon::common::notifications {

namespace {
/* Resource state (as an int) to notification_flag. Indexed by host_state
 * (up/down/unreachable) and service_state (ok/warning/critical/unknown), which
 * matches Engine's get_current_state_int(). */
constexpr std::array<uint32_t, 3> host_state_flag{up, down, unreachable};
constexpr std::array<uint32_t, 4> service_state_flag{ok, warning, critical,
                                                     unknown};

/* The notification library logs through common/log_v2, not through host-app
 * globals — one less dependency on Engine/Broker (mirrors notification_manager
 * .cc). */
std::shared_ptr<spdlog::logger> notifications_logger() {
  return com::centreon::common::log_v2::log_v2::instance().get(
      com::centreon::common::log_v2::log_v2::NOTIFICATIONS);
}
}  // namespace

/**
 * @brief Decide whether a contact must be notified, applying the historical
 * Engine per-contact viability on a pure value snapshot (shared by Engine's
 * notifier::get_contacts_to_notify and Broker's deliver()).
 *
 * The two environment-dependent inputs are resolved by the caller so the
 * function stays free of any timeperiod / notification-history dependency:
 * @p in_period is the contact's notification period evaluated in the contact's
 * own timezone (an empty or unknown period must be passed as true), and
 * @p already_notified tells whether the contact was among those told about the
 * ongoing problem (only consulted for a recovery).
 *
 * @param c The contact snapshot.
 * @param is_host true when the notified resource is a host (service_id == 0).
 * @param cat The notification category.
 * @param type The notification reason (used to pick the flapping flag).
 * @param current_state The resource's current state as an int (host_state
 *        up/down/unreachable or service_state ok/warning/critical/unknown),
 *        used for a normal notification.
 * @param in_period Whether the contact's notification period is currently open.
 * @param already_notified Whether the contact was notified of the problem
 *        (recovery routing).
 *
 * @return true when the contact must be notified.
 */
bool should_notify_contact(const contact& c,
                           bool is_host,
                           notification_category cat,
                           reason_type type,
                           int current_state,
                           bool in_period,
                           bool already_notified) {
  /* 1. The contact's resource notification-enable flag. */
  if (is_host ? !c.host_notifications_enabled
              : !c.service_notifications_enabled) {
    SPDLOG_LOGGER_INFO(notifications_logger(),
                       "contact '{}' shouldn't be notified from {}s", c.name,
                       is_host ? "host" : "service");
    return false;
  }

  /* 2. The contact's own notification period (resolved by the caller; an empty
   * or unknown period is passed as true, matching Engine's "no period =>
   * always in"). */
  if (!in_period) {
    SPDLOG_LOGGER_INFO(notifications_logger(),
                       "contact '{}' shouldn't be notified at this time",
                       c.name);
    return false;
  }

  /* 3. Category dispatch on the relevant bitmask */
  const uint32_t mask =
      is_host ? c.host_notification_options : c.service_notification_options;
  switch (cat) {
    case cat_normal: {
      uint32_t flag = 0;
      if (is_host) {
        if (current_state >= 0 &&
            current_state < static_cast<int>(host_state_flag.size()))
          flag = host_state_flag[current_state];
      } else if (current_state >= 0 &&
                 current_state < static_cast<int>(service_state_flag.size())) {
        flag = service_state_flag[current_state];
      }
      if ((mask & flag) == 0) {
        SPDLOG_LOGGER_INFO(notifications_logger(),
                           "contact '{}' shouldn't be notified about state {}: "
                           "not configured for this contact",
                           c.name, current_state);
        return false;
      }
      return true;
    }
    case cat_recovery:
      /* Engine checks the ok and up bits against the resource's own mask (only
       * one of them can be set for a given type), then requires the contact to
       * have been notified of the incident. */
      if ((mask & (ok | up)) == 0) {
        SPDLOG_LOGGER_INFO(notifications_logger(),
                           "contact '{}' shouldn't be notified about a {} "
                           "recovery",
                           c.name, is_host ? "host" : "service");
        return false;
      }
      if (!already_notified) {
        SPDLOG_LOGGER_INFO(notifications_logger(),
                           "contact '{}' shouldn't be notified about a {} "
                           "recovery: not notified about the incident",
                           c.name, is_host ? "host" : "service");
        return false;
      }
      return true;
    case cat_flapping: {
      uint32_t flag = 0;
      if (type == reason_flappingstart)
        flag = flappingstart;
      else if (type == reason_flappingstop)
        flag = flappingstop;
      else if (type == reason_flappingdisabled)
        flag = flappingdisabled;
      if ((mask & flag) == 0) {
        SPDLOG_LOGGER_INFO(notifications_logger(),
                           "contact '{}' shouldn't be notified about flapping "
                           "events",
                           c.name);
        return false;
      }
      return true;
    }
    case cat_downtime:
      if ((mask & downtime) == 0) {
        SPDLOG_LOGGER_INFO(notifications_logger(),
                           "contact '{}' shouldn't be notified about downtime "
                           "events",
                           c.name);
        return false;
      }
      return true;
    case cat_acknowledgement:
    case cat_custom:
      return true;
  }
  return false;
}

}  // namespace com::centreon::common::notifications
