/**
 * Copyright 2011-2013,2015-2024 Centreon
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
#ifndef CCE_RETENTION_SERVICE_HH
#define CCE_RETENTION_SERVICE_HH

#include <optional>
#include "com/centreon/engine/customvariable.hh"
#include "com/centreon/engine/retention/object.hh"

namespace com::centreon::engine {

namespace retention {
class service : public object {
 public:
  service();
  service(type_id object_type);
  service(service const& right);
  ~service() noexcept override;
  service& operator=(service const& right);
  bool operator==(service const& right) const noexcept;
  bool operator!=(service const& right) const noexcept;
  bool set(char const* key, char const* value) override;

  std::optional<int> const& acknowledgement_type() const noexcept;
  std::optional<bool> const& active_checks_enabled() const noexcept;
  std::optional<std::string> const& check_command() const noexcept;
  std::optional<double> const& check_execution_time() const noexcept;
  std::optional<int> const& check_flapping_recovery_notification()
      const noexcept;
  std::optional<double> const& check_latency() const noexcept;
  std::optional<int> const& check_options() const noexcept;
  std::optional<std::string> const& check_period() const noexcept;
  std::optional<int> const& check_type() const noexcept;
  std::optional<int> const& current_attempt() const noexcept;
  std::optional<uint64_t> const& current_event_id() const noexcept;
  std::optional<uint64_t> const& current_notification_id() const noexcept;
  std::optional<int> const& current_notification_number() const noexcept;
  std::optional<uint64_t> const& current_problem_id() const noexcept;
  std::optional<int> const& current_state() const noexcept;
  map_customvar const& customvariables() const noexcept;
  std::optional<std::string> const& event_handler() const noexcept;
  std::optional<bool> const& event_handler_enabled() const noexcept;
  std::optional<bool> const& flap_detection_enabled() const noexcept;
  std::optional<bool> const& has_been_checked() const noexcept;
  uint64_t host_id() const noexcept;
  std::string const& host_name() const noexcept;
  std::optional<bool> const& is_flapping() const noexcept;
  std::optional<uint64_t> const& flapping_comment_id() const noexcept;
  std::optional<uint64_t> const& acknowledgement_comment_id() const noexcept;
  std::optional<time_t> const& last_acknowledgement() const noexcept;
  std::optional<time_t> const& last_check() const noexcept;
  std::optional<uint64_t> const& last_event_id() const noexcept;
  std::optional<time_t> const& last_hard_state() const noexcept;
  std::optional<time_t> const& last_hard_state_change() const noexcept;
  std::optional<time_t> const& last_notification() const noexcept;
  std::optional<uint64_t> const& last_problem_id() const noexcept;
  std::optional<time_t> const& last_state() const noexcept;
  std::optional<time_t> const& last_state_change() const noexcept;
  std::optional<time_t> const& last_time_critical() const noexcept;
  std::optional<time_t> const& last_time_ok() const noexcept;
  std::optional<time_t> const& last_time_unknown() const noexcept;
  std::optional<time_t> const& last_time_warning() const noexcept;
  std::optional<std::string> const& long_plugin_output() const noexcept;
  std::optional<unsigned int> const& max_attempts() const noexcept;
  std::optional<unsigned long> const& modified_attributes() const noexcept;
  std::optional<time_t> const& next_check() const noexcept;
  std::optional<unsigned int> const& normal_check_interval() const noexcept;
  std::optional<std::string> const& notification_period() const noexcept;
  std::optional<bool> const& notifications_enabled() const noexcept;
  std::optional<bool> const& notified_on_critical() const noexcept;
  std::optional<bool> const& notified_on_unknown() const noexcept;
  std::optional<bool> const& notified_on_warning() const noexcept;
  std::optional<int> const& obsess_over_service() const noexcept;
  std::optional<bool> const& passive_checks_enabled() const noexcept;
  std::optional<double> const& percent_state_change() const noexcept;
  std::optional<std::string> const& performance_data() const noexcept;
  std::optional<std::string> const& plugin_output() const noexcept;
  std::optional<bool> const& problem_has_been_acknowledged() const noexcept;
  std::optional<int> const& process_performance_data() const noexcept;
  std::optional<unsigned int> const& retry_check_interval() const noexcept;
  uint64_t service_id() const noexcept;
  std::string const& service_description() const noexcept;
  std::optional<std::vector<int> > const& state_history() const noexcept;
  std::optional<int> const& state_type() const noexcept;
  bool has_notifications() const;
  std::array<std::string, 6> notifications() const noexcept;

 private:
  struct setters {
    char const* name;
    bool (*func)(service&, char const*);
  };

  bool _set_acknowledgement_type(int value);
  bool _set_active_checks_enabled(bool value);
  bool _set_check_command(std::string const& value);
  bool _set_check_execution_time(double value);
  bool _set_check_flapping_recovery_notification(int value);
  bool _set_check_latency(double value);
  bool _set_check_options(int value);
  bool _set_check_period(std::string const& value);
  bool _set_check_type(int value);
  bool _set_current_attempt(int value);
  bool _set_current_event_id(uint64_t value);
  bool _set_current_notification_id(uint64_t value);
  bool _set_current_notification_number(int value);
  bool _set_current_problem_id(uint64_t value);
  bool _set_current_state(int value);
  bool _set_event_handler(std::string const& value);
  bool _set_event_handler_enabled(bool value);
  bool _set_failure_prediction_enabled(bool value);
  bool _set_flap_detection_enabled(bool value);
  bool _set_has_been_checked(bool value);
  bool _set_host_id(uint64_t value);
  bool _set_host_name(std::string const& value);
  bool _set_is_flapping(bool value);
  bool _set_flapping_comment_id(uint64_t value);
  bool _set_acknowledgement_comment_id(uint64_t value);
  bool _set_last_acknowledgement(time_t value);
  bool _set_last_check(time_t value);
  bool _set_last_event_id(uint64_t value);
  bool _set_last_hard_state(time_t value);
  bool _set_last_hard_state_change(time_t value);
  bool _set_last_notification(time_t value);
  bool _set_last_problem_id(uint64_t value);
  bool _set_last_state(time_t value);
  bool _set_last_state_change(time_t value);
  bool _set_last_time_critical(time_t value);
  bool _set_last_time_ok(time_t value);
  bool _set_last_time_unknown(time_t value);
  bool _set_last_time_warning(time_t value);
  bool _set_long_plugin_output(std::string const& value);
  bool _set_max_attempts(unsigned int value);
  bool _set_modified_attributes(unsigned long value);
  bool _set_next_check(time_t value);
  bool _set_normal_check_interval(unsigned int value);

  template <int N>
  bool _set_notification(std::string const& value) {
    if (N < 6 && N >= 0) {
      _notification[N] = value;
      return true;
    } else
      return false;
  }

  bool _set_notification_period(std::string const& value);
  bool _set_notifications_enabled(bool value);
  bool _set_notified_on_critical(bool value);
  bool _set_notified_on_unknown(bool value);
  bool _set_notified_on_warning(bool value);
  bool _set_obsess_over_service(int value);
  bool _set_passive_checks_enabled(bool value);
  bool _set_percent_state_change(double value);
  bool _set_performance_data(std::string const& value);
  bool _set_plugin_output(std::string const& value);
  bool _set_problem_has_been_acknowledged(bool value);
  bool _set_process_performance_data(int value);
  bool _set_retry_check_interval(unsigned int value);
  bool _set_service_id(uint64_t value);
  bool _set_service_description(std::string const& value);
  bool _set_state_history(std::string const& value);
  bool _set_state_type(int value);

  std::optional<int> _acknowledgement_type;
  std::optional<bool> _active_checks_enabled;
  std::optional<std::string> _check_command;
  std::optional<double> _check_execution_time;
  std::optional<int> _check_flapping_recovery_notification;
  std::optional<double> _check_latency;
  std::optional<int> _check_options;
  std::optional<std::string> _check_period;
  std::optional<int> _check_type;
  std::optional<int> _current_attempt;
  std::optional<uint64_t> _current_event_id;
  std::optional<uint64_t> _current_notification_id;
  std::optional<int> _current_notification_number;
  std::optional<uint64_t> _current_problem_id;
  std::optional<int> _current_state;
  map_customvar _customvariables;
  std::optional<std::string> _event_handler;
  std::optional<bool> _event_handler_enabled;
  std::optional<bool> _flap_detection_enabled;
  std::optional<bool> _has_been_checked;
  uint64_t _host_id;
  std::string _host_name;
  std::optional<bool> _is_flapping;
  std::optional<uint64_t> _flapping_comment_id;
  std::optional<uint64_t> _acknowledgement_comment_id;
  std::optional<time_t> _last_acknowledgement;
  std::optional<time_t> _last_check;
  std::optional<uint64_t> _last_event_id;
  std::optional<time_t> _last_hard_state;
  std::optional<time_t> _last_hard_state_change;
  std::optional<time_t> _last_notification;
  std::optional<uint64_t> _last_problem_id;
  std::optional<time_t> _last_state;
  std::optional<time_t> _last_state_change;
  std::optional<time_t> _last_time_critical;
  std::optional<time_t> _last_time_ok;
  std::optional<time_t> _last_time_unknown;
  std::optional<time_t> _last_time_warning;
  std::optional<std::string> _long_plugin_output;
  std::optional<unsigned int> _max_attempts;
  std::optional<unsigned long> _modified_attributes;
  std::optional<time_t> _next_check;
  setters const* _next_setter;
  std::optional<unsigned int> _normal_check_interval;
  std::optional<std::string> _notification_period;
  std::optional<bool> _notifications_enabled;
  std::optional<bool> _notified_on_critical;
  std::optional<bool> _notified_on_unknown;
  std::optional<bool> _notified_on_warning;
  std::optional<int> _obsess_over_service;
  std::optional<bool> _passive_checks_enabled;
  std::optional<double> _percent_state_change;
  std::optional<std::string> _performance_data;
  std::optional<std::string> _plugin_output;
  std::optional<bool> _problem_has_been_acknowledged;
  std::optional<int> _process_performance_data;
  std::optional<unsigned int> _retry_check_interval;
  uint64_t _service_id;
  std::string _service_description;
  static setters const _setters[];
  std::optional<std::vector<int> > _state_history;
  std::optional<int> _state_type;
  std::array<std::string, 6> _notification;
};

typedef std::shared_ptr<service> service_ptr;
typedef std::list<service_ptr> list_service;
}  // namespace retention

}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_SERVICE_HH
