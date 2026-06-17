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

#ifndef CCE_NOTIFICATIONS_NOTIFICATION_CALLBACKS_HH
#define CCE_NOTIFICATIONS_NOTIFICATION_CALLBACKS_HH

#include <cstdint>
#include <ctime>
#include <set>
#include <string>

#include "engine/src/notifications/notification_types.hh"

namespace com::centreon::engine::notifications {

/**
 * @brief Connection from the notification library to its host application.
 *
 * Same pattern as common/downtimes' downtime_callbacks: the engine provides a
 * single concrete implementation, injected as a unique_ptr into the
 * notification_manager. Everything is addressed by logical id
 * (host_id, service_id) — service_id == 0 designates a host. The library has
 * no dependency on any engine object type.
 */
class notification_callbacks {
 public:
  virtual ~notification_callbacks() = default;

  /**
   * @brief Get the program-wide notification configuration.
   *
   * @return The global notification configuration snapshot.
   */
  virtual global_config get_global_config() const = 0;

  /**
   * @brief Get the notification-relevant state of a resource.
   *
   * @param host_id The host id.
   * @param service_id The service id; 0 designates a host.
   * @param now The current time, used to evaluate the notification period.
   *
   * @return The per-resource state snapshot.
   */
  virtual resource_state get_state(uint64_t host_id,
                                   uint64_t service_id,
                                   std::time_t now) const = 0;

  /**
   * @brief Select contacts (escalations included, consulting the manager for
   * recovery routing) and actually send the notification.
   *
   * @param host_id The host id.
   * @param service_id The service id; 0 designates a host.
   * @param cat The notification category.
   * @param type The notification reason.
   * @param notification_id The unique notification id.
   * @param notification_number The notification number.
   * @param author The notification author.
   * @param message The notification message/comment.
   * @param options The notification options.
   *
   * @return Who was notified, plus the escalation-adjusted interval and the
   * escalated flag.
   */
  virtual delivery_result deliver(uint64_t host_id,
                                  uint64_t service_id,
                                  notification_category cat,
                                  reason_type type,
                                  uint64_t notification_id,
                                  uint32_t notification_number,
                                  const std::string& author,
                                  const std::string& message,
                                  notification_option options) = 0;

  /**
   * @brief Notify the host application that a resource's notification number
   * changed (status push to Broker).
   *
   * @param host_id The host id.
   * @param service_id The service id; 0 designates a host.
   */
  virtual void on_notification_number_changed(uint64_t host_id,
                                              uint64_t service_id) = 0;
};

}  // namespace com::centreon::engine::notifications

#endif  // !CCE_NOTIFICATIONS_NOTIFICATION_CALLBACKS_HH
