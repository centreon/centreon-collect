/*
** Copyright 2011-2012, 2021-2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_CONFIG_APPLIER_STATE_HH
#define CCB_CONFIG_APPLIER_STATE_HH

#include <absl/container/flat_hash_map.h>

#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/config/state.hh"

namespace com::centreon::broker {

namespace config {
namespace applier {
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
  std::string _cache_dir;
  uint32_t _poller_id;
  uint32_t _rpc_port;
  bbdo::bbdo_version _bbdo_version;
  std::string _poller_name;
  size_t _pool_size;
  modules _modules;

  static stats _stats_conf;

  absl::flat_hash_map<uint64_t, std::string> _connected_pollers;
  mutable std::mutex _connected_pollers_m;

  state();
  ~state() noexcept = default;

 public:
  static state& instance();
  static void load();
  static void unload();
  static bool loaded();

  state(const state&) = delete;
  state& operator=(const state&) = delete;
  void apply(const config::state& s, bool run_mux = true);
  const std::string& cache_dir() const noexcept;
  uint32_t rpc_port() const noexcept;
  bbdo::bbdo_version get_bbdo_version() const noexcept;
  uint32_t poller_id() const noexcept;
  size_t pool_size() const noexcept;
  const std::string& poller_name() const noexcept;
  modules& get_modules();
  void add_poller(uint64_t poller_id, const std::string& poller_name);
  void remove_poller(uint64_t poller_id);
  bool has_connection_from_poller(uint64_t poller_id) const;
  static stats& mut_stats_conf();
  static const stats& stats_conf();
};
}  // namespace applier
}  // namespace config

}  // namespace com::centreon::broker

#endif  // !CCB_CONFIG_APPLIER_STATE_HH
