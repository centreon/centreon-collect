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

#include <absl/container/btree_map.h>
#include <boost/asio/steady_timer.hpp>
#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/file/directory_watcher.hh"
#include "com/centreon/broker/stats/center.hh"
#include "common.pb.h"
#include "common/engine_conf/state_helper.hh"

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
    std::string available_conf;
    /* The current Engine configuration known by this poller. Only available
     * for an Engine peer. */
    std::string engine_conf;
  };

 private:
  const common::PeerType _peer_type;
  std::string _cache_dir;
  std::shared_ptr<spdlog::logger> _logger;
  uint32_t _poller_id;
  uint32_t _rpc_port;
  bbdo::bbdo_version _bbdo_version;
  std::string _poller_name;
  std::string _broker_name;
  size_t _pool_size;
  std::filesystem::path _proto_conf;
  std::unique_ptr<boost::asio::steady_timer> _watch_engine_conf_timer;
  std::atomic_bool _watch_occupied;

  /* In a cbmod configuration, this string contains the directory containing
   * the Engine configuration. */
  // std::filesystem::path _engine_config_dir;

  /* Currently, this is the poller configurations known by this instance of
   * Broker. It is updated during neb::instance and
   * bbdo::pb_engine_configuration messages. And it is used in unified_sql
   * stream when the neb::pb_instance_configuration is handled. */
  absl::flat_hash_map<uint64_t, std::string> _engine_configuration
      ABSL_GUARDED_BY(_connected_peers_m);

  /* In a Broker configuration, this object contains the configuration cache
   * directory used by php. We can find there all the pollers configurations. */
  std::filesystem::path _cache_config_dir;
  /* This object is used to watch the _cache_config_dir. */
  std::unique_ptr<file::directory_watcher> _cache_config_dir_watcher;

  /* In a Broker configuration, this object contains the pollers configurations
   * known by the Broker. These directories are copies from the
   * _cache_config_dir and are copied once Broker has written them in the
   * storage database. */
  std::filesystem::path _pollers_config_dir;

  modules _modules;

  std::shared_ptr<com::centreon::broker::stats::center> _center;
  absl::Mutex _diff_state_m;
  std::unique_ptr<com::centreon::engine::configuration::DiffState> _diff_state;

  static stats _stats_conf;

  /* This map is indexed by the tuple {poller_id, poller_name, broker_name}. */
  absl::btree_map<std::tuple<uint64_t, std::string, std::string>, peer>
      _connected_peers ABSL_GUARDED_BY(_connected_peers_m);
  mutable absl::Mutex _connected_peers_m;

  state(common::PeerType peer_type,
        const std::shared_ptr<spdlog::logger>& logger);
  ~state() noexcept;
  std::vector<uint32_t> _watch_engine_conf();
  void _start_watch_engine_conf_timer();
  void _check_last_engine_conf();
  void _prepare_diff_for_poller(
      uint64_t poller_id,
      std::unique_ptr<engine::configuration::State>&& state)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

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
  const std::string& broker_name() const noexcept;
  const std::string& poller_name() const noexcept;
  // const std::filesystem::path& engine_config_dir() const noexcept;
  // void set_engine_config_dir(const std::filesystem::path& dir);
  const std::filesystem::path& cache_config_dir() const noexcept;
  void set_cache_config_dir(const std::filesystem::path& engine_conf_dir);
  const std::filesystem::path& pollers_config_dir() const noexcept;
  void set_pollers_config_dir(const std::filesystem::path& pollers_conf_dir);
  modules& get_modules();
  void add_peer(uint64_t poller_id,
                const std::string& poller_name,
                const std::string& broker_name,
                common::PeerType peer_type,
                bool extended_negotiation,
                const std::string& engine_conf)
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
  void set_proto_conf(const std::filesystem::path& proto_conf);
  const std::filesystem::path& proto_conf() const;
  std::shared_ptr<com::centreon::broker::stats::center> center() const;
  bool engine_peer_needs_update(uint64_t poller_id) const;
  void set_engine_peer_updated(uint64_t poller_id);
  void set_diff_state(const std::shared_ptr<io::data>& diff);
  std::unique_ptr<com::centreon::engine::configuration::DiffState> diff_state();
  bool set_engine_conf_watcher_occupied(bool occupied,
                                        const std::string_view& owner);
};
}  // namespace com::centreon::broker::config::applier

#endif  // !CCB_CONFIG_APPLIER_STATE_HH
