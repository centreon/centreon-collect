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
 * @return A vector of engine_peers, each one with a connection date.
 */
std::vector<peer_registry::engine_peer> peer_registry::connected_pollers()
    const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<engine_peer> retval;
  retval.reserve(_engine_peers.size());
  for (const auto& [_, peer] : _engine_peers) {
    if (peer.connected())
      retval.push_back(peer);
  }

  return retval;
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
  for (const auto& [_, ep] : _engine_peers) {
    /* A peer known from topology.cache has no connection to report. */
    if (!ep.connected())
      continue;
    retval.push_back({.poller_id = ep.poller_id,
                      .poller_name = ep.poller_name,
                      .connected_since = *ep.connected_since,
                      .extended_negotiation = ep.extended_negotiation,
                      .peer_type = common::ENGINE,
                      .available_conf = ep.available_conf,
                      .engine_conf = ep.engine_conf,
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
  return it != _engine_peers.end() && it->second.engine_running;
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
  it->second.engine_running = running;
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
    if (!_engine_peers.contains(e.poller_id()))
      _engine_peers[e.poller_id()] =
          engine_peer{.poller_id = e.poller_id(),
                      .poller_name = {},
                      .connected_since = std::nullopt,
                      .extended_negotiation = false,
                      .available_conf = {},
                      .engine_conf = {},
                      .available_conf_sent = false,
                      .conf_acknowledged = true,
                      .broker_knows_poller_conf = true,
                      .via_remote = e.relay_id()};
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
 * @param engine_conf The configuration version the peer says it runs, if any.
 * @param timezone The IANA timezone advertised by the poller machine.
 *
 * @return The configuration version this peer is known to run.
 */
std::string peer_registry::add_peer(uint64_t poller_id,
                                    const std::string& poller_name,
                                    const std::string& broker_name,
                                    common::PeerType peer_type,
                                    bool extended_negotiation,
                                    const std::string& engine_conf,
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

  /* For ENGINE reconnections, preserve the known engine_conf if the caller
   * did not supply a new one. */
  std::string effective_engine_conf = engine_conf;
  if (effective_engine_conf.empty() && peer_type == common::ENGINE &&
      found_engine != _engine_peers.end())
    effective_engine_conf = found_engine->second.engine_conf;

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
                      .connected_since = time(nullptr),
                      .extended_negotiation = extended_negotiation,
                      .available_conf = {},
                      .engine_conf = effective_engine_conf,
                      .available_conf_sent = false,
                      .conf_acknowledged = true,
                      .broker_knows_poller_conf = true,
                      .via_remote = 0,
                      .timezone = timezone};
      break;
    default:
      _unknown_peers[key] =
          unknown_peer{poller_id,     poller_name, broker_name,
                       time(nullptr), peer_type,   extended_negotiation};
      break;
  }
  return effective_engine_conf;
}

/**
 * @brief Remove a peer from the registry.
 *
 * @param poller_id The poller ID.
 * @param poller_name The poller name.
 * @param broker_name The name of the Broker instance on the peer side.
 *
 * @return true when an entry was actually removed.
 */
bool peer_registry::remove_peer(uint64_t poller_id,
                                const std::string& poller_name,
                                const std::string& broker_name) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  const peer_key key{poller_id, poller_name, broker_name};
  return _engine_peers.erase(poller_id) || _broker_peers.erase(key) ||
         _unknown_peers.erase(key);
}

/**
 * @brief Register a poller reached through a relay. Called at the central
 * when it receives a ConfigRequest from relay R for poller N.
 *
 * @param poller_id       Poller ID of the poller behind the relay.
 * @param poller_name     Its name.
 * @param relay_poller_id Poller ID of the relay that sent the ConfigRequest.
 * @param config_version  Config version currently known by the relay (may be
 *                        empty if the relay has no cached config for N).
 *
 * @return The relay this Engine was reached through until now, when it
 * changed -- a migration whose old relay the caller must revoke -- and 0
 * otherwise.
 */
