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

#ifndef CCB_CONFIG_APPLIER_BROKER_STATE_HH
#define CCB_CONFIG_APPLIER_BROKER_STATE_HH
#include "broker/core/config/applier/state.hh"

namespace com::centreon::broker::config::applier {
class broker_state : public state {
 public:
  struct engine_peer {
    uint64_t poller_id;
    std::string poller_name;
    time_t connected_since;
    /* Does the peer support extended negotiation? */
    bool extended_negotiation;
    /* Does this peer need an update concerning the engine configuration? */
    std::string available_conf;
    /* The current Engine configuration known by this poller. Only available
     * for an Engine peer. */
    std::string engine_conf;
    /* The available_conf_sent flag is set to true when the available
     * configuration has been sent to the Engine peer. Otherwise, it is false.
     */
    bool available_conf_sent;
    /* The conf_acknowledged flag is set to false when a new configuration
     * concerning this Engine peer must be sent to it. Otherwise, it is true. */
    bool conf_acknowledged;
    /* If the conf is unknown by broker, that is to say no available conf from
     * php and no <ID>.prot file, this flag is set to true. And this is the
     * way for Broker to ask its configuration to Engine. */
    bool conf_unknown;
    /* poller_id of the remote peer that is in front of this peer or 0. */
    uint64_t via_remote;
    /* Set to true only when a pb_instance(running=true) content event has been
     * received for this peer in the current Broker session. False at TCP
     * connect time. Used to distinguish real Engine stops from historical
     * running=false events replayed on Broker reconnect. */
    bool running = false;
  };
  struct peer {
    uint64_t poller_id;
    std::string poller_name;
    std::string broker_name{};
    time_t connected_since;
    bool extended_negotiation;
    common::PeerType peer_type;
    // Engine-specific (valid when peer_type == ENGINE):
    std::string available_conf{};
    std::string engine_conf{};
    uint64_t via_remote{0};
  };
  struct broker_peer {
    uint64_t poller_id;
    std::string poller_name;
    std::string broker_name;
    time_t connected_since;
    bool extended_negotiation;
  };
  struct unknown_peer {
    uint64_t poller_id;
    std::string poller_name;
    std::string broker_name;
    time_t connected_since;
    common::PeerType peer_type;
    bool extended_negotiation;
  };

 private:
  /* In a Broker configuration, this object contains the configuration cache
   * directory used by php. We can find there all the pollers configurations. */
  std::filesystem::path _cache_config_dir;

  /* In a Broker configuration, this object contains the pollers configurations
   * known by the Broker. These directories are copies from the
   * _cache_config_dir and are copied once Broker has written them in the
   * storage database. */
  std::filesystem::path _pollers_config_dir;

  /* This object is used to watch the _cache_config_dir. */
  std::unique_ptr<file::directory_watcher> _cache_config_dir_watcher;

  /* Each map is indexed by the tuple {poller_id, poller_name, broker_name}.
   * Peers are split by type so callers never need variant dispatch. */
  using peer_key = std::tuple<uint64_t, std::string, std::string>;
  /* The map of Engine peers, indexed by the poller ID */
  absl::flat_hash_map<uint64_t, engine_peer> _engine_peers
      ABSL_GUARDED_BY(_connected_peers_m);
  absl::flat_hash_map<peer_key, broker_peer> _broker_peers
      ABSL_GUARDED_BY(_connected_peers_m);
  absl::flat_hash_map<peer_key, unknown_peer> _unknown_peers
      ABSL_GUARDED_BY(_connected_peers_m);
  /* When this instance is a relay (no pollers_config_dir), Engine peers that
   * need a ConfigRequest sent to the upstream Broker are queued here.  The map
   * value is the config version currently known by the Engine peer. */
  absl::flat_hash_map<uint64_t, std::pair<std::string, std::string>>
      _pending_config_requests ABSL_GUARDED_BY(_connected_peers_m);
  /* Central: persistent map of (engine_poller_id → relay_poller_id) for all
   * engine peers ever registered via relay.  Unlike _engine_peers this is NOT
   * cleared by remove_peer(), so topology.cache always has entries at shutdown
   * even when all peers have disconnected before the destructor runs. */
  absl::flat_hash_map<uint64_t, uint64_t> _last_known_topology
      ABSL_GUARDED_BY(_connected_peers_m);
  /* Central: poller IDs to revoke, indexed by relay_id.  Populated when a
   * migration is detected (engine N moved from relay R1 to relay R2).  Each
   * relay's BROKER stream drains its own slice in read(). */
  absl::flat_hash_map<uint64_t, std::vector<uint64_t>> _pending_config_revokes
      ABSL_GUARDED_BY(_connected_peers_m);
  /* Relay: DiffStates received from central, queued for forwarding to Engine.
   * Indexed by poller_id. */
  absl::flat_hash_map<uint64_t, std::shared_ptr<io::data>> _pending_diff_states
      ABSL_GUARDED_BY(_connected_peers_m);
  /* Relay: DiffStateAcks received from Engine, queued for forwarding to the
   * upstream central. */
  std::vector<std::shared_ptr<io::data>> _pending_diff_state_acks
      ABSL_GUARDED_BY(_connected_peers_m);
  mutable absl::Mutex _connected_peers_m;
  /* Currently, this is the poller configurations known by this instance of
   * Broker. It is updated during neb::instance and
   * bbdo::pb_engine_configuration messages. And it is used in unified_sql
   * stream when the neb::pb_instance_configuration is handled. */
  absl::flat_hash_map<uint64_t, std::string> _engine_configuration
      ABSL_GUARDED_BY(_connected_peers_m);
  std::atomic<bool> _watch_engine_conf_stopped{false};
  std::unique_ptr<boost::asio::steady_timer> _watch_engine_conf_timer;
  mutable absl::Mutex _lck_set_m;
  absl::flat_hash_set<uint32_t> _lck_set ABSL_GUARDED_BY(_lck_set_m);

