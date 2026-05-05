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
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/common/file.hh"
#include "com/centreon/common/pool.hh"
#include "common/engine_conf/indexed_state.hh"
#include "common/engine_conf/parser.hh"

namespace com::centreon::broker::config::applier {

/**
 * @brief Destructor of the state class.
 */
broker_state::~broker_state() {
  if (_watch_engine_conf_timer) {
    _watch_engine_conf_stopped.store(true);
    _watch_engine_conf_timer->cancel();
  }
}

/**
 *  Apply a configuration state.
 *
 *  @param[in] s       State to apply.
 *  @param[in] run_mux Set to true if multiplexing must be run.
 */
void broker_state::apply(const com::centreon::broker::config::state& s,
                         bool run_mux) {
  state::apply(s, run_mux);

  // FIXME DBO: before modules application, or this can be later?
  if (s.get_bbdo_version().major_v >= 3) {
    // Configuration cache directory (for broker, from php).
    set_cache_config_dir(s.cache_config_dir());

    // Pollers configuration directory (for Broker).
    // If not provided in the configuration, use a default directory.
    if (!s.cache_config_dir().empty() && _pollers_config_dir.empty())
      set_pollers_config_dir(std::filesystem::path(cache_dir()) /
                             "pollers-configuration/");
    else
      set_pollers_config_dir(s.pollers_config_dir());
  }
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
  } else if (_cache_config_dir_watcher) {
    _logger->info("Stop watching for changes in '{}'",
                  _cache_config_dir.string());
    _cache_config_dir_watcher.reset();
  }
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

