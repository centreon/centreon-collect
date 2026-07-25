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

#include "broker/core/config/applier/broker_state.hh"

#include <future>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/post.hpp>

#include "bbdo/bbdo.pb.h"
#include "bbdo/neb.pb.h"
#include "com/centreon/broker/broker_downtime_callbacks.hh"
#include "com/centreon/broker/broker_notification_callbacks.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/common/file.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/downtimes/downtime_manager.hh"
#include "common/engine_conf/indexed_state.hh"
#include "common/engine_conf/parser.hh"
#include "common/notifications/notification_manager.hh"

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker::config::applier {

/**
 * @brief Destructor of the state class.
 */
broker_state::~broker_state() {
  /* Unregister the notification sink so the engine stops referencing the
   * dispatcher before it is destroyed. In the normal shutdown order the engine
   * is already unloaded (deinit() unloads it before state::unload()), so this
   * is defensive; instance_ptr() is null then. */
  if (auto engine = multiplexing::engine::instance_ptr())
    engine->set_notification_sink(nullptr);
  if (_watch_engine_conf_timer) {
    _watch_engine_conf_stopped.store(true);
    _watch_engine_conf_timer->cancel();
    /* Drain the watcher: post a barrier on the strand and wait for it. Because
     * the strand serializes every watcher handler, when the barrier runs no
     * handler is in flight or queued, so the resources used by the handler
     * (e.g. _cache_config_dir_watcher) can be destroyed below without a race.
     * This is deadlock-free here: the pool is still running at shutdown (it is
     * stopped only after deinit()), this destructor runs on the main thread
     * (not a pool thread), and it holds no lock the handler could wait on. */
    if (_watch_strand) {
      std::promise<void> drained;
      auto fut = drained.get_future();
      boost::asio::post(*_watch_strand, [&drained] { drained.set_value(); });
      fut.wait();
    }
  }
  save_topology_cache();
  /* Hand the started downtimes over to the global cache so they are persisted
   * with it and can be re-injected on the next start. This must happen BEFORE
   * unload() (which destroys the manager) and while the cache is still alive
   * (the base state destructor, which owns it, runs after this one). */
  if (com::centreon::common::downtimes::downtime_manager::is_loaded()) {
    std::vector<Downtime> active;
    for (const auto& [_, dt] :
         com::centreon::common::downtimes::downtime_manager::instance()
             .get_scheduled_downtimes()) {
      if (!dt->is_in_effect())
        continue;
      Downtime d;
      d.set_id(dt->get_downtime_id());
      d.set_host_id(dt->host_id());
      d.set_service_id(dt->service_id());
      d.set_author(dt->get_author());
      d.set_comment_data(dt->get_comment());
      d.set_entry_time(dt->get_entry_time());
      d.set_start_time(dt->get_start_time());
      d.set_end_time(dt->get_end_time());
      d.set_fixed(dt->is_fixed());
      d.set_triggered_by(dt->get_triggered_by());
      d.set_duration(dt->get_duration());
      d.set_started(true);
      d.set_comment_id(dt->get_comment_id());
      d.set_type(dt->service_id() == 0 ? Downtime_DowntimeType_HOST
                                       : Downtime_DowntimeType_SERVICE);
      active.push_back(std::move(d));
    }
    config::applier::state::instance().cache().set_active_downtimes(
        std::move(active));
  }
  com::centreon::common::downtimes::downtime_manager::unload();
  com::centreon::common::notifications::notification_manager::unload();
}

/**
 *  Apply a configuration state.
 *
 *  @param[in] s       State to apply.
 *  @param[in] run_mux Set to true if multiplexing must be run.
 */
void broker_state::apply(const com::centreon::broker::config::state& s,
                         bool run_mux) {
  auto logger = log_v2::instance().get(log_v2::CORE);
  /* Load the downtime_manager BEFORE state::apply(): the latter initializes
   * (and, in legacy mode, loads from disk) the global cache, and the cache
   * load re-injects the persisted active downtimes into the manager — so the
   * manager must already exist at that point. */
  {
    auto it = s.params().find("notification_mode");
    _notification_mode = (it != s.params().end() && it->second == "broker")
                             ? notification_mode_broker
                             : notification_mode_engine;
  }
  if (_notification_mode == notification_mode_broker) {
    com::centreon::common::downtimes::downtime_manager::load(
        std::make_unique<broker_downtime_callbacks>(
            com::centreon::common::pool::instance().io_context()));
    /* This message is the signal that Broker now owns downtime management and
     * that the gRPC ScheduleDowntime/DeleteDowntime endpoints are usable. It
     * goes to the CORE logger (enabled at info by default) rather than the
     * CONFIG logger (error by default) so it is reliably observable. */
    logger->info(
        "notification_mode=broker: downtime management enabled, downtime "
        "manager loaded");

    /* Broker owns the notification decision: inject the Broker backend into the
     * notification library. The execution is dispatched to the pollers via
     * pb_notification_execute. In engine mode the manager is never loaded here.
     */
    com::centreon::common::notifications::notification_manager::load(
        std::make_unique<broker_notification_callbacks>());
    logger->info(
        "notification_mode=broker: notification decision enabled, "
        "notification manager loaded");

    /* Register the notification trigger as an event_sink on the multiplexing
     * engine: it drives the notification_manager on each host/service status
     * batch. */
    _notification_dispatcher =
        std::make_unique<broker_notification_dispatcher>();
    multiplexing::engine::instance_ptr()->set_notification_sink(
        _notification_dispatcher.get());
  }

  state::apply(s, run_mux);

  /* The persisted active downtimes are re-injected from
   * _maybe_release_barrier() once the startup readiness barrier releases (i.e.
   * after every output stream has emitted its startup definitions and the
   * engine has flushed them). Doing it here, before the barrier, would let a
   * stale BA service definition clobber the re-injected inherited-downtime
   * depth. In centralized mode the resources are not known yet and the
   * re-injection is a no-op anyway (done later from _process_engine_state after
   * merge). */

  if (s.get_bbdo_version().major_v >= 3) {
    // Configuration cache directory (for broker, from php).
    set_cache_config_dir(s.cache_config_dir());

    // Pollers configuration directory (for Broker).
    // If not provided in the configuration, use a default directory.
    if (!s.cache_config_dir().empty() && _pollers_config_dir.empty()) {
      set_pollers_config_dir(std::filesystem::path(cache_dir()) /
                             "pollers-configuration/");
      load_topology_cache();
    } else
      set_pollers_config_dir(s.pollers_config_dir());
  }
}

/**
 * @brief Invoked by the base startup readiness barrier right after the
 * multiplexing engine is started. Re-inject the persisted active downtimes so
 * they are ordered after the startup definitions the engine just flushed (e.g.
 * the BA virtual service definitions). No-op in centralized mode (resources not
 * known yet; re-injected later from _process_engine_state after merge).
 */
void broker_state::_on_barrier_released() {
  if (_notification_mode == notification_mode_broker)
    cache().reinject_pending_downtimes();
}

/**
 * @brief Get the pollers configurations directory.
 *
 * @return The pollers configurations directory.
 */
const std::filesystem::path& broker_state::pollers_config_dir() const noexcept {
  return _pollers_config_dir;
}

/**
 * @brief Set the pollers configurations directory.
 *
 * @param pollers_config_dir The pollers configurations directory.
 */
void broker_state::set_pollers_config_dir(
    const std::filesystem::path& pollers_config_dir) {
  _pollers_config_dir = pollers_config_dir;
}

/**
 * @brief Set the configuration cache directory.
 *
 * @param engine_conf_dir The configuration cache directory.
 */
void broker_state::set_cache_config_dir(
    const std::filesystem::path& cache_config_dir) {
  _cache_config_dir = cache_config_dir;
  if (!_cache_config_dir.empty()) {
    _logger->info("Watching for changes in '{}'", _cache_config_dir.string());
    _cache_config_dir_watcher = std::make_unique<file::directory_watcher>(
        _cache_config_dir, IN_CREATE | IN_MODIFY | IN_ATTRIB, true);
    if (!_watch_engine_conf_timer) {
      _logger->debug("Starting engine configuration watcher");
      _watch_strand = std::make_unique<
          boost::asio::strand<boost::asio::io_context::executor_type>>(
          com::centreon::common::pool::instance().io_context().get_executor());
      _watch_engine_conf_timer = std::make_unique<boost::asio::steady_timer>(
          com::centreon::common::pool::instance().io_context());
      _start_watch_engine_conf_timer();
    }
  } else if (_cache_config_dir_watcher) {
    _logger->info("Stop watching for changes in '{}'",
                  _cache_config_dir.string());
    _cache_config_dir_watcher.reset();
  }
}

/**
 * @brief Write the topology cache to disk. Called on clean shutdown. Persists
 * (poller_id, relay_id) pairs for all engine peers reachable via a relay so
 * that the central can pre-populate routing hints on restart.
 */
void broker_state::save_topology_cache() const {
  if (_pollers_config_dir.empty())
    return;
  TopologyCache cache;
  {
    absl::ReaderMutexLock lck(&_connected_peers_m);
    for (const auto& [poller_id, relay_id] : _last_known_topology) {
      auto* e = cache.add_entries();
      e->set_poller_id(poller_id);
      e->set_relay_id(relay_id);
    }
  }
  const auto path = _pollers_config_dir / "topology.cache";
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    _logger->warn("Cannot write topology cache: '{}' not accessible",
                  path.string());
    return;
  }
  if (!cache.SerializeToOstream(&f))
    _logger->error("Failed to write topology cache to '{}'", path.string());
  else
    _logger->info("Topology cache written: {} entries", cache.entries_size());
}

