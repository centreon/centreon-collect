/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef CCB_CACHE_NOTIFICATION_TOGGLES_HH
#define CCB_CACHE_NOTIFICATION_TOGGLES_HH

#include <cstdint>
#include <optional>
#include <string>

namespace com::centreon::broker::cache {

class broker_cache;

/**
 * @brief Notifications-enabled switches of the Broker cache
 * (notification_mode = broker): the counterpart of Engine's
 * ENABLE/DISABLE_*_NOTIFICATIONS external commands and their propagation to
 * services and child hosts. The state lives in the cache
 * (broker_cache::set_notify); these free functions only implement the
 * propagation scopes on top of it.
 */
namespace notification_toggles {

/* Propagation of a host switch, mirroring the Engine external commands. */
enum class scope {
  host,               // the host only
  host_and_services,  // the host and all its services
  host_and_children,  // the host and its child hosts, recursively (no service)
  beyond_host,        // the child hosts, recursively, with their services; the
                      // host itself is left untouched
};

uint32_t set_host_notifications(broker_cache& cache,
                                uint64_t host_id,
                                bool enabled,
                                scope sc);

bool set_service_notifications(broker_cache& cache,
                               uint64_t host_id,
                               uint64_t service_id,
                               bool enabled);

enum class notifier {
  host,     // the contact's host notifications
  service,  // the contact's service notifications
};

bool set_contact_notifications(broker_cache& cache,
                               const std::string& name,
                               notifier n,
                               bool enabled);

std::optional<uint32_t> set_contactgroup_notifications(broker_cache& cache,
                                                       const std::string& name,
                                                       notifier n,
                                                       bool enabled);

bool set_contact_notification_period(broker_cache& cache,
                                     const std::string& name,
                                     notifier n,
                                     const std::string& period);

bool set_poller_notifications(broker_cache& cache,
                              uint64_t poller_id,
                              bool enabled);

}  // namespace notification_toggles
}  // namespace com::centreon::broker::cache

#endif /* !CCB_CACHE_NOTIFICATION_TOGGLES_HH */
