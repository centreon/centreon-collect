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
#include <absl/strings/match.h>
#include <absl/time/time.h>
#include <filesystem>

#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/vars.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/engine_conf/parser.hh"
#include "common/engine_conf/state_helper.hh"

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
state::state(common::PeerType peer_type,
             const std::shared_ptr<spdlog::logger>& logger)
    : _peer_type{peer_type},
      _logger{logger},
      _poller_id(0),
      _rpc_port(0),
      _bbdo_version{2u, 0u, 0u},
      _modules{logger},
      _center{std::make_shared<com::centreon::broker::stats::center>()} {}

/**
 * @brief Destructor of the state class.
 */
state::~state() noexcept {
  if (_watch_engine_conf_timer) {
    boost::system::error_code ec;
    _watch_engine_conf_timer->cancel(ec);
    if (ec)
      _logger->error("Cannot cancel watch engine conf timer: {}", ec.message());
  }
}

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
  _broker_name = s.broker_name();
  _poller_name = s.poller_name();
  _rpc_port = s.rpc_port();
  _bbdo_version = s.get_bbdo_version();

  // Thread pool size.
  _pool_size = s.pool_size();

  // Set cache directory.
  std::filesystem::path cache_dir;
  if (s.cache_directory().empty())
    cache_dir = PREFIX_VAR;
  else
    cache_dir = s.cache_directory();

  _cache_dir = cache_dir.string() + "/" + s.broker_name();

  if (s.get_bbdo_version().major_v >= 3) {
    // Engine configuration directory (for cbmod).
    //    if (!s.engine_config_dir().empty())
    //      set_engine_config_dir(s.engine_config_dir());

    // Configuration cache directory (for broker, from php).
    set_cache_config_dir(s.cache_config_dir());

    // Pollers configuration directory (for Broker).
    // If not provided in the configuration, use a default directory.
    if (!s.cache_config_dir().empty() && _pollers_config_dir.empty())
      set_pollers_config_dir(cache_dir / "pollers-configuration/");
    else
      set_pollers_config_dir(s.pollers_config_dir());
  }

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
  endpoint::instance().apply(st.endpoints(), st.params());

  // Enable multiplexing loop.
  if (run_mux)
    com::centreon::broker::multiplexing::engine::instance_ptr()->start();
}

/**
 *  Get applied cache directory.
 *
 *  @return Cache directory.
 */
