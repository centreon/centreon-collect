/**
 * Copyright 2011-2012, 2021-2025 Centreon
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
#include "com/centreon/broker/stats/center.hh"
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
    std::string poller_name;
    std::string broker_name;
    time_t connected_since;
    /* Is it a broker, an engine, a map or an unknown peer? */
    common::PeerType peer_type;
    /* Does the peer support extended negotiation? */
    bool extended_negotiation;
    /* Does this peer need an update concerning the engine configuration? */
    bool needs_update;
    /* Is this peer ready to receive data? That's to say negociation and engine
     * configuration exchanged. */
    bool ready = false;
    std::string engine_conf;
  };

 private:
  const common::PeerType _peer_type;
  std::string _engine_conf;
  std::string _cache_dir;
  uint32_t _poller_id;
  uint32_t _rpc_port;
  bbdo::bbdo_version _bbdo_version;
  std::string _poller_name;
  std::string _broker_name;
  size_t _pool_size;

  /* In a cbmod configuration, this string contains the directory containing
   * the Engine configuration. */
  std::filesystem::path _engine_config_dir;

  /* Currently, this is the poller configurations known by this instance of
   * Broker. It is updated during neb::instance and
   * bbdo::pb_engine_configuration messages. And it is used in unified_sql
   * stream when the neb::pb_instance_configuration is handled. */
  absl::flat_hash_map<uint64_t, std::string> _engine_configuration
      ABSL_GUARDED_BY(_connected_peers_m);

  /* In a Broker configuration, this object contains the configuration cache
   * directory used by php. We can find there all the pollers configurations. */
  std::filesystem::path _config_cache_dir;

  /* In a Broker configuration, this object contains the pollers configurations
   * known by the Broker. These directories are copies from the
   * _config_cache_dir and are copied once Broker has written them in the
   * storage database. */
  std::filesystem::path _pollers_config_dir;

  modules _modules;

  std::shared_ptr<com::centreon::broker::stats::center> _center;

  static stats _stats_conf;

  /* This map is indexed by the tuple {poller_id, poller_name, broker_name}. */
  absl::flat_hash_map<std::tuple<uint64_t, std::string, std::string>, peer>
      _connected_peers ABSL_GUARDED_BY(_connected_peers_m);
  mutable absl::Mutex _connected_peers_m;

  state(common::PeerType peer_type,
        const std::string& engine_conf_version,
        const std::shared_ptr<spdlog::logger>& logger);
  ~state() noexcept = default;

 public:
  static state& instance();
  static void load(common::PeerType peer_type,
                   const std::string& engine_conf_version);
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
  const std::string& broker_name() const noexcept;
  const std::string& poller_name() const noexcept;
  const std::filesystem::path& engine_config_dir() const noexcept;
  void set_engine_config_dir(const std::filesystem::path& dir);
  const std::filesystem::path& config_cache_dir() const noexcept;
  void set_config_cache_dir(const std::filesystem::path& engine_conf_dir);
  const std::filesystem::path& pollers_config_dir() const noexcept;
  void set_pollers_config_dir(const std::filesystem::path& pollers_conf_dir);
  modules& get_modules();
  void add_peer(uint64_t poller_id,
                const std::string& poller_name,
                const std::string& broker_name,
                common::PeerType peer_type,
                bool extended_negotiation)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void remove_peer(uint64_t poller_id,
                   const std::string& poller_name,
                   const std::string& broker_name)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool has_connection_from_poller(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  static stats& mut_stats_conf();
  static const stats& stats_conf();
  std::vector<peer> connected_peers() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  common::PeerType peer_type() const;
  std::string get_engine_conf_from_cache(uint64_t poller_id);
  void set_broker_needs_update(uint64_t poller_id,
                               const std::string& poller_name,
                               const std::string& broker_name,
                               common::PeerType peer_type,
                               bool need_update)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_peers_ready() ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool broker_needs_update(uint64_t poller_id,
                           const std::string& poller_name,
                           const std::string& broker_name) const;
  bool broker_needs_update() const;
  void set_engine_configuration(uint64_t poller_id, const std::string& conf);
  std::string engine_configuration(uint64_t poller_id) const;
  std::shared_ptr<com::centreon::broker::stats::center> center() const;
};
}  // namespace com::centreon::broker::config::applier

#endif  // !CCB_CONFIG_APPLIER_STATE_HH
