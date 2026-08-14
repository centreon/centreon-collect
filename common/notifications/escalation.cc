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

#include "common/notifications/escalation.hh"

#include <array>

namespace com::centreon::common::notifications {

/**
 * @brief Evaluate the escalations attached to a resource for the current
 * notification.
 *
 * An escalation is viable when its escalate_on covers the current state, the
 * notification number falls in its [first, last] range (last == 0 meaning
 * unbounded) and now is inside its escalation period (precomputed in @c
 * in_period). This is the mutualization of Engine's escalation::is_viable (plus
 * the host/serviceescalation state override) and its smallest-interval /
 * contactgroup-union aggregation (notifier::get_contacts_to_notify), and of
 * Broker's broker_cache::notification_escalation.
 *
 * @param escalations The escalations attached to the resource.
 * @param type Whether the resource is a host or a service; selects the
 * state-to-flag mapping.
 * @param state The current resource state (neb enum, contiguous from 0).
 * @param notification_number The current notification number of the resource.
 *
 * @return The escalation evaluation (see escalation_evaluation).
 */
escalation_evaluation evaluate_escalations(
    const std::vector<escalation>& escalations,
    notifier_type type,
    int state,
    uint32_t notification_number) {
  escalation_evaluation retval;

  /* neb state enums are contiguous from 0, aligned with these flag arrays. */
  static constexpr std::array<uint32_t, 3> host_flag{up, down, unreachable};
  static constexpr std::array<uint32_t, 4> service_flag{ok, warning, critical,
                                                        unknown};

  uint32_t state_flag;
  if (type == host_notification) {
    if (state < 0 || state >= static_cast<int>(host_flag.size()))
      return retval;
    state_flag = host_flag[state];
  } else {
    if (state < 0 || state >= static_cast<int>(service_flag.size()))
      return retval;
    state_flag = service_flag[state];
  }

  for (const escalation& e : escalations) {
    /* An escalation is viable when its escalate_on covers the current state,
     * [first,last] covers the notification number and now is inside its
     * escalation period. */
    if (!(e.escalate_on & state_flag))
      continue;
    if (notification_number < e.first_notification ||
        (e.last_notification != 0 && notification_number > e.last_notification))
      continue;
    if (!e.in_period)
      continue;

    /* Among viable escalations, we keep the smallest notification interval. */
    if (retval.escalated) {
      if (e.notification_interval < retval.notification_interval)
        retval.notification_interval = e.notification_interval;
    } else {
      retval.escalated = true;
      retval.notification_interval = e.notification_interval;
    }
    retval.contactgroups.insert(e.contactgroups.begin(), e.contactgroups.end());
  }
  return retval;
}

}  // namespace com::centreon::common::notifications
