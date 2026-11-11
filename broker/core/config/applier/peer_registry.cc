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
#include "broker/core/config/applier/peer_registry.hh"

namespace com::centreon::broker::config::applier {

/**
 * @brief Get the Engine peers currently connected, directly or through a
 * relay.
 *
 * @return The connected pollers, each one with a connection date.
 */
std::vector<peer_registry::engine_peer> peer_registry::connected_pollers()
    const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<engine_peer> retval;
  retval.reserve(_engine_peers.size());
  for (const auto& [poller_id, peer] : _engine_peers) {
    if (peer.connected())
      retval.push_back(peer);
  }

  return retval;
}

/**
 * @brief A copy of the entry of an Engine peer, connected or restored from
 * topology.cache.
 *
 * @param poller_id The poller ID.
 *
 * @return The entry, or std::nullopt if the registry has none.
 */
std::optional<peer_registry::engine_peer> peer_registry::engine_peer_of(
    uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  if (it == _engine_peers.end())
    return std::nullopt;
  return it->second;
}

/**
 * @brief Get the list of connected peers, whatever their type.
 *
 * @return A vector of peers.
 */
std::vector<peer_registry::peer> peer_registry::connected_peers() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<peer> retval;
  retval.reserve(_engine_peers.size() + _broker_peers.size() +
                 _unknown_peers.size());
  for (const auto& [_, bp] : _broker_peers) {
    retval.push_back({.poller_id = bp.poller_id,
                      .poller_name = bp.poller_name,
                      .broker_name = bp.broker_name,
                      .connected_since = bp.connected_since,
                      .extended_negotiation = bp.extended_negotiation,
                      .peer_type = common::BROKER});
  }
  for (const auto& [poller_id, ep] : _engine_peers) {
    /* A peer known from topology.cache has no connection to report. */
    if (!ep.connected())
      continue;
    retval.push_back({.poller_id = ep.poller_id,
                      .poller_name = ep.poller_name,
                      .connected_since = ep.connected_since,
                      .extended_negotiation = ep.extended_negotiation,
                      .peer_type = common::ENGINE,
                      .via_remote = ep.via_remote,
                      .timezone = ep.timezone});
  }
  for (const auto& [_, up] : _unknown_peers) {
    retval.push_back({.poller_id = up.poller_id,
                      .poller_name = up.poller_name,
                      .broker_name = up.broker_name,
                      .connected_since = up.connected_since,
                      .extended_negotiation = up.extended_negotiation,
                      .peer_type = up.peer_type});
  }
  return retval;
}

/**
 * @brief Whether this poller has a live link to us, directly or through a
 * relay.
 *
 * An entry restored from topology.cache is not a connection: it only says
 * which relay to talk to should that poller show up, and carries no connection
 * date. It is excluded here.
 *
 * @param poller_id The poller ID.
 *
 * @return true when the poller is connected.
 */
bool peer_registry::is_poller_connected(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  return it != _engine_peers.end() && it->second.connected();
}

/**
 * @brief Whether this poller's Engine is running, as told by the last
 * pb_instance received for it. A connected peer whose Engine has not announced
 * itself yet is not running.
 *
 * @param poller_id The poller ID.
 *
 * @return true when the Engine of this poller is running.
 */
bool peer_registry::is_poller_running(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  return it != _engine_peers.end() && it->second.state == peer_state::running;
}

/**
 * @brief Record what the last pb_instance said about this poller's Engine.
 *
 * A peer that is not connected cannot be running: an entry restored from
 * topology.cache is a routing hint, and marking it running would make it look
 * like a live poller to everything downstream. Such an event is dropped -- in
 * a sane run it cannot happen, since a peer is registered at negotiation, well
 * before its Engine announces itself.
 *
 * @param poller_id The poller ID.
 * @param running What the event said.
 */
