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
 */

#include "com/centreon/broker/otlp/resource_enricher.hh"

#include "broker/core/cache/broker_cache.hh"
#include "broker/core/config/applier/state.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;

/**
 * @brief Host name, from broker_cache.
 *
 * @param host_id
 * @return the name, or nullopt if the applier state is not loaded or the host
 * is unknown
 */
std::optional<std::string> broker_cache_enricher::host_name(uint64_t host_id) {
  if (!config::applier::state::loaded())
    return std::nullopt;
  auto h = config::applier::state::instance().cache().host(host_id);
  if (!h)
    return std::nullopt;
  return h->obj().name();
}

/**
 * @brief Service description, from broker_cache.
 *
 * @param host_id
 * @param service_id
 * @return the description, or nullopt if the applier state is not loaded or
 * the service is unknown
 */
std::optional<std::string> broker_cache_enricher::service_description(
    uint64_t host_id,
    uint64_t service_id) {
  if (!config::applier::state::loaded())
    return std::nullopt;
  auto s =
      config::applier::state::instance().cache().service(host_id, service_id);
  if (!s)
    return std::nullopt;
  return s->obj().description();
}

/**
 * @brief OTEL_SERVICE_NAME and OTEL_SERVICE_NAMESPACE of a host, from
 * broker_cache.
 *
 * @param host_id
 * @return the identity; a field is empty when its custom variable is not set
 * or the applier state is not loaded
 */
otel_service broker_cache_enricher::host_otel_service(uint64_t host_id) {
  if (!config::applier::state::loaded())
    return {};
  cache::otel_service found =
      config::applier::state::instance().cache().host_otel_service(host_id);
  return {std::move(found.name), std::move(found.name_space)};
}
