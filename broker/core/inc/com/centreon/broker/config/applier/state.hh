/**
 * Copyright 2011-2012, 2021-2024 Centreon
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

#include <absl/container/flat_hash_map.h>

#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/config/applier/peer.hh"
#include "com/centreon/broker/config/state.hh"

namespace com::centreon::broker::config::applier {

/**
 *  @class state state.hh "com/centreon/broker/config/applier/state.hh"
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
  std::filesystem::path _cache_dir;
  uint32_t _poller_id;
  uint32_t _rpc_port;
  bbdo::bbdo_version _bbdo_version;
  std::string _poller_name;
  size_t _pool_size;

  /* In a cbmod configuration, this string contains the directory containing
   * the Engine configuration */
  std::filesystem::path _engine_conf_dir;

  /* In a Broker Central, currently we need the cache directory used by the PHP
   * to generate all the pollers configurations. */
  std::filesystem::path _pollers_conf_dir;

  /* There is also the local pollers configuration directory where broker copies
   * the pollers configuration. It is safer to work in a separate directory. */
  std::filesystem::path _local_pollers_conf_dir;

  modules _modules;

  static stats _stats_conf;

  /* list of peers connected to this. They are not always pollers since this can
   * be a poller and so a peer would be a broker. And for a broker we could
   * also have map instances. */
  absl::flat_hash_map<uint64_t, peer> _connected_peers;
  mutable std::mutex _connected_peers_m;

  state(const std::shared_ptr<spdlog::logger>& logger);
  ~state() noexcept = default;

 public:
  static state& instance();
  static void load();
  static void unload();
  static bool loaded();

  state(const state&) = delete;
  state& operator=(const state&) = delete;
  void apply(const config::state& s, bool run_mux = true);
  const std::filesystem::path& cache_dir() const noexcept;
  uint32_t rpc_port() const noexcept;
  bbdo::bbdo_version get_bbdo_version() const noexcept;
  uint32_t poller_id() const noexcept;
  size_t pool_size() const noexcept;
  const std::string& poller_name() const noexcept;
  const std::filesystem::path& engine_conf_dir() const;
  void set_engine_conf_dir(const std::string& engine_conf_dir);
  const std::filesystem::path& local_pollers_conf_dir() const;
  const std::filesystem::path& pollers_conf_dir() const noexcept;
  void set_pollers_conf_dir(const std::string& dir);
  modules& get_modules();
  void add_peer(PeerType type,
                uint64_t poller_id,
                const std::string& poller_name,
                const std::string& version_conf);

  absl::flat_hash_map<uint64_t, peer> peers() const;
  void remove_poller(uint64_t poller_id);
  bool has_connection_from_poller(uint64_t poller_id) const;
  std::string known_engine_conf() const;
  void synchronize_peer(uint64_t poller_id);
  void synchronize_peer();
  static stats& mut_stats_conf();
  static const stats& stats_conf();
};
}  // namespace com::centreon::broker::config::applier

#endif  // !CCB_CONFIG_APPLIER_STATE_HH