/**
 * @brief Load the topology cache from disk. Called once at startup, after
 * _pollers_config_dir is set. Populates _engine_peers with via_remote hints
 * so that PHP diffs pushed during the outage are routed correctly before
 * the relays reconnect.
 */
void broker_state::load_topology_cache() {
  if (_pollers_config_dir.empty())
    return;
  const auto path = _pollers_config_dir / "topology.cache";
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return;
  TopologyCache cache;
  if (!cache.ParseFromIstream(&f)) {
    _logger->warn("Failed to parse topology cache from '{}'", path.string());
    return;
  }
  absl::WriterMutexLock lck(&_connected_peers_m);
  for (const auto& e : cache.entries()) {
    _last_known_topology[e.poller_id()] = e.relay_id();
    if (!_engine_peers.count(e.poller_id())) {
      _engine_peers[e.poller_id()] =
          engine_peer{e.poller_id(), "",   0,     false,       "", "",
                      false,         true, false, e.relay_id()};
    }
  }
  _logger->info("Topology cache loaded: {} hints", cache.entries_size());
}

/**
 * @brief Create the <ID>.prot file for a poller with the given configuration.
 * This file will be used by broker to fill the cache and prepare the storage
 * database. The configuration is sent by the poller when Broker lost it.
 *
 * Before writing, we check whether a newer configuration is already in place
 * or being processed (a .lck file from PHP, a new-<ID>.prot being prepared, or
 * a <ID>.prot already installed by the normal flow). In those cases we skip the
 * write so as not to overwrite a more recent configuration.
 *
 * @param conf The configuration of the poller to create the <ID>.prot file for.
 */