uint64_t peer_registry::register_poller_via_relay(
    uint64_t poller_id,
    const std::string& poller_name,
    uint64_t relay_poller_id,
    const std::string& config_version) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  uint64_t migrated_from = 0;

  auto it = _engine_peers.find(poller_id);
  if (it != _engine_peers.end()) {
    const uint64_t old_relay = it->second.via_remote;
    if (old_relay != 0 && old_relay != relay_poller_id)
      migrated_from = old_relay;
    it->second.via_remote = relay_poller_id;
    /* Reaching us through a relay is a connection: an entry that was only a
     * hint restored from topology.cache becomes a peer here. */
    if (!it->second.connected())
      it->second.connected_since = time(nullptr);
    if (!poller_name.empty())
      it->second.poller_name = poller_name;
    if (!config_version.empty())
      it->second.engine_conf = config_version;
  } else {
    _engine_peers[poller_id] = engine_peer{.poller_id = poller_id,
                                           .poller_name = poller_name,
                                           .connected_since = time(nullptr),
                                           .extended_negotiation = true,
                                           .available_conf = {},
                                           .engine_conf = config_version,
                                           .available_conf_sent = false,
                                           .conf_acknowledged = true,
                                           .broker_knows_poller_conf = true,
                                           .via_remote = relay_poller_id};
  }
  _poller_to_relay[poller_id] = relay_poller_id;
  return migrated_from;
}

/**
 * @brief Take exclusive access to one Engine peer for the duration of a
 * read-modify-write sequence. See locked_engine_peer.
 *
 * @param poller_id The poller ID.
 *
 * @return A handle that is false when the peer is unknown -- and that holds
 * the registry lock either way, until it goes out of scope.
 */
peer_registry::locked_engine_peer peer_registry::lock_engine_peer(
    uint64_t poller_id) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  /* Locked here and released by the handle: the pointer below is only valid
   * while the map cannot rehash under it. */
  _connected_peers_m.Lock();
  auto it = _engine_peers.find(poller_id);
  return locked_engine_peer{&_connected_peers_m,
                            it == _engine_peers.end() ? nullptr : &it->second};
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
 * @brief Whether Broker holds the content of the configuration this poller
 * runs. False after losing its .prot file with no .lck file to replace it,
 * which is what makes Broker ask the poller for its configuration back.
 *
 * Says nothing about engine_conf: the poller may perfectly well have told us
 * the version it runs while we have no idea what that version contains.
 *
 * @param poller_id The poller ID.
 *
 * @return true when Broker knows this poller's configuration.
 */
bool peer_registry::broker_knows_poller_conf(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end())
    return found->second.broker_knows_poller_conf;
  return true;
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
 * @brief Whether a configuration is prepared for this peer and still owes it a
 * delivery.
 *
 * @param peer The peer to look at.
 */
bool peer_registry::_peer_needs_update(const engine_peer& peer) {
  return !peer.available_conf_sent && !peer.available_conf.empty() &&
         peer.available_conf != peer.engine_conf;
}

/**
 * @brief Whether a configuration is prepared for this poller and still owes it
 * a delivery.
 *
 * @param poller_id The poller ID.
 */
bool peer_registry::poller_needs_update(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  _logger->trace("poller_needs_update called for poller id {}", poller_id);
  auto found = _engine_peers.find(poller_id);
  if (found == _engine_peers.end())
    return false;
  const auto& peer = found->second;
  if (_peer_needs_update(peer)) {
    _logger->debug("Available conf: '{}', current conf: '{}' for poller {}",
                   peer.available_conf, peer.engine_conf, poller_id);
    return true;
  }
  return false;
}

/**
 * @brief Set the configuration version a poller says it runs.
 *
 * @param poller_id The poller ID.
 * @param engine_conf The new Engine configuration version.
 */
void peer_registry::set_poller_engine_conf(uint64_t poller_id,
                                           const std::string& engine_conf) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found == _engine_peers.end()) {
    _logger->info("Poller with id {} not found in connected peers", poller_id);
  } else {
    auto& peer = found->second;
    _logger->info(
        "Poller with id {} available conf '{}' and current version changed "
        "from '{}' to '{}'",
        poller_id, peer.available_conf, peer.engine_conf, engine_conf);
    peer.engine_conf = engine_conf;
  }
}

