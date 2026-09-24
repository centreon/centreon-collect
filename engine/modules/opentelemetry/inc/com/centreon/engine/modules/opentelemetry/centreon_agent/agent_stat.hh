/**
 * Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_STAT_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_STAT_HH

#include <absl/base/thread_annotations.h>
#include <boost/system/detail/error_code.hpp>
#include "centreon_agent/agent.pb.h"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

class agent_stat : public std::enable_shared_from_this<agent_stat> {
  using agent_info_set = std::set<const void*>;
  struct group_by_key
      : public std::tuple<
            unsigned /*agent major version*/,
            unsigned /*agent minor version*/,
            unsigned /*agent patch*/,
            bool /*reverse*/,
            std::string /*os almalinux, windows, windows-server...*/,
            std::string /*os version*/> {
   public:
    group_by_key(const com::centreon::agent::AgentInfo& agent_info,
                 bool reversed);
  };

  using agent_info_map = absl::flat_hash_map<group_by_key, agent_info_set>;

  agent_info_map _data ABSL_GUARDED_BY(_protect);

  /**
   * @brief last host information received from each connected agent, sent to
   * broker on change and every host_info_snapshot_ticks timer ticks so that a
   * restarted broker recovers it.
   */
  struct host_info {
    com::centreon::agent::AgentInfo info;
    std::chrono::system_clock::time_point observed_at;
  };
  absl::flat_hash_map<const void*, host_info> _host_infos
      ABSL_GUARDED_BY(_protect);
  unsigned _ticks_since_host_info_snapshot ABSL_GUARDED_BY(_protect);

  std::shared_ptr<asio::io_context> _io_context;
  asio::system_timer _send_timer ABSL_GUARDED_BY(_protect);
  bool _dirty ABSL_GUARDED_BY(_protect);

  mutable absl::Mutex _protect;

  void _on_stat_update() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);

  static void _send_host_infos(std::vector<host_info>&& to_send);

  void _start_send_timer();
  void _send_timer_handler(const boost::system::error_code& err);

 public:
  using pointer = std::shared_ptr<agent_stat>;

  /* the timer ticks every minute, so host information is re-sent every 5
   * minutes */
  static constexpr unsigned host_info_snapshot_ticks = 5;

  agent_stat(const std::shared_ptr<asio::io_context>& io_context);

  static pointer load(const std::shared_ptr<asio::io_context>& io_context);

  void stop_send_timer();

  void add_agent(const com::centreon::agent::AgentInfo& agent_info,
                 bool reversed,
                 const void* reactor);
  void remove_agent(const com::centreon::agent::AgentInfo& agent_info,
                    bool reversed,
                    const void* reactor);

  void set_host_info(const com::centreon::agent::AgentInfo& agent_info,
                     const void* reactor);
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent
#endif