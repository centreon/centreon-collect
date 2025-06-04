/**
 * Copyright 2011-2013,2015-2016, 2020-2025 Centreon
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
#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>
#include <cstring>
#include <filesystem>
#include <system_error>

#include "bbdo/internal.hh"
#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/vars.hh"
#include "com/centreon/common/file.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/engine_conf/indexed_state.hh"
#include "common/engine_conf/parser.hh"
#include "common/log_v2/log_v2.hh"
#include "state.pb.h"

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
             const std::string& engine_conf_version,
             const std::shared_ptr<spdlog::logger>& logger)
    : _peer_type{peer_type},
      _engine_conf{engine_conf_version},
      _logger{logger},
      _poller_id(0),
      _rpc_port(0),
      _bbdo_version{2u, 0u, 0u},
      _watch_occupied{false},
      _modules{logger},
      _center{std::make_shared<com::centreon::broker::stats::center>()},
      _diff_state_applied{false} {}

/**
 * @brief Destructor of the state class.
 */
state::~state() noexcept {
  if (_watch_engine_conf_timer) {
    try {
      _watch_engine_conf_timer->cancel();
    } catch (const std::exception& e) {
      _logger->error("Cannot cancel watch engine conf timer: {}", e.what());
    }
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
void state::load(common::PeerType peer_type,
                 const std::string& engine_conf_version) {
  if (!gl_state)
    gl_state = new state(peer_type, engine_conf_version,
                         log_v2::instance().get(log_v2::CONFIG));
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
                     bool extended_negotiation,
                     const std::string& engine_conf) {
  assert(poller_id && !broker_name.empty());
  absl::WriterMutexLock lck(&_connected_peers_m);
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
      peer{poller_id,     poller_name, broker_name,
           time(nullptr), peer_type,   extended_negotiation,
           engine_conf,   engine_conf, true};
  if (extended_negotiation && _peer_type == common::BROKER) {
    if (!_watch_engine_conf_timer) {
      _watch_engine_conf_timer = std::make_unique<boost::asio::steady_timer>(
          com::centreon::common::pool::instance().io_context());
      _start_watch_engine_conf_timer();
    }
  }
}

/**
 * @brief Set the engine configuration for a poller.
 *
 * @param poller_id The poller ID.
 * @param engine_conf The new Engine configuration version.
 */
void state::set_poller_engine_conf(uint64_t poller_id,
                                   const std::string& poller_name,
                                   const std::string& broker_name,
                                   const std::string& engine_conf) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _connected_peers.find({poller_id, poller_name, broker_name});
  if (found == _connected_peers.end()) {
    _logger->info("Poller with id {} not found in connected peers", poller_id);
  } else {
    _logger->info("Poller with id {} has its version changed from '{}' to '{}'",
                  found->second.engine_conf, engine_conf);
    found->second.engine_conf = engine_conf;
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
  absl::WriterMutexLock lck(&_connected_peers_m);
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
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto lower = _connected_peers.lower_bound({poller_id, "", ""});
  for (auto end = _connected_peers.end();
       lower != end && lower->second.poller_id == poller_id; ++lower)
    if (lower->second.peer_type == common::ENGINE)
      return true;
  return false;
}

/**
 * @brief Get the list of connected pollers.
 *
 * @return A vector of pairs containing the poller id and the poller name.
 */
std::vector<state::peer> state::connected_peers() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<peer> retval;
  for (auto it = _connected_peers.begin(); it != _connected_peers.end(); ++it)
    retval.push_back(it->second);
  return retval;
}

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
      if (((event & IN_CREATE) || (event & IN_MODIFY)) &&
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
          acknowledge_engine_peer(poller_id, false);
          retval.push_back(poller_id);
        } else
          _logger->warn("Change in '{}' detected but poller id not found",
                        _cache_config_dir.string());
      }
    }
  }
  return retval;
}

/**
 * @brief If occupied is true, set the occupied flag to true if possible (we
 * cannot set it to true if it is already enabled). If occupied is false,
 * set the occupied flag to false.
 *
 * @param occupied True if we want to set the occupied flag to true, false if we
 * want to set it to false.
 *
 * @return True on success, false otherwise. An action linked to this occupied
 * flag should work only if this function returns true.
 */
bool state::set_engine_conf_watcher_occupied(bool occupied,
                                             const std::string_view& owner) {
  if (occupied) {
    bool expected = false;
    if (_watch_occupied.compare_exchange_strong(expected, true)) {
      _logger->trace("watcher taken by '{}'", owner);
      return true;
    } else
      return false;
  } else {
    _watch_occupied = false;
    _logger->trace("watcher released by '{}'", owner);
    return true;
  }
}

/**
 * @brief Check if all Engine peers acknowledged their configuration.
 * If it is the case, Broker can prepare the database for them.
 *
 * @return True if all Engine peers acknowledged their configuration,
 * false otherwise.
 */
bool state::all_engine_peers_acknowledged() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  for (const auto& peer : _connected_peers) {
    if (peer.second.peer_type == common::ENGINE &&
        !peer.second.conf_acknowledged)
      return false;
  }
  return true;
}

void state::_start_watch_engine_conf_timer() {
  _watch_engine_conf_timer->expires_after(std::chrono::seconds(5));
  _watch_engine_conf_timer->async_wait(
      [this](const boost::system::error_code& ec) {
        if (!ec) {
          if (set_engine_conf_watcher_occupied(true,
                                               "applier::state::watcher")) {
            _check_last_engine_conf();
            set_engine_conf_watcher_occupied(false, "applier::state::watcher");
          }
          _start_watch_engine_conf_timer();
        }
      });
}