  // If PHP has already sent a new configuration for this poller (signalled by
  // a .lck file), let the normal configuration flow handle it rather than
  // overwriting with the engine's current (possibly older) state.
  if (!_cache_config_dir.empty()) {
    std::filesystem::path lck_file =
        _cache_config_dir / fmt::format("{}.lck", poller_id);
    if (std::filesystem::is_regular_file(lck_file)) {
      _logger->info(
          "Skipping prot file creation for poller {}: '{}' exists, the "
          "normal configuration flow will handle it",
          poller_id, lck_file.string());
      set_poller_engine_conf_unknown(poller_id, false);
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
    _logger->info(
        "Skipping prot file creation for poller {}: '{}' already exists, the "
        "normal configuration flow will handle it",
        poller_id, new_prot_file.string());
    set_poller_engine_conf_unknown(poller_id, false);
    return;
  }
  if (std::filesystem::is_regular_file(prot_file)) {
    _logger->info(
        "Skipping prot file creation for poller {}: '{}' already exists, the "
        "normal configuration flow has already handled it",
        poller_id, prot_file.string());
    set_poller_engine_conf_unknown(poller_id, false);
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
                            const std::string& engine_conf) {
  assert(poller_id && !broker_name.empty());
  {
    absl::WriterMutexLock lck(&_connected_peers_m);
    const peer_key key{poller_id, poller_name, broker_name};

    bool already_present = _engine_peers.count(key) ||
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
      auto it = _engine_peers.find(key);
      if (it != _engine_peers.end())
        effective_engine_conf = it->second.engine_conf;
    }

    /* Remove from all maps in case the peer type changed. */
    _engine_peers.erase(key);
    _broker_peers.erase(key);
    _unknown_peers.erase(key);

    switch (peer_type) {
      case common::BROKER:
        _broker_peers[key] = broker_peer{poller_id, poller_name, broker_name,
                                         time(nullptr), extended_negotiation};
        break;
      case common::ENGINE:
        _engine_peers[key] = engine_peer{poller_id,
                                         poller_name,
                                         broker_name,
                                         time(nullptr),
                                         extended_negotiation,
                                         "",
                                         effective_engine_conf,
                                         false,
                                         true,
                                         false,
                                         0u};
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
                                          const std::string& poller_name,
                                          const std::string& broker_name,
                                          const std::string& engine_conf) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  const peer_key key{poller_id, poller_name, broker_name};
  auto found = _engine_peers.find(key);
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
  for (auto& [key, peer] : _engine_peers) {
    if (std::get<0>(key) == poller_id) {
      _logger->info("Poller with id {} engine conf is now {}", poller_id,
                    unknown ? "unknown" : "known");
      peer.conf_unknown = unknown;
    }
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
  for (const auto& [key, peer] : _engine_peers) {
    if (std::get<0>(key) == poller_id)
      return !peer.conf_unknown;
  }
  return true;
}

bool broker_state::poller_is_up_to_date(uint32_t poller_id,
                                        const std::string& poller_name) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  for (const auto& [key, peer] : _engine_peers) {
    if (std::get<0>(key) != poller_id || std::get<1>(key) != poller_name)
      continue;
    const auto& current_conf = peer.engine_conf;
    const auto& available_conf = peer.available_conf;
    _logger->debug(
        "Poller '{}' with id {} current conf: '{}' - available conf '{}'",
        peer.poller_name, poller_id, current_conf, available_conf);
    /* Two cases are possible:
     * - available_conf is empty: it means there is no new configuration
     *   available for this poller. It is up to date if its current_conf
     * is not empty (it has already a configuration).
     * - available_conf is not empty: it means there is a new
     * configuration available for this poller. It is up to date if its
     * current_conf is equal to available_conf.
     */
    return (available_conf.empty() && !current_conf.empty()) ||
           (!available_conf.empty() && current_conf == available_conf);
  }
  _logger->warn("Poller with id {} and name '{}' not found", poller_id,
                poller_name);
  return false;
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
  bool erased = _engine_peers.erase(key) || _broker_peers.erase(key) ||
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
  for (const auto& [key, peer] : _engine_peers) {
    if (std::get<0>(key) == poller_id)
      return true;
  }
  return false;
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
  for (const auto& [key, peer] : _engine_peers) {
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
  for (const auto& [key, bp] : _broker_peers) {
    retval.emplace_back(
        peer{.peer =
                 engine_peer{
                     .poller_id = bp.poller_id,
                     .poller_name = bp.poller_name,
                     .broker_name = bp.broker_name,
                     .connected_since = bp.connected_since,
                     .extended_negotiation = bp.extended_negotiation,
                     .available_conf = "",
                     .engine_conf = "",
                     .available_conf_sent = false,
                     .conf_acknowledged = false,
                     .conf_unknown = false,
                 },
             .peer_type = common::BROKER});
  }
  for (const auto& [key, ep] : _engine_peers) {
    retval.emplace_back(peer{.peer = ep, .peer_type = common::ENGINE});
  }
  for (const auto& [key, up] : _unknown_peers) {
    retval.emplace_back(
        peer{.peer =
                 engine_peer{
                     .poller_id = up.poller_id,
                     .poller_name = up.poller_name,
                     .broker_name = up.broker_name,
                     .connected_since = up.connected_since,
                     .extended_negotiation = up.extended_negotiation,
                     .available_conf = "",
                     .engine_conf = "",
                     .available_conf_sent = false,
                     .conf_acknowledged = false,
                     .conf_unknown = false,
                     .via_remote = 0u,
                 },
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
            std::error_code rm_ec;
            std::filesystem::remove(p, rm_ec);
            if (rm_ec)
              _logger->warn("Cannot remove orphan lock file '{}': {}",
                            p.string(), rm_ec.message());
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
          // FIXME DBO
          assert(state->hosts_size());

          state->SerializeToOstream(&f);
          f.close();
          std::filesystem::path lck_file =
              cache_config_dir() / fmt::format("{}.lck", poller_id);
          std::filesystem::remove(lck_file, ec);
          if (ec)
            _logger->warn("Cannot remove lock file '{}': {}", lck_file.string(),
                          ec.message());
          else
            _logger->debug("Removed lock file '{}' after processing",
                           lck_file.string());
        } else {
          _logger->error(
              "Cannot write the new Engine protobuf configuration '{}': {}",
              last_prot_conf.string(), strerror(errno));
        }
        _prepare_diff_for_poller(poller_id, std::move(state));
      } catch (const std::exception& e) {
        _logger->error("error while parsing poller {} Engine configuration: {}",
                       poller_id, e.what());
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
  _watch_engine_conf_timer->async_wait(
      [this](const boost::system::error_code& ec) {
        if (!ec) {
          _check_last_engine_conf();
          _start_watch_engine_conf_timer();
        } else if (ec) {
          _logger->error("Error in engine configuration watcher: {}",
                         ec.message());
        }
      });
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
      std::string_view event_str;
      if (event & IN_CREATE)
        event_str = "IN_CREATE";
      else if (event & (IN_MODIFY | IN_ATTRIB))
        event_str = "IN_MODIFY";
      else
        event_str = "UNKNOWN";
      if (((event & IN_CREATE) || (event & (IN_MODIFY | IN_ATTRIB))) &&
          absl::EndsWith(name, ".lck")) {
        std::string_view prefix(name.data(), name.size() - 4);
        uint32_t poller_id;
        if (absl::SimpleAtoi(prefix, &poller_id)) {
          _logger->info(
              "New Engine configuration available, change in '{}' detected "
              "for poller id '{}'",
              name, poller_id);
          std::error_code ec;
          std::filesystem::remove(_cache_config_dir / name, ec);
          if (ec)
            _logger->error("Cannot remove lock file '{}': {}", name,
                           ec.message());
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
void broker_state::_prepare_diff_for_poller(
    uint64_t poller_id,
    std::unique_ptr<engine::configuration::State>&& state) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  for (auto& [key, peer] : _engine_peers) {
    if (std::get<0>(key) != poller_id)
      continue;
    if (peer.engine_conf != state->config_version()) {
      _logger->debug(
          "Poller '{}' with id {} has a new configuration available "
          "(old: '{}', new: '{}')",
          peer.poller_name, poller_id, peer.engine_conf,
          state->config_version());
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
      } else {
        _logger->error(
            "Cannot write the diff Engine protobuf configuration '{}': {}",
            diff_prot_conf.string(), strerror(errno));
      }
      break;
    } else {
      _logger->info(
          "Poller '{}' with id {} already has the latest configuration "
          "(conf: '{}')",
          peer.poller_name, poller_id, peer.engine_conf);
    }
  }
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
  for (const auto& [key, peer] : _engine_peers) {
    if (std::get<0>(key) != poller_id)
      continue;
    if (peer.available_conf_sent)
      return false;
    if (!peer.available_conf.empty() &&
        peer.available_conf != peer.engine_conf) {
      _logger->debug("Available conf: '{}', current conf: '{}' for poller {}",
                     peer.available_conf, peer.engine_conf, poller_id);
      return true;
    }
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
  for (auto& [key, peer] : _engine_peers) {
    if (std::get<0>(key) == poller_id) {
      peer.conf_acknowledged = true;
      break;
    }
  }
}

/**
 * @brief Called from Broker side when the new configuration has been sent
 * to the poller engine peer.
 *
 * @param poller_id
 */
void broker_state::set_available_conf_sent_to_engine_peer(uint32_t poller_id) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  for (auto& [key, peer] : _engine_peers) {
    if (std::get<0>(key) == poller_id) {
      peer.available_conf_sent = true;
      peer.conf_acknowledged = false;
      _logger->debug("New configuration sent to poller {}", poller_id);
      return;
    }
  }
  _logger->info("Unable to send configuration to poller {}: it doesn't exist",
                poller_id);
}

const std::filesystem::path& broker_state::cache_config_dir() const noexcept {
  return _cache_config_dir;
}
}  // namespace com::centreon::broker::config::applier