void broker_state::create_prot_file(
    const com::centreon::engine::configuration::State& conf) {
  assert(conf.poller_id());
  const uint32_t poller_id = conf.poller_id();

  // Logs the skip reason, clears the unknown flag, and signals to the caller
  // that creation should be skipped.
  auto skip = [&](std::string_view reason) {
    _logger->info("Skipping prot file creation for poller {}: {}", poller_id,
                  reason);
    set_poller_engine_conf_unknown(poller_id, false);
  };

  // If PHP has already sent a new configuration for this poller (signalled by
  // a .lck file), let the normal configuration flow handle it rather than
  // overwriting with the engine's current (possibly older) state.
  if (!_cache_config_dir.empty()) {
    std::filesystem::path lck_file =
        _cache_config_dir / fmt::format("{}.lck", poller_id);
    if (std::filesystem::is_regular_file(lck_file)) {
      skip(fmt::format(
          "'{}' exists, the normal configuration flow will handle it",
          lck_file.string()));
      return;
    }
  }

  // If the normal flow is already preparing a new-<ID>.prot or has already
  // installed a <ID>.prot, do not overwrite it.
  std::filesystem::path prot_file =
      pollers_config_dir() / fmt::format("{}.prot", poller_id);
  std::filesystem::path new_prot_file =
      pollers_config_dir() / fmt::format("new-{}.prot", poller_id);
  if (std::filesystem::is_regular_file(new_prot_file)) {
    skip(fmt::format(
        "'{}' already exists, the normal configuration flow will handle it",
        new_prot_file.string()));
    return;
  }
  if (std::filesystem::is_regular_file(prot_file)) {
    skip(
        fmt::format("'{}' already exists, the normal configuration flow has "
                    "already handled it",
                    prot_file.string()));
    return;
  }

  std::ofstream f(prot_file);
  if (f) {
    conf.SerializeToOstream(&f);
    f.close();
    _logger->debug("Created prot file '{}' for poller id {}",
                   prot_file.string(), poller_id);
    set_poller_engine_conf_unknown(poller_id, false);
    _feed_cache_and_wake_up_resources(poller_id);
  } else {
    _logger->error("Unable to create '{}'", prot_file.string());
  }
}

/**
 * @brief Add a poller to the list of connected pollers.
 *
 * @param poller_id The id of the poller (an id by host)
 * @param broker_name The name of the poller
 */
void broker_state::add_peer(uint64_t poller_id,
                            const std::string& poller_name,
                            const std::string& broker_name,
                            common::PeerType peer_type,
                            bool extended_negotiation,
                            const std::string& engine_conf,
                            const std::string& timezone) {
  assert(poller_id && !broker_name.empty());
  {
    absl::WriterMutexLock lck(&_connected_peers_m);
    const peer_key key{poller_id, poller_name, broker_name};

    auto found_engine = _engine_peers.find(poller_id);

    bool already_present = (found_engine != _engine_peers.end() &&
                            found_engine->second.poller_name == poller_name) ||
                           _broker_peers.count(key) ||
                           _unknown_peers.count(key);
    if (already_present) {
      _logger->warn(
          "Poller '{}' with id {} already known as connected. Replacing it.",
          broker_name, poller_id);
    } else {
      _logger->info("Poller '{}' with id {} connected", broker_name, poller_id);
    }

    /* For ENGINE reconnections, preserve the known engine_conf if the caller
     * did not supply a new one. */
    std::string effective_engine_conf = engine_conf;
    if (effective_engine_conf.empty() && peer_type == common::ENGINE) {
      if (found_engine != _engine_peers.end())
        effective_engine_conf = found_engine->second.engine_conf;
    }

    /* Remove from all maps in case the peer type changed.
     * For BROKER peers, do NOT erase _engine_peers: a relay-registered engine
     * peer (via_remote) and a broker peer may legitimately share the same
     * poller_id (e.g. rrd and Engine both on poller 1) and must not interfere.
     * The engine_peer entry was created by register_engine_peer_via_relay and
     * must survive until _prepare_diff_for_poller uses it. */
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
        _engine_peers[poller_id] = engine_peer{poller_id,
                                               poller_name,
                                               time(nullptr),
                                               extended_negotiation,
                                               "",
                                               effective_engine_conf,
                                               false,
                                               true,
                                               false,
                                               0u};
        _engine_peers[poller_id].timezone = timezone;
        if (is_relay() && extended_negotiation)
          _pending_config_requests[poller_id] = {poller_name,
                                                 effective_engine_conf};
        break;
      default:
        _unknown_peers[key] =
            unknown_peer{poller_id,     poller_name, broker_name,
                         time(nullptr), peer_type,   extended_negotiation};
        break;
    }
  }
  if (extended_negotiation) {
    if (!_watch_engine_conf_timer) {
      _logger->debug("Starting engine configuration watcher");
      _watch_strand = std::make_unique<
          boost::asio::strand<boost::asio::io_context::executor_type>>(
          com::centreon::common::pool::instance().io_context().get_executor());
      _watch_engine_conf_timer = std::make_unique<boost::asio::steady_timer>(
          com::centreon::common::pool::instance().io_context());
      _start_watch_engine_conf_timer();
    }

    /* Feeding the cache and waking up resources in the database */
    _feed_cache_and_wake_up_resources(poller_id);
  }
}

/**
 * @brief Feed the global cache with the poller configuration and wake up
 * resources in the database. Reads the <poller_id>.prot file and publishes
 * the Engine state. If neither a .prot file nor a .lck file is found, the
 * poller configuration is considered lost and Broker will request it from
 * Engine via a DiffState{unknown=true} at the next negotiation.
 *
 * @param poller_id The poller ID.
 * @return true if the configuration was found, false if it is lost/unknown.
 */
bool broker_state::_feed_cache_and_wake_up_resources(uint64_t poller_id) {
  bool retval = true;
  std::filesystem::path prot_file =
      pollers_config_dir() / fmt::format("{}.prot", poller_id);
  std::fstream f(prot_file);
  multiplexing::publisher pblshr;
  bool poller_conf_lost = false;
  if (f) {
    auto engine_state = std::make_shared<neb::pb_engine_state>();
    auto& state = engine_state->mut_obj();
    state.ParseFromIstream(&f);
    _logger->debug("Publishing poller {} configuration", poller_id);
    pblshr.write(engine_state);
  } else {
    _logger->info("Unable to fill global cache: cannot open '{}'",
                  prot_file.string());
    poller_conf_lost = true;
  }

  /* The directory watcher has been started but may be there were <ID>.lck
   * files already present in the cache directory. We need to check them
   * and apply the diff if needed.
   */
  _logger->debug("Checking for existing {}.lck file", poller_id);
  uint32_t existing_lck = _get_lck_file_if_exists(poller_id);
  if (existing_lck) {
    absl::MutexLock lck(&_lck_set_m);
    _lck_set.insert(existing_lck);
    poller_conf_lost = false;
  }
  if (poller_conf_lost) {
    /* Broker is unable to update the cache concerning this poller because
     * no <ID>.prot file is present in the pollers configuration directory
     * and no <ID>.lck file is present in the cache configuration directory.
     * So, no known configuration and no new configuration for this poller.
     * In that case, Broker sends an empty DiffState to the poller that
     * forces the poller to send its current configuration if it has one.
     */
    _logger->info(
        "The configuration of poller {} seems lost or unknown, asking for "
        "it "
        "to the poller",
        poller_id);
    set_poller_engine_conf_unknown(poller_id, true);
    retval = false;
  }
  return retval;
}

