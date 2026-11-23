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

#include "com/centreon/broker/neb/remote_actions.hh"

#include "broker/core/bbdo/internal.hh"
#include "broker/core/bbdo/remote.hh"
#include "com/centreon/engine/remote_bridge.hh"
#include "common/log_v2/log_v2.hh"

#include <absl/container/flat_hash_set.h>
#include <absl/synchronization/mutex.h>
#include <deque>
#include <future>

using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

namespace {

/* Request ids already handled, to drop a replayed request. */
constexpr size_t max_known_requests = 1000;
absl::Mutex g_known_m;
std::deque<std::string> g_known_order ABSL_GUARDED_BY(g_known_m);
absl::flat_hash_set<std::string> g_known ABSL_GUARDED_BY(g_known_m);

/**
 * @brief Remember a request id.
 *
 * @return false if it was already known.
 */
bool remember(const std::string& request_id) {
  absl::MutexLock lck(&g_known_m);
  if (!g_known.insert(request_id).second)
    return false;
  g_known_order.push_back(request_id);
  if (g_known_order.size() > max_known_requests) {
    g_known.erase(g_known_order.front());
    g_known_order.pop_front();
  }
  return true;
}

void reply(const std::shared_ptr<bbdo::remote_channel>& channel,
           const std::string& request_id,
           int32_t index,
           RemoteStatus status,
           const std::string& message) {
  auto r = std::make_shared<bbdo::pb_remote_result>();
  auto& obj = r->mut_obj();
  obj.set_request_id(request_id);
  obj.set_index(index);
  obj.set_status(status);
  obj.set_message(message);
  if (!channel->push(r, message.size()))
    log_v2::instance()
        .get(log_v2::NEB)
        ->warn("remote actions: cannot answer request '{}', stream closed",
               request_id);
}

/**
 * @brief Queue the commands on the Engine main loop; each one answers with
 * its own RemoteResult.
 */
void handle_command(const RemoteCommand& cmd,
                    const std::shared_ptr<bbdo::remote_channel>& channel) {
  auto logger = log_v2::instance().get(log_v2::NEB);
  const std::string& request_id = cmd.request_id();
  if (!remember(request_id)) {
    SPDLOG_LOGGER_INFO(logger,
                       "remote actions: request '{}' already handled, ignored",
                       request_id);
    return;
  }
  time_t deadline = cmd.deadline();
  if (deadline && time(nullptr) > static_cast<time_t>(deadline)) {
    reply(channel, request_id, -1, REMOTE_REJECTED,
          "request expired before reaching the poller");
    return;
  }
  SPDLOG_LOGGER_INFO(logger, "remote actions: request '{}' with {} commands",
                     request_id, cmd.commands_size());
  for (int i = 0; i < cmd.commands_size(); ++i) {
    com::centreon::engine::remote_bridge::run_external_command(
        cmd.commands(i),
        [channel, request_id, i](int code, const std::string& error) {
          /* On the Engine main loop, after execution. */
          if (code == 0)
            reply(channel, request_id, i, REMOTE_APPLIED, "");
          else
            reply(channel, request_id, i, REMOTE_REJECTED, error);
        });
  }
}

/**
 * @brief Action handler registered in the BBDO remote registry. Called on the
 * thread owning the stream: never blocks.
 */
void on_action(const std::shared_ptr<io::data>& d,
               const std::shared_ptr<bbdo::remote_channel>& channel) {
  switch (d->type()) {
    case bbdo::pb_remote_command::static_type():
      handle_command(
          std::static_pointer_cast<bbdo::pb_remote_command>(d)->obj(),
          channel);
      break;
    case bbdo::pb_config_chunk::static_type(): {
      const auto& obj =
          std::static_pointer_cast<bbdo::pb_config_chunk>(d)->obj();
      if (obj.index() == 0 && obj.last())
        reply(channel, obj.request_id(), -1, REMOTE_REJECTED,
              "configuration deployment not available on this poller yet");
    } break;
    case bbdo::pb_config_commit::static_type():
      reply(channel,
            std::static_pointer_cast<bbdo::pb_config_commit>(d)
                ->obj()
                .request_id(),
            -1, REMOTE_REJECTED,
            "configuration deployment not available on this poller yet");
      break;
    default:
      break;
  }
}

}  // namespace

/**
 * @brief Register the action handler of this poller.
 */
void neb::remote_actions::load() {
  bbdo::remote_registry::instance().set_action_handler(&on_action);
}

/**
 * @brief Unregister the action handler.
 */
void neb::remote_actions::unload() {
  bbdo::remote_registry::instance().set_action_handler(nullptr);
}

/**
 * @brief Outcome of an Engine reload (main loop or reload thread). Used by
 * configuration deployments.
 */
void neb::remote_actions::reload_done(bool success,
                                      const std::string& message) {
  auto logger = log_v2::instance().get(log_v2::NEB);
  SPDLOG_LOGGER_DEBUG(logger, "remote actions: reload done, success: {} {}",
                      success, message);
}
