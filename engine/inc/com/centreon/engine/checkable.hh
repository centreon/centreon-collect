/*
 * Copyright 2019,2024 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CHECKABLE_HH
#define CCE_CHECKABLE_HH

#include <ctime>
#include <forward_list>
#include <memory>
#include <string>

namespace com::centreon::engine {
namespace commands {
class command;
}
class timeperiod;
class severity;
class tag;

class checkable {
 public:
  enum check_type {
    check_active,  /* 0: Engine performed the check. */
    check_passive, /* 1: Check result submitted by an external source. */
  };

  enum state_type { soft, hard };

 private:
  /**
   * @brief we store in this struct the last result of whitelist
   * check in order to not check command line each time
   *
   */
  struct command_allowed {
    std::string process_cmd;
    bool allowed;
  };

  struct whitelist_last_result {
    unsigned whitelist_instance_id;
    /* We need a command for each type of command */
    std::array<command_allowed, 4> command;
  };

  std::string _name;
  std::string _display_name;
  std::string _check_command;
  uint32_t _check_interval;
  uint32_t _retry_interval;
  int _max_attempts;
  std::string _check_period;
  std::string _event_handler;
  bool _event_handler_enabled;
  std::string _action_url;
  std::string _icon_image;
  std::string _icon_image_alt;
  std::string _notes;
  std::string _notes_url;
  std::string _plugin_output;
  std::string _long_plugin_output;
  std::string _perf_data;
  bool _flap_detection_enabled;
  double _low_flap_threshold;
  double _high_flap_threshold;
  bool _obsess_over;
  std::string _timezone;
  bool _checks_enabled;
  bool _accept_passive_checks;
  bool _check_freshness;
  int _freshness_threshold;
  check_type _check_type;
  int _current_attempt;
  bool _has_been_checked;
  int _scheduled_downtime_depth;
  double _execution_time;
  bool _is_flapping;
  int _last_check;
  double _latency;
  std::time_t _next_check;
  bool _should_be_scheduled;
  uint32_t _state_history_index;
  std::time_t _last_state_change;
  std::time_t _last_hard_state_change;
  enum state_type _state_type;
  double _percent_state_change;
  commands::command* _event_handler_ptr;
  std::shared_ptr<commands::command> _check_command_ptr;
  bool _is_executing;
  std::shared_ptr<severity> _severity;
  uint64_t _icon_id;
  std::forward_list<std::shared_ptr<tag>> _tags;

  // whitelist cache
  whitelist_last_result _whitelist_last_result;

 public:
  /**
   * This structure is used by the static command_is_allowed_by_whitelist()
   * method. You have to maintain your own following structure and use it with
   * the static method. */
  struct static_whitelist_last_result {
    unsigned whitelist_instance_id;
    /* We need a command for each type of command */
    command_allowed command;
  };

  /* This enum is used to store the whitelist cache. A checkable can execute
   * all the following types of command, so for each one, we store if the
   * whitelist allows it or not. */
  enum command_type {
    CHECK_TYPE = 0,
    NOTIF_TYPE = 1,
    EVH_TYPE = 2,
    OBSESS_TYPE = 3,
  };

  checkable(const std::string& name,
            std::string const& display_name,
            std::string const& check_command,
            bool checks_enabled,
            bool accept_passive_checks,
            uint32_t check_interval,
            uint32_t retry_interval,
            int max_attempts,
            std::string const& check_period,
            std::string const& event_handler,
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
            uint64_t icon_id);
  virtual ~checkable() noexcept = default;

  std::string const& get_display_name() const;
  void set_display_name(std::string const& name);
  const std::string& name() const;
  virtual void set_name(const std::string& name);
  std::string const& check_command() const;
  void set_check_command(std::string const& check_command);
  uint32_t check_interval() const;
  void set_check_interval(uint32_t check_interval);
  double retry_interval() const;
  void set_retry_interval(double retry_interval);
  int max_check_attempts() const;
  void set_max_attempts(int max_attempts);
  std::string const& check_period() const;
  void set_check_period(std::string const& check_period);
  std::string const& event_handler() const;
  void set_event_handler(std::string const& event_handler);
  bool event_handler_enabled() const;
  void set_event_handler_enabled(bool event_handler_enabled);
  std::string const& get_action_url() const;
  void set_action_url(std::string const& action_url);
  std::string const& get_icon_image() const;
  void set_icon_image(std::string const& icon_image);
  std::string const& get_icon_image_alt() const;
  void set_icon_image_alt(std::string const& icon_image_alt);
  std::string const& get_notes() const;
  void set_notes(std::string const& notes);
  std::string const& get_notes_url() const;
  void set_notes_url(std::string const& notes_url);
  std::string const& get_plugin_output() const;
  void set_plugin_output(std::string const& plugin_output);
  std::string const& get_long_plugin_output() const;
  void set_long_plugin_output(std::string const& long_plugin_output);
  std::string const& get_perf_data() const;
  void set_perf_data(std::string const& perf_data);
  bool flap_detection_enabled() const;
  void set_flap_detection_enabled(bool flap_detection_enabled);
  double get_low_flap_threshold() const;
  void set_low_flap_threshold(double low_flap_threshold);
  double get_high_flap_threshold() const;
  void set_high_flap_threshold(double high_flap_threshold);
  bool obsess_over() const;
  void set_obsess_over(bool obsess_over_host);
  std::string const& get_timezone() const;
  void set_timezone(std::string const& timezone);
  bool active_checks_enabled() const;
  void set_checks_enabled(bool checks_enabled);
  bool passive_checks_enabled() const;
  void set_accept_passive_checks(bool accept_passive_checks);
  bool check_freshness_enabled() const;
  void set_check_freshness(bool check_freshness);
  std::time_t get_last_state_change() const;
  void set_last_state_change(std::time_t last_state_change);
  std::time_t get_last_hard_state_change() const;
  void set_last_hard_state_change(std::time_t last_hard_state_change);
  uint32_t get_state_history_index() const;
  void set_state_history_index(uint32_t state_history_index);
  enum checkable::check_type get_check_type() const;
  void set_check_type(check_type check_type);
  int get_current_attempt() const;
  void set_current_attempt(int current_attempt);
  void add_current_attempt(int num);
  bool has_been_checked() const;
  void set_has_been_checked(bool has_been_checked);
  int get_scheduled_downtime_depth() const;
  void set_scheduled_downtime_depth(int scheduled_downtime_depth) noexcept;
  void dec_scheduled_downtime_depth() noexcept;
  void inc_scheduled_downtime_depth() noexcept;
  double get_execution_time() const;
  void set_execution_time(double execution_time);
  int get_freshness_threshold() const;
  void set_freshness_threshold(int freshness_threshold);
  bool get_is_flapping() const;
  void set_is_flapping(bool is_flapping);
  enum state_type get_state_type() const;
  void set_state_type(enum state_type state_type);
  double get_percent_state_change() const;
  void set_percent_state_change(double percent_state_change);
  std::time_t get_last_check() const;
  void set_last_check(time_t last_check);
  double get_latency() const;
  void set_latency(double latency);
  std::time_t get_next_check() const;
  void set_next_check(std::time_t next_check);
  bool get_should_be_scheduled() const;
  void set_should_be_scheduled(bool should_be_scheduled);
  virtual std::string const& get_current_state_as_string() const = 0;
  virtual bool is_in_downtime() const = 0;
  void set_event_handler_ptr(commands::command* cmd);
  commands::command* get_event_handler_ptr() const;
  virtual void set_check_command_ptr(
      const std::shared_ptr<commands::command>& cmd);
  inline const std::shared_ptr<commands::command>& get_check_command_ptr()
      const {
    return _check_command_ptr;
  }
  bool get_is_executing() const;
  void set_is_executing(bool is_executing);
  void set_severity(std::shared_ptr<severity> sv);
  const std::shared_ptr<severity>& get_severity() const;
  void set_icon_id(uint64_t icon_id);
  uint64_t get_icon_id() const;
  uint64_t icon_id() const;
  std::forward_list<std::shared_ptr<tag>>& mut_tags();
  const std::forward_list<std::shared_ptr<tag>>& tags() const;

  bool command_is_allowed_by_whitelist(const std::string& process_cmd,
                                       command_type typ);
  static bool command_is_allowed_by_whitelist(
      const std::string& process_cmd,
      static_whitelist_last_result& cached_cmd);

  timeperiod* check_period_ptr;
};

}  // namespace com::centreon::engine

#endif /* !CCE_CHECKABLE */