/**
 * @brief Record whether Broker holds the content of this poller's
 * configuration. When set to false, Broker sends a DiffState{unknown=true} to
 * Engine at the next negotiation, asking it to send its full configuration
 * back.
 *
 * @param poller_id The poller ID.
 * @param known false when Broker has nothing stored for this poller.
 */
void peer_registry::set_broker_knows_poller_conf(uint64_t poller_id,
                                                 bool known) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end()) {
    _logger->info("Broker {} the configuration of poller {}",
                  known ? "knows" : "does not know", poller_id);
    found->second.broker_knows_poller_conf = known;
  }
}

/**
 * @brief Record that the prepared configuration has been handed over to this
 * poller. This opens a round for it: the acknowledgement it owed for the
 * previous one no longer counts.
 *
 * @param poller_id The poller ID.
 */
void peer_registry::set_poller_conf_sent(uint64_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end()) {
    found->second.available_conf_sent = true;
    found->second.conf_acknowledged = false;
    _logger->debug("New configuration sent to poller {}", poller_id);
  } else {
    _logger->info("Unable to send configuration to poller {}: it doesn't exist",
                  poller_id);
  }
}

/**
 * @brief Record that this poller acknowledged the configuration it was sent,
 * which closes the round opened by set_poller_conf_sent().
 *
 * @param poller_id The poller ID.
 */
void peer_registry::set_poller_conf_acknowledged(uint64_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end())
    found->second.conf_acknowledged = true;
}

/**
 * @brief Close the configuration round when every poller has played its part,
 * that is to say every poller the configuration was sent to acknowledged it,
 * and none is still owed a delivery. Broker can then prepare the database for
 * them.
 *
 * Closing is not free of consequence, hence the try_: on success the round is
 * *consumed*, every conf_acknowledged and available_conf_sent going back to
 * false. Two acknowledgement handlers checking at the same time would
 * otherwise both enter the global diff block, and the second one would publish
 * a round that no longer exists.
 *
 * @return True when the round was closed -- and only then.
 */
bool peer_registry::try_close_conf_round() {
  absl::WriterMutexLock lck(&_connected_peers_m);
  bool retval = true;
  uint32_t engine_count = 0;
  uint32_t engine_good = 0;
  uint32_t engine_pending = 0;
  for (const auto& [key, peer] : _engine_peers) {
    /* A peer whose configuration is prepared but not delivered yet is part of
     * this round, even though nothing was sent to it. Leaving it out would let
     * one peer's acknowledgement close the round on its own -- and the global
     * diff then consumes every diff-<N>.prot of the directory, including the
     * ones still waiting for their poller. Those files are never written again,
     * so the peer keeps asking to be updated and Broker keeps failing to open a
     * file that no longer exists. */
    if (_peer_needs_update(peer)) {
      ++engine_pending;
      retval = false;
      continue;
    }
    if (peer.available_conf_sent) {
      if (!peer.conf_acknowledged)
        retval = false;
      else
        ++engine_good;
      ++engine_count;
    }
  }
  _logger->debug(
      "All engine peers acknowledged? {}/{} acknowledged, {} still waiting to "
      "be sent",
      engine_good, engine_count, engine_pending);
  if (retval && engine_count > 0) {
    /* Reset all flags so that a concurrent or subsequent call won't trigger a
     * second global diff publication for the same round. */
    for (auto& [key, peer] : _engine_peers) {
      peer.conf_acknowledged = false;
      peer.available_conf_sent = false;
    }
  }
  return retval && engine_count > 0;
}

/**
 * @brief Returns the poller IDs reachable through the given relay that are
 * still owed a configuration.
 *
 * The test is _peer_needs_update() and nothing else: writing the same
 * conditions a second time here is what let the two drift apart once, and a
 * poller left out of one of them loses its configuration.
 *
 * @param relay_id The poller ID of the relay.
 *
 * @return Vector of poller IDs needing an update through this relay.
 */
std::vector<uint64_t> peer_registry::pollers_via_relay_needing_update(
    uint64_t relay_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<uint64_t> result;
  for (const auto& [id, peer] : _engine_peers) {
    if (peer.via_remote == relay_id && _peer_needs_update(peer))
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
