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

#include "com/centreon/broker/broker_notification_dispatcher.hh"

#include "com/centreon/broker/neb/internal.hh"
#include "common/log_v2/log_v2.hh"
#include "common/notifications/notification_manager.hh"

using log_v2 = com::centreon::common::log_v2::log_v2;
namespace notifications = com::centreon::common::notifications;

namespace com::centreon::broker {

broker_notification_dispatcher::broker_notification_dispatcher()
    : _logger{log_v2::instance().get(log_v2::NOTIFICATIONS)} {}

/**
 * @brief Drive the notification decision for a batch of events.
 *
 * Only host/service status events matter: each HARD state is handed to the
 * notification_manager, which decides viability (number, timing, dependencies,
 * suppression) and, when a notification is due, dispatches its execution to the
 * supervising poller. The current state is read from the status event itself;
 * the reason is recovery on a return to OK/UP, a problem otherwise. SOFT states
 * are ignored (Engine only notifies on HARD).
 *
 * @param events The batch drained from the multiplexing engine.
 */
void broker_notification_dispatcher::on_events(
    const std::deque<std::shared_ptr<io::data>>& events) {
  auto& mgr = notifications::notification_manager::instance();
  for (const auto& e : events) {
    switch (e->type()) {
      case neb::pb_service_status::static_type(): {
        const ServiceStatus& s =
            std::static_pointer_cast<neb::pb_service_status>(e)->obj();
        if (s.state_type() != ServiceStatus::HARD)
          break;
        notifications::reason_type reason = s.state() == ServiceStatus::OK
                                                ? notifications::reason_recovery
                                                : notifications::reason_normal;
        mgr.notify(s.host_id(), s.service_id(), reason, "", "",
                   notifications::notification_option_none);
      } break;
      case neb::pb_host_status::static_type(): {
        const HostStatus& h =
            std::static_pointer_cast<neb::pb_host_status>(e)->obj();
        if (h.state_type() != HostStatus::HARD)
          break;
        notifications::reason_type reason = h.state() == HostStatus::UP
                                                ? notifications::reason_recovery
                                                : notifications::reason_normal;
        mgr.notify(h.host_id(), 0, reason, "", "",
                   notifications::notification_option_none);
      } break;
      default:
        break;
    }
  }
}

}  // namespace com::centreon::broker
