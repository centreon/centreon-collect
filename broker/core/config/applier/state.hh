/**
 * Copyright 2011-2012, 2021-2026 Centreon
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

#ifndef CCB_CONFIG_APPLIER_STATE_HH
#define CCB_CONFIG_APPLIER_STATE_HH

#include <boost/asio/steady_timer.hpp>
#include "broker/core/cache/broker_cache.hh"
#include "broker/core/config/applier/modules.hh"
#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/file/directory_watcher.hh"
#include "common.pb.h"
#include "common/engine_conf/state_helper.hh"

namespace com::centreon::broker::config::applier {
/**
 *  @class state state.hh "broker/core/config/applier/state.hh"
 *  @brief Apply a configuration.
 *
 *  Apply some configuration state.
 */
class state {
 public:
  struct stats {
    uint32_t sql_slowest_statements_count = false;
    uint32_t sql_slowest_queries_count = false;
  };

 private:
  const common::PeerType _peer_type;
  std::string _cache_dir;
  uint32_t _poller_id;
  uint32_t _rpc_port;
  bbdo::bbdo_version _bbdo_version;
  std::string _poller_name;
  std::string _broker_name;
  size_t _pool_size;

  /* This is the Broker's global cache. */
  std::unique_ptr<com::centreon::broker::cache::broker_cache> _global_cache;

  modules _modules;

  std::shared_ptr<com::centreon::broker::stats::center> _center;

  static stats _stats_conf;

  /* --- Startup readiness barrier ---
   * On (re)start the multiplexing engine is not started until every output
   * stream created at startup has registered as ready (notify_output_ready),
   * so state restored on restart (BAM inherited downtimes, Lua state, ...) is
   * published only after each output has emitted its own startup definitions
   * and they have been flushed. A timeout releases the barrier anyway so the
   * process is never left mute. Shared by all state subclasses (cbd and cbmod);
   * only the post-release action differs (see _on_barrier_released()). */
  mutable absl::Mutex _barrier_m;
  bool _barrier_armed ABSL_GUARDED_BY(_barrier_m) = false;
  bool _barrier_released ABSL_GUARDED_BY(_barrier_m) = false;
  absl::flat_hash_set<std::string> _barrier_expected ABSL_GUARDED_BY(_barrier_m);
  absl::flat_hash_set<std::string> _barrier_ready ABSL_GUARDED_BY(_barrier_m);
  std::unique_ptr<boost::asio::steady_timer> _barrier_timer
      ABSL_GUARDED_BY(_barrier_m);
  void _maybe_release_barrier(bool forced) ABSL_LOCKS_EXCLUDED(_barrier_m);

 protected:
  std::shared_ptr<spdlog::logger> _logger;
  state(common::PeerType peer_type,
        const std::shared_ptr<spdlog::logger>& logger);
  virtual ~state() = default;

  /* Arm the startup readiness barrier at the end of apply() (or start the engine
   * immediately when there is nothing to wait for / in --check mode). */
  void _enable_multiplexing(bool run_mux);
  /* Hook invoked once, right after the engine is started when the barrier
   * releases. The base does nothing; broker_state re-injects persisted active
   * downtimes here so they are ordered after the flushed startup definitions. */
  virtual void _on_barrier_released() {}

 public:
  static state& instance();
  template <typename State>
  static void load(const std::string& engine_conf_version);
  static void unload();
  static bool loaded();

  state(const state&) = delete;
  state& operator=(const state&) = delete;
  void set_cache_dir(const std::filesystem::path& cache_dir);
  virtual void apply(const config::state& s, bool run_mux = true);
  virtual void add_peer(uint64_t poller_id,
                        const std::string& poller_name,
                        const std::string& broker_name,
                        common::PeerType peer_type,
                        bool extended_negotiation,
                        const std::string& engine_conf) = 0;
  virtual bool is_peer_conf_known(uint64_t poller_id [[maybe_unused]]) const {
    return true;
  }
  virtual void remove_peer(uint64_t poller_id,
                           const std::string& poller_name,
                           const std::string& broker_name) = 0;
  const std::string& cache_dir() const noexcept;
  uint32_t rpc_port() const noexcept;
  bbdo::bbdo_version get_bbdo_version() const noexcept;
  uint32_t poller_id() const noexcept;
  size_t pool_size() const noexcept;
  const std::string& broker_name() const noexcept;
  const std::string& poller_name() const noexcept;
  modules& get_modules();
  static stats& mut_stats_conf();
  static const stats& stats_conf();
  common::PeerType peer_type() const;
  std::shared_ptr<com::centreon::broker::stats::center> center() const;

  void set_engine_conf(const std::string& engine_conf);
  const std::string& engine_conf() const;
  com::centreon::broker::cache::broker_cache& cache() noexcept {
    assert(_global_cache);
    return *_global_cache;
  }
  virtual bool has_connection_from_poller(uint64_t poller_id) const = 0;
  virtual void set_instance_running(uint64_t /*poller_id*/,
                                    bool /*running*/) noexcept {}
  /* Called by an output stream's failover once it has completed its first
   * open()/load, to release the startup readiness barrier when all output
   * streams created at startup are ready. */
  void notify_output_ready(const std::string& endpoint_name);
  virtual bool supports_centralized_conf() const { return false; }
  void initialize_cache();
  void clear_cache();
};

}  // namespace com::centreon::broker::config::applier

#endif  // !CCB_CONFIG_APPLIER_STATE_HH
