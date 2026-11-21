/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef CCC_NOTIFICATIONS_NOTIFICATION_MANAGER_HH
#define CCC_NOTIFICATIONS_NOTIFICATION_MANAGER_HH

#include <array>
#include <utility>

#include "absl/container/flat_hash_map.h"

#include "common/notifications/notification_types.hh"

namespace com::centreon::common::notifications {

class notification_callbacks;

/**
 * @brief Central manager for notifications.
 *
 * It owns the notification policy (viability), the per-resource runtime state
 * (number, ids, timings, live events) and the notify() orchestration. It does
 * not depend on any engine object: it reaches the host application only through
 * an injected notification_callbacks, addressing resources by logical id
 * (host_id, service_id) — service_id == 0 designates a host.
 *
 * The singleton has an explicitly controlled lifetime (created by load(),
 * destroyed by unload()): a Meyers singleton would be destroyed before the
 * global host/service objects at program exit, so the late ~notifier() calls
 * to forget() would touch a destroyed instance.
 */
class notification_manager {
  static notification_manager* _instance;

  /* Per-resource notification runtime state. */
  struct notification_state {
    uint64_t number = 0;
    uint64_t current_id = 0;
    std::time_t last = 0;
    std::time_t next = 0;
    std::time_t initial = 0;
    std::array<std::unique_ptr<notification>, 6> events;
  };

  using key = std::pair<uint64_t, uint64_t>;

  uint64_t _next_notification_id = 1ull;
  std::unique_ptr<notification_callbacks> _callbacks;
  absl::flat_hash_map<key, notification_state> _states;

  notification_state& _state(uint64_t host_id, uint64_t service_id);

  /* Construction/destruction are private: the only instance is owned through
   * _instance and managed by load()/unload(). */
  notification_manager();
  ~notification_manager();

  bool _is_notification_viable_normal(uint64_t host_id,
                                      uint64_t service_id,
                                      const resource_state& rs,
                                      std::time_t now,
                                      reason_type type);
  bool _is_notification_viable_recovery(uint64_t host_id,
                                        uint64_t service_id,
                                        const resource_state& rs,
                                        const config& cfg,
                                        std::time_t now,
                                        reason_type type,
                                        notification_option options);
  bool _is_notification_viable_acknowledgement(const resource_state& rs,
                                               reason_type type);
  bool _is_notification_viable_flapping(uint64_t host_id,
                                        uint64_t service_id,
                                        const resource_state& rs,
                                        reason_type type);
  bool _is_notification_viable_downtime(const resource_state& rs,
                                        reason_type type);
  bool _is_notification_viable_custom(const resource_state& rs,
                                      reason_type type);

 public:
  static constexpr std::array<std::string_view, 9> tab_notification_str{{
      "NORMAL",
      "RECOVERY",
      "ACKNOWLEDGEMENT",
      "FLAPPINGSTART",
      "FLAPPINGSTOP",
      "FLAPPINGDISABLED",
      "DOWNTIMESTART",
      "DOWNTIMEEND",
      "DOWNTIMECANCELLED",
  }};

  static constexpr std::array<std::string_view, 2> tab_state_type{
      {"SOFT", "HARD"}};

  static notification_manager& instance();
  /* Inject the host-application backend (mirrors downtime_manager::load).
   * load() also creates the singleton and unload() destroys it: the lifetime
   * is controlled on purpose (see class doc). */
  static void load(std::unique_ptr<notification_callbacks> callbacks);
  /* Release the backend, drop all per-resource state and destroy the singleton.
   */
  static void unload();

  notification_manager(const notification_manager&) = delete;
  notification_manager& operator=(const notification_manager&) = delete;
  notification_manager(notification_manager&&) = delete;
  notification_manager& operator=(notification_manager&&) = delete;

  uint64_t next_notification_id() noexcept;
  uint64_t get_next_notification_id() const noexcept;

  static notification_category get_category(reason_type type);
  bool is_notification_viable(uint64_t host_id,
                              uint64_t service_id,
                              notification_category cat,
                              reason_type type,
                              notification_option options);
  int32_t notify(uint64_t host_id,
                 uint64_t service_id,
                 reason_type type,
                 const std::string& not_author,
                 const std::string& not_data,
                 notification_option options);

  notification* current_notification(uint64_t host_id,
                                     uint64_t service_id,
                                     notification_category cat) const;
  std::array<notification*, 6> current_notifications(uint64_t host_id,
                                                     uint64_t service_id) const;
  void set_notification(uint64_t host_id,
                        uint64_t service_id,
                        notification_category cat,
                        std::unique_ptr<notification> ev);
  static void forget(uint64_t host_id, uint64_t service_id);

  /* Per-resource notification runtime state. */
  uint64_t notification_number(uint64_t host_id, uint64_t service_id) const;
  void set_notification_number(uint64_t host_id,
                               uint64_t service_id,
                               uint64_t number);
  void inc_notification_number(uint64_t host_id, uint64_t service_id);
  uint64_t current_notification_id(uint64_t host_id, uint64_t service_id) const;
  void set_current_notification_id(uint64_t host_id,
                                   uint64_t service_id,
                                   uint64_t id);
  std::time_t last_notification(uint64_t host_id, uint64_t service_id) const;
  void set_last_notification(uint64_t host_id,
                             uint64_t service_id,
                             std::time_t t);
  std::time_t next_notification(uint64_t host_id, uint64_t service_id) const;
  void set_next_notification(uint64_t host_id,
                             uint64_t service_id,
                             std::time_t t);
  std::time_t initial_notif_time(uint64_t host_id, uint64_t service_id) const;
  void set_initial_notif_time(uint64_t host_id,
                              uint64_t service_id,
                              std::time_t t);
};

}  // namespace com::centreon::common::notifications

#endif  // !CCC_NOTIFICATIONS_NOTIFICATION_MANAGER_HH
