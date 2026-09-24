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

#include <google/protobuf/util/message_differencer.h>

#include "agent_info_refresher.hh"

using namespace com::centreon::agent;

/**
 * @brief Construct a new agent info refresher, use load instead
 *
 * @param io_context
 * @param logger
 * @param period delay between two collects
 * @param collect fills an AgentInfo with the current host information
 * @param send sends a message to engine
 */
agent_info_refresher::agent_info_refresher(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::chrono::system_clock::duration& period,
    collector&& collect,
    sender&& send)
    : _io_context(io_context),
      _logger(logger),
      _period(period),
      _collector(std::move(collect)),
      _sender(std::move(send)),
      _timer(*io_context) {}

/**
 * @brief construct an agent_info_refresher and start its timer
 */
agent_info_refresher::pointer agent_info_refresher::load(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::chrono::system_clock::duration& period,
    collector&& collect,
    sender&& send) {
  pointer ret = std::make_shared<agent_info_refresher>(
      io_context, logger, period, std::move(collect), std::move(send));
  absl::MutexLock l(ret->_protect);
  ret->_start_timer();
  return ret;
}

/**
 * @brief to call each time an init message is sent to engine, collected
 * information will be compared to this one
 *
 * @param sent AgentInfo sent in the init message
 */
void agent_info_refresher::set_last_sent(const AgentInfo& sent) {
  absl::MutexLock l(_protect);
  _last_sent = std::make_unique<AgentInfo>(sent);
}

void agent_info_refresher::stop() {
  absl::MutexLock l(_protect);
  _stopped = true;
  _timer.cancel();
}

void agent_info_refresher::_start_timer() {
  if (_stopped)
    return;
  _timer.expires_after(_period);
  _timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_on_timer(err);
      });
}

void agent_info_refresher::_on_timer(const boost::system::error_code& err) {
  if (err)
    return;

  std::shared_ptr<MessageFromAgent> to_send;
  {
    absl::MutexLock l(_protect);
    if (_stopped)
      return;
    if (_last_sent) {
      auto msg = std::make_shared<MessageFromAgent>();
      _collector(msg->mutable_info_update());
      if (!google::protobuf::util::MessageDifferencer::Equals(
              *_last_sent, msg->info_update())) {
        SPDLOG_LOGGER_INFO(_logger, "host information changed, send {}",
                           msg->info_update());
        *_last_sent = msg->info_update();
        to_send = std::move(msg);
      }
    }
    _start_timer();
  }
  /* sender takes the connection mutex that may be held by a thread calling
   * set_last_sent, so it must be called without _protect */
  if (to_send) {
    _sender(to_send);
  }
}
