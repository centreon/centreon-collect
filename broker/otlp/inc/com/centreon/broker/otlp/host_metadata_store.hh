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

namespace com::centreon::broker::otlp {

/**
 * @brief Host information of a Centreon Monitoring Agent, as last received
 * from engine (AgentHostInfo event).
 */
struct host_metadata {
  uint64_t poller_id = 0;
  std::string os_type;
  std::string os_name;
  std::string os_version;
  std::string arch;
  std::string machine_id;
  std::vector<std::string> ips;
  /* engine clock: orders events of the same host */
  std::time_t observed_at = 0;
  /* broker clock: expiration, so that engine/broker clock skew doesn't matter
   */
  std::time_t received_at = 0;
};

/**
 * @brief In-memory store of CMA host information, keyed by host id.
 *
 * Lifecycle rules:
 *  - each AgentHostInfo event replaces the whole record of its host;
 *  - an event older (observed_at) than the stored one is ignored, whatever
 *    its poller, so a late event of the former poller of a moved host can't
 *    overwrite the information sent by the new one;
 *  - a record not refreshed during ttl is forgotten. Engine re-sends the
 *    information of each connected agent periodically, so a deleted host, a
 *    host moved away, or an agent that has disconnected disappears after ttl,
 *    and a restarted broker recovers every connected agent within one engine
 *    snapshot period.
 *
 * The store is shared by the successive streams of an endpoint, so it is
 * protected by its own mutex.
 */
class host_metadata_store {
  const std::chrono::seconds _ttl;
  mutable absl::Mutex _protect;
  absl::flat_hash_map<uint64_t, host_metadata> _data ABSL_GUARDED_BY(_protect);

 public:
  using pointer = std::shared_ptr<host_metadata_store>;

  enum class update_result {
    /* first information for this host, or information changed */
    updated,
    /* same information, only the freshness has been updated */
    refreshed,
    /* host.id changed: a resource already built for this host describes
     * another machine */
    identity_changed,
    /* older than the stored information */
    ignored
  };

  explicit host_metadata_store(std::chrono::seconds ttl) : _ttl(ttl) {}

  update_result update(const AgentHostInfo& info, std::time_t now);

  /**
   * @brief information of a host, nullopt if unknown or expired
   */
  std::optional<host_metadata> get(uint64_t host_id, std::time_t now);

  size_t size() const;
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_HOST_METADATA_STORE_HH
