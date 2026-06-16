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

#ifndef CCE_NOTIFICATIONS_NOTIFICATION_MANAGER_HH
#define CCE_NOTIFICATIONS_NOTIFICATION_MANAGER_HH

#include <array>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"

namespace com::centreon::engine {
class notifier;

namespace notifications {
class notification;

/* Status attributes. Used as argument in the notifier::update_status(). */
enum status_attribute {
  STATUS_NONE = 0,
  STATUS_DOWNTIME_DEPTH = 1 << 0,
  STATUS_NOTIFICATION_NUMBER = 1 << 1,
  STATUS_ACKNOWLEDGEMENT = 1 << 2,
  STATUS_ALL = ~0u,
};

enum notification_category {
  cat_normal,
  cat_recovery,
  cat_acknowledgement,
  cat_flapping,
  cat_downtime,
  cat_custom,
};

enum notification_flag {
  none = 0,
  // Host
  up = 1 << 0,
  down = 1 << 1,
  unreachable = 1 << 2,
  // Service
  ok = 1 << 3,
  warning = 1 << 4,
  critical = 1 << 5,
  unknown = 1 << 6,
  // Flapping
  flappingstart = 1 << 7,
  flappingstop = 1 << 8,
  flappingdisabled = 1 << 9,

  // Downtime
  downtime = 1 << 10,
};

enum notifier_type {
  host_notification,
  service_notification,
};

enum reason_type {
  reason_normal,
  reason_recovery,
  reason_acknowledgement,
  reason_flappingstart,
  reason_flappingstop,
  reason_flappingdisabled,
  reason_downtimestart,
  reason_downtimeend,
  reason_downtimecancelled,
  reason_custom = 99,
};

enum notification_option {
  notification_option_none = 0,
  notification_option_broadcast = 1,
  notification_option_forced = 2,
  notification_option_increment = 4,
};

/**
 * @brief Central manager for notifications.
 *
 * Singleton with an explicitly controlled lifetime (same pattern as
 * checks::checker): the instance is created by init() and destroyed by
 * deinit(). We must control when it is destroyed because notifier destructors
 * call forget() on it; a Meyers singleton would be destroyed before the global
 * host/service objects at program exit, so those late ~notifier() calls would
 * touch a destroyed instance. Not copyable nor movable.
 */
class notification_manager {
  static notification_manager* _instance;

  /* Per-notifier notification runtime state. Centralizes what used to be
   * scattered members of the notifier (number, ids, timings) plus the live
   * notification events, one slot per notification_category. */
  struct notification_state {
    uint64_t number = 0;
    uint64_t current_id = 0;
    std::time_t last = 0;
    std::time_t next = 0;
    std::time_t initial = 0;
    std::array<std::unique_ptr<notification>, 6> events;
  };

  uint64_t _next_notification_id = 1ull;
  absl::flat_hash_map<notifier*, notification_state> _states;

  notification_state& _state(notifier* n);

  /* Construction/destruction are private: the only instance is owned through
   * _instance and managed by init()/deinit(). */
  notification_manager();
  ~notification_manager() = default;

  bool _is_notification_viable_normal(notifier& n,
                                      reason_type type,
                                      notification_option options);
  bool _is_notification_viable_recovery(notifier& n,
                                        reason_type type,
                                        notification_option options);
  bool _is_notification_viable_acknowledgement(notifier& n,
                                               reason_type type,
                                               notification_option options);
  bool _is_notification_viable_flapping(notifier& n,
                                        reason_type type,
                                        notification_option options);
  bool _is_notification_viable_downtime(notifier& n,
                                        reason_type type,
                                        notification_option options);
  bool _is_notification_viable_custom(notifier& n,
                                      reason_type type,
                                      notification_option options);

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
  static void init();
  static void deinit();

  notification_manager(const notification_manager&) = delete;
  notification_manager& operator=(const notification_manager&) = delete;
  notification_manager(notification_manager&&) = delete;
  notification_manager& operator=(notification_manager&&) = delete;

  uint64_t next_notification_id() noexcept;
  uint64_t get_next_notification_id() const noexcept;

  static notification_category get_category(reason_type type);
  bool is_notification_viable(notifier& n,
                              notification_category cat,
                              reason_type type,
                              notification_option options);
  int32_t notify(notifier& n,
                 reason_type type,
                 const std::string& not_author,
                 const std::string& not_data,
                 notification_option options);

  notification* current_notification(notifier* n,
                                     notification_category cat) const;
  std::array<notification*, 6> current_notifications(const notifier* n) const;
  void set_notification(notifier* n,
                        notification_category cat,
                        std::unique_ptr<notification> ev);
  static void forget(notifier* n);

  /* Per-notifier notification runtime state (storage moved out of notifier;
   * notifier keeps thin delegators). */
  uint64_t notification_number(const notifier* n) const;
  void set_notification_number(notifier* n, uint64_t number);
  void inc_notification_number(notifier* n);
  uint64_t current_notification_id(const notifier* n) const;
  void set_current_notification_id(notifier* n, uint64_t id);
  std::time_t last_notification(const notifier* n) const;
  void set_last_notification(notifier* n, std::time_t t);
  std::time_t next_notification(const notifier* n) const;
  void set_next_notification(notifier* n, std::time_t t);
  std::time_t initial_notif_time(const notifier* n) const;
  void set_initial_notif_time(notifier* n, std::time_t t);
};

}  // namespace notifications
}  // namespace com::centreon::engine

#endif  // !CCE_NOTIFICATIONS_NOTIFICATION_MANAGER_HH
