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
    : _ticks_since_host_info_snapshot(0),
      _io_context(io_context),
      _send_timer(*io_context),
      _dirty(false) {}

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
  absl::MutexLock l(_protect);
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
  absl::MutexLock l(_protect);
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
  /* nothing is sent to broker: it forgets the information of a host not
   * refreshed, and a reconnecting agent may already have a new reactor */
  _host_infos.erase(reactor);
}

/**
 * @brief stores the host information of an agent (received in init or
 * info_update message) and sends it to broker
 *
 * @param agent_info
 * @param reactor connection that received agent_info
 */
void agent_stat::set_host_info(
    const com::centreon::agent::AgentInfo& agent_info,
    const void* reactor) {
  host_info to_store{agent_info, std::chrono::system_clock::now()};
  std::vector<host_info> to_send{to_store};
  {
    absl::MutexLock l(_protect);
    _host_infos[reactor] = std::move(to_store);
  }
  _send_host_infos(std::move(to_send));
}

/**
 * @brief post to the main thread the sending of host information to broker.
 * Host id is resolved from host name in the main thread; hosts unknown by
 * engine are ignored (they are reported by UnknownHost event)
 *
 * @param to_send
 */
void agent_stat::_send_host_infos(std::vector<host_info>&& to_send) {
  if (to_send.empty())
    return;
  auto fn = std::packaged_task<int(void)>([infos =
                                               std::move(to_send)]() mutable {
    for (const host_info& h : infos) {
      auto found = host::hosts.find(h.info.host());
      if (found == host::hosts.end())
        continue;
      com::centreon::broker::AgentHostInfo event;
      event.set_host_id(found->second->host_id());
      event.set_host_name(h.info.host());
      event.set_observed_at(std::chrono::duration_cast<std::chrono::seconds>(
                                h.observed_at.time_since_epoch())
                                .count());
      event.set_os_type(h.info.os_type());
      event.set_os_name(h.info.os_name());
      event.set_os_version(h.info.os_version());
      event.set_arch(h.info.arch());
      event.set_machine_id(h.info.machine_id());
      for (const std::string& ip : h.info.ips()) {
        event.add_ips(ip);
      }
      broker_agent_host_info(event);
    }
    return OK;
  });
  command_manager::instance().enqueue(std::move(fn));
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
  absl::MutexLock l(_protect);
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
  std::vector<host_info> host_infos;
  {
    absl::MutexLock l(_protect);
    if (_dirty) {
      _dirty = false;
      _on_stat_update();
    }
    /* unconditional snapshot: unlike stats, broker keeps host information
     * only in memory */
    if (++_ticks_since_host_info_snapshot >= host_info_snapshot_ticks) {
      _ticks_since_host_info_snapshot = 0;
      host_infos.reserve(_host_infos.size());
      for (const auto& [reactor, info] : _host_infos) {
        host_infos.push_back(info);
      }
    }
  }
  _send_host_infos(std::move(host_infos));
  _start_send_timer();
}

/**
 * @brief to call on module unload
 *
 */
void agent_stat::stop_send_timer() {
  absl::MutexLock l(_protect);
  _send_timer.cancel();
}
