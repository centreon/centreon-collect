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

#include <optional>

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
 *
 * What Broker knows of the configuration of each poller is not here: it lives
 * in broker_state, with its own lock (see poller_conf). The registry lock is
 * always taken last, and never held while calling out of the registry.
 */
class peer_registry {
 public:
  /* Where an Engine peer stands. Each state implies the previous one: an
   * Engine cannot be running on a poller that is not connected. There is no
   * "disconnected" state: a peer that leaves is removed from the registry. */
  enum class peer_state : uint8_t {
    /* Known from topology.cache only: a routing hint, not a peer. It has not
     * been seen connected in this Broker session. */
    expected,
    /* Connected, directly or through a relay, its Engine not announced as
     * running -- the case at TCP connect time. */
    connected,
    /* Connected, and the last pb_instance received for it said its Engine
     * runs. */
    running,
  };

  struct engine_peer {
    uint64_t poller_id;
    std::string poller_name;
    peer_state state = peer_state::expected;
    /* When the peer connected, directly or through a relay. Meaningless while
     * state is peer_state::expected. */
    time_t connected_since = 0;
    /* Does the peer support extended negotiation? */
    bool extended_negotiation;
    /* poller_id of the remote peer that is in front of this peer or 0. */
    uint64_t via_remote;

    /* Local timezone (IANA name) of the poller machine, advertised in the
     * Welcome message. Empty when the peer did not send one (e.g. legacy
     * Engine or relay-registered peer). Used as the timezone fallback when a
     * host/service carries no explicit timezone. */
    std::string timezone{};

    /* Whether this entry describes a peer that connected, as opposed to one
     * restored from topology.cache. */
    bool connected() const noexcept { return state != peer_state::expected; }
  };
  struct peer {
    uint64_t poller_id;
    std::string poller_name;
    std::string broker_name{};
    time_t connected_since;
    bool extended_negotiation;
    common::PeerType peer_type;
    /* Engine-specific (valid when peer_type == ENGINE). The two versions are
     * filled by broker_state, which holds the configuration state. */
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

 private:
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
  /* What remove_peer() removed. */
  enum class removed : uint8_t { nothing, engine, other };
  /* What register_poller_via_relay() found. */
  struct relay_registration {
    /* The relay this Engine was reached through until now, when it changed --
     * a migration the caller has to revoke on the old relay -- and 0
     * otherwise. */
    uint64_t migrated_from = 0;
    /* Whether the registry already had an entry for this poller, connected or
     * restored from topology.cache. */
    bool known = false;
  };

  peer_registry(const std::shared_ptr<spdlog::logger>& logger)
      : _logger{logger} {}

  /* --- Peer lifecycle ------------------------------------------------- */
  /* Returns whether an Engine entry for this poller was there before, and
   * replaced. */
  bool add_peer(uint64_t poller_id,
                const std::string& poller_name,
                const std::string& broker_name,
                common::PeerType peer_type,
                bool extended_negotiation,
                const std::string& timezone)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  removed remove_peer(uint64_t poller_id,
                      const std::string& poller_name,
                      const std::string& broker_name)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  relay_registration register_poller_via_relay(uint64_t poller_id,
                                               const std::string& poller_name,
                                               uint64_t relay_poller_id)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* --- Snapshots ------------------------------------------------------- */
  std::vector<engine_peer> connected_pollers() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::vector<peer> connected_peers() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::optional<engine_peer> engine_peer_of(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::vector<uint64_t> pollers_via_relay(uint64_t relay_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* --- Peer state ------------------------------------------------------ */
  bool has_engine_peer(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool is_poller_connected(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool is_poller_running(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_instance_running(uint64_t poller_id, bool running) noexcept
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::string poller_timezone(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  bool broker_peer_supports_extended_negotiation() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);

  /* --- Topology --------------------------------------------------------- */
  TopologyCache topology_cache() const ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void restore_pollers_from_cache(const TopologyCache& cache)
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
};
}  // namespace com::centreon::broker::config::applier

#endif /* ! CCB_CONFIG_APPLIER_PEER_REGISTRY_HH */
