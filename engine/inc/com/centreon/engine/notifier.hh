/*
 * Copyright 2019-2024 Centreon (https://www.centreon.com/)
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

#ifndef CCE_NOTIFIER_HH
#define CCE_NOTIFIER_HH

#include "bbdo/neb.pb.h"
#include "com/centreon/engine/checkable.hh"
#include "com/centreon/engine/contactgroup.hh"
#include "com/centreon/engine/customvariable.hh"
#include "com/centreon/engine/dependency.hh"
#include "engine/src/notifications/notification_manager.hh"
#include "common.hh"

class nagios_macros;
namespace com::centreon::engine {
// Forward declarations
class escalation;
class contact;
class timeperiod;
class notification_ev;

using AckType = com::centreon::broker::AckType;

class notifier : public checkable {
  /* The notification_manager holds the notification viability policy and needs
   * access to the private notification state of the notifier. */
  friend class notifications::notification_manager;

 public:
  notifier(notifications::notifier_type notification_flag,
           const std::string& name,
           std::string const& display_name,
           std::string const& check_command,
           bool checks_enabled,
           bool accept_passive_checks,
           uint32_t check_interval,
           uint32_t retry_interval,
           uint32_t notification_interval,
           int max_attempts,
           int32_t notify,
           int32_t stalk,
           uint32_t first_notification_delay,
           uint32_t recovery_notification_delay,
           std::string const& notification_period,
           bool notifications_enabled,
           const std::string& check_period,
           const std::string& event_handler,
           bool event_handler_enabled,
           std::string const& notes,
           std::string const& notes_url,
           std::string const& action_url,
           std::string const& icon_image,
           std::string const& icon_image_alt,
           bool flap_detection_enabled,
           double low_flap_threshold,
           double high_flap_threshold,
           bool check_freshness,
           int freshness_threshold,
           bool obsess_over,
           std::string const& timezone,
           bool retain_status_information,
           bool retain_nonstatus_information,
           bool is_volatile,
           uint64_t icon_id);
  ~notifier();

  void set_notification(int32_t idx, std::string const& value);

  bool get_notify_on(notifications::notification_flag type) const noexcept;
  uint32_t get_notify_on() const noexcept;
  void set_notify_on(uint32_t type) noexcept;
  void add_notify_on(notifications::notification_flag type) noexcept;
  void remove_notify_on(notifications::notification_flag type) noexcept;
  virtual bool get_notify_on_current_state() const = 0;

  bool get_notified_on(notifications::notification_flag type) const noexcept;
  uint32_t get_notified_on() const noexcept;
  void set_notified_on(uint32_t type) noexcept;
  void add_notified_on(notifications::notification_flag type) noexcept;
  void remove_notified_on(notifications::notification_flag type) noexcept;

  bool get_stalk_on(notifications::notification_flag type) const noexcept;
  uint32_t get_stalk_on() const noexcept;
  void set_stalk_on(uint32_t type) noexcept;
  void add_stalk_on(notifications::notification_flag type) noexcept;

  bool get_flap_detection_on(
      notifications::notification_flag type) const noexcept;
  uint32_t get_flap_detection_on() const noexcept;
  void set_flap_detection_on(uint32_t type) noexcept;
  void add_flap_detection_on(notifications::notification_flag type) noexcept;

  unsigned long get_current_event_id() const;
  void set_current_event_id(unsigned long current_event_id) noexcept;
  unsigned long get_last_event_id() const noexcept;
  void set_last_event_id(unsigned long last_event_id) noexcept;

  virtual bool schedule_check(time_t check_time,
                              uint32_t options,
                              bool no_update_status_now) = 0;

  /**
   * @brief Update the status of the notifier partially. attributes is a bits
   * field based on enum status_attribute specifying what has to be updated.
   *
   * @param attributes A bits field based on enum status_attribute.
   */
  virtual void update_status(uint32_t attributes) = 0;
  int notify(notifications::reason_type type,
             std::string const& not_author,
             std::string const& not_data,
             notifications::notification_option options);

  void set_current_notification_id(uint64_t id) noexcept;
  uint64_t get_current_notification_id() const noexcept;
  virtual void grab_macros_r(nagios_macros* mac) = 0;
  virtual int notify_contact(nagios_macros* mac,
                             contact* cntct,
                             notifications::reason_type type,
                             std::string const& not_author,
                             std::string const& not_data,
                             int options,
                             int escalated) = 0;
  time_t get_next_notification() const noexcept;
  void set_next_notification(time_t next_notification) noexcept;
  time_t get_last_notification() const noexcept;
  void set_last_notification(time_t last_notification) noexcept;
  virtual void update_notification_flags() = 0;
  time_t get_next_notification_time(time_t offset);
  void set_initial_notif_time(time_t notif_time) noexcept;
  time_t get_initial_notif_time() const noexcept;
  void set_acknowledgement_timeout(int timeout) noexcept;
  void set_last_acknowledgement(time_t ack) noexcept;
  time_t last_acknowledgement() const noexcept;
  uint32_t get_notification_interval(void) const noexcept;
  void set_notification_interval(uint32_t notification_interval) noexcept;
  std::string const& notification_period() const noexcept;
  void set_notification_period(std::string const& notification_period) noexcept;

  uint32_t get_first_notification_delay(void) const noexcept;
  void set_first_notification_delay(uint32_t notification_delay) noexcept;
  uint32_t get_recovery_notification_delay(void) const noexcept;
  void set_recovery_notification_delay(uint32_t notification_delay) noexcept;
  bool get_notifications_enabled() const noexcept;
  void set_notifications_enabled(bool notifications_enabled) noexcept;
  uint64_t get_flapping_comment_id(void) const noexcept;
  void set_flapping_comment_id(uint64_t comment_id) noexcept;
  uint64_t get_acknowledgement_comment_id(void) const noexcept;
  void set_acknowledgement_comment_id(uint64_t comment_id) noexcept;
  void delete_acknowledgement_comment() noexcept;
  int get_check_options(void) const noexcept;
  void set_check_options(int option) noexcept;
  int get_retain_status_information(void) const noexcept;
  void set_retain_status_information(bool retain_status_informations) noexcept;
  bool get_retain_nonstatus_information(void) const noexcept;
  void set_retain_nonstatus_information(
      bool retain_non_status_informations) noexcept;
  bool get_is_being_freshened(void) const noexcept;
  void set_is_being_freshened(bool freshened) noexcept;
  std::list<escalation*>& get_escalations() noexcept;
  std::list<escalation*> const& get_escalations() const noexcept;
  virtual bool is_valid_escalation_for_notification(escalation const* e,
                                                    int options) const = 0;
  void add_modified_attributes(uint32_t attr) noexcept;
  uint32_t get_modified_attributes() const noexcept;
  void set_modified_attributes(uint32_t modified_attributes) noexcept;
  AckType get_acknowledgement() const noexcept;
  bool problem_has_been_acknowledged() const noexcept;
  void set_acknowledgement(AckType acknowledge_type) noexcept;
  virtual bool recovered() const = 0;
  virtual int get_current_state_int() const = 0;
  bool get_no_more_notifications() const noexcept;
  void set_no_more_notifications(bool no_more_notifications) noexcept;
  bool notifications_available(int options) const;
  int get_notification_number() const noexcept;
  void set_notification_number(int number);

  virtual bool authorized_by_dependencies(
      dependency::types dependency_type) const = 0;
  virtual timeperiod* get_notification_timeperiod() const = 0;
  static notifications::notification_category get_category(
      notifications::reason_type type);
  bool is_notification_viable(notifications::notification_category cat,
                              notifications::reason_type type,
                              notifications::notification_option options);
  std::unordered_set<std::shared_ptr<contact>> get_contacts_to_notify(
      notifications::notification_category cat,
      notifications::reason_type type,
      uint32_t& notification_interval,
      bool& escalated);
  notifications::notifier_type get_notifier_type() const noexcept;
  absl::flat_hash_map<std::string, std::shared_ptr<contact>>&
  mut_contacts() noexcept;
  const absl::flat_hash_map<std::string, std::shared_ptr<contact>>& contacts()
      const noexcept;
  contactgroup_map& get_contactgroups() noexcept;
  const contactgroup_map& get_contactgroups() const noexcept;
  void resolve(uint32_t& w, uint32_t& e);
  std::array<int, MAX_STATE_HISTORY_ENTRIES> const& get_state_history() const;
  std::array<int, MAX_STATE_HISTORY_ENTRIES>& get_state_history();
  std::array<std::unique_ptr<notification_ev>, 6> const&
  get_current_notifications() const;
  int get_pending_flex_downtime() const;
  void inc_pending_flex_downtime() noexcept;
  void dec_pending_flex_downtime() noexcept;
  void set_flap_type(uint32_t type) noexcept;
  timeperiod* get_notification_period_ptr() const noexcept;
  void set_notification_period_ptr(timeperiod* tp) noexcept;
  int acknowledgement_timeout() const noexcept;

  map_customvar custom_variables;

 private:
  notifications::notifier_type _notifier_type;
  int32_t _stalk_type;
  uint32_t _flap_type;
  unsigned long _current_event_id;
  unsigned long _last_event_id;

  time_t _initial_notif_time;
  int _acknowledgement_timeout;
  time_t _last_acknowledgement;
  int32_t _out_notification_type;
  uint32_t _current_notifications;
  uint32_t _notification_interval;
  uint32_t _modified_attributes;
  uint64_t _current_notification_id;
  time_t _next_notification;
  time_t _last_notification;
  std::string _notification_period;
  timeperiod* _notification_period_ptr;
  uint32_t _first_notification_delay;
  uint32_t _recovery_notification_delay;
  bool _notifications_enabled;
  std::list<escalation*> _escalations;
  bool _no_more_notifications;
  uint64_t _flapping_comment_id;
  uint64_t _acknowledgement_comment_id;
  int _check_options;
  AckType _acknowledgement_type;
  bool _retain_status_information;
  bool _retain_nonstatus_information;
  bool _is_being_freshened;

  /*if notification_interval at 0 and is on time period off.
  is set as true to send the notification on the next starting time period*/
  bool _notification_to_interval_on_timeperiod_in;

  /* New ones */
  int _notification_number;
  // reason_type _type;
  contact_map _contacts;
  contactgroup_map _contact_groups;
  std::array<std::unique_ptr<notification_ev>, 6> _notification;
  std::array<int, MAX_STATE_HISTORY_ENTRIES> _state_history;
  int _pending_flex_downtime;
};

}  // namespace com::centreon::engine

bool is_contact_for_notifier(com::centreon::engine::notifier* notif,
                             com::centreon::engine::contact* cntct);

#endif  // !CCE_NOTIFIER_HH