/**
 * @brief Check whether the given poller is currently registered as a connected
 * Engine peer. A pending configuration can only be delivered to a connected
 * poller, so this gates the consumption of its <ID>.lck file.
 *
 * @param poller_id The poller ID.
 * @return true if the poller is a connected Engine peer.
 */
bool broker_state::_is_engine_peer_connected(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  return _engine_peers.contains(poller_id);
}

/**
 * @brief Get the local timezone advertised by an Engine peer at negotiation
 * time.
 *
 * @param poller_id The poller ID.
 * @return The poller machine's timezone (IANA name), or an empty string when
 * the poller is unknown or sent no timezone.
 */
std::string broker_state::poller_timezone(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found == _engine_peers.end())
    return {};
  return found->second.timezone;
}

/**
 * @brief Get the current lck files in the cache configuration directory.
 * This method is used to check if some Engine configurations are already
 * present in the cache directory when the watcher is started.
 *
 * @return The poller ID if <poller_id>.lck file exists in the cache
 * configuration directory, 0 otherwise.
 */
uint32_t broker_state::_get_lck_file_if_exists(uint32_t poller_id) noexcept {
  if (!_cache_config_dir_watcher) {
    return 0;
  }

  std::error_code ec;
  std::filesystem::path lck_file(_cache_config_dir /
                                 fmt::format("{}.lck", poller_id));

  if (!std::filesystem::is_regular_file(lck_file, ec)) {
    if (ec) {
      _logger->warn("Cannot check if '{}' is a regular file: {}",
                    lck_file.string(), ec.message());
    }
    return 0;
  }

  _logger->debug("Found lock file '{}' for poller id {}", lck_file.string(),
                 poller_id);
  return poller_id;
}

/**
 * @brief Called from a Broker. Set the engine configuration of a poller
 * among the list of connected peers.
 *
 * @param poller_id The poller ID.
 * @param engine_conf The new Engine configuration version.
 */
void broker_state::set_poller_engine_conf(uint32_t poller_id,
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
 * @brief Set the engine configuration unknown flag for the given poller.
 * When set to true, Broker will send a DiffState{unknown=true} to Engine
 * at the next negotiation, asking it to send back its full configuration.
 *
 * @param poller_id The poller ID.
 * @param unknown true to mark the configuration as unknown, false
 * otherwise.
 */
void broker_state::set_poller_engine_conf_unknown(uint64_t poller_id,
                                                  bool unknown) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end()) {
    _logger->info("Poller with id {} engine conf is now {}", poller_id,
                  unknown ? "unknown" : "known");
    found->second.conf_unknown = unknown;
  }
}

/**
 * @brief Check if the Engine configuration for the given poller is known
 * to Broker. Returns false if the configuration has been marked unknown,
 * for example after losing its .prot file with no .lck file available.
 *
 * @param poller_id The poller ID.
 * @return true if the configuration is known, false if unknown.
 */
bool broker_state::is_peer_conf_known(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end()) {
    return !found->second.conf_unknown;
  }
  return true;
}

/**
 * @brief Remove a poller from the list of connected pollers.
 *
 * @param poller_id The id of the poller to remove.
 */
void broker_state::remove_peer(uint64_t poller_id,
                               const std::string& poller_name,
                               const std::string& broker_name) {
  assert(poller_id && !broker_name.empty());
  absl::WriterMutexLock lck(&_connected_peers_m);
  const peer_key key{poller_id, poller_name, broker_name};
  bool erased = _engine_peers.erase(poller_id) || _broker_peers.erase(key) ||
                _unknown_peers.erase(key);
  if (erased) {
    _logger->info("Peer poller: '{}' - broker: '{}' with id {} disconnected",
                  poller_name, broker_name, poller_id);
  } else {
    _logger->warn(
        "Peer poller: '{}' - broker: '{}' with id {} not found in connected "
        "peers",
        poller_name, broker_name, poller_id);
  }
}

/**
 * @brief Check if a poller is currently connected.
 *
 * @param poller_id The poller to check.
 */
bool broker_state::has_connection_from_poller(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  return it != _engine_peers.end() && it->second.running;
}

void broker_state::set_instance_running(uint64_t poller_id,
                                        bool running) noexcept {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  if (it != _engine_peers.end())
    it->second.running = running;
}

/**
 * @brief Get the list of connected pollers.
 *
 * @return A vector of engine_peers.
 */
std::vector<broker_state::engine_peer> broker_state::connected_pollers() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<engine_peer> retval;
  retval.reserve(_engine_peers.size());
  for (const auto& [_, peer] : _engine_peers) {
    retval.push_back(peer);
  }
  return retval;
}

/**
 * @brief Get the list of connected peers.
 *
 * @return A vector of peers.
 */