const std::string& state::cache_dir() const noexcept {
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
void state::load(common::PeerType peer_type) {
  if (!gl_state)
    gl_state = new state(peer_type, log_v2::instance().get(log_v2::CONFIG));
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
 *  Get the broker name.
 *
 *  @return Broker name of this Broker instance.
 */
const std::string& state::broker_name() const noexcept {
  return _broker_name;
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
 * @param broker_name The name of the poller
 */
void state::add_peer(uint64_t poller_id,
                     const std::string& poller_name,
                     const std::string& broker_name,
                     common::PeerType peer_type,
                     bool extended_negotiation) {
  assert(poller_id && !broker_name.empty());
  absl::MutexLock lck(&_connected_peers_m);
  auto found = _connected_peers.find({poller_id, poller_name, broker_name});
  if (found == _connected_peers.end()) {
    _logger->info("Poller '{}' with id {} connected", broker_name, poller_id);
  } else {
    _logger->warn(
        "Poller '{}' with id {} already known as connected. Replacing it.",
        broker_name, poller_id);
    _connected_peers.erase(found);
  }
  _connected_peers[{poller_id, poller_name, broker_name}] =
      peer{poller_id, poller_name,          broker_name, time(nullptr),
           peer_type, extended_negotiation, true,        false};
  if (extended_negotiation && _peer_type == common::BROKER) {
    if (!_watch_engine_conf_timer) {
      _watch_engine_conf_timer = std::make_unique<boost::asio::steady_timer>(
          com::centreon::common::pool::instance().io_context());
      _start_watch_engine_conf_timer();
    }
  }
}

/**
 * @brief Remove a poller from the list of connected pollers.
 *
 * @param poller_id The id of the poller to remove.
 */
void state::remove_peer(uint64_t poller_id,
                        const std::string& poller_name,
                        const std::string& broker_name) {
  assert(poller_id && !broker_name.empty());
  absl::MutexLock lck(&_connected_peers_m);
  auto found = _connected_peers.find({poller_id, poller_name, broker_name});
  if (found != _connected_peers.end()) {
    _logger->info("Peer poller: '{}' - broker: '{}' with id {} disconnected",
                  poller_name, broker_name, poller_id);
    _connected_peers.erase(found);
  } else {
    _logger->warn(
        "Peer poller: '{}' - broker: '{}' with id {} and type '{}' not found "
        "in connected peers",
        poller_name, broker_name, poller_id);
  }
}

/**
 * @brief Check if a poller is currently connected.
 *
 * @param poller_id The poller to check.
 */
bool state::has_connection_from_poller(uint64_t poller_id) const {
  absl::MutexLock lck(&_connected_peers_m);
  for (auto& p : _connected_peers)
    if (p.second.poller_id == poller_id && p.second.peer_type == common::ENGINE)
      return true;
  return false;
}

/**
 * @brief Get the list of connected pollers.
 *
 * @return A vector of pairs containing the poller id and the poller name.
 */
std::vector<state::peer> state::connected_peers() const {
  absl::MutexLock lck(&_connected_peers_m);
  std::vector<peer> retval;
  for (auto it = _connected_peers.begin(); it != _connected_peers.end(); ++it)
    retval.push_back(it->second);
  return retval;
}

/**
 * @brief Get the Engine configuration directory.
 *
 * @return The Engine configuration directory.
 */
// const std::filesystem::path& state::engine_config_dir() const noexcept {
//   return _engine_config_dir;
// }
//
///**
// * @brief Set the Engine configuration directory.
// *
// * @param engine_conf_dir The Engine configuration directory.
// */
// void state::set_engine_config_dir(const std::filesystem::path& dir) {
//  _engine_config_dir = dir;
//}

/**
 * @brief Get the configuration cache directory used by php to write
 * pollers' configurations.
 *
 * @return The configuration cache directory.
 */
const std::filesystem::path& state::cache_config_dir() const noexcept {
  return _cache_config_dir;
}

/**
 * @brief Set the configuration cache directory.
 *
 * @param engine_conf_dir The configuration cache directory.
 */
void state::set_cache_config_dir(
    const std::filesystem::path& cache_config_dir) {
  _cache_config_dir = cache_config_dir;
  if (!_cache_config_dir.empty()) {
    _logger->info("Watching for changes in '{}'", _cache_config_dir.string());
    _cache_config_dir_watcher = std::make_unique<file::directory_watcher>(
        _cache_config_dir, IN_CREATE | IN_MODIFY, true);
  } else if (_cache_config_dir_watcher) {
    _logger->info("Stop watching for changes in '{}'",
                  _cache_config_dir.string());
    _cache_config_dir_watcher.reset();
  }
}

/**
 * @brief Get the pollers configurations directory.
 *
 * @return The pollers configurations directory.
 */
const std::filesystem::path& state::pollers_config_dir() const noexcept {
  return _pollers_config_dir;
}

/**
 * @brief Set the pollers configurations directory.
 *
 * @param pollers_config_dir The pollers configurations directory.
 */
void state::set_pollers_config_dir(
    const std::filesystem::path& pollers_config_dir) {
  _pollers_config_dir = pollers_config_dir;
}

/**
 * @brief Get the type of peer this state is defined for.
 *
 * @return A PeerType enum.
 */
com::centreon::common::PeerType state::peer_type() const {
  return _peer_type;
}

/**
 * @brief Specify if a broker needs an update. And then set the broker as ready
 * to receive data.
 *
 * @param poller_id The poller id.
 * @param broker_name The poller name.
 * @param peer_type The peer type.
 * @param need_update true if the broker needs an update, false otherwise.
 */
void state::set_broker_needs_update(uint64_t poller_id,
                                    const std::string& poller_name,
                                    const std::string& broker_name,
                                    common::PeerType peer_type,
                                    bool need_update) {
  absl::MutexLock lck(&_connected_peers_m);
  auto found = _connected_peers.find({poller_id, poller_name, broker_name});
  if (found != _connected_peers.end()) {
    found->second.needs_update = need_update;
    found->second.ready = true;
  } else {
    _logger->warn(
        "Poller '{}' with id {} and type '{}' not found in connected peers",
        broker_name, poller_id,
        common::PeerType_descriptor()->FindValueByNumber(peer_type)->name());
  }
}

/**
 * @brief Set all the connected peers as ready to receive data (no extended
 * negociation available).
 */
void state::set_peers_ready() {
  absl::MutexLock lck(&_connected_peers_m);
  for (auto& p : _connected_peers)
    p.second.ready = true;
}

/**
 * @brief Check if a broker needs an update.
 *
 * @param poller_id The poller id.
 * @param broker_name The poller name.
 * @param peer_type The peer type.
 *
 * @return true if the broker needs an update, false otherwise.
 */
bool state::broker_needs_update(uint64_t poller_id,
                                const std::string& poller_name,
                                const std::string& broker_name) const {
  auto found = _connected_peers.find({poller_id, poller_name, broker_name});
  if (found != _connected_peers.end())
    return found->second.needs_update;
  else
    return false;
}

/**
 * @brief Wait for 20 seconds for all Brokers to be ready and then check if at
 * least one broker needs an update.
 *
 * @return true if at least one broker needs an update, false otherwise.
 */
bool state::broker_needs_update() const {
  auto brokers_ready = [this] {
    for (auto& p : _connected_peers) {
      if (p.second.peer_type == common::BROKER && !p.second.ready)
        return false;
    }
    return true;
  };

  absl::MutexLock lck(&_connected_peers_m);
  // Let's wait for at most 20 seconds for all brokers to be ready.
  _connected_peers_m.AwaitWithTimeout(absl::Condition(&brokers_ready),
                                      absl::Seconds(20));

  // Now, we can check if they need some updates.
  for (auto& p : _connected_peers) {
    if (p.second.peer_type == common::BROKER && p.second.needs_update)
      return true;
  }
  return false;
}

/**
 * @brief The peer with the given poller_id has its engine configuration version
 * set to the given one.
 *
 * @param poller_id Poller ID concerned by the modification.
 * @param version The version to set.
 */
void state::set_engine_configuration(uint64_t poller_id,
                                     const std::string& version) {
  absl::MutexLock lck(&_connected_peers_m);
  _engine_configuration[poller_id] = version;
}

/**
 * @brief Get the engine configuration for a poller. On error an empty string is
 * returned.
 *
 * @param poller_id The poller id.
 *
 * @return The engine configuration as a string.
 */
std::string state::engine_configuration(uint64_t poller_id) const {
  absl::MutexLock lck(&_connected_peers_m);
  auto found = _engine_configuration.find(poller_id);
  if (found != _engine_configuration.end())
    return found->second;
  else
    return "";
}

/**
 * @brief Get the stats center.
 *
 * @return The stats center.
 */
std::shared_ptr<com::centreon::broker::stats::center> state::center() const {
  return _center;
}

/**
 * @brief Set the path to the Engine protobuf configuration directory.
 *
 * @param proto_conf
 */
void state::set_proto_conf(const std::filesystem::path& proto_conf) {
  _proto_conf = proto_conf;
}

/**
 * @brief Get the path to the Engine protobuf configuration directory.
 *
 * @return The path to the Engine protobuf configuration directory.
 */
const std::filesystem::path& state::proto_conf() const {
  return _proto_conf;
}

/**
 * @brief Check if some new engine configurations are available.
 *
 * @return A vector of poller IDs concerned by some new configuration or an
 * empty vector.
 */
std::vector<uint32_t> state::_watch_engine_conf() {
  std::vector<uint32_t> retval;
  if (_cache_config_dir_watcher) {
    auto it = _cache_config_dir_watcher->watch();
    for (auto end = _cache_config_dir_watcher->end(); it != end; ++it) {
      auto [event, name] = *it;
      std::string_view event_str;
      if (event & IN_CREATE)
        event_str = "IN_CREATE";
      else if (event & IN_MODIFY)
        event_str = "IN_MODIFY";
      else
        event_str = "UNKNOWN";
      _logger->debug("Change in '{}' detected: event '{}'", name, event_str);
      if (((event & IN_CREATE) || (event & IN_MODIFY)) &&
          absl::EndsWith(name, ".lck")) {
        std::string_view prefix(name.data(), name.size() - 4);
        uint32_t poller_id;
        if (absl::SimpleAtoi(prefix, &poller_id)) {
          _logger->info(
              "New Engine configuration available, change in '{}' detected "
              "for poller id '{}'",
              name, poller_id);
          retval.push_back(poller_id);
        } else
          _logger->warn("Change in '{}' detected but poller id not found",
                        _cache_config_dir.string());
      }
    }
  }
  return retval;
}

void state::_start_watch_engine_conf_timer() {
  _watch_engine_conf_timer->expires_from_now(std::chrono::seconds(5));
  _watch_engine_conf_timer->async_wait(
      [this](const boost::system::error_code& ec) {
        if (!ec) {
          _check_last_engine_conf();
          _start_watch_engine_conf_timer();
        }
      });
}

void state::_check_last_engine_conf() {
  _logger->debug("Checking if there is a new engine configuration");
  auto pollers_vec = config::applier::state::instance()._watch_engine_conf();
  for (uint32_t poller_id : pollers_vec) {
    engine::configuration::State state;
    engine::configuration::state_helper state_hlp(&state);
    engine::configuration::error_cnt err;
    engine::configuration::parser p;
    std::filesystem::path centengine_test =
        config::applier::state::instance().cache_config_dir() /
        fmt::to_string(poller_id) / "centengine.test";
    std::filesystem::path centengine_cfg =
        config::applier::state::instance().cache_config_dir() /
        fmt::to_string(poller_id) / "centengine.cfg";
    std::error_code ec;
    engine::configuration::parser::build_test_file(centengine_test,
                                                   centengine_cfg, ec);
    if (!ec) {
      try {
        p.parse(centengine_test, &state, err);
        state_hlp.expand(err);
        std::filesystem::path last_prot_conf =
            config::applier::state::instance().pollers_config_dir() /
            fmt::format("{}.prot", poller_id);
        std::ofstream f(last_prot_conf);
        if (f) {
          state.SerializeToOstream(&f);
          f.close();
        } else {
          _logger->error(
              "Cannot write the new Engine protobuf configuration '{}': {}",
              last_prot_conf.string(), ec.message());
        }
      } catch (const std::exception& e) {
        _logger->error("error while parsing poller {} Engine configuration: {}",
                       poller_id, e.what());
      }
    } else
      _logger->error("Cannot create Engine configuration test file '{}': {}",
                     centengine_test.string(), ec.message());
  }
}
