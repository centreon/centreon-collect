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
#include "broker/core/config/applier/cbmod_state.hh"
#include "bbdo/internal.hh"

namespace com::centreon::broker::config::applier {

cbmod_state::cbmod_state(const std::string& engine_conf_version,
                         const std::shared_ptr<spdlog::logger>& logger)
    : state(common::PeerType::ENGINE, logger),
      _engine_conf{engine_conf_version},
      _diff_state_applied{false} {}
/**
 * @brief Set the Engine configuration version. This method has sense only When
 * called from Engine. Broker instance does not have a configuration version.
 *
 * @param engine_conf The Engine configuration version.
 */
void cbmod_state::set_engine_conf(const std::string& engine_conf) {
  _engine_conf = engine_conf;
}

/**
 * @brief Get the Engine configuration version. This method has sense only when
 * called from Engine. Broker instance does not have a configuration version.
 *
 * @return The Engine configuration version.
 */
const std::string& cbmod_state::engine_conf() const {
  return _engine_conf;
}

/**
 * @brief Add a poller to the list of connected pollers.
 *
 * @param poller_id The id of the poller (an id by host)
 * @param broker_name The name of the poller
 */
void cbmod_state::add_peer(uint64_t poller_id,
                           const std::string& poller_name,
                           const std::string& broker_name,
                           common::PeerType peer_type,
                           bool extended_negotiation,
                           const std::string& engine_conf,
                           const std::string& timezone [[maybe_unused]]) {
  assert(poller_id && !broker_name.empty());
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _connected_peers.find({poller_id, poller_name, broker_name});
  if (found == _connected_peers.end()) {
    _logger->info("Poller '{}' with id {} connected", broker_name, poller_id);
    _connected_peers[{poller_id, poller_name, broker_name}] =
        peer{poller_id, poller_name,          broker_name, time(nullptr),
             peer_type, extended_negotiation, ""};
  } else {
    _logger->warn(
        "Poller '{}' with id {} already known as connected. Replacing it.",
        broker_name, poller_id);
    found->second.connected_since = time(nullptr);
    found->second.peer_type = peer_type;
    found->second.extended_negotiation = extended_negotiation;
    found->second.engine_conf = engine_conf;
    /* available_conf is already set. */
  }
}

/**
 * @brief Remove a poller from the list of connected pollers.
 *
 * @param poller_id The id of the poller to remove.
 */
void cbmod_state::remove_peer(uint64_t poller_id,
                              const std::string& poller_name,
                              const std::string& broker_name) {
  assert(poller_id && !broker_name.empty());
  absl::WriterMutexLock lck(&_connected_peers_m);
  auto found = _connected_peers.find({poller_id, poller_name, broker_name});
  if (found != _connected_peers.end()) {
    _logger->info("Peer poller: '{}' - broker: '{}' with id {} disconnected",
                  poller_name, broker_name, poller_id);
    _connected_peers.erase(found);
  } else {
    _logger->warn(
        "Peer poller: '{}' - broker: '{}' with id {} and type '{}' not found "
        "in connected peers",
        poller_name, broker_name, poller_id);
  }
}

/**
 * @brief Called from an Engine. This method tells if the connected Broker
 * supports extended negotiation.
 *
 * @return A boolean.
 */
bool cbmod_state::broker_peer_supports_extended_negotiation() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  for (auto& p : _connected_peers) {
    if (p.second.peer_type == common::BROKER)
      return p.second.extended_negotiation;
  }
  return false;
}

/**
 * @brief Check if a poller is currently connected.
 *
 * @param poller_id The poller to check.
 */
bool cbmod_state::has_connection_from_poller(uint64_t poller_id
                                             [[maybe_unused]]) const {
  return false;
}

/**
 * @brief Get the list of connected pollers.
 *
 * @return A vector of pairs containing the poller id and the poller name.
 */
std::vector<cbmod_state::peer> cbmod_state::connected_peers() const {
  absl::ReaderMutexLock lck(&_connected_peers_m);
  std::vector<peer> retval;
  for (auto it = _connected_peers.begin(); it != _connected_peers.end(); ++it)
    retval.push_back(it->second);
  return retval;
}

/**
 * @brief Set the path to the Engine protobuf configuration directory.
 *
 * @param proto_conf
 */
void cbmod_state::set_proto_conf(const std::filesystem::path& proto_conf) {
  _proto_conf = proto_conf;
}

/**
 * @brief Get the path to the Engine protobuf configuration directory.
 *
 * @return The path to the Engine protobuf configuration directory.
 */
const std::filesystem::path& cbmod_state::proto_conf() const {
  return _proto_conf;
}

/**
 * @brief Called on Engine side when a pb_diff_state is received to keep the
 * contained DiffState available for the next Engine update.
 *
 * @param diff The diff state.
 */
void cbmod_state::set_diff_state(const std::shared_ptr<io::data>& diff) {
  assert(diff.unique());
  absl::MutexLock lck(&_diff_state_m);
  auto diff_state =
      std::static_pointer_cast<com::centreon::broker::bbdo::pb_diff_state>(
          diff);
  _diff_state =
      std::make_unique<com::centreon::engine::configuration::DiffState>();
  _diff_state->Swap(&diff_state->mut_obj());
}

std::unique_ptr<com::centreon::engine::configuration::DiffState>
cbmod_state::diff_state() {
  absl::MutexLock lck(&_diff_state_m);
  return std::move(_diff_state);
}

void cbmod_state::set_diff_state_applied(bool done) {
  _diff_state_applied = done;
}

/**
 * @brief Store a notification-execute event received from Broker so the Engine
 * event loop can run it (notification_mode=broker). Called on the Engine side
 * from cbmod_stream when a pb_notification_execute is received.
 *
 * @param ne A shared pointer to a bbdo::pb_notification_execute event.
 */
void cbmod_state::push_notification_execute(
    const std::shared_ptr<io::data>& ne) {
  auto evt =
      std::static_pointer_cast<com::centreon::broker::bbdo::pb_notification_execute>(
          ne);
  absl::MutexLock lck(&_pending_notifications_m);
  _pending_notifications.push_back(evt->obj());
}

/**
 * @brief Drain all the notification-execute events pending for this poller.
 * Called from the Engine event loop, which then runs each one (macros +
 * notification commands). The queue is emptied.
 *
 * @return The pending notifications, in arrival order.
 */
std::vector<NotificationExecute> cbmod_state::drain_notification_executes() {
  absl::MutexLock lck(&_pending_notifications_m);
  std::vector<NotificationExecute> retval;
  retval.reserve(_pending_notifications.size());
  for (auto& ne : _pending_notifications)
    retval.push_back(std::move(ne));
  _pending_notifications.clear();
  return retval;
}

/**
 * @brief Store the current Engine configuration to be sent to Broker.
 * Called when Broker requests the Engine configuration via a
 * DiffState{unknown=true} message.
 *
 * @param conf The current Engine configuration state to store.
 */
void cbmod_state::set_current_engine_conf(
    std::unique_ptr<com::centreon::engine::configuration::State>& conf) {
  _current_engine_state = std::move(conf);
}

/**
 * @brief Get and consume the current Engine configuration.
 * Returns the stored configuration and clears the internal pointer.
 * Returns nullptr if no configuration was set.
 *
 * @return The current Engine configuration state, or nullptr.
 */
std::unique_ptr<com::centreon::engine::configuration::State>
cbmod_state::current_engine_conf() {
  return std::move(_current_engine_state);
}

}  // namespace com::centreon::broker::config::applier