std::vector<broker_state::peer> broker_state::connected_peers() const {
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
    retval.push_back({.poller_id = ep.poller_id,
                      .poller_name = ep.poller_name,
                      .connected_since = ep.connected_since,
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
 * @brief Check if all Engine peers, whose an available configuration has
 * been sent, acknowledged their configuration. If it is the case, Broker
 * can prepare the database for them.
 *
 * This function is a "test-and-reset": if all ENGINE peers have
 * acknowledged, it resets all their conf_acknowledged flags to false before
 * returning true. This prevents two concurrent ack handlers from both
 * entering the global diff block when they check simultaneously under the
 * same write lock.
 *
 * @return True if all Engine peers acknowledged their configuration,
 * false otherwise.
 */
bool broker_state::all_engine_peers_acknowledged() {
  absl::WriterMutexLock lck(&_connected_peers_m);
  bool retval = true;
  uint32_t engine_count = 0;
  uint32_t engine_good = 0;
  for (const auto& [key, peer] : _engine_peers) {
    if (peer.available_conf_sent) {
      if (!peer.conf_acknowledged)
        retval = false;
      else
        ++engine_good;
      ++engine_count;
    }
  }
  _logger->debug("All engine peers acknowledged? {}/{} acknowledged",
                 engine_good, engine_count);
  if (retval && engine_count > 0) {
    /* Reset all flags so that a concurrent or subsequent call won't
     * trigger a second global diff publication for the same round. */
    for (auto& [key, peer] : _engine_peers) {
      peer.conf_acknowledged = false;
      peer.available_conf_sent = false;
    }
  }
  return retval && engine_count > 0;
}

/**
 * For each <ID>.lck file found in the cache directory, this function checks
 * if there is a new Engine configuration for the poller with this ID and
 * prepares the diff to propagate.
 *
 */
void broker_state::_check_last_engine_conf() {
  _logger->trace("Checking for new Engine configurations");
  absl::flat_hash_set<uint32_t> pollers_set;
  {
    absl::MutexLock lck(&_lck_set_m);
    pollers_set.swap(_lck_set);
  }

  _watch_engine_conf(&pollers_set);

  /* Fallback: scan the directory for any .lck files that inotify may have
   * missed (e.g. when multiple rapid touch() calls overflow the inotify
   * queue). This ensures that no configuration update is permanently lost.
   */
  if (!_cache_config_dir.empty()) {
    std::error_code scan_ec;
    std::filesystem::directory_iterator dir_it(_cache_config_dir, scan_ec);
    if (scan_ec) {
      _logger->warn("Error scanning engine config directory '{}': {}",
                    _cache_config_dir.string(), scan_ec.message());
    } else {
      for (const auto& entry : dir_it) {
        const auto& p = entry.path();
        if (p.extension() == ".lck") {
          std::string stem = p.stem().string();
          uint32_t poller_id;
          if (absl::SimpleAtoi(stem, &poller_id)) {
            if (pollers_set.contains(poller_id))
              continue;  // already queued by inotify
            _logger->info(
                "Found orphan lock file '{}' not reported by inotify — "
                "scheduling configuration check for poller {}",
                p.string(), poller_id);
            /* The .lck is kept until the poller is connected and its
             * configuration has been delivered. It is consumed in
             * _check_last_engine_conf. */
            pollers_set.insert(poller_id);
          }
        }
      }
    }
  }

  std::error_code ec;
  for (uint32_t poller_id : pollers_set) {
    _logger->debug(
        "Checking if there is a new Engine configuration for poller {}",
        poller_id);
    /* The configuration of a poller can only be delivered once that poller is
     * connected. If it is not connected yet but its configuration has already
     * been prepared (new-{ID}.prot present), there is nothing to do but wait:
     * keep the .lck and avoid re-parsing the whole configuration on every
     * watcher cycle. The configuration will be delivered when the poller
     * connects (its connection re-queues the poller through
     * _get_lck_file_if_exists). */
    if (!_is_engine_peer_connected(poller_id) &&
        std::filesystem::exists(pollers_config_dir() /
                                fmt::format("new-{}.prot", poller_id))) {
      _logger->debug(
          "Poller {} configuration already prepared; waiting for the poller to "
          "connect before delivering it",
          poller_id);
      continue;
    }
    auto state = std::make_unique<engine::configuration::State>();
    engine::configuration::state_helper state_hlp(state.get());
    engine::configuration::error_cnt err;
    std::string version = common::hash_directory(
        cache_config_dir() / fmt::to_string(poller_id), ec);
    if (ec) {
      _logger->error(
          "Cannot compute the Engine configuration version for poller "
          "'{}': "
          "{}",
          poller_id, ec.message());
      continue;
    }
    engine::configuration::parser p;
    std::filesystem::path centengine_test =
        cache_config_dir() / fmt::to_string(poller_id) / "centengine.test";
    std::filesystem::path centengine_cfg =
        cache_config_dir() / fmt::to_string(poller_id) / "centengine.cfg";
    engine::configuration::parser::build_test_file(centengine_test,
                                                   centengine_cfg, ec);
    if (!ec) {
      try {
        p.parse(centengine_test, state.get(), err);
        state->set_config_version(version);
        state->set_poller_id(poller_id);
        state_hlp.expand(err);
        // We do not trust the pushed configuration: validate it before storing
        // and delivering it. If it is invalid, refuse to push it (the throw is
        // caught below, so the .prot is not written and no diff is prepared).
        state_hlp.resolve(err, _logger);
        if (err.config_errors)
          throw com::centreon::exceptions::msg_fmt(
              "configuration for poller {} (version '{}') has {} error(s); "
              "refusing to push it to the poller",
              poller_id, version, err.config_errors);
        if (!std::filesystem::exists(pollers_config_dir())) {
          std::filesystem::create_directories(pollers_config_dir(), ec);
          if (ec) {
            _logger->error(
                "Cannot create pollers configuration directory '{}': {}",
                pollers_config_dir().string(), ec.message());
          }
        }
        std::filesystem::path last_prot_conf =
            pollers_config_dir() / fmt::format("new-{}.prot", poller_id);
        std::ofstream f(last_prot_conf);
        if (f) {
          state->SerializeToOstream(&f);
          f.close();
          _logger->info(
              "New Engine configuration for poller {} stored, version '{}'",
              poller_id, version);
        } else {
          _logger->error(
              "Cannot write the new Engine protobuf configuration '{}': {}",
              last_prot_conf.string(), strerror(errno));
        }
        /* The .lck marks a PHP-pushed configuration still pending delivery.
         * Consume it only once the poller is connected, so the diff prepared
         * below can be attached to its peer and delivered. If the poller is not
         * connected yet, keep the .lck so the configuration is retried — and
         * recovered through _get_lck_file_if_exists when the poller finally
         * connects — instead of being silently dropped, which would otherwise
         * leave Broker believing the poller configuration is "lost or unknown".
         */
        bool peer_connected = _is_engine_peer_connected(poller_id);
        _prepare_diff_for_poller(poller_id, std::move(state));
        if (peer_connected) {
          std::filesystem::path lck_file =
              cache_config_dir() / fmt::format("{}.lck", poller_id);
          std::filesystem::remove(lck_file, ec);
          if (ec)
            _logger->warn("Cannot remove lock file '{}': {}", lck_file.string(),
                          ec.message());
          else
            _logger->debug("Removed lock file '{}' after processing",
                           lck_file.string());
        } else
          _logger->info(
              "Poller {} is not connected yet; keeping its lock file so its "
              "configuration is retried once it connects",
              poller_id);
      } catch (const std::exception& e) {
        _logger->error("rejecting invalid configuration for poller {}: {}",
                       poller_id, e.what());
        /* The pushed configuration is structurally invalid
         * (parse/expand/resolve error): it will never become valid on its own,
         * so consume its .lck unconditionally instead of retrying it forever.
         * PHP creates a fresh .lck when it pushes a corrected configuration. */
        std::filesystem::path lck_file =
            cache_config_dir() / fmt::format("{}.lck", poller_id);
        std::error_code lck_ec;
        std::filesystem::remove(lck_file, lck_ec);
        if (lck_ec)
          _logger->warn("Cannot remove lock file '{}' of rejected config: {}",
                        lck_file.string(), lck_ec.message());
      }
    } else
      _logger->error("Cannot create Engine configuration test file '{}': {}",
                     centengine_test.string(), ec.message());
  }
}

/**
 * @brief Start the timer to watch for changes in the Engine configurations
 * directory.
 *
 */
void broker_state::_start_watch_engine_conf_timer() {
  bool expected = false;
  if (!_watch_engine_conf_stopped.compare_exchange_strong(expected, false))
    return;

  _logger->trace(
      "Starting watch engine configuration timer with a 5 seconds delay");
  _watch_engine_conf_timer->expires_after(std::chrono::seconds(5));
  /* The handler is bound to _watch_strand so it is serialized with the drain
   * barrier posted by the destructor: once that barrier runs, no watcher
   * handler is in flight or queued and the watched resources can be destroyed
   * safely. */
  _watch_engine_conf_timer->async_wait(boost::asio::bind_executor(
      *_watch_strand,
      [this, logger = _logger](const boost::system::error_code& ec) {
        if (ec) {
          logger->error("Error in engine configuration watcher: {}",
                        ec.message());
          return;
        }
        if (_watch_engine_conf_stopped.load())
          return;
        _check_last_engine_conf();
        _start_watch_engine_conf_timer();
      }));
}

/**
 * @brief Check if some new engine configurations are available.
 *
 * @param poller_ids A set to fill with the poller IDs concerned by some new
 * configuration. This set can already contain some poller IDs to check
 * (for example when a poller initiates its connection to Broker).
 */
void broker_state::_watch_engine_conf(
    absl::flat_hash_set<uint32_t>* poller_ids) {
  if (_cache_config_dir_watcher) {
    _logger->debug("Watch engine configuration directory");
    auto it = _cache_config_dir_watcher->watch();
    for (auto end = _cache_config_dir_watcher->end(); it != end; ++it) {
      _logger->debug("Change detected in '{}'", _cache_config_dir.string());
      auto [event, name] = *it;
      _logger->debug("event: {}, name: '{}'", event, name);
      if (absl::EndsWith(name, ".lck")) {
        std::string_view prefix(name.data(), name.size() - 4);
        uint32_t poller_id;
        if (absl::SimpleAtoi(prefix, &poller_id)) {
          _logger->info(
              "New Engine configuration available, change in '{}' detected "
              "for poller id '{}'",
              name, poller_id);
          /* The .lck is NOT removed here: it marks a configuration still
           * pending delivery. It is consumed later, in _check_last_engine_conf,
           * only once the poller is connected. Keeping it until then allows the
           * configuration to be recovered (via _get_lck_file_if_exists) should
           * the poller connect after this detection. */
          poller_ids->insert(poller_id);
        } else
          _logger->warn("Change in '{}' detected but poller id not found",
                        _cache_config_dir.string());
      }
    }
  }
}

/**
 * @brief Prepare the diff between the previous and the new Engine
 * configurations.
 *
 * @param poller_id The poller ID.
 * @param state The new Engine configuration.
 */
bool broker_state::_prepare_diff_for_poller(
    uint64_t poller_id,
    std::unique_ptr<engine::configuration::State>&& state) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _engine_peers.find(poller_id);
  if (it == _engine_peers.end())
    return false;
  auto& peer = it->second;
  if (peer.engine_conf == state->config_version()) {
    _logger->info(
        "Poller '{}' with id {} already has the latest configuration "
        "(conf: '{}')",
        peer.poller_name, poller_id, peer.engine_conf);
    return false;
  }
  _logger->debug(
      "Poller '{}' with id {} has a new configuration available "
      "(old: '{}', new: '{}')",
      peer.poller_name, poller_id, peer.engine_conf, state->config_version());
  std::filesystem::path previous_prot_conf =
      pollers_config_dir() / fmt::format("{}.prot", poller_id);
  std::fstream f(previous_prot_conf);
  std::unique_ptr<engine::configuration::DiffState> diff_state;
  std::string new_version = state->config_version();
  if (f) {
    /* There is a previous configuration */
    auto previous_state = std::make_unique<engine::configuration::State>();
    previous_state->ParseFromIstream(&f);
    /* If the known configuration by Broker is the same as the one
     * sent by the poller, we can compute the diff. */
    if (previous_state->config_version() == peer.engine_conf) {
      diff_state = std::make_unique<engine::configuration::DiffState>();
      auto previous_indexed_state =
          engine::configuration::indexed_state(std::move(previous_state));
      previous_indexed_state.diff_with_new_config(*state, _logger,
                                                  diff_state.get());
    } else {
      /* Otherwise, we do as if there was no previous configuration,
       * so the diff will be the whole new configuration. */
      _logger->warn(
          "Poller '{}' with id {} has a new configuration available, but "
          "the previous configuration is not the same as the one sent by "
          "the poller (previous: '{}', new: '{}'). The diff will be the "
          "whole new configuration.",
          peer.poller_name, poller_id, peer.engine_conf,
          state->config_version());
      diff_state = std::make_unique<engine::configuration::DiffState>();
      diff_state->set_allocated_state(state.release());
    }
  } else {
    /* No previous configuration */
    diff_state = std::make_unique<engine::configuration::DiffState>();
    diff_state->set_allocated_state(state.release());
  }
  std::filesystem::path diff_prot_conf =
      pollers_config_dir() / fmt::format("diff-{}.prot", poller_id);
  std::ofstream df(diff_prot_conf);
  if (df) {
    diff_state->SerializeToOstream(&df);
    df.close();

    /* The new configuration to send to the poller is
     * new-<poller-ID>.prot. Once sent to it, this file must be renamed
     * into <poller-ID>.prot and the diff file can be removed. */
    peer.available_conf = new_version;
    peer.available_conf_sent = false;
    return true;
  }
  _logger->error("Cannot write the diff Engine protobuf configuration '{}': {}",
                 diff_prot_conf.string(), strerror(errno));
  return false;
}

/**
 * @brief Check if the poller engine peer needs an update. This function is
 * called from Broker.
 *
 * @param poller_id The poller ID.
 *
 * @return A boolean indicating if the poller engine peer needs an update.
 */
bool broker_state::engine_peer_needs_update(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  _logger->trace("engine_peer_needs_update called for poller id {}", poller_id);
  auto found = _engine_peers.find(poller_id);
  if (found == _engine_peers.end())
    return false;
  const auto& peer = found->second;
  if (peer.available_conf_sent)
    return false;
  if (!peer.available_conf.empty() && peer.available_conf != peer.engine_conf) {
    _logger->debug("Available conf: '{}', current conf: '{}' for poller {}",
                   peer.available_conf, peer.engine_conf, poller_id);
    return true;
  }
  return false;
}

/**
 * @brief Acknowledge or not the poller engine peer configuration. When
 * true, the poller is well up to date. When false, broker has a new
 * configuration and the poller did not send any acknowledgement.
 *
 * @param poller_id
 */
void broker_state::acknowledge_engine_peer(uint64_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _engine_peers.find(poller_id);
  if (found != _engine_peers.end())
    found->second.conf_acknowledged = true;
}

/**
 * @brief Called from Broker side when the new configuration has been sent
 * to the poller engine peer.
 *
 * @param poller_id
 */
void broker_state::set_available_conf_sent_to_engine_peer(uint32_t poller_id) {
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

const std::filesystem::path& broker_state::cache_config_dir() const noexcept {
  return _cache_config_dir;
}

/**
 * @brief Returns true if at least one connected Broker peer has
 * extended_negotiation enabled (i.e. is a BBDO3 central broker or relay).
 */
bool broker_state::broker_peer_supports_extended_negotiation() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  for (const auto& [key, bp] : _broker_peers) {
    if (bp.extended_negotiation)
      return true;
  }
  return false;
}

/**
 * @brief Returns true when this broker instance is a relay.  A relay is a cbd
 * that participates in BBDO3 centralized configuration (it has Engine peers
 * with extended_negotiation) but does NOT own a pollers_config_dir: it
 * forwards configuration requests upstream to the central Broker.
 */
bool broker_state::is_relay() const noexcept {
  return _pollers_config_dir.empty();
}

/**
 * @brief Atomically drains and returns all pending ConfigRequests.
 * Each entry is {poller_id, config_version_known_by_relay}.
 */
std::vector<std::tuple<uint64_t, std::string, std::string>>
broker_state::pop_pending_config_requests() {
  absl::WriterMutexLock lck(&_connected_peers_m);
  std::vector<std::tuple<uint64_t, std::string, std::string>> result;
  result.reserve(_pending_config_requests.size());
  for (auto& [id, p] : _pending_config_requests) {
    result.emplace_back(id, std::move(p.first), std::move(p.second));
  }
  _pending_config_requests.clear();
  return result;
}

void broker_state::push_pending_diff_state(uint64_t poller_id,
                                           std::shared_ptr<io::data> diff) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  _pending_diff_states[poller_id] = std::move(diff);
}

std::shared_ptr<io::data> broker_state::pop_pending_diff_state_for_engine(
    uint64_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _pending_diff_states.find(poller_id);
  if (it == _pending_diff_states.end())
    return nullptr;
  auto result = std::move(it->second);
  _pending_diff_states.erase(it);
  return result;
}

void broker_state::push_pending_diff_state_ack(std::shared_ptr<io::data> ack) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  _pending_diff_state_acks.push_back(std::move(ack));
}

