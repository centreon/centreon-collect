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

#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/command_manager.hh"

#include <absl/synchronization/mutex.h>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <memory>
#include "centreon_agent/agent_stat.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

/**
 * @brief Construct a new agent stat::agent stat object don't use it, use load
 * instead
 *
 * @param io_context
 */
agent_stat::agent_stat(const std::shared_ptr<asio::io_context>& io_context)
    : _io_context(io_context), _send_timer(*io_context), _dirty(false) {}

/**
 * @brief static method to construct a agent_stat object
 *
 * @param context
 * @return agent_stat::pointer
 */
agent_stat::pointer agent_stat::load(
    const std::shared_ptr<asio::io_context>& io_context) {
  pointer ret = std::make_shared<agent_stat>(io_context);
  ret->_start_send_timer();
  return ret;
}

/**
 * @brief Construct a new agent stat::group by key::group by key object
 *
 * @param agent_info
 * @param reversed
 */
agent_stat::group_by_key::group_by_key(
    const com::centreon::agent::AgentInfo& agent_info,
    bool reversed)
    : std::tuple<unsigned, unsigned, unsigned, bool, std::string, std::string>(
          agent_info.centreon_version().major(),
          agent_info.centreon_version().minor(),
          agent_info.centreon_version().patch(),
          reversed,
          agent_info.os(),
          agent_info.os_version()) {}

/**
 * @brief Adds an agent to the agent statistics.
 *
 * This function adds an agent to the internal data structure that keeps track
 * of agent statistics. If the agent is not already present, it is added to the
 * data structure.
 *
 * @param agent_info The information about the agent to be added.
 * @param reversed A boolean flag indicating whether the agent is connected in
 * reverse mode
 * @param reactor A pointer to the reactor object associated with the agent
 */
void agent_stat::add_agent(const com::centreon::agent::AgentInfo& agent_info,
                           bool reversed,
                           const void* reactor) {
  group_by_key key(agent_info, reversed);
  absl::MutexLock l(&_protect);
  auto it = _data.find(key);
  if (it == _data.end()) {
    it = _data.emplace(key, agent_info_set()).first;
  }
  if (it->second.insert(reactor).second) {
    // The agent was added.
    _dirty = true;
  }
}

/**
 * @brief Removes an agent from the agent statistics.
 *
 * This function removes an agent from the internal data structure that keeps
 * track of agent statistics. If the agent is present, it is removed from the
 * data structure. If the set of agents for the given key becomes empty after
 * removal, the key is also removed from the data structure.
 *
 * @param agent_info The information about the agent to be removed.
 * @param reversed A boolean flag indicating whether the agent is connected in
 * reverse mode.
 * @param reactor The pointer to the reactor object that is removed.
 */
void agent_stat::remove_agent(const com::centreon::agent::AgentInfo& agent_info,
                              bool reversed,
                              const void* reactor) {
  group_by_key key(agent_info, reversed);
  absl::MutexLock l(&_protect);
  auto it = _data.find(key);
  if (it != _data.end()) {
    size_t erased = it->second.erase(reactor);
    if (it->second.empty()) {
      _data.erase(it);
    }
    if (erased) {
      // The agent was removed.
      _dirty = true;
    }
  }
}

/**
 * @brief When an agent connect or disconnect from engine, we send a message to
 * broker
 *
 */
void agent_stat::_on_stat_update() const {
  nebstruct_agent_stats_data stats;
  stats.data =
      std::make_unique<std::vector<nebstruct_agent_stats_data::cumul_data>>();
  stats.data->reserve(_data.size());
  for (const auto& agent : _data) {
    stats.data->emplace_back(std::get<0>(agent.first), std::get<1>(agent.first),
                             std::get<2>(agent.first), std::get<3>(agent.first),
                             std::get<4>(agent.first), std::get<5>(agent.first),
                             agent.second.size());
  }

  // we post all check results in the main thread
  auto fn =
      std::packaged_task<int(void)>([to_send = std::move(stats)]() mutable {
        broker_agent_stats(to_send);
        return OK;
      });
  command_manager::instance().enqueue(std::move(fn));
}

void agent_stat::_start_send_timer() {
  absl::MutexLock l(&_protect);
  _send_timer.expires_after(std::chrono::minutes(1));
  _send_timer.async_wait(
      [this, me = shared_from_this()](const boost::system::error_code& err) {
        _send_timer_handler(err);
      });
}

void agent_stat::_send_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  }
  {
    absl::MutexLock l(&_protect);
    if (_dirty) {
      _dirty = false;
      _on_stat_update();
    }
  }
  _start_send_timer();
}

/**
 * @brief to call on module unload
 *
 */
void agent_stat::stop_send_timer() {
  absl::MutexLock l(&_protect);
  _send_timer.cancel();
}
