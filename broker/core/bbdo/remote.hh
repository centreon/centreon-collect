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

#ifndef CCB_BBDO_REMOTE_HH
#define CCB_BBDO_REMOTE_HH

#include <absl/base/thread_annotations.h>
#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include "bbdo/bbdo.pb.h"
#include "bbdo/common.pb.h"
#include "com/centreon/broker/io/data.hh"

namespace com::centreon::broker::bbdo {

/**
 * @brief Outbound queue of downstream events (remote commands, configuration
 * chunks, results) attached to one negotiated BBDO stream.
 *
 * Any thread may push events; only the thread that owns the stream drains the
 * queue, in stream::read(), so that writes stay on the stream owner. These
 * events never go through the muxers, the retention files or the global
 * cache.
 */
class remote_channel {
 public:
  struct peer_info {
    uint64_t poller_id = 0u;
    std::string poller_name;
    std::string broker_name;
    common::PeerType peer_type = common::UNKNOWN;
    /* The peer announced it accepts downstream events. */
    bool remote_actions = false;
    /* The peer announced it runs RemoteExec requests. */
    bool remote_exec = false;
    /* The stream is encrypted and the peer certificate is verified. */
    bool tls = false;
    time_t connected_since = 0;
  };

 private:
  const peer_info _info;
  const size_t _max_bytes;
  mutable absl::Mutex _m;
  std::deque<std::shared_ptr<io::data>> _queue ABSL_GUARDED_BY(_m);
  size_t _bytes ABSL_GUARDED_BY(_m) = 0u;
  bool _closed ABSL_GUARDED_BY(_m) = false;

 public:
  remote_channel(const peer_info& info, size_t max_bytes = 64u << 20);
  remote_channel(const remote_channel&) = delete;
  remote_channel& operator=(const remote_channel&) = delete;

  const peer_info& info() const { return _info; }
  bool push(std::shared_ptr<io::data> d, size_t size_hint = 0u)
      ABSL_LOCKS_EXCLUDED(_m);
  std::deque<std::shared_ptr<io::data>> pop_all() ABSL_LOCKS_EXCLUDED(_m);
  size_t queued_bytes() const ABSL_LOCKS_EXCLUDED(_m);
  void close() ABSL_LOCKS_EXCLUDED(_m);
  bool closed() const ABSL_LOCKS_EXCLUDED(_m);
};

/**
 * @brief Process-wide registry of the negotiated BBDO peers and of the
 * handlers of downstream events.
 *
 * On the central cbd, the remote API finds the channel of a poller here and
 * receives RemoteResult events through the result handler. On a poller
 * (cbmod), the action handler executes RemoteCommand, ConfigChunk and
 * ConfigCommit and RemoteExec events and answers through the channel it
 * receives. RemoteExecResult events reach the exec result handler (central).
 */
class remote_registry {
 public:
  using action_handler =
      std::function<void(const std::shared_ptr<io::data>& d,
                         const std::shared_ptr<remote_channel>& reply)>;
  using result_handler =
      std::function<void(uint64_t poller_id, const RemoteResult& result)>;
  using exec_result_handler =
      std::function<void(uint64_t poller_id, const RemoteExecResult& result)>;
  using disconnect_handler = std::function<void(uint64_t poller_id)>;

 private:
  mutable absl::Mutex _m;
  absl::flat_hash_map<std::pair<uint64_t, int>,
                      std::shared_ptr<remote_channel>>
      _channels ABSL_GUARDED_BY(_m);
  action_handler _action_handler ABSL_GUARDED_BY(_m);
  result_handler _result_handler ABSL_GUARDED_BY(_m);
  exec_result_handler _exec_result_handler ABSL_GUARDED_BY(_m);
  disconnect_handler _disconnect_handler ABSL_GUARDED_BY(_m);

  /* Configuration of this process (Broker configuration keys
   * remote_actions / remote_actions_insecure). */
  bool _remote_actions ABSL_GUARDED_BY(_m) = false;
  bool _insecure ABSL_GUARDED_BY(_m) = false;

  remote_registry() = default;

 public:
  static remote_registry& instance();

  void set_config(bool remote_actions, bool insecure) ABSL_LOCKS_EXCLUDED(_m);
  bool remote_actions() const ABSL_LOCKS_EXCLUDED(_m);
  bool insecure() const ABSL_LOCKS_EXCLUDED(_m);

  void add(const std::shared_ptr<remote_channel>& channel)
      ABSL_LOCKS_EXCLUDED(_m);
  void remove(const std::shared_ptr<remote_channel>& channel)
      ABSL_LOCKS_EXCLUDED(_m);
  std::shared_ptr<remote_channel> find(uint64_t poller_id,
                                       common::PeerType type) const
      ABSL_LOCKS_EXCLUDED(_m);
  std::vector<std::shared_ptr<remote_channel>> list() const
      ABSL_LOCKS_EXCLUDED(_m);

  void set_action_handler(action_handler&& h) ABSL_LOCKS_EXCLUDED(_m);
  void set_result_handler(result_handler&& h) ABSL_LOCKS_EXCLUDED(_m);
  void set_exec_result_handler(exec_result_handler&& h)
      ABSL_LOCKS_EXCLUDED(_m);
  void set_disconnect_handler(disconnect_handler&& h) ABSL_LOCKS_EXCLUDED(_m);

  bool on_action(const std::shared_ptr<io::data>& d,
                 const std::shared_ptr<remote_channel>& reply) const
      ABSL_LOCKS_EXCLUDED(_m);
  void on_exec_result(uint64_t poller_id, const RemoteExecResult& result) const
      ABSL_LOCKS_EXCLUDED(_m);
  void on_result(uint64_t poller_id, const RemoteResult& result) const
      ABSL_LOCKS_EXCLUDED(_m);
};

}  // namespace com::centreon::broker::bbdo

#endif  // !CCB_BBDO_REMOTE_HH