std::vector<std::shared_ptr<io::data>>
broker_state::pop_pending_diff_state_acks() {
  absl::WriterMutexLock lck(&_connected_peers_m);
  std::vector<std::shared_ptr<io::data>> result;
  std::swap(result, _pending_diff_state_acks);
  return result;
}

/**
 * @brief Register an engine peer that is reachable via a relay.  Called at
 * the central when it receives a ConfigRequest from relay R for poller N.
 * Creates (or updates) an engine_peer entry in _engine_peers with
 * via_remote = relay_poller_id.
 *
 * @param engine_id       Poller ID of the Engine behind the relay.
 * @param relay_poller_id Poller ID of the relay that sent the ConfigRequest.
 * @param config_version  Config version currently known by the relay (may be
 *                        empty if the relay has no cached config for N).
 */
void broker_state::register_engine_peer_via_relay(
    uint64_t engine_id,
    const std::string& engine_name,
    uint64_t relay_poller_id,
    const std::string& config_version) {
  absl::WriterMutexLock lck(&_connected_peers_m);

  auto it = _engine_peers.find(engine_id);
  if (it != _engine_peers.end()) {
    const uint64_t old_relay = it->second.via_remote;
    if (old_relay != 0 && old_relay != relay_poller_id) {
      _logger->info(
          "Engine {} migrated from relay {} to relay {} — queuing ConfigRevoke "
          "for old relay",
          engine_id, old_relay, relay_poller_id);
      _pending_config_revokes[old_relay].push_back(engine_id);
    } else {
      _logger->info("Updating engine peer {} via relay {}: config version '{}'",
                    engine_id, relay_poller_id, config_version);
    }
    it->second.via_remote = relay_poller_id;
    if (!config_version.empty())
      it->second.engine_conf = config_version;
  } else {
    _logger->info(
        "Registering engine peer {} via relay {} with config version '{}'",
        engine_id, relay_poller_id, config_version);
    _engine_peers[engine_id] = engine_peer{
        engine_id,      engine_name, time(nullptr), true,  "",
        config_version, false,       true,          false, relay_poller_id};
  }
  _last_known_topology[engine_id] = relay_poller_id;
}

