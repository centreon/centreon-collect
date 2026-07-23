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

#ifndef CCB_CONFIG_APPLIER_CBMOD_STATE_HH
#define CCB_CONFIG_APPLIER_CBMOD_STATE_HH
#include <deque>

#include "bbdo/bbdo.pb.h"
#include "broker/core/config/applier/state.hh"

namespace com::centreon::broker::config::applier {
class cbmod_state : public state {
 public:
  /* For an Engine, peers can only be Brokers. */
  struct peer {
    uint64_t poller_id;
    std::string poller_name;
    std::string broker_name;
    time_t connected_since;
    /* Is it a broker or an unknown peer which is also a broker but without
     * extended negotiation? */
    common::PeerType peer_type;
    /* Does the peer support extended negotiation? */
    bool extended_negotiation;
    /* The current Engine configuration known by this poller. Only available
     * for an Engine peer. */
    std::string engine_conf;
  };

 private:
  std::string _engine_conf;
  std::filesystem::path _proto_conf;
  /* This map is indexed by the tuple {poller_id, poller_name, broker_name}. */
  absl::btree_map<std::tuple<uint64_t, std::string, std::string>, peer>
      _connected_peers ABSL_GUARDED_BY(_connected_peers_m);
  mutable absl::Mutex _connected_peers_m;
  std::atomic_bool _diff_state_applied;
  mutable absl::Mutex _diff_state_m;
  std::unique_ptr<com::centreon::engine::configuration::DiffState> _diff_state;
  std::unique_ptr<com::centreon::engine::configuration::State>
      _current_engine_state;
  /* Notification-execute events pushed by Broker (notification_mode=broker):
   * Broker made the decision and dispatched the execution here; the Engine event
   * loop drains this queue and runs the notification commands. Several may be
   * pending between two loop iterations, hence a queue and not a single slot. */
  std::deque<NotificationExecute> _pending_notifications
      ABSL_GUARDED_BY(_pending_notifications_m);
  mutable absl::Mutex _pending_notifications_m;

 public:
  cbmod_state(const std::string& engine_conf_version,
              const std::shared_ptr<spdlog::logger>& logger);
  void set_engine_conf(const std::string& engine_conf);
  const std::string& engine_conf() const;
  void add_peer(uint64_t poller_id,
                const std::string& poller_name,
                const std::string& broker_name,
                common::PeerType peer_type,
                bool extended_negotiation,
                const std::string& engine_conf,
                const std::string& timezone) override;
  void remove_peer(uint64_t poller_id,
                   const std::string& poller_name,
                   const std::string& broker_name) override;
  bool broker_peer_supports_extended_negotiation() const;
  bool has_connection_from_poller(uint64_t poller_id) const override
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  std::vector<peer> connected_peers() const
      ABSL_LOCKS_EXCLUDED(_connected_peers_m);
  void set_diff_state(const std::shared_ptr<io::data>& diff);
  std::unique_ptr<com::centreon::engine::configuration::DiffState> diff_state();
  void set_diff_state_applied(bool done);
  void push_notification_execute(const std::shared_ptr<io::data>& ne)
      ABSL_LOCKS_EXCLUDED(_pending_notifications_m);
  std::vector<NotificationExecute> drain_notification_executes()
      ABSL_LOCKS_EXCLUDED(_pending_notifications_m);
  /**
   * @brief Check if the diff state has been applied. This method is called from
   * Engine. It must return true if the diff state has been applied but only
   * once.
   *
   * @return a boolean.
   */
  bool diff_state_applied() {
    bool expected = true;
    return _diff_state_applied.compare_exchange_strong(expected, false);
  }

  void set_proto_conf(const std::filesystem::path& proto_conf);
  const std::filesystem::path& proto_conf() const;
  bool supports_centralized_conf() const override {
    return !_proto_conf.empty();
  }
  void set_current_engine_conf(
      std::unique_ptr<com::centreon::engine::configuration::State>& conf);
  std::unique_ptr<com::centreon::engine::configuration::State>
  current_engine_conf();
};
}  // namespace com::centreon::broker::config::applier

#endif  // CCB_CONFIG_APPLIER_CBMOD_STATE_HH
