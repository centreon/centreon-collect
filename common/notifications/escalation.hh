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

#ifndef CCC_NOTIFICATIONS_ESCALATION_HH
#define CCC_NOTIFICATIONS_ESCALATION_HH

#include <vector>

#include "common/notifications/notification_types.hh"

namespace com::centreon::common::notifications {

/**
 * @brief Value snapshot of a notification escalation, decoupled from the
 * Engine escalation object and the Broker cache multi_index entry.
 *
 * The host application fills one per escalation attached to the resource being
 * notified; evaluate_escalations() then reasons on a pure value.
 *
 * @c notification_interval stays in raw configuration units (the same units
 * both sides store); evaluate_escalations() only min-compares it, so the unit
 * is irrelevant to the library and the caller multiplies by its own
 * interval_length afterwards.
 *
 * @c escalate_on is a bitmask of notification_flag.
 *
 * @c in_period is precomputed by the host (now inside the escalation period):
 * the period-timeperiod lookup and its timezone differ between Engine and
 * Broker, so the snapshot absorbs that difference and the library stays free of
 * any timeperiod dependency.
 */
struct escalation {
  uint32_t first_notification = 0;
  uint32_t last_notification = 0;  // 0 = unbounded
  uint32_t notification_interval = 0;
  uint32_t escalate_on = 0;
  bool in_period = true;
  std::vector<std::string> contactgroups;
};

/**
 * @brief Outcome of evaluating the escalations attached to a resource for the
 * current (state, notification_number, time).
 *
 * @c escalated is true as soon as one escalation is viable; in that case @c
 * notification_interval is the smallest interval among the viable escalations
 * (raw configuration units) and @c contactgroups is the union of their
 * contactgroup names. When no escalation is viable @c escalated is false and
 * the caller falls back to the resource's direct contacts.
 */
struct escalation_evaluation {
  bool escalated = false;
  uint32_t notification_interval = 0;
  absl::btree_set<std::string> contactgroups;
};

escalation_evaluation evaluate_escalations(
    const std::vector<escalation>& escalations,
    notifier_type type,
    int state,
    uint32_t notification_number);

}  // namespace com::centreon::common::notifications

#endif  // !CCC_NOTIFICATIONS_ESCALATION_HH