/**
 * @brief Load new-{N}.prot and call _prepare_diff_for_poller so that
 * diff-{N}.prot is written (if the relay does not already have the latest
 * version).
 *
 * @return std::nullopt if new-{N}.prot was absent or unreadable (caller should
 *         continue to the next lookup step); true if it was found and a diff
 *         was written; false if it was found but no diff was needed (relay
 *         already has the latest version).
 */
std::optional<bool> broker_state::_prepare_diff_from_new_prot_file(
    uint64_t poller_id) {
  const auto new_file =
      pollers_config_dir() / fmt::format("new-{}.prot", poller_id);
  std::ifstream f(new_file);
  if (!f)
    return std::nullopt;
  auto state = std::make_unique<engine::configuration::State>();
  if (!state->ParseFromIstream(&f)) {
    _logger->error("Failed to parse new-{}.prot for poller {}", poller_id,
                   poller_id);
    return std::nullopt;
  }
  f.close();
  return _prepare_diff_for_poller(poller_id, std::move(state));
}

/**
 * @brief Determine what DiffState to send to a relay in response to a
 * ConfigRequest for engine poller @p engine_id, and write diff-{N}.prot if
 * it does not exist yet.
 *
 * Lookup order:
 * 1. diff-{N}.prot already exists → diff_ready
 * 2. new-{N}.prot exists → delegate to _prepare_diff_for_poller via
 *    _prepare_diff_from_new_prot_file; if diff-{N}.prot was written →
 *    diff_ready, otherwise → up_to_date (relay already has latest version)
 * 3. {N}.prot exists (last acknowledged state):
 *    - version matches relay → up_to_date
 *    - relay is behind → write full state to diff-{N}.prot → diff_ready
 * 4. No file → unknown
 *
 * @param engine_id            Poller ID of the Engine behind the relay.
 * @param relay_config_version Config version currently known by the relay.
 */
