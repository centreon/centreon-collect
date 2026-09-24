/**
 * Copyright 2026 Centreon
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

#ifndef CENTREON_AGENT_AGENT_INFO_REFRESHER_HH
#define CENTREON_AGENT_AGENT_INFO_REFRESHER_HH

#include "agent.pb.h"

namespace com::centreon::agent {

/**
 * @brief AgentInfo is sent once, at connection. Some of its fields (ips) may
 * change while the connection lives, so this object collects them
 * periodically and sends an info_update message when they differ from the
 * last sent ones.
 * Nothing is sent until set_last_sent has been called, i.e. while not
 * connected.
 */
class agent_info_refresher
    : public std::enable_shared_from_this<agent_info_refresher> {
 public:
  using collector = std::function<void(AgentInfo*)>;
  using sender = std::function<void(const std::shared_ptr<MessageFromAgent>&)>;

 private:
  const std::shared_ptr<boost::asio::io_context> _io_context;
  const std::shared_ptr<spdlog::logger> _logger;
  const std::chrono::system_clock::duration _period;
  const collector _collector;
  const sender _sender;

  mutable absl::Mutex _protect;
  boost::asio::system_timer _timer ABSL_GUARDED_BY(_protect);
  std::unique_ptr<AgentInfo> _last_sent ABSL_GUARDED_BY(_protect);
  bool _stopped ABSL_GUARDED_BY(_protect) = false;

  void _start_timer() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_protect);
  void _on_timer(const boost::system::error_code& err);

 public:
  using pointer = std::shared_ptr<agent_info_refresher>;

  static constexpr std::chrono::minutes default_period{5};

  agent_info_refresher(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::chrono::system_clock::duration& period,
      collector&& collect,
      sender&& send);

  static pointer load(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::chrono::system_clock::duration& period,
      collector&& collect,
      sender&& send);

  void set_last_sent(const AgentInfo& sent);

  void stop();
};

}  // namespace com::centreon::agent

#endif
