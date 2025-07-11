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

#include "com/centreon/broker/config/applier/endpoint.hh"
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
    _cache_dir.append(PREFIX_VAR);
  _cache_dir.append("/");
  _cache_dir.append(s.broker_name());

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
void state::add_poller(uint64_t poller_id, const std::string& poller_name) {
  std::lock_guard<std::mutex> lck(_connected_pollers_m);
  auto logger = log_v2::instance().get(log_v2::CORE);
  auto found = _connected_pollers.find(poller_id);
  if (found == _connected_pollers.end()) {
    logger->info("Poller '{}' with id {} connected", poller_name, poller_id);
    _connected_pollers[poller_id] = poller_name;
  } else {
    logger->warn(
        "Poller '{}' with id {} already known as connected. Replacing it "
        "with '{}'",
        _connected_pollers[poller_id], poller_id, poller_name);
    found->second = poller_name;
  }
}

/**
 * @brief Remove a poller from the list of connected pollers.
 *
 * @param poller_id The id of the poller to remove.
 */
void state::remove_poller(uint64_t poller_id) {
  std::lock_guard<std::mutex> lck(_connected_pollers_m);
  auto logger = log_v2::instance().get(log_v2::CORE);
  auto found = _connected_pollers.find(poller_id);
  if (found == _connected_pollers.end())
    logger->warn("There is currently no poller {} connected", poller_id);
  else {
    logger->info("Poller '{}' with id {} just disconnected",
                 _connected_pollers[poller_id], poller_id);
    _connected_pollers.erase(found);
  }
}

/**
 * @brief Check if a poller is currently connected.
 *
 * @param poller_id The poller to check.
 */
bool state::has_connection_from_poller(uint64_t poller_id) const {
  std::lock_guard<std::mutex> lck(_connected_pollers_m);
  return _connected_pollers.contains(poller_id);
}
