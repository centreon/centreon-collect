/**
 * Copyright 2011-2013,2015-2017,2022-2024 Centreon
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
#ifndef CCE_CONFIGURATION_HOST_HH
#define CCE_CONFIGURATION_HOST_HH

#include "com/centreon/common/opt.hh"
#include "customvariable.hh"
#include "group.hh"
#include "object.hh"
#include "point_2d.hh"
#include "point_3d.hh"

using com::centreon::common::opt;

namespace com::centreon::engine::configuration {

class host : public object {
 public:
  enum action_on {
    none = 0,
    up = (1 << 0),
    down = (1 << 1),
    unreachable = (1 << 2),
    flapping = (1 << 3),
    downtime = (1 << 4)
  };
  typedef uint64_t key_type;

  host(key_type const& key = 0);
  host(host const& other);
  host& operator=(host const& other);
  bool operator==(host const& other) const noexcept;
  bool operator!=(host const& other) const noexcept;
  bool operator<(host const& other) const noexcept;
  void check_validity(error_cnt& err) const override;
  key_type key() const noexcept;
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  std::string const& action_url() const noexcept;
  std::string const& address() const noexcept;
  std::string const& alias() const noexcept;
  bool checks_active() const noexcept;
  bool checks_passive() const noexcept;
  std::string const& check_command() const noexcept;
  bool check_freshness() const noexcept;
  unsigned int check_interval() const noexcept;
  std::string const& check_period() const noexcept;
  set_string const& contactgroups() const noexcept;
  set_string const& contacts() const noexcept;
  point_2d const& coords_2d() const noexcept;
  point_3d const& coords_3d() const noexcept;
  const std::unordered_map<std::string, customvariable>& customvariables()
      const noexcept;
  std::unordered_map<std::string, customvariable>&
  mut_customvariables() noexcept;
  std::string const& display_name() const noexcept;
  std::string const& event_handler() const noexcept;
  bool event_handler_enabled() const noexcept;
  unsigned int first_notification_delay() const noexcept;
  bool flap_detection_enabled() const noexcept;
  unsigned int flap_detection_options() const noexcept;
  unsigned int freshness_threshold() const noexcept;
  bool have_coords_2d() const noexcept;
  bool have_coords_3d() const noexcept;
  unsigned int high_flap_threshold() const noexcept;
  set_string& hostgroups() noexcept;
  set_string const& hostgroups() const noexcept;
  uint64_t host_id() const noexcept;
  std::string const& host_name() const noexcept;
  std::string const& icon_image() const noexcept;
  std::string const& icon_image_alt() const noexcept;
  unsigned int low_flap_threshold() const noexcept;
  unsigned int max_check_attempts() const noexcept;
  std::string const& notes() const noexcept;
  std::string const& notes_url() const noexcept;
  bool notifications_enabled() const noexcept;
  unsigned int notification_interval() const noexcept;
  unsigned int notification_options() const noexcept;
  std::string const& notification_period() const noexcept;
  bool obsess_over_host() const noexcept;
  set_string& parents() noexcept;
  set_string const& parents() const noexcept;
  bool process_perf_data() const noexcept;
  bool retain_nonstatus_information() const noexcept;
  bool retain_status_information() const noexcept;
  unsigned int retry_interval() const noexcept;
  unsigned int recovery_notification_delay() const noexcept;
  unsigned int stalking_options() const noexcept;
  std::string const& statusmap_image() const noexcept;
  std::string const& timezone() const noexcept;
  std::string const& vrml_image() const noexcept;
  int acknowledgement_timeout() const noexcept;
  bool set_acknowledgement_timeout(int value);
  uint64_t severity_id() const noexcept;
  uint64_t icon_id() const noexcept;
  const std::set<std::pair<uint64_t, uint16_t>>& tags() const noexcept;

 private:
  typedef bool (*setter_func)(host&, char const*);

  bool _set_action_url(std::string const& value);
  bool _set_address(std::string const& value);
  bool _set_alias(std::string const& value);
  bool _set_checks_active(bool value);
  bool _set_checks_passive(bool value);
  bool _set_check_command(std::string const& value);
  bool _set_check_freshness(bool value);
  bool _set_check_interval(unsigned int value);
  bool _set_check_period(std::string const& value);
  bool _set_contactgroups(std::string const& value);
  bool _set_contacts(std::string const& value);
  bool _set_coords_2d(std::string const& value);
  bool _set_coords_3d(std::string const& value);
  bool _set_display_name(std::string const& value);
  bool _set_event_handler(std::string const& value);
  bool _set_event_handler_enabled(bool value);
  bool _set_first_notification_delay(unsigned int value);
  bool _set_flap_detection_enabled(bool value);
  bool _set_flap_detection_options(std::string const& value);
  bool _set_freshness_threshold(unsigned int value);
  bool _set_high_flap_threshold(unsigned int value);
  bool _set_host_id(uint64_t value);
  bool _set_host_name(std::string const& value);
  bool _set_hostgroups(std::string const& value);
  bool _set_icon_image(std::string const& value);
  bool _set_icon_image_alt(std::string const& value);
  bool _set_low_flap_threshold(unsigned int value);
  bool _set_max_check_attempts(unsigned int value);
  bool _set_notes(std::string const& value);
  bool _set_notes_url(std::string const& value);
  bool _set_notifications_enabled(bool value);
  bool _set_notification_interval(unsigned int value);
  bool _set_notification_options(std::string const& value);
  bool _set_notification_period(std::string const& value);
  bool _set_obsess_over_host(bool value);
  bool _set_parents(std::string const& value);
  bool _set_process_perf_data(bool value);
  bool _set_retain_nonstatus_information(bool value);
  bool _set_retain_status_information(bool value);
  bool _set_retry_interval(unsigned int value);
  bool _set_recovery_notification_delay(unsigned int value);
  bool _set_stalking_options(std::string const& value);
  bool _set_statusmap_image(std::string const& value);
  bool _set_timezone(std::string const& value);
  bool _set_vrml_image(std::string const& value);
  bool _set_severity_id(uint64_t severity_id);
  bool _set_icon_id(uint64_t icon_id);
  bool _set_category_tags(const std::string& value);
  bool _set_group_tags(const std::string& value);

  opt<int> _acknowledgement_timeout;
  std::string _action_url;
  std::string _address;
  std::string _alias;
  opt<bool> _checks_active;
  opt<bool> _checks_passive;
  std::string _check_command;
  opt<bool> _check_freshness;
  opt<unsigned int> _check_interval;
  std::string _check_period;
  group<set_string> _contactgroups;
  group<set_string> _contacts;
  opt<point_2d> _coords_2d;
  opt<point_3d> _coords_3d;
  std::unordered_map<std::string, customvariable> _customvariables;
  std::string _display_name;
  std::string _event_handler;
  opt<bool> _event_handler_enabled;
  opt<unsigned int> _first_notification_delay;
  opt<bool> _flap_detection_enabled;
  opt<unsigned int> _flap_detection_options;
  opt<unsigned int> _freshness_threshold;
  opt<unsigned int> _high_flap_threshold;
  group<set_string> _hostgroups;
  uint64_t _host_id;
  std::string _host_name;
  std::string _icon_image;
  std::string _icon_image_alt;
  opt<unsigned int> _low_flap_threshold;
  opt<unsigned int> _max_check_attempts;
  std::string _notes;
  std::string _notes_url;
  opt<bool> _notifications_enabled;
  opt<unsigned int> _notification_interval;
  opt<unsigned int> _notification_options;
  std::string _notification_period;
  opt<bool> _obsess_over_host;
  group<set_string> _parents;
  opt<bool> _process_perf_data;
  opt<bool> _retain_nonstatus_information;
  opt<bool> _retain_status_information;
  opt<unsigned int> _retry_interval;
  opt<unsigned int> _recovery_notification_delay;
  static std::unordered_map<std::string, setter_func> const _setters;
  opt<unsigned int> _stalking_options;
  std::string _statusmap_image;
  opt<std::string> _timezone;
  std::string _vrml_image;
  opt<uint64_t> _severity_id;
  opt<uint64_t> _icon_id;
  std::set<std::pair<uint64_t, uint16_t>> _tags;
};

using host_ptr = std::shared_ptr<host>;
typedef std::list<host> list_host;
using set_host = std::set<host>;
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_HOST_HH
