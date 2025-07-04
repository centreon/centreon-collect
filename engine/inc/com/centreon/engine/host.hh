/**
 * Copyright 2011-2024 Centreon
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

#ifndef CCE_HOST_HH
#define CCE_HOST_HH

#include "com/centreon/engine/service.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class contact;
class host;
class hostgroup;
class hostescalation;
}  // namespace com::centreon::engine

using host_map =
    absl::flat_hash_map<std::string,
                        std::shared_ptr<com::centreon::engine::host>>;
using host_map_unsafe =
    absl::flat_hash_map<std::string, com::centreon::engine::host*>;
using host_id_map =
    absl::flat_hash_map<uint64_t, std::shared_ptr<com::centreon::engine::host>>;

namespace com::centreon::engine {
class host : public notifier {
 public:
  static std::array<std::pair<uint32_t, std::string>, 3> const tab_host_states;

  enum host_state { state_up, state_down, state_unreachable };

  host(uint64_t host_id,
       std::string const& name,
       std::string const& display_name,
       std::string const& alias,
       std::string const& address,
       std::string const& check_period,
       uint32_t check_interval,
       uint32_t retry_interval,
       int max_attempts,
       int notify_up,
       int notify_down,
       int notify_unreachable,
       int notify_flapping,
       int notify_downtime,
       uint32_t notification_interval,
       uint32_t first_notification_delay,
       uint32_t recovery_notification_delay,
       std::string const& notification_period,
       bool notifications_enabled,
       std::string const& check_command,
       bool checks_enabled,
       bool accept_passive_checks,
       std::string const& event_handler,
       bool event_handler_enabled,
       bool flap_detection_enabled,
       double low_flap_threshold,
       double high_flap_threshold,
       int flap_detection_on_up,
       int flap_detection_on_down,
       int flap_detection_on_unreachable,
       int stalk_on_up,
       int stalk_on_down,
       int stalk_on_unreachable,
       bool process_perfdata,
       bool check_freshness,
       int freshness_threshold,
       std::string const& notes,
       std::string const& notes_url,
       std::string const& action_url,
       std::string const& icon_image,
       std::string const& icon_image_alt,
       std::string const& vrml_image,
       std::string const& statusmap_image,
       double x_2d,
       double y_2d,
       bool have_2d_coords,
       double x_3d,
       double y_3d,
       double z_3d,
       bool have_3d_coords,
       bool should_be_drawn,
       bool retain_status_information,
       bool retain_nonstatus_information,
       bool obsess_over_host,
       std::string const& timezone,
       uint64_t icon_id);
  ~host();
  uint64_t host_id() const;
  void set_host_id(uint64_t id);
  void set_name(const std::string& name) override;
  void add_child_host(host* child);
  void add_parent_host(std::string const& host_name);
  int log_event();
  int handle_async_check_result_3x(const check_result& queued_check_result);
  int run_scheduled_check(int check_options, double latency);
  int run_async_check(int check_options,
                      double latency,
                      bool scheduled_check,
                      bool reschedule_check,
                      bool* time_is_valid,
                      time_t* preferred_time) noexcept;
  bool schedule_check(time_t check_time,
                      uint32_t options,
                      bool no_update_status_now = false) override;
  void check_for_flapping(bool update,
                          bool actual_check,
                          bool allow_flapstart_notification);
  void set_flap(double percent_change,
                double high_threshold,
                double low_threshold,
                bool allow_flapstart_notification);
  void clear_flap(double percent_change,
                  double high_threshold,
                  double low_threshold);
  void update_status(uint32_t attributes = STATUS_ALL) override;
  void update_adaptive_data();
  void check_for_expired_acknowledgement();
  //  bool               check_notification_viability(reason_type type,
  //                                                  int options) override;
  int handle_state();
  void update_performance_data();
  bool verify_check_viability(int check_options,
                              bool* time_is_valid,
                              time_t* new_time);
  void grab_macros_r(nagios_macros* mac) override;
  bool operator==(host const& other) = delete;  // throw ();
  bool operator!=(host const& other) = delete;  // throw ();
  bool is_result_fresh(time_t current_time, int log_this);

  int run_sync_check_3x(enum host::host_state* check_result_code,
                        int check_options,
                        int use_cached_result,
                        unsigned long check_timestamp_horizon);
  int perform_on_demand_check_3x(enum host::host_state* check_result_code,
                                 int check_options,
                                 int use_cached_result,
                                 unsigned long check_timestamp_horizon);

  int perform_on_demand_check(enum host::host_state* check_return_code,
                              int check_options,
                              int use_cached_result,
                              unsigned long check_timestamp_horizon);
  int process_check_result_3x(enum host::host_state new_state,
                              std::string const& old_plugin_output,
                              int check_options,
                              int reschedule_check,
                              int use_cached_result,
                              unsigned long check_timestamp_horizon);
  void adjust_check_attempt(bool is_active);
  bool authorized_by_dependencies(
      dependency::types dependency_type) const override;
  static void check_for_orphaned();
  static void check_result_freshness();

  enum host_state determine_host_reachability(enum host_state new_state);
  bool recovered() const override;
  int get_current_state_int() const override;
  std::string const& get_current_state_as_string() const override;

  // setters / getters
  std::string const& get_alias() const;
  void set_alias(std::string const& alias);
  std::string const& get_address() const;
  void set_address(std::string const& address);
  bool get_process_performance_data() const;
  void set_process_performance_data(bool process_performance_data);
  bool get_should_reschedule_current_check() const;
  void set_should_reschedule_current_check(bool should_reschedule);
  std::string const& get_vrml_image() const;
  void set_vrml_image(std::string const& image);
  std::string const& get_statusmap_image() const;
  void set_statusmap_image(std::string const& image);
  bool get_have_2d_coords() const;
  void set_have_2d_coords(bool has_coords);
  bool get_have_3d_coords() const;
  void set_have_3d_coords(bool has_coords);
  double get_x_2d() const;
  void set_x_2d(double x);
  double get_y_2d() const;
  void set_y_2d(double y);
  double get_x_3d() const;
  void set_x_3d(double x);
  double get_y_3d() const;
  void set_y_3d(double y);
  double get_z_3d() const;
  void set_z_3d(double z);
  int get_should_be_drawn() const;
  void set_should_be_drawn(int should_be_drawn);
  time_t get_last_time_down() const;
  void set_last_time_down(time_t last_time);
  time_t get_last_time_unreachable() const;
  void set_last_time_unreachable(time_t last_time);
  time_t get_last_time_up() const;
  void set_last_time_up(time_t last_time);
  time_t get_last_state_history_update() const;
  void set_last_state_history_update(time_t last_state_history_update);
  void disable_flap_detection();
  void enable_flap_detection();
  int get_total_services() const;
  void set_total_services(int total_services);
  unsigned long get_total_service_check_interval() const;
  void set_total_service_check_interval(
      unsigned long total_service_check_interval);
  int get_circular_path_checked() const;
  void set_circular_path_checked(int check_level);
  bool get_contains_circular_path() const;
  void set_contains_circular_path(bool contains_circular_path);
  enum host_state get_current_state() const;
  void set_current_state(enum host_state current_state);
  enum host_state get_last_state() const;
  void set_last_state(enum host_state last_state);
  enum host_state get_last_hard_state() const;
  void set_last_hard_state(enum host_state last_hard_state);
  enum host_state get_initial_state() const;
  void set_initial_state(enum host_state current_state);
  int notify_contact(nagios_macros* mac,
                     contact* cntct,
                     reason_type type,
                     std::string const& not_author,
                     std::string const& not_data,
                     int options,
                     int escalated) override;
  void update_notification_flags() override;
  void schedule_acknowledgement_expiration();
  bool is_valid_escalation_for_notification(escalation const* e,
                                            int options) const override;
  void handle_flap_detection_disabled();
  timeperiod* get_notification_timeperiod() const override;
  bool get_notify_on_current_state() const override;
  bool is_in_downtime() const override;
  void resolve(uint32_t& w, uint32_t& e);

  void set_check_command_ptr(
      const std::shared_ptr<commands::command>& cmd) override;

  host_map parent_hosts;
  host_map_unsafe child_hosts;
  static host_map hosts;
  static host_id_map hosts_by_id;

  service_map_unsafe services;
  std::list<hostgroup*> const& get_parent_groups() const;
  std::list<hostgroup*>& get_parent_groups();

  std::string get_check_command_line(nagios_macros* macros);

 private:
  void _switch_all_services_to_unknown();

  uint64_t _id;
  std::string _alias;
  std::string _address;
  bool _process_performance_data;
  std::string _vrml_image;
  std::string _statusmap_image;
  bool _have_2d_coords;
  bool _have_3d_coords;
  double _x_2d;
  double _y_2d;
  double _x_3d;
  double _y_3d;
  double _z_3d;
  int _should_be_drawn;
  bool _should_reschedule_current_check;
  time_t _last_time_down;
  time_t _last_time_unreachable;
  time_t _last_time_up;
  time_t _last_state_history_update;
  int _total_services;
  unsigned long _total_service_check_interval;
  int _circular_path_checked;
  bool _contains_circular_path;

  enum host_state _initial_state;
  enum host_state _last_state;
  enum host_state _last_hard_state;
  enum host_state _current_state;

  std::list<hostgroup*> _hostgroups;
};

}  // namespace com::centreon::engine

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::host const& obj);
std::ostream& operator<<(std::ostream& os, host_map_unsafe const& obj);
std::ostream& operator<<(std::ostream& os, host_map const& obj);

namespace com::centreon::engine {

void check_for_expired_acknowledgement(com::centreon::engine::host* h);
com::centreon::engine::host& find_host(uint64_t host_id);
bool host_exists(uint64_t host_id) noexcept;
uint64_t get_host_id(std::string const& name);
std::string get_host_name(const uint64_t host_id);

}  // namespace com::centreon::engine

#endif  // !CCE_HOST_HH
