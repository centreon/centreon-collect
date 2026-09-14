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
#ifndef CCB_CONFIG_APPLIER_PEER_REGISTRY_HH
#define CCB_CONFIG_APPLIER_PEER_REGISTRY_HH
#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include "bbdo/bbdo.pb.h"
#include "bbdo/common.pb.h"

namespace com::centreon::broker::config::applier {
/**
 * @brief The peers this Broker instance knows about.
 *
 * "Known" and "connected" are two different things here, and the distinction
 * is the reason this class exists. An Engine peer enters the registry either
 * because it connected -- directly or through a relay -- or because
 * topology.cache said, at the previous shutdown, that it was reachable through
 * such a relay. The latter is a routing hint: a configuration pushed by PHP
 * during the outage can be prepared and addressed to the right relay before
 * that relay is back. It is not a connection, and nothing that reports on
 * connections may count it.
 *
 * Accessors are therefore named after what they return: connected_pollers()
 * and connected_peers() leave out what only a routing hint stands for, while
 * everything a configuration is prepared from works on the whole map.
 */
class peer_registry {
 public:
  struct engine_peer {
    uint64_t poller_id;
    std::string poller_name;
    /* When the peer connected, directly or through a relay. Empty while it has
     * never been seen connected in this Broker session -- which is the case of
     * an entry restored from topology.cache, a routing hint rather than a
     * peer. */
    std::optional<time_t> connected_since;
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
    /* Whether Broker holds the content of the configuration this poller runs:
     * a <ID>.prot on disk, or a <ID>.lck announcing one. False is not a
     * property of the poller -- which may well tell us in engine_conf the
     * version it runs -- but of Broker, and it is what makes it ask the poller
     * for its configuration back. */
    bool broker_knows_poller_conf;
    /* poller_id of the remote peer that is in front of this peer or 0. */
    uint64_t via_remote;
    /* Whether this poller's Engine is running, as told by the last pb_instance
     * received for it. False at TCP connect time: a peer may well be connected
     * with its Engine not started yet. Maintained by the BAM module, the only
     * reader and writer of it, which relies on reading it before flipping it
     * to tell a real Engine stop from a running=false event replayed on
     * Broker reconnect. */
    bool engine_running = false;

    /* Local timezone (IANA name) of the poller machine, advertised in the
     * Welcome message. Empty when the peer did not send one (e.g. legacy
     * Engine or relay-registered peer). Used as the timezone fallback when a
     * host/service carries no explicit timezone. */
    std::string timezone{};

    /* Whether this entry describes a peer that connected, as opposed to one
     * restored from topology.cache. */
    bool connected() const noexcept { return connected_since.has_value(); }
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
    /* Local timezone (IANA name) advertised by the poller machine. Only set
     * for ENGINE peers; empty otherwise. */
    std::string timezone{};
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

  /**
   * @brief Exclusive access to one Engine peer.
   *
   * The read-modify-write sequences around a configuration -- read the version
   * the poller runs, build the diff, then arm the new one -- must not
   * interleave with one another: two of them preparing the same poller would
   * write the same file twice. They therefore hold the registry lock from the
   * first read to the last write, which this object materializes. Everything
   * else uses the plain accessors below.
   */
  class locked_engine_peer {
    absl::Mutex* _m;
    engine_peer* _peer;

   public:
    /* Takes over a mutex the factory has already locked. */
    locked_engine_peer(absl::Mutex* m, engine_peer* peer) noexcept
        : _m{m}, _peer{peer} {}
    locked_engine_peer(const locked_engine_peer&) = delete;
    locked_engine_peer& operator=(const locked_engine_peer&) = delete;
    locked_engine_peer(locked_engine_peer&& other) noexcept
        : _m{other._m}, _peer{other._peer} {
      other._m = nullptr;
      other._peer = nullptr;
    }
    locked_engine_peer& operator=(locked_engine_peer&&) = delete;
    ~locked_engine_peer() ABSL_NO_THREAD_SAFETY_ANALYSIS {
      if (_m)
        _m->Unlock();
    }
    engine_peer* operator->() const noexcept { return _peer; }
    engine_peer& operator*() const noexcept { return *_peer; }
    explicit operator bool() const noexcept { return _peer != nullptr; }
  };

