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

#ifndef CCE_ENGINE_NOTIFICATION_CALLBACKS_HH
#define CCE_ENGINE_NOTIFICATION_CALLBACKS_HH

#include <cstdint>
#include <ctime>
#include <string>

#include "engine/src/notifications/notification_callbacks.hh"

namespace com::centreon::engine {

/**
 * @brief Engine implementation of the notification library's connection.
 *
 * Resolves resources by logical id (host_id, service_id; service_id == 0 means
 * a host) and bridges the notification_manager to the engine notifier/host/
 * service objects, contacts and macros. Injected once into the
 * notification_manager via notification_manager::load().
 */
class engine_notification_callbacks
    : public notifications::notification_callbacks {
 public:
  engine_notification_callbacks() = default;
  ~engine_notification_callbacks() override = default;

  /**
   * @brief Get the program-wide notification configuration.
   *
   * @return The global notification configuration snapshot.
   */
  notifications::global_config get_global_config() const override;

  /**
   * @brief Get the notification-relevant state of a resource.
   *
   * @param host_id The host id.
   * @param service_id The service id; 0 designates a host.
   * @param now The current time, used to evaluate the notification period.
   *
   * @return The resource state snapshot (all-default if the resource is
   * unknown).
   */
  notifications::resource_state get_state(uint64_t host_id,
                                          uint64_t service_id,
                                          std::time_t now) const override;

  /**
   * @brief Select the contacts and actually send the notification.
   *
   * @param host_id The host id.
   * @param service_id The service id; 0 designates a host.
   * @param cat The notification category.
   * @param type The notification reason.
   * @param notification_id The unique notification id (for macros).
   * @param notification_number The notification number (for macros).
   * @param author The notification author.
   * @param message The notification message/comment.
   * @param options The notification options.
   *
   * @return Who was notified, the escalation-adjusted interval and the
   * escalated flag.
   */
  notifications::delivery_result deliver(
      uint64_t host_id,
      uint64_t service_id,
      notifications::notification_category cat,
      notifications::reason_type type,
      uint64_t notification_id,
      uint32_t notification_number,
      const std::string& author,
      const std::string& message,
      notifications::notification_option options) override;

  /**
   * @brief Push the new notification number of a resource to Broker.
   *
   * @param host_id The host id.
   * @param service_id The service id; 0 designates a host.
   */
  void on_notification_number_changed(uint64_t host_id,
                                      uint64_t service_id) override;
};

}  // namespace com::centreon::engine

#endif  // !CCE_ENGINE_NOTIFICATION_CALLBACKS_HH
