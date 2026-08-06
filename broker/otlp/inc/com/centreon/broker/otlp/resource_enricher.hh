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

#ifndef CCB_OTLP_RESOURCE_ENRICHER_HH
#define CCB_OTLP_RESOURCE_ENRICHER_HH

namespace com::centreon::broker::otlp {

/**
 * @brief Resolves the human-readable names OTel resource attributes need.
 *
 * host.name is the correlation key with CLM, so this is the component the
 * acceptance criterion rests on. Kept behind an interface so tests can supply
 * names without a memory-mapped cache file: config::applier::state is not
 * loaded under unit tests, and global_cache would have nothing to map.
 */
class resource_enricher {
 public:
  virtual ~resource_enricher() = default;

  /**
   * @brief Centreon host name for a host id, or nullopt if unknown.
   */
  virtual std::optional<std::string> host_name(uint64_t host_id) = 0;

  /**
   * @brief Centreon service description, or nullopt if unknown.
   */
  virtual std::optional<std::string> service_description(
      uint64_t host_id,
      uint64_t service_id) = 0;
};

/**
 * @brief resource_enricher backed by the persistent broker cache.
 *
 * The cache is memory-mapped and survives a broker restart: its conf half is
 * updated only by configuration events, and getters fall back to it when the
 * real-time half was discarded after a crash. So host names are available
 * without waiting for a fresh configuration dump.
 */
class global_cache_enricher : public resource_enricher {
 public:
  std::optional<std::string> host_name(uint64_t host_id) override;
  std::optional<std::string> service_description(uint64_t host_id,
                                                 uint64_t service_id) override;
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_RESOURCE_ENRICHER_HH
