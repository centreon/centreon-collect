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

#ifndef CCB_BROKER_NOTIFICATION_CALLBACKS_HH
#define CCB_BROKER_NOTIFICATION_CALLBACKS_HH

#include <spdlog/logger.h>

#include "common/notifications/notification_callbacks.hh"

namespace notifications = com::centreon::common::notifications;

namespace com::centreon::broker {

/**
 * @brief Broker implementation of the notification library's connection.
 *
 */
class broker_notification_callbacks
    : public notifications::notification_callbacks {
  std::shared_ptr<spdlog::logger> _logger;

 public:
  broker_notification_callbacks();
  ~broker_notification_callbacks() override = default;

  common::notifications::config get_config(uint64_t host_id,
                                           uint64_t service_id) const override;

  common::notifications::resource_state get_state(
      uint64_t host_id,
      uint64_t service_id) const override;

  common::notifications::delivery_result deliver(
      uint64_t host_id,
      uint64_t service_id,
      common::notifications::notification_category cat,
      common::notifications::reason_type type,
      uint64_t notification_id,
      uint32_t notification_number,
      const std::string& author,
      const std::string& message,
      common::notifications::notification_option options) override;

  void on_notification_number_changed(uint64_t host_id,
                                      uint64_t service_id) override;
};

}  // namespace com::centreon::broker

#endif  // !CCB_BROKER_NOTIFICATION_CALLBACKS_HH
