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

#include <absl/container/btree_map.h>
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

 protected:
  std::shared_ptr<spdlog::logger> _logger;
  state(common::PeerType peer_type,
        const std::shared_ptr<spdlog::logger>& logger);
  virtual ~state() = default;

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
  virtual bool supports_centralized_conf() const { return false; }
  void initialize_cache();
  void clear_cache();
};

}  // namespace com::centreon::broker::config::applier

#endif  // !CCB_CONFIG_APPLIER_STATE_HH
