/**
 * Copyright 2011-2013,2015-2016, 2020-2024 Centreon
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

#include "com/centreon/broker/config/applier/state.hh"
#include <filesystem>

#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/instance_broadcast.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/vars.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::config::applier;

using log_v2 = com::centreon::common::log_v2::log_v2;

// Class instance.
static state* gl_state = nullptr;

/**
 * @brief this conf info may be used by late thread like database connection
 * after state::unload
 * So it's static
 *
 */
state::stats state::_stats_conf;

/**
 *  Default constructor.
 */
state::state(const std::shared_ptr<spdlog::logger>& logger)
    : _poller_id(0),
      _rpc_port(0),
      _bbdo_version{2u, 0u, 0u},
      _modules{logger} {}

/**
 *  Apply a configuration state.
 *
 *  @param[in] s       State to apply.
 *  @param[in] run_mux Set to true if multiplexing must be run.
 */
void state::apply(const com::centreon::broker::config::state& s, bool run_mux) {
  auto logger = log_v2::instance().get(log_v2::CONFIG);

  /* With bbdo 3.0, unified_sql must replace sql/storage */
  if (s.get_bbdo_version().major_v >= 3) {
    auto& lst = s.module_list();
    bool found_sql =
        std::find(lst.begin(), lst.end(), "80-sql.so") != lst.end();
    bool found_storage =
        std::find(lst.begin(), lst.end(), "20-storage.so") != lst.end();
    if (found_sql || found_storage) {
      logger->error(
          "Configuration check error: bbdo versions >= 3.0.0 need the "
          "unified_sql module to be configured.");
      throw msg_fmt(
          "Configuration check error: bbdo versions >= 3.0.0 need the "
          "unified_sql module to be configured.");
    }
  }

  // Sanity checks.
  static char const* const allowed_chars(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_.");
  if (!s.poller_id() || s.poller_name().empty())
    throw msg_fmt(
        "state applier: poller information are "
        "not set: please fill poller_id and poller_name");
  if (s.broker_name().find_first_not_of(allowed_chars) != std::string::npos)
    throw msg_fmt(
        "state applier: broker_name is not valid: allowed characters are {}",
        allowed_chars);
  for (auto& e : s.endpoints()) {
    if (e.name.empty())
      throw msg_fmt(
          "state applier: endpoint name is not set: please fill name of all "
          "endpoints");
    if (e.name.find_first_not_of(allowed_chars) != std::string::npos)
      throw msg_fmt(
          "state applier: endpoint name '{}' is not valid: allowed characters "
          "are '{}'",
          e.name, allowed_chars);
  }

  // Set Broker instance ID.
  io::data::broker_id = s.broker_id();

  // Set poller instance.
  _poller_id = s.poller_id();
  _poller_name = s.poller_name();
  _rpc_port = s.rpc_port();
  _bbdo_version = s.get_bbdo_version();

  // Thread pool size.
  _pool_size = s.pool_size();

  // Set cache directory.
  _cache_dir = s.cache_directory();
  if (_cache_dir.empty())
    _cache_dir = std::filesystem::path(PREFIX_VAR) / s.broker_name();

  // Set pollers directory (for broker central).
  _pollers_conf_dir = s.pollers_conf_dir();
  if (!_pollers_conf_dir.empty()) {
    logger->debug("Pollers configuration php cache detected at '{}'",
                  _pollers_conf_dir.string());
    _local_pollers_conf_dir = _cache_dir / "pollers";
    try {
      if (!std::filesystem::exists(_local_pollers_conf_dir))
        std::filesystem::create_directory(_local_pollers_conf_dir);
      else if (!std::filesystem::is_directory(_local_pollers_conf_dir)) {
        std::filesystem::remove(_local_pollers_conf_dir);
        std::filesystem::create_directory(_local_pollers_conf_dir);
      }
      std::filesystem::path working = _local_pollers_conf_dir / "working";
      std::filesystem::remove_all(working);
      std::filesystem::create_directory(working);
    } catch (const std::exception& e) {
      logger->error(
          "Issues while attempting to create the local pollers configuration "
          "directory '{}': {}",
          _local_pollers_conf_dir.string(), e.what());
      /* They are disabled because of the previous errors. */
      _pollers_conf_dir = "";
      _local_pollers_conf_dir = "";
    }
  } else {
    logger->info(
        "This server is not a central as there is no pollers configuration "
        "directory");
  }

  // Engine configuration directory (for cbmod)
  _engine_conf_dir = s.engine_conf_dir();

  // Apply modules configuration.
  _modules.apply(s.module_list(), s.module_directory(), &s);
  static bool first_application(true);
  if (first_application)
    first_application = false;
  else {
    uint32_t module_count = _modules.size();
    if (module_count)
      logger->info("applier: {} modules loaded", module_count);
    else
      logger->info(
          "applier: no module loaded, you might want to check the "
          "'module_directory' directory");
  }

  // Event queue max size (used to limit memory consumption).
  com::centreon::broker::multiplexing::muxer::event_queue_max_size(
      s.event_queue_max_size());

  com::centreon::broker::config::state st{s};

  // Apply input and output configuration.
  endpoint::instance().apply(st.endpoints());

  // Create instance broadcast event.
  auto ib{std::make_shared<instance_broadcast>()};
  ib->broker_id = io::data::broker_id;
  ib->poller_id = _poller_id;
  ib->poller_name = _poller_name;
  ib->enabled = true;
  com::centreon::broker::multiplexing::engine::instance_ptr()->publish(ib);

  // Enable multiplexing loop.
  if (run_mux)
    com::centreon::broker::multiplexing::engine::instance_ptr()->start();
}

/**
 *  Get applied cache directory.
 *
 *  @return Cache directory.
 */
const std::filesystem::path& state::cache_dir() const noexcept {
  return _cache_dir;
}

/**
 * @brief Get the configured BBDO version
 *
 * @return The bbdo version.
 */
bbdo::bbdo_version state::get_bbdo_version() const noexcept {
  return _bbdo_version;
}

/**
 *  Get the instance of this object.
 *
 *  @return Class instance.
 */
state& state::instance() {
  assert(gl_state);
  return *gl_state;
}

/**
 *  Load singleton.
 */
void state::load() {
  if (!gl_state)
    gl_state = new state(log_v2::instance().get(log_v2::CONFIG));
}

/**
 * @brief Returns if the state instance is already loaded.
 *
 * @return a boolean.
 */
bool state::loaded() {
  return gl_state;
}

/**
 *  Get the poller ID.
 *
 *  @return Poller ID of this Broker instance.
 */
uint32_t state::poller_id() const noexcept {
  return _poller_id;
}

/**
 *  Get the poller name.
 *
 *  @return Poller name of this Broker instance.
 */
const std::string& state::poller_name() const noexcept {
  return _poller_name;
}

/**
 * @brief Get the Engine configuration directory. Available on an Engine cbmod.
 *
 * @return The Engine configuration directory.
 */
const std::filesystem::path& state::engine_conf_dir() const {
  return _engine_conf_dir;
}

/**
 * @brief Set the Engine configuration directory. Available on an Engine cbmod.
 *
 * @param engine_conf_dir The directory to set.
 */
void state::set_engine_conf_dir(const std::string& engine_conf_dir) {
  _engine_conf_dir = engine_conf_dir;
}

/**
 * @brief Get the thread pool size.
 *
 * @return Number of threads in the pool or 0 which means the number of threads
 * will be computed as max(2, number of CPUs / 2).
 */
size_t state::pool_size() const noexcept {
  return _pool_size;
}

/**
 *  Unload singleton.
 */
void state::unload() {
  delete gl_state;
  gl_state = nullptr;
}

config::applier::modules& state::get_modules() {
  return _modules;
}

config::applier::state::stats& state::mut_stats_conf() {
  return _stats_conf;
}

const config::applier::state::stats& state::stats_conf() {
  return _stats_conf;
}

/**
 * @brief Add a poller to the list of connected pollers.
 *
 * @param poller_id The id of the poller (an id by host)
 * @param poller_name The name of the poller
 */
void state::add_peer(PeerType type,
                     uint64_t poller_id,
                     const std::string& poller_name,
                     const std::string& version_conf) {
  std::lock_guard<std::mutex> lck(_connected_peers_m);
  auto logger = log_v2::instance().get(log_v2::CORE);
  auto found = _connected_peers.find(poller_id);
  if (found == _connected_peers.end()) {
    logger->info("Poller '{}' with id {} connected", poller_name, poller_id);
    _connected_peers[poller_id] = peer(type, poller_name, version_conf);
  } else {
    logger->warn(
        "Poller '{}' with id {} already known as connected. Replacing it "
        "with '{}'",
        _connected_peers[poller_id].name(), poller_id, poller_name);
    peer& peer = found->second;
    peer.set_type(type);
    peer.set_name(poller_name);
    peer.set_engine_configuration_version(version_conf);
  }
}

/**
 * @brief Remove a poller from the list of connected pollers.
 *
 * @param poller_id The id of the poller to remove.
 */
void state::remove_poller(uint64_t poller_id) {
  std::lock_guard<std::mutex> lck(_connected_peers_m);
  auto logger = log_v2::instance().get(log_v2::CORE);
  auto found = _connected_peers.find(poller_id);
  if (found == _connected_peers.end())
    logger->warn("There is currently no poller {} connected", poller_id);
  else {
    logger->info("Poller '{}' with id {} just disconnected",
                 _connected_peers[poller_id].name(), poller_id);
    _connected_peers.erase(found);
  }
}

/**
 * @brief Check if a poller is currently connected.
 *
 * @param poller_id The poller to check.
 */
bool state::has_connection_from_poller(uint64_t poller_id) const {
  std::lock_guard<std::mutex> lck(_connected_peers_m);
  return _connected_peers.contains(poller_id);
}

/**
 * @brief This method is called from cbmod when the instance message is sent.
 * In that case, we are only interested by the peers of type INPUT. And we
 * should only have one.
 *
 * @return The Engine configuration version known by the input peer or an empty
 * string.
 */
std::string state::known_engine_conf() const {
  std::lock_guard<std::mutex> lck(_connected_peers_m);
  for (auto& p : _connected_peers) {
    if (p.second.type() == INPUT)
      return p.second.engine_configuration_version();
  }
  return "";
}

/**
 * @brief This method is called from an input. The peer is of type OUTPUT.
 * This peer is set as synchronized.
 */
void state::synchronize_peer(uint64_t poller_id) {
  std::lock_guard<std::mutex> lck(_connected_peers_m);
  auto it = _connected_peers.find(poller_id);
  if (it != _connected_peers.end() && it->second.type() == OUTPUT)
    it->second.set_synchronized(true);
}

/**
 * @brief This method is called from cbmod. So the peer is of type INPUT and
 * there is only one. This peer is set as synchronized.
 */
void state::synchronize_peer() {
  std::lock_guard<std::mutex> lck(_connected_peers_m);
  for (auto& p : _connected_peers) {
    if (p.second.type() == INPUT)
      p.second.set_synchronized(true);
  }
}

/**
 * @brief Returns a copy of the connected peers. The copy is because of the
 * concurrent accesses.
 *
 * @return A map of the connected peers.
 */
absl::flat_hash_map<uint64_t, peer> state::peers() const {
  std::lock_guard<std::mutex> lck(_connected_peers_m);
  return _connected_peers;
}

/**
 * @brief Get the pollers configuration directory used by the php if available.
 * Otherwise returns an empty string.
 *
 * @return A directory name or an empty string.
 */
const std::filesystem::path& state::pollers_conf_dir() const noexcept {
  return _pollers_conf_dir;
}

/**
 * @brief Set the pollers configuration directory used by the php.
 *
 * @param dir The name of the directory to set.
 */
void state::set_pollers_conf_dir(const std::string& dir) {
  _pollers_conf_dir = dir;
}

/**
 * @brief Get the local pollers configuration directory used by Broker and that
 * is a copy of the php cache.
 * Otherwise returns an empty string.
 *
 * @return A directory name or an empty string.
 */
const std::filesystem::path& state::local_pollers_conf_dir() const {
  return _local_pollers_conf_dir;
}