void peer_registry::set_instance_running(uint64_t poller_id,
                                         bool running) noexcept {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  if (it == _engine_peers.end() || !it->second.connected())
    return;
  it->second.state = running ? peer_state::running : peer_state::connected;
}

/**
 * @brief Restore the routing hints saved at the previous shutdown.
 *
 * The entries created here describe pollers that may connect, not pollers that
 * are connected: they carry no connection date, and no name -- topology.cache
 * holds none. Everything else is left at the value that says "nothing known
 * yet", so that the first real connection overwrites the entry rather than
 * inheriting from it.
 *
 * @param cache The topology cache read from disk.
 */
void peer_registry::restore_pollers_from_cache(const TopologyCache& cache) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  for (const auto& e : cache.entries()) {
    _poller_to_relay[e.poller_id()] = e.relay_id();
    if (!_engine_peers.contains(e.poller_id())) {
      _engine_peers[e.poller_id()] = engine_peer{.poller_id = e.poller_id(),
                                                 .poller_name = {},
                                                 .state = peer_state::expected,
                                                 .extended_negotiation = false,
                                                 .via_remote = e.relay_id()};
    }
  }
}

/**
 * @brief Add a peer to the registry, or replace the entry a previous
 * connection left behind.
 *
 * @param poller_id The poller ID.
 * @param poller_name The poller name.
 * @param broker_name The name of the Broker instance on the peer side.
 * @param peer_type What kind of peer just connected.
 * @param extended_negotiation Whether the peer speaks the extended
 * negotiation.
 * @param timezone The IANA timezone advertised by the poller machine.
 *
 * @return true if an Engine entry for this poller was there before, and is
 * replaced or removed.
 */
bool peer_registry::add_peer(uint64_t poller_id,
                             const std::string& poller_name,
                             const std::string& broker_name,
                             common::PeerType peer_type,
                             bool extended_negotiation,
                             const std::string& timezone) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  const peer_key key{poller_id, poller_name, broker_name};

  auto found_engine = _engine_peers.find(poller_id);

  if ((found_engine != _engine_peers.end() &&
       found_engine->second.poller_name == poller_name) ||
      _broker_peers.count(key) || _unknown_peers.count(key))
    _logger->warn(
        "Poller '{}' with id {} already known as connected. Replacing it.",
        broker_name, poller_id);
  else
    _logger->info("Poller '{}' with id {} connected", broker_name, poller_id);

  const bool engine_replaced =
      peer_type != common::BROKER && found_engine != _engine_peers.end();

  /* Remove from all maps in case the peer type changed.
   * For BROKER peers, do NOT erase _engine_peers: a relay-registered engine
   * peer (via_remote) and a broker peer may legitimately share the same
   * poller_id (e.g. rrd and Engine both on poller 1) and must not interfere.
   * The engine_peer entry was created by register_poller_via_relay and
   * must survive until the diff is prepared for it. */
  if (peer_type != common::BROKER)
    _engine_peers.erase(poller_id);
  _broker_peers.erase(key);
  _unknown_peers.erase(key);

  switch (peer_type) {
    case common::BROKER:
      _broker_peers[key] = broker_peer{poller_id, poller_name, broker_name,
                                       time(nullptr), extended_negotiation};
      break;
    case common::ENGINE:
      _engine_peers[poller_id] =
          engine_peer{.poller_id = poller_id,
                      .poller_name = poller_name,
                      .state = peer_state::connected,
                      .connected_since = time(nullptr),
                      .extended_negotiation = extended_negotiation,
                      .via_remote = 0,
                      .timezone = timezone};
      break;
    default:
      _unknown_peers[key] =
          unknown_peer{poller_id,     poller_name, broker_name,
                       time(nullptr), peer_type,   extended_negotiation};
      break;
  }
  return engine_replaced;
}

/**
 * @brief Remove a peer from the registry.
 *
 * @param poller_id The poller ID.
 * @param poller_name The poller name.
 * @param broker_name The name of the Broker instance on the peer side.
 *
 * @return What was removed: an Engine entry, another one, or nothing.
 */
