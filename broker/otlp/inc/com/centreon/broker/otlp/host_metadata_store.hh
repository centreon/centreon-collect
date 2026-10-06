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

#ifndef CCB_OTLP_HOST_METADATA_STORE_HH
#define CCB_OTLP_HOST_METADATA_STORE_HH

#include "bbdo/neb.pb.h"

namespace com::centreon::broker {
class persistent_cache;
}

namespace com::centreon::broker::otlp {

/**
 * @brief Host information of a Centreon Monitoring Agent, as last received
 * from engine (AgentHostInfo event).
 */
struct host_metadata {
  std::string os_type;
  std::string os_name;
  std::string os_version;
  std::string arch;
  std::string machine_id;
  std::vector<std::string> ips;

  bool operator==(const host_metadata& other) const {
    return os_type == other.os_type && os_name == other.os_name &&
           os_version == other.os_version && arch == other.arch &&
           machine_id == other.machine_id && ips == other.ips;
  }
  bool operator!=(const host_metadata& other) const {
    return !(*this == other);
  }
};

/**
 * @brief In-memory store of CMA host information, keyed by host id.
 *
 * An agent sends its host information only at connection and it doesn't
 * change while the agent runs, so the last AgentHostInfo event of a host
 * replaces its whole record. Records are never forgotten: the information of
 * a disconnected agent still describes its machine. Engine re-sends the
 * information of each connected agent periodically, so a restarted broker
 * recovers every connected agent within one engine snapshot period.
 *
 * The store is shared by the successive streams of an endpoint, so it is
 * protected by its own mutex.
 */
class host_metadata_store {
  mutable absl::Mutex _protect;
  absl::flat_hash_map<uint64_t, host_metadata> _data ABSL_GUARDED_BY(_protect);
  const std::shared_ptr<persistent_cache> _cache;
  /* records changed since the last save */
  bool _modified ABSL_GUARDED_BY(_protect) = false;
  std::chrono::steady_clock::time_point _last_save ABSL_GUARDED_BY(_protect) =
      std::chrono::steady_clock::time_point::min();

 public:
  using pointer = std::shared_ptr<host_metadata_store>;

  explicit host_metadata_store(
      std::shared_ptr<persistent_cache> cache = nullptr);

  bool set(const AgentHostInfo& info);

  /**
   * @brief information of a host, nullopt if unknown
   */
  std::optional<host_metadata> get(uint64_t host_id) const;

  size_t size() const;

  size_t load();
  bool save(std::chrono::seconds min_interval = std::chrono::seconds(0));
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_HOST_METADATA_STORE_HH