void state::_check_last_engine_conf() {
  _logger->debug("Checking if there is a new engine configuration");
  auto pollers_vec = config::applier::state::instance()._watch_engine_conf();
  std::error_code ec;
  for (uint32_t poller_id : pollers_vec) {
    auto state = std::make_unique<engine::configuration::State>();
    engine::configuration::state_helper state_hlp(state.get());
    engine::configuration::error_cnt err;
    std::string version = common::hash_directory(
        config::applier::state::instance().cache_config_dir() /
            fmt::to_string(poller_id),
        ec);
    if (ec) {
      _logger->error(
          "Cannot compute the Engine configuration version for poller '{}': {}",
          poller_id, ec.message());
      continue;
    }
    engine::configuration::parser p;
    std::filesystem::path centengine_test =
        config::applier::state::instance().cache_config_dir() /
        fmt::to_string(poller_id) / "centengine.test";
    std::filesystem::path centengine_cfg =
        config::applier::state::instance().cache_config_dir() /
        fmt::to_string(poller_id) / "centengine.cfg";
    engine::configuration::parser::build_test_file(centengine_test,
                                                   centengine_cfg, ec);
    if (!ec) {
      try {
        p.parse(centengine_test, state.get(), err);
        state->set_config_version(version);
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
          state->SerializeToOstream(&f);
          f.close();
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
 * @brief Prepare the diff between the previous and the new Engine
 * configurations. The occupied flag must be enabled during this function.
 *
 * @param poller_id The poller ID.
 * @param state The new Engine configuration.
 */
void state::_prepare_diff_for_poller(
    uint64_t poller_id,
    std::unique_ptr<engine::configuration::State>&& state) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto lower = _connected_peers.lower_bound({poller_id, "", ""});
  for (auto end = _connected_peers.end();
       lower != end && lower->second.poller_id == poller_id; ++lower) {
    if (lower->second.peer_type == common::ENGINE) {
      if (lower->second.engine_conf != state->config_version()) {
        _logger->debug(
            "Poller '{}' with id {} has a new configuration available (old: "
            "'{}', new: '{}')",
            lower->second.poller_name, poller_id, lower->second.engine_conf,
            state->config_version());
        std::filesystem::path previous_prot_conf =
            pollers_config_dir() / fmt::format("{}.prot", poller_id);
        std::fstream f(previous_prot_conf);
        std::unique_ptr<engine::configuration::DiffState> diff_state;
        std::string new_version = state->config_version();
        if (f) {
          /* There is a previous configuration */
          auto previous_state =
              std::make_unique<engine::configuration::State>();
          previous_state->ParseFromIstream(&f);
          diff_state = std::make_unique<engine::configuration::DiffState>();
          auto previous_indexed_state =
              engine::configuration::indexed_state(std::move(previous_state));
          previous_indexed_state.diff_with_new_config(*state, _logger,
                                                      diff_state.get());
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

          /* The new configuration to send to the poller is new-<poller ID>.prot
           * Once sent to it, this file must be renamed into <poller ID>.prot
           * and the diff file can be removed. */
          lower->second.available_conf = new_version;
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
            lower->second.poller_name, poller_id, lower->second.engine_conf);
      }
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
bool state::engine_peer_needs_update(uint64_t poller_id) const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  auto lower = _connected_peers.lower_bound({poller_id, "", ""});
  for (auto end = _connected_peers.end();
       lower != end && lower->second.poller_id == poller_id; ++lower) {
    if (lower->second.peer_type == common::ENGINE) {
      return lower->second.available_conf != lower->second.engine_conf;
    }
  }
  return false;
}

/**
 * @brief Acknowledge or not the poller engine peer configuration. When true,
 * the poller is well up to date. When false, broker has a new configuration
 * and the poller did not send any acknowledgement.
 *
 * @param poller_id
 * @param ack
 */
void state::acknowledge_engine_peer(uint64_t poller_id, bool ack) {
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto lower = _connected_peers.lower_bound({poller_id, "", ""});
  for (auto end = _connected_peers.end();
       lower != end && lower->second.poller_id == poller_id; ++lower) {
    if (lower->second.peer_type == common::ENGINE) {
      lower->second.conf_acknowledged = ack;
      break;
    }
  }
}

/**
 * @brief Called on Engine side when a pb_diff_state is received to keep the
 * contained DiffState available for the next Engine update.
 *
 * @param diff The diff state.
 */
void state::set_diff_state(const std::shared_ptr<io::data>& diff) {
  assert(diff.unique());
  absl::MutexLock lck(&_diff_state_m);
  auto diff_state =
      std::static_pointer_cast<com::centreon::broker::bbdo::pb_diff_state>(
          diff);
  _diff_state =
      std::make_unique<com::centreon::engine::configuration::DiffState>();
  _diff_state->Swap(&diff_state->mut_obj());
}

std::unique_ptr<com::centreon::engine::configuration::DiffState>
state::diff_state() {
  absl::MutexLock lck(&_diff_state_m);
  return std::move(_diff_state);
}

void state::set_diff_state_applied(bool done) {
  _diff_state_applied = done;
}

/**
 * @brief Set the Engine configuration version. This method has sense only When
 * called from Engine. Broker instance does not have a configuration version.
 *
 * @param engine_conf The Engine configuration version.
 */
void state::set_engine_conf(const std::string& engine_conf) {
  _engine_conf = engine_conf;
}

/**
 * @brief Get the Engine configuration version. This method has sense only when
 * called from Engine. Broker instance does not have a configuration version.
 *
 * @return The Engine configuration version.
 */
const std::string& state::engine_conf() const {
  return _engine_conf;
}
