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
 * @brief Get every Engine peer this Broker knows about, connected or not.
 *
 * The peers restored from topology.cache are part of the answer: they are what
 * lets a configuration be prepared and routed to the right relay before that
 * relay reconnects. Anything that reports on connections wants
 * connected_pollers() instead.
 *
 * @return A vector of engine_peers.
 */
std::vector<peer_registry::engine_peer> peer_registry::known_engine_peers()
    const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<engine_peer> retval;
  retval.reserve(_engine_peers.size());
  for (const auto& [_, peer] : _engine_peers)
    retval.push_back(peer);

  return retval;
}

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
 * @brief Whether the registry holds an entry for this Engine peer -- which
 * says nothing about it being connected: a peer restored from topology.cache
 * is known and absent.
 *
 * @param poller_id The poller ID.
 *
 * @return true when an entry exists.
 */
bool peer_registry::is_engine_peer_known(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  return _engine_peers.contains(poller_id);
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
bool peer_registry::is_engine_running(uint64_t poller_id) const {
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
 * @brief The relay to talk to in order to reach this Engine peer.
 *
 * @param poller_id The poller ID of the Engine peer.
 *
 * @return The poller ID of the relay, or 0 when the peer is reached directly
 * or is unknown.
 */
uint64_t peer_registry::relay_for_poller(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto it = _poller_to_relay.find(poller_id);
  return it == _poller_to_relay.end() ? 0 : it->second;
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
                      .conf_unknown = false,
                      .via_remote = e.relay_id()};
  }
}

}  // namespace com::centreon::broker::config::applier