 private:
  /* Whether a configuration is prepared for this peer and still owes it a
   * delivery. */
  static bool _peer_needs_update(const engine_peer& peer);

  std::shared_ptr<spdlog::logger> _logger;
  mutable absl::Mutex _connected_peers_m;
  /* Each map is indexed by the tuple {poller_id, poller_name, broker_name}.
   * Peers are split by type. */
  using peer_key = std::tuple<uint64_t, std::string, std::string>;
  /* Gives the relay poller ID to discuss with to reach an Engine peer with
   * poller ID. */
  absl::flat_hash_map<uint64_t, uint64_t> _poller_to_relay
      ABSL_GUARDED_BY(_connected_peers_m);
  /* The map of Engine peers, indexed by the poller ID. Holds both connected
   * peers and peers merely known to exist -- see engine_peer::connected(). */
  absl::flat_hash_map<uint64_t, engine_peer> _engine_peers
      ABSL_GUARDED_BY(_connected_peers_m);
  absl::flat_hash_map<peer_key, broker_peer> _broker_peers
      ABSL_GUARDED_BY(_connected_peers_m);
  absl::flat_hash_map<peer_key, unknown_peer> _unknown_peers
      ABSL_GUARDED_BY(_connected_peers_m);

 public:
  peer_registry(const std::shared_ptr<spdlog::logger>& logger)
      : _logger{logger} {}

  /* --- Peer lifecycle ------------------------------------------------- */
  /* Returns the configuration version the peer is known to run: the one
   * supplied, or the one already recorded when the caller supplied none. */
  std::string add_peer(uint64_t poller_id,
                       const std::string& poller_name,
                       const std::string& broker_name,
                       common::PeerType peer_type,
                       bool extended_negotiation,
                       const std::string& engine_conf,
                       const std::string& timezone)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool remove_peer(uint64_t poller_id,
                   const std::string& poller_name,
                   const std::string& broker_name)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  /* Registers a poller reached through a relay. Returns the relay it was
   * reached through until now when that changed -- a migration the caller has
   * to revoke on the old relay -- and 0 otherwise. */
  uint64_t register_poller_via_relay(uint64_t poller_id,
                                     const std::string& poller_name,
                                     uint64_t relay_poller_id,
                                     const std::string& config_version)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* --- Snapshots ------------------------------------------------------- */
  std::vector<engine_peer> connected_pollers() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::vector<peer> connected_peers() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  locked_engine_peer lock_engine_peer(uint64_t poller_id)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* --- Peer state ------------------------------------------------------ */
  bool is_poller_connected(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool is_poller_running(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_instance_running(uint64_t poller_id, bool running) noexcept
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::string poller_timezone(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool broker_knows_poller_conf(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool broker_peer_supports_extended_negotiation() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* --- Configuration round --------------------------------------------- */
  bool poller_needs_update(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_poller_engine_conf(uint64_t poller_id,
                              const std::string& engine_conf)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_broker_knows_poller_conf(uint64_t poller_id, bool known)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_poller_conf_sent(uint64_t poller_id)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_poller_conf_acknowledged(uint64_t poller_id)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool try_close_conf_round() ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::vector<uint64_t> pollers_via_relay_needing_update(
      uint64_t relay_id) const ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* --- Topology --------------------------------------------------------- */
  TopologyCache topology_cache() const ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void restore_pollers_from_cache(const TopologyCache& cache)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
};
}  // namespace com::centreon::broker::config::applier

#endif /* ! CCB_CONFIG_APPLIER_PEER_REGISTRY_HH */
