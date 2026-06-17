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

#ifndef CCE_NOTIFICATIONS_NOTIFICATION_TYPES_HH
#define CCE_NOTIFICATIONS_NOTIFICATION_TYPES_HH

#include <cstdint>
#include <ctime>
#include <ostream>
#include <string>
#include <string_view>

#include "absl/container/btree_set.h"

namespace com::centreon::engine::notifications {

/* Status attributes. Used as argument in the notifier::update_status(). */
enum status_attribute {
  STATUS_NONE = 0,
  STATUS_DOWNTIME_DEPTH = 1 << 0,
  STATUS_NOTIFICATION_NUMBER = 1 << 1,
  STATUS_ACKNOWLEDGEMENT = 1 << 2,
  STATUS_ALL = ~0u,
};

enum notification_category {
  cat_normal,
  cat_recovery,
  cat_acknowledgement,
  cat_flapping,
  cat_downtime,
  cat_custom,
};

enum notification_flag {
  none = 0,
  // Host
  up = 1 << 0,
  down = 1 << 1,
  unreachable = 1 << 2,
  // Service
  ok = 1 << 3,
  warning = 1 << 4,
  critical = 1 << 5,
  unknown = 1 << 6,
  // Flapping
  flappingstart = 1 << 7,
  flappingstop = 1 << 8,
  flappingdisabled = 1 << 9,

  // Downtime
  downtime = 1 << 10,
};

enum notifier_type {
  host_notification,
  service_notification,
};

enum reason_type {
  reason_normal,
  reason_recovery,
  reason_acknowledgement,
  reason_flappingstart,
  reason_flappingstop,
  reason_flappingdisabled,
  reason_downtimestart,
  reason_downtimeend,
  reason_downtimecancelled,
  reason_custom = 99,
};

enum notification_option {
  notification_option_none = 0,
  notification_option_broadcast = 1,
  notification_option_forced = 2,
  notification_option_increment = 4,
};

/**
 * @brief Global notification configuration, as a value snapshot.
 *
 * Mirrors the few program-wide settings the viability logic needs, decoupled
 * from engine's pb_indexed_config.
 */
struct global_config {
  bool enabled = false;
  uint32_t interval_length = 0;
  bool send_recovery_notifications_anyway = false;
};

/**
 * @brief Per-resource state snapshot, as seen by the notification engine.
 *
 * One backend call fills this; the viability logic then reasons on a pure
 * value, with no engine object dependency.
 */
struct resource_state {
  bool notifications_enabled = false;
  bool in_downtime = false;
  bool flapping = false;
  bool is_volatile = false;
  bool hard_state = false;
  bool acknowledged = false;
  bool notify_on_current_state = false;
  bool authorized_by_dependencies = false;
  bool in_notification_period = false;
  int current_state = 0;
  int scheduled_downtime_depth = 0;
  std::time_t last_hard_state_change = 0;
  std::string_view current_state_as_string;
  uint32_t notify_on = 0;  // bitmask of notification_flag
  uint32_t notification_interval = 0;
  uint32_t first_notification_delay = 0;
  uint32_t recovery_notification_delay = 0;
};

/**
 * @brief Result of delivering a notification: who was actually notified plus
 * the escalation-adjusted parameters computed during contact selection.
 */
struct delivery_result {
  absl::btree_set<std::string> notified_contacts;
  uint32_t notification_interval = 0;
  bool escalated = false;
};

/**
 * @brief The last notification emitted for a (resource, category): the runtime
 * state the manager keeps to drive re-notification timing, flapping start/stop
 * discrimination and recovery routing.
 *
 * It is pure data: it neither carries the resource it relates to nor performs
 * the delivery. The contact set records who was told about the ongoing problem,
 * so the recovery notification reaches the same contacts.
 */
struct notification {
  reason_type type;
  uint32_t interval = 0;
  absl::btree_set<std::string> notified_contacts;

  /** @brief Tell whether @p user was among the notified contacts. */
  bool sent_to(const std::string& user) const {
    return notified_contacts.find(user) != notified_contacts.end();
  }

  /** @brief Merge @p contacts into the notified contact set. */
  void add_contacts(const absl::btree_set<std::string>& contacts) {
    notified_contacts.insert(contacts.begin(), contacts.end());
  }
};

/** @brief Dump a notification to a stream (debug/retention output). */
inline std::ostream& operator<<(std::ostream& os, const notification& n) {
  os << "type: " << n.type << ", interval: " << n.interval << ", contacts: ";
  for (const auto& c : n.notified_contacts)
    os << c << ",";
  os << "\n";
  return os;
}

}  // namespace com::centreon::engine::notifications

#endif  // !CCE_NOTIFICATIONS_NOTIFICATION_TYPES_HH
