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
 * @brief Decide whether a contact must be notified.
 *
 * @p in_period is resolved by the caller because it cannot be computed here:
 * it needs the runtime timeperiod and the contact's timezone, which Engine and
 * Broker hold in different forms. The notification history, on the contrary, is
 * a shared type both sides read from the same notification_manager, so it is
 * passed as is and interpreted below.
 *
 * @param c The contact snapshot.
 * @param is_host true when the notified resource is a host (service_id == 0).
 * @param cat The notification category.
 * @param type The notification reason (used to pick the flapping flag).
 * @param current_state The resource's current state as an int (host_state
 *        up/down/unreachable or service_state ok/warning/critical/unknown),
 *        used for a normal notification.
 * @param in_period Whether the contact's notification period is currently open
 *        (an empty or unknown period must be passed as true).
 * @param ongoing_problem The resource's last normal notification, as kept
 *        by the manager, or nullptr when it never notified anybody. Only
 *        consulted for a recovery, to restrict it to the contacts that heard
 *        about the incident.
 *
 * @return true when the contact must be notified.
 */
bool should_notify_contact(const contact& c,
                           bool is_host,
                           notification_category cat,
                           reason_type type,
                           int current_state,
                           bool in_period,
                           const notification* ongoing_problem) {
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
      if (!ongoing_problem || !ongoing_problem->sent_to(c.name)) {
        SPDLOG_LOGGER_INFO(notifications_logger(),
                           "contact '{}' shouldn't be notified about a {} "
                           "recovery: not notified about the incident",
                           c.name, is_host ? "host" : "service");
        return false;
      }
      return true;
    case cat_flapping: {
      uint32_t flag = 0;
      switch (type) {
        case reason_flappingstart:
          flag = flappingstart;
          break;
        case reason_flappingstop:
          flag = flappingstop;
          break;
        case reason_flappingdisabled:
          flag = flappingdisabled;
          break;
        default:
          /* This case should not appear */
          SPDLOG_LOGGER_INFO(notifications_logger(),
                             "contact '{}' shouldn't be notified about "
                             "flapping events: unknown reason type {}",
                             c.name, static_cast<int>(type));
          return false;
      }

      if ((mask & flag) == 0) {
        SPDLOG_LOGGER_INFO(
            notifications_logger(),
            "contact '{}' shouldn't be notified about flapping events", c.name);
        return false;
      }
      return true;
    }
    case cat_downtime:
      if ((mask & downtime) == 0) {
        SPDLOG_LOGGER_INFO(
            notifications_logger(),
            "contact '{}' shouldn't be notified about downtime events", c.name);
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
