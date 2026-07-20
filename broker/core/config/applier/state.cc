/**
 * Copyright 2011-2013,2015-2016, 2020-2026 Centreon
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
#include <absl/time/time.h>
#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/cbmod_state.hh"

#include "bbdo/internal.hh"
#include "broker/core/config/applier/endpoint.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/vars.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

using log_v2 = com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker::config::applier {

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
      _poller_id(0),
      _rpc_port(0),
      _bbdo_version{2u, 0u, 0u},
      _modules{logger},
      _center{std::make_shared<com::centreon::broker::stats::center>()},
      _logger{logger} {}

/**
 * @brief Useful in unit tests to set a custom cache directory. The cache
 * directory is used to store the cache on disk, and to load it at startup. It
 * is also used to store the configuration cache when centralized configuration
 * is enabled.
 *
 * @param cache_dir The cache directory to set.
 */
void state::set_cache_dir(const std::filesystem::path& cache_dir) {
  _cache_dir = cache_dir;
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

  _cache_dir = cache_dir / s.broker_name();

  //  if (s.get_bbdo_version().major_v >= 3) {
  //    // Configuration cache directory (for broker, from php).
  //    set_cache_config_dir(s.cache_config_dir());
  //
  //    // Pollers configuration directory (for Broker).
  //    // If not provided in the configuration, use a default directory.
  //    if (!s.cache_config_dir().empty() && _pollers_config_dir.empty())
  //      set_pollers_config_dir(cache_dir / "pollers-configuration/");
  //    else
  //      set_pollers_config_dir(s.pollers_config_dir());
  //  }

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

  initialize_cache();
  // Event queue max size (used to limit memory consumption).
  com::centreon::broker::multiplexing::muxer::event_queue_max_size(
      s.event_queue_max_size());
  com::centreon::broker::multiplexing::muxer::priority_age_threshold(
      s.priority_age_threshold());

  com::centreon::broker::config::state st{s};

  // Apply input and output configuration.
  endpoint::instance().apply(st.endpoints(), st.params());

  // Enable multiplexing loop.
  if (run_mux)
    com::centreon::broker::multiplexing::engine::instance_ptr()->start();
}

void state::initialize_cache() {
  if (!_global_cache)
    _global_cache = std::make_unique<cache::broker_cache>(
        log_v2::instance().get(log_v2::CACHE));
}

/**
 * @brief Clear the cache. The cache is totally cleared and reinitialized.
 * This method is used in unit tests.
 */
void state::clear_cache() {
  _global_cache = std::make_unique<cache::broker_cache>(
      log_v2::instance().get(log_v2::CACHE));
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

#if defined(CBMOD_COMPILATION)
/**
 *  Load singleton.
 */
template <>
void state::load<cbmod_state>(const std::string& engine_conf_version) {
  if (!gl_state) {
    gl_state = new cbmod_state(engine_conf_version,
                               log_v2::instance().get(log_v2::CONFIG));
  }
}
#elif defined(BROKER_COMPILATION)
template <>
void state::load<broker_state>(const std::string& engine_conf_version
                               [[maybe_unused]]) {
  if (!gl_state) {
    gl_state = new broker_state(log_v2::instance().get(log_v2::CONFIG));
  }
}
#endif

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

modules& state::get_modules() {
  return _modules;
}

state::stats& state::mut_stats_conf() {
  return _stats_conf;
}

const state::stats& state::stats_conf() {
  return _stats_conf;
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

}  // namespace com::centreon::broker::config::applier
