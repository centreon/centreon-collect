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

#include "com/centreon/broker/cache/global_cache.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;

std::optional<std::string> global_cache_enricher::host_name(uint64_t host_id) {
  auto instance = cache::global_cache::instance_ptr();
  if (!instance)
    return std::nullopt;

  /* The cache lives in a mapped segment that is remapped when it grows, which
   * invalidates any pointer into it. The lock must be held for as long as the
   * returned pointer is used, hence the copy into std::string before it goes
   * out of scope. */
  cache::global_cache::lock l;
  const cache::host* h = instance->get_host(host_id, l);
  if (!h)
    return std::nullopt;
  return std::string(h->name().c_str(), h->name().length());
}

std::optional<std::string> global_cache_enricher::service_description(
    uint64_t host_id,
    uint64_t service_id) {
  auto instance = cache::global_cache::instance_ptr();
  if (!instance)
    return std::nullopt;

  cache::global_cache::lock l;
  const cache::service* s = instance->get_service(host_id, service_id, l);
  if (!s)
    return std::nullopt;
  return std::string(s->description().c_str(), s->description().length());
}
