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

#ifndef CCC_NOTIFICATIONS_NOTIFICATION_CALLBACKS_HH
#define CCC_NOTIFICATIONS_NOTIFICATION_CALLBACKS_HH

#include "common/notifications/notification_types.hh"

namespace com::centreon::common::notifications {

/**
 * @brief Connection from the notification library to its host application.
 *
 * Engine provides a single concrete implementation, injected as a unique_ptr
 * into the notification_manager. Everything is addressed by logical id
 * (host_id, service_id) and service_id == 0 designates a host. The library has
 * no dependency on any Engine object type.
 */
class notification_callbacks {
 public:
  virtual ~notification_callbacks() = default;

  /**
   * @brief Get the effective notification configuration for a resource.
   *
   * The returned @c enabled folds in the resource's own notification-enable
   * flag together with the program-wide (Engine) or per-poller (Broker)
   * setting, so a resource whose notifications are disabled yields
   * @c enabled == false. The host application resolves the poller from the
   * resource id; the library stays poller-agnostic.
   *
   * @param host_id The host id.
   * @param service_id The service id; 0 designates a host.
   *
   * @return The effective notification configuration snapshot.
   */
  virtual config get_config(uint64_t host_id, uint64_t service_id) const = 0;

  /**
   * @brief Get the notification-relevant state of a resource.
   *
   * @param host_id The host id.
   * @param service_id The service id; 0 designates a host.
   *
   * @return The per-resource state snapshot.
   */
  virtual resource_state get_state(uint64_t host_id,
                                   uint64_t service_id) const = 0;

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

}  // namespace com::centreon::common::notifications

#endif  // !CCC_NOTIFICATIONS_NOTIFICATION_CALLBACKS_HH