broker_state::relay_config_response broker_state::prepare_relay_config_response(
    uint64_t engine_id,
    const std::string& relay_config_version) {
  const auto diff_file =
      pollers_config_dir() / fmt::format("diff-{}.prot", engine_id);
  const auto prev_file =
      pollers_config_dir() / fmt::format("{}.prot", engine_id);

  /* 1. */
  if (std::filesystem::exists(diff_file))
    return relay_config_response::diff_ready;

  /* 2. */
  if (auto r = _prepare_diff_from_new_prot_file(engine_id))
    return *r ? relay_config_response::diff_ready
              : relay_config_response::up_to_date;

  /* 3. */
  {
    std::ifstream f(prev_file);
    if (f) {
      auto state = std::make_unique<engine::configuration::State>();
      if (!state->ParseFromIstream(&f)) {
        _logger->error("Failed to parse {}.prot for relay poller {}", engine_id,
                       engine_id);
        return relay_config_response::unknown;
      }
      f.close();
      if (state->config_version() == relay_config_version)
        return relay_config_response::up_to_date;

      /* Relay is behind the last acknowledged state: send it as full state. */
      const std::string version = state->config_version();
      engine::configuration::DiffState diff;
      diff.set_allocated_state(state.release());
      std::ofstream df(diff_file);
      if (!df) {
        _logger->error("Cannot write diff-{}.prot for relay poller {}: {}",
                       engine_id, engine_id, strerror(errno));
        return relay_config_response::unknown;
      }
      diff.SerializeToOstream(&df);
      df.close();
      {
        absl::WriterMutexLock lck(&_connected_peers_m);
        auto it = _engine_peers.find(engine_id);
        if (it != _engine_peers.end()) {
          it->second.available_conf = version;
          it->second.available_conf_sent = false;
        }
      }
      return relay_config_response::diff_ready;
    }
  }

  /* 4. */
  _logger->debug("Relay config response for poller {}: unknown (no prot file)",
                 engine_id);
  return relay_config_response::unknown;
}

/**
 * @brief Drain and return all ConfigRevoke poller IDs destined for relay_id.
 * Called from the BROKER-connected stream's read() to send ConfigRevoke
 * messages to the relay when a poller has migrated away from it.
 *
 * @param relay_id The poller ID of the relay to drain revokes for.
 * @return Vector of engine poller IDs that the relay must revoke.
 */
std::vector<uint64_t> broker_state::pop_pending_config_revokes(
    uint64_t relay_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto it = _pending_config_revokes.find(relay_id);
  if (it == _pending_config_revokes.end())
    return {};
  std::vector<uint64_t> result = std::move(it->second);
  _pending_config_revokes.erase(it);
  return result;
}

/**
 * @brief Clear all pending state (DiffStates, ConfigRequests) for a poller
 * that the central just revoked.  Called on the relay side when a ConfigRevoke
 * is received, to discard any stale forwarding state for that poller.
 *
 * @param poller_id The poller ID to clear.
 */
void broker_state::clear_pending_for_poller(uint64_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  _pending_diff_states.erase(poller_id);
  _pending_config_requests.erase(poller_id);
}

/**
 * @brief Returns the poller IDs of engine peers that are reachable via the
 * given relay and have a pending configuration update (available_conf differs
 * from engine_conf and has not yet been sent to the relay).
 *
 * Called from the BROKER-connected stream's read() on the central to push
 * new DiffStates to the relay after a PHP configuration push.
 *
 * @param relay_id The poller ID of the relay.
 * @return Vector of engine poller IDs needing an update via this relay.
 */
std::vector<uint64_t> broker_state::engine_peers_via_relay_needing_update(
    uint64_t relay_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<uint64_t> result;
  for (const auto& [id, peer] : _engine_peers) {
    if (peer.via_remote != relay_id)
      continue;
    if (peer.available_conf_sent)
      continue;
    if (!peer.available_conf.empty() && peer.available_conf != peer.engine_conf)
      result.push_back(id);
  }
  return result;
}

}  // namespace com::centreon::broker::config::applier
