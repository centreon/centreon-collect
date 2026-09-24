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

#include "broker/core/cache/notification_toggles.hh"

#include <absl/container/flat_hash_set.h>

#include "broker/core/cache/broker_cache.hh"

namespace com::centreon::broker::cache::notification_toggles {

namespace {

/**
 * @brief Set the switch on a host and, when asked, on all its services.
 *
 * @param cache           The Broker cache.
 * @param host_id         The host id.
 * @param enabled         The switch value.
 * @param affect_host     Whether the host itself is toggled.
 * @param affect_services Whether its services are toggled.
 *
 * @return The number of resources toggled.
 */
uint32_t toggle_host(broker_cache& cache,
                     uint64_t host_id,
                     bool enabled,
                     bool affect_host,
                     bool affect_services) {
  uint32_t count = 0;
  if (affect_host && cache.set_notify(host_id, 0, enabled))
    ++count;
  if (affect_services)
    for (uint64_t service_id : cache.service_ids_for_host(host_id))
      if (cache.set_notify(host_id, service_id, enabled))
        ++count;
  return count;
}

/**
 * @brief Walk the child hosts recursively (depth first, each host once even
 * with several parents) and toggle them, the same way Engine's
 * enable_and_propagate_notifications() does.
 *
 * @param cache           The Broker cache.
 * @param host_id         The host whose children are walked.
 * @param enabled         The switch value.
 * @param affect_services Whether the children's services are toggled too.
 * @param visited         Hosts already toggled.
 *
 * @return The number of resources toggled.
 */
uint32_t toggle_children(broker_cache& cache,
                         uint64_t host_id,
                         bool enabled,
                         bool affect_services,
                         absl::flat_hash_set<uint64_t>& visited) {
  uint32_t count = 0;
  for (uint64_t child : cache.children_of(host_id)) {
    if (!visited.insert(child).second)
      continue;
    count += toggle_host(cache, child, enabled, true, affect_services);
    count += toggle_children(cache, child, enabled, affect_services, visited);
  }
  return count;
}

}  // namespace

/**
 * @brief Enable or disable the notifications of a host per scope.
 *
 * Scopes follow the Engine external commands: host is
 * ENABLE/DISABLE_HOST_NOTIFICATIONS, host_and_services is
 * *_HOST_SVC_NOTIFICATIONS plus the host itself, host_and_children is
 * *_HOST_AND_CHILD_NOTIFICATIONS (hosts only, recursively), beyond_host is
 * *_ALL_NOTIFICATIONS_BEYOND_HOST (the descendants and their services, the
 * host itself untouched).
 *
 * @param cache   The Broker cache.
 * @param host_id The host id (must exist in the cache).
 * @param enabled The switch value.
 * @param sc      The propagation scope.
 *
 * @return The number of resources toggled.
 */
uint32_t set_host_notifications(broker_cache& cache,
                                uint64_t host_id,
                                bool enabled,
                                scope sc) {
  absl::flat_hash_set<uint64_t> visited{host_id};
  switch (sc) {
    case scope::host:
      return toggle_host(cache, host_id, enabled, true, false);
    case scope::host_and_services:
      return toggle_host(cache, host_id, enabled, true, true);
    case scope::host_and_children:
      return toggle_host(cache, host_id, enabled, true, false) +
             toggle_children(cache, host_id, enabled, false, visited);
    case scope::beyond_host:
      return toggle_children(cache, host_id, enabled, true, visited);
  }
  return 0;
}

/**
 * @brief Enable or disable the notifications of a service
 * (ENABLE/DISABLE_SVC_NOTIFICATIONS).
 *
 * @param cache      The Broker cache.
 * @param host_id    The host id.
 * @param service_id The service id.
 * @param enabled    The switch value.
 *
 * @return False when the service is unknown to the cache.
 */
bool set_service_notifications(broker_cache& cache,
                               uint64_t host_id,
                               uint64_t service_id,
                               bool enabled) {
  return cache.set_notify(host_id, service_id, enabled);
}

}  // namespace com::centreon::broker::cache::notification_toggles