peer_registry::removed peer_registry::remove_peer(
    uint64_t poller_id,
    const std::string& poller_name,
    const std::string& broker_name) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  const peer_key key{poller_id, poller_name, broker_name};
  if (_engine_peers.erase(poller_id))
    return removed::engine;
  if (_broker_peers.erase(key) || _unknown_peers.erase(key))
    return removed::other;
  return removed::nothing;
}

/**
 * @brief Register a poller reached through a relay. Called at the central
 * when it receives a ConfigRequest from relay R for poller N.
 *
 * @param poller_id       Poller ID of the poller behind the relay.
 * @param poller_name     Its name.
 * @param relay_poller_id Poller ID of the relay that sent the ConfigRequest.
 *
 * @return The relay this Engine was reached through until now when it changed,
 * and whether the poller was already known.
 */
peer_registry::relay_registration peer_registry::register_poller_via_relay(
    uint64_t poller_id,
    const std::string& poller_name,
    uint64_t relay_poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  relay_registration retval;

  auto it = _engine_peers.find(poller_id);
  if (it != _engine_peers.end()) {
    retval.known = true;
    const uint64_t old_relay = it->second.via_remote;
    if (old_relay != 0 && old_relay != relay_poller_id)
      retval.migrated_from = old_relay;
    it->second.via_remote = relay_poller_id;
    /* Reaching us through a relay is a connection: an entry that was only a
     * hint restored from topology.cache becomes a peer here. */
    if (!it->second.connected()) {
      it->second.state = peer_state::connected;
      it->second.connected_since = time(nullptr);
    }
    if (!poller_name.empty())
      it->second.poller_name = poller_name;
  } else {
    _engine_peers[poller_id] = engine_peer{.poller_id = poller_id,
                                           .poller_name = poller_name,
                                           .state = peer_state::connected,
                                           .connected_since = time(nullptr),
                                           .extended_negotiation = true,
                                           .via_remote = relay_poller_id};
  }
  _poller_to_relay[poller_id] = relay_poller_id;
  return retval;
}

/**
 * @brief Get the local timezone advertised by an Engine peer at negotiation
 * time.
 *
 * @param poller_id The poller ID.
 *
 * @return The poller machine's timezone (IANA name), or an empty string when
 * the poller is unknown or sent no timezone.
 */
std::string peer_registry::poller_timezone(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found == _engine_peers.end())
    return {};
  return found->second.timezone;
}

/**
 * @brief Returns true if at least one connected Broker peer has
 * extended_negotiation enabled (i.e. is a BBDO3 central broker or relay).
 */
bool peer_registry::broker_peer_supports_extended_negotiation() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  for (const auto& [key, bp] : _broker_peers) {
    if (bp.extended_negotiation)
      return true;
  }
  return false;
}

/**
 * @brief The Engine peers reached through a relay.
 *
 * @param relay_id The poller ID of the relay.
 *
 * @return The poller IDs of the Engine peers behind it.
 */
std::vector<uint64_t> peer_registry::pollers_via_relay(
    uint64_t relay_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<uint64_t> result;
  for (const auto& [id, peer] : _engine_peers) {
    if (peer.via_remote == relay_id)
      result.push_back(id);
  }
  return result;
}

/**
 * @brief The routing hints to persist, so that the next start knows which
 * relay leads to which poller.
 *
 * @return The cache to write to disk.
 */
TopologyCache peer_registry::topology_cache() const {
  TopologyCache cache;
  absl::ReaderMutexLock lck(&_connected_peers_m);
  for (const auto& [poller_id, relay_id] : _poller_to_relay) {
    auto* e = cache.add_entries();
    e->set_poller_id(poller_id);
    e->set_relay_id(relay_id);
  }
  return cache;
}

}  // namespace com::centreon::broker::config::applier
