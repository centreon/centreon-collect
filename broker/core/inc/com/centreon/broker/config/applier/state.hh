/**
 * Copyright 2011-2012, 2021-2024 Centreon
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

#ifndef CCB_CONFIG_APPLIER_STATE_HH
#define CCB_CONFIG_APPLIER_STATE_HH

#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/config/state.hh"
#include "common.pb.h"

namespace com::centreon::broker::config::applier {
/**
 *  @class state state.hh "com/centreon/broker/config/applier/state.hh"
 *  @brief Apply a configuration.
 *
 *  Apply some configuration state.
 */
class state {
 public:
  struct stats {
    uint32_t sql_slowest_statements_count = false;
    uint32_t sql_slowest_queries_count = false;
  };

  struct peer {
    uint64_t poller_id;
    std::string name;
    time_t connected_since;
    common::PeerType peer_type;
    bool extended_negotiation;

    peer(uint64_t poller_id,
         const std::string& name,
         time_t connected_since,
         common::PeerType peer_type,
         bool extended_negotiation)
        : poller_id{poller_id},
          name{name},
          connected_since{connected_since},
          peer_type{peer_type},
          extended_negotiation{extended_negotiation} {}
    peer(const peer& other)
        : poller_id{other.poller_id},
          name{other.name},
          connected_since{other.connected_since},
          peer_type{other.peer_type},
          extended_negotiation{other.extended_negotiation} {}
    peer(peer&& other)
        : poller_id{other.poller_id},
          name{std::move(other.name)},
          connected_since{std::move(other.connected_since)},
          peer_type{other.peer_type},
          extended_negotiation{other.extended_negotiation} {}
    /* The connected_since information is not used in the comparison otherwise
     * we would have difficulties to compare the peers. */
    bool operator==(const peer& other) const {
      return poller_id == other.poller_id && name == other.name &&
             peer_type == other.peer_type &&
             extended_negotiation == other.extended_negotiation;
    }

    template <typename H>
    friend H AbslHashValue(H h, const peer& p) {
      return H::combine(std::move(h), p.poller_id, p.name, p.peer_type,
                        p.extended_negotiation);
    }
  };

 private:
  const common::PeerType _peer_type;
  std::string _cache_dir;
  uint32_t _poller_id;
  uint32_t _rpc_port;
  bbdo::bbdo_version _bbdo_version;
  std::string _poller_name;
  size_t _pool_size;

  /* In a cbmod configuration, this string contains the directory containing
   * the Engine configuration. */
  std::filesystem::path _engine_config_dir;

  /* In a Broker configuration, this object contains the configuration cache
   * directory used by php. We can find there all the pollers configurations. */
  std::filesystem::path _config_cache_dir;
  modules _modules;

  static stats _stats_conf;

  absl::flat_hash_set<peer> _connected_peers;
  mutable std::mutex _connected_peers_m;

  state(common::PeerType peer_type,
        const std::shared_ptr<spdlog::logger>& logger);
  ~state() noexcept = default;

 public:
  static state& instance();
  static void load(common::PeerType peer_type);
  static void unload();
  static bool loaded();

  state(const state&) = delete;
  state& operator=(const state&) = delete;
  void apply(const config::state& s, bool run_mux = true);
  const std::string& cache_dir() const noexcept;
  uint32_t rpc_port() const noexcept;
  bbdo::bbdo_version get_bbdo_version() const noexcept;
  uint32_t poller_id() const noexcept;
  size_t pool_size() const noexcept;
  const std::string& poller_name() const noexcept;
  const std::filesystem::path& engine_config_dir() const noexcept;
  void set_engine_config_dir(const std::filesystem::path& dir);
  const std::filesystem::path& config_cache_dir() const noexcept;
  void set_config_cache_dir(const std::filesystem::path& engine_conf_dir);
  modules& get_modules();
  void add_peer(uint64_t poller_id,
                const std::string& poller_name,
                common::PeerType peer_type,
                bool extended_negotiation);
  void remove_peer(uint64_t poller_id,
                   const std::string& poller_name,
                   common::PeerType peer_type);
  bool has_connection_from_poller(uint64_t poller_id) const;
  static stats& mut_stats_conf();
  static const stats& stats_conf();
  std::vector<peer> connected_peers() const;
  common::PeerType peer_type() const;
  std::string get_engine_conf_from_cache();
};
}  // namespace com::centreon::broker::config::applier

#endif  // !CCB_CONFIG_APPLIER_STATE_HH
