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

#include "broker/core/bbdo/remote.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bbdo;

/**
 * @brief Constructor.
 *
 * @param info Peer of the stream owning this channel.
 * @param max_bytes Maximum size of the queued events, push() fails above.
 */
remote_channel::remote_channel(const peer_info& info, size_t max_bytes)
    : _info(info), _max_bytes(max_bytes) {}

/**
 * @brief Queue an event to be written by the stream owner.
 *
 * @param d The event.
 * @param size_hint Approximate size of the event, used for the bound.
 *
 * @return false if the channel is closed or full.
 */
bool remote_channel::push(std::shared_ptr<io::data> d, size_t size_hint) {
  absl::MutexLock lck(&_m);
  if (_closed || (_bytes + size_hint > _max_bytes && !_queue.empty()))
    return false;
  _bytes += size_hint;
  _queue.push_back(std::move(d));
  return true;
}

/**
 * @brief Take every queued event. Called by the stream owner only.
 *
 * @return The events, in push order.
 */
std::deque<std::shared_ptr<io::data>> remote_channel::pop_all() {
  std::deque<std::shared_ptr<io::data>> retval;
  absl::MutexLock lck(&_m);
  std::swap(retval, _queue);
  _bytes = 0u;
  return retval;
}

/**
 * @brief Approximate size of the queued events.
 */
size_t remote_channel::queued_bytes() const {
  absl::MutexLock lck(&_m);
  return _bytes;
}

/**
 * @brief Mark the channel closed: the stream is gone, pending events are
 * dropped and further push() fail.
 */
void remote_channel::close() {
  absl::MutexLock lck(&_m);
  _closed = true;
  _queue.clear();
  _bytes = 0u;
}

/**
 * @brief Tell if the channel is closed.
 */
bool remote_channel::closed() const {
  absl::MutexLock lck(&_m);
  return _closed;
}

/**
 * @brief The registry of this process.
 */
remote_registry& remote_registry::instance() {
  static remote_registry instance;
  return instance;
}

/**
 * @brief Set the configuration of this process.
 *
 * @param remote_actions This process accepts downstream events.
 * @param insecure Accept them even without a verified TLS peer (PoC only).
 */
void remote_registry::set_config(bool remote_actions, bool insecure) {
  absl::MutexLock lck(&_m);
  _remote_actions = remote_actions;
  _insecure = insecure;
}

/**
 * @brief This process accepts downstream events.
 */
bool remote_registry::remote_actions() const {
  absl::MutexLock lck(&_m);
  return _remote_actions;
}

/**
 * @brief Downstream events are accepted without a verified TLS peer.
 */
bool remote_registry::insecure() const {
  absl::MutexLock lck(&_m);
  return _insecure;
}

/**
 * @brief Register the channel of a negotiated stream. A newer stream of the
 * same peer replaces the previous one.
 */
void remote_registry::add(const std::shared_ptr<remote_channel>& channel) {
  absl::MutexLock lck(&_m);
  _channels[{channel->info().poller_id,
             static_cast<int>(channel->info().peer_type)}] = channel;
}

/**
 * @brief Unregister the channel of a stream that goes away. Nothing is done
 * if a newer stream of the same peer already replaced it. The disconnect
 * handler is called for an Engine peer so that its pending requests fail at
 * once.
 */
void remote_registry::remove(const std::shared_ptr<remote_channel>& channel) {
  disconnect_handler h;
  uint64_t poller_id = channel->info().poller_id;
  {
    absl::MutexLock lck(&_m);
    auto it = _channels.find(
        {poller_id, static_cast<int>(channel->info().peer_type)});
    if (it == _channels.end() || it->second != channel)
      return;
    _channels.erase(it);
    if (channel->info().peer_type == common::ENGINE)
      h = _disconnect_handler;
  }
  if (h)
    h(poller_id);
}

/**
 * @brief Find the channel of a peer.
 */
std::shared_ptr<remote_channel> remote_registry::find(
    uint64_t poller_id,
    common::PeerType type) const {
  absl::MutexLock lck(&_m);
  auto it = _channels.find({poller_id, static_cast<int>(type)});
  if (it == _channels.end())
    return nullptr;
  return it->second;
}

/**
 * @brief List the registered channels.
 */
std::vector<std::shared_ptr<remote_channel>> remote_registry::list() const {
  std::vector<std::shared_ptr<remote_channel>> retval;
  absl::MutexLock lck(&_m);
  retval.reserve(_channels.size());
  for (auto& p : _channels)
    retval.push_back(p.second);
  return retval;
}

void remote_registry::set_action_handler(action_handler&& h) {
  absl::MutexLock lck(&_m);
  _action_handler = std::move(h);
}

void remote_registry::set_result_handler(result_handler&& h) {
  absl::MutexLock lck(&_m);
  _result_handler = std::move(h);
}

void remote_registry::set_exec_result_handler(exec_result_handler&& h) {
  absl::MutexLock lck(&_m);
  _exec_result_handler = std::move(h);
}

/**
 * @brief Hand a RemoteExecResult to the exec result handler (central side).
 */
void remote_registry::on_exec_result(uint64_t poller_id,
                                     const RemoteExecResult& result) const {
  exec_result_handler h;
  {
    absl::MutexLock lck(&_m);
    h = _exec_result_handler;
  }
  if (h)
    h(poller_id, result);
}

void remote_registry::set_disconnect_handler(disconnect_handler&& h) {
  absl::MutexLock lck(&_m);
  _disconnect_handler = std::move(h);
}

/**
 * @brief Hand a downstream event to the action handler (poller side). The
 * handler must not block: it runs on the thread that owns the stream.
 *
 * @return false if no handler is registered.
 */
bool remote_registry::on_action(
    const std::shared_ptr<io::data>& d,
    const std::shared_ptr<remote_channel>& reply) const {
  action_handler h;
  {
    absl::MutexLock lck(&_m);
    h = _action_handler;
  }
  if (!h)
    return false;
  h(d, reply);
  return true;
}

/**
 * @brief Hand a RemoteResult to the result handler (central side).
 */
void remote_registry::on_result(uint64_t poller_id,
                                const RemoteResult& result) const {
  result_handler h;
  {
    absl::MutexLock lck(&_m);
    h = _result_handler;
  }
  if (h)
    h(poller_id, result);
}
