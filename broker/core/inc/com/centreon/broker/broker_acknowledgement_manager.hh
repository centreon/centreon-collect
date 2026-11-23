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
 *
 */

#ifndef CCB_BROKER_ACKNOWLEDGEMENT_MANAGER_HH
#define CCB_BROKER_ACKNOWLEDGEMENT_MANAGER_HH

#include <spdlog/spdlog.h>

#include "bbdo/neb.pb.h"

namespace com::centreon::broker {

/**
 * @brief Broker-side acknowledgement authority (notification_mode = broker).
 *
 * In that mode Engine is never told about acknowledgements: Broker receives
 * them through the BrokerRpc endpoints, stores them in its cache (persisted
 * across restarts), writes them to the database and clears them itself when
 * the resource recovers, exactly as Engine does in notification_mode=engine
 * (engine/src/commands/commands.cc and notifier::handle_state).
 *
 * The class is a singleton loaded by broker_state only in broker mode, like
 * the downtime_manager. All the state lives in the Broker cache: this class
 * only orchestrates the events.
 */
class broker_acknowledgement_manager {
  std::shared_ptr<spdlog::logger> _logger;

  static std::unique_ptr<broker_acknowledgement_manager> _instance;

  broker_acknowledgement_manager();

  uint64_t _create_comment(uint64_t host_id,
                           uint64_t service_id,
                           uint32_t instance_id,
                           const std::string& author,
                           const std::string& comment_data,
                           bool persistent,
                           time_t entry_time);
  void _delete_comment(uint64_t comment_id, uint32_t instance_id);
  void _publish_ack_type(uint64_t host_id, uint64_t service_id, AckType type);
  void _publish_log(uint64_t host_id,
                    uint64_t service_id,
                    uint32_t instance_id,
                    const std::string& author,
                    const std::string& output,
                    LogEntry_MsgType msg_type);

 public:
  static void load();
  static void unload();
  static bool is_loaded() noexcept { return _instance != nullptr; }
  static broker_acknowledgement_manager& instance();

  broker_acknowledgement_manager(const broker_acknowledgement_manager&) =
      delete;
  broker_acknowledgement_manager& operator=(
      const broker_acknowledgement_manager&) = delete;

  std::string acknowledge(uint64_t host_id,
                          uint64_t service_id,
                          const std::string& author,
                          const std::string& comment_data,
                          bool sticky,
                          bool notify,
                          bool persistent);
  std::string remove(uint64_t host_id, uint64_t service_id);
  void clear_on_state_change(uint64_t host_id,
                             uint64_t service_id,
                             uint32_t state);
};

}  // namespace com::centreon::broker

#endif /* !CCB_BROKER_ACKNOWLEDGEMENT_MANAGER_HH */