  bool _prepare_diff_for_poller(
      uint64_t poller_id,
      std::unique_ptr<engine::configuration::State>&& state)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::optional<bool> _prepare_diff_from_new_prot_file(uint64_t poller_id)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void _start_watch_engine_conf_timer();
  uint32_t _get_lck_file_if_exists(uint32_t poller_id) noexcept;
  void _watch_engine_conf(absl::flat_hash_set<uint32_t>* poller_ids);
  void _check_last_engine_conf() ABSL_LOCKS_EXCLUDED(_lck_set_m);
  bool _feed_cache_and_wake_up_resources(uint64_t poller_id);
  bool _is_engine_peer_connected(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void save_topology_cache() const ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void load_topology_cache() ABSL_LOCKS_EXCLUDED(_connected_peers_m);

 public:
  broker_state(const std::shared_ptr<spdlog::logger>& logger)
      : state(common::PeerType::BROKER, logger) {}
  ~broker_state();

  bool broker_peer_supports_extended_negotiation() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool is_relay() const noexcept;
  std::vector<std::tuple<uint64_t, std::string, std::string>>
  pop_pending_config_requests() ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void add_peer(uint64_t poller_id,
                const std::string& poller_name,
                const std::string& broker_name,
                common::PeerType peer_type,
                bool extended_negotiation,
                const std::string& engine_conf) override
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool is_peer_conf_known(uint64_t poller_id) const override
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void remove_peer(uint64_t poller_id,
                   const std::string& poller_name,
                   const std::string& broker_name) override
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool has_connection_from_poller(uint64_t poller_id) const override
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_instance_running(uint64_t poller_id, bool running) noexcept
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::vector<engine_peer> connected_pollers() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::vector<peer> connected_peers() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool engine_peer_needs_update(uint64_t poller_id) const;
  void acknowledge_engine_peer(uint64_t poller_id);
  void set_poller_engine_conf(uint32_t poller_id,
                              const std::string& engine_conf);
  void set_poller_engine_conf_unknown(uint64_t poller_id, bool unknown);
  bool all_engine_peers_acknowledged();
  void set_available_conf_sent_to_engine_peer(uint32_t poller_id);
  void apply(const com::centreon::broker::config::state& s,
             bool run_mux = true) override;
  const std::filesystem::path& pollers_config_dir() const noexcept;
  void set_pollers_config_dir(const std::filesystem::path& pollers_conf_dir);
  void set_cache_config_dir(const std::filesystem::path& engine_conf_dir);
  const std::filesystem::path& cache_config_dir() const noexcept;
  bool supports_centralized_conf() const override {
    return !_pollers_config_dir.empty();
  }
  void create_prot_file(
      const com::centreon::engine::configuration::State& conf);

  enum class relay_config_response { unknown, up_to_date, diff_ready };
  void register_engine_peer_via_relay(uint64_t engine_id,
                                      const std::string& engine_name,
                                      uint64_t relay_poller_id,
                                      const std::string& config_version)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  relay_config_response prepare_relay_config_response(
      uint64_t engine_id,
      const std::string& relay_config_version)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* Relay: queue a DiffState for forwarding to the Engine stream of poller N.
   * Called on the BROKER-connected stream when it receives a DiffState from
   * the central. */
  void push_pending_diff_state(uint64_t poller_id,
                               std::shared_ptr<io::data> diff_state)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  /* Relay: pop the pending DiffState for Engine poller N (or nullptr). Called
   * from the ENGINE-connected stream's read() to forward it downstream. */
  std::shared_ptr<io::data> pop_pending_diff_state_for_engine(
      uint64_t poller_id) ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  /* Relay: queue a DiffStateAck for forwarding to the upstream central.
   * Called on the ENGINE-connected stream when it receives an ack from Engine.
   */
  void push_pending_diff_state_ack(std::shared_ptr<io::data> ack)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  /* Relay: drain and return all queued DiffStateAcks.  Called from the
   * BROKER-connected stream's read() to forward them upstream. */
  std::vector<std::shared_ptr<io::data>> pop_pending_diff_state_acks()
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* Central: returns the poller IDs of engine peers reachable via relay_id
   * that have a pending configuration update not yet sent. */
  std::vector<uint64_t> engine_peers_via_relay_needing_update(
      uint64_t relay_id) const ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* Central: drain and return all ConfigRevoke poller IDs destined for
   * relay_id.  Called from the BROKER-connected stream's read(). */
  std::vector<uint64_t> pop_pending_config_revokes(uint64_t relay_id)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* Relay: clear all pending state (DiffStates, ConfigRequests) for a poller
   * that the central just revoked.  Called on ConfigRevoke receipt. */
  void clear_pending_for_poller(uint64_t poller_id)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
};
}  // namespace com::centreon::broker::config::applier

#endif  // CCB_CONFIG_APPLIER_BROKER_STATE_HH
