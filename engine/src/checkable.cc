/**
 * Copyright 2011-2019,2022-2024 Centreon
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
#include "com/centreon/engine/checkable.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

#include "com/centreon/engine/configuration/whitelist.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

checkable::checkable(const std::string& name,
                     const std::string& display_name,
                     const std::string& check_command,
                     bool checks_enabled,
                     bool accept_passive_checks,
                     uint32_t check_interval,
                     uint32_t retry_interval,
                     int max_attempts,
                     const std::string& check_period,
                     const std::string& event_handler,
                     bool event_handler_enabled,
                     const std::string& notes,
                     const std::string& notes_url,
                     const std::string& action_url,
                     const std::string& icon_image,
                     const std::string& icon_image_alt,
                     bool flap_detection_enabled,
                     double low_flap_threshold,
                     double high_flap_threshold,
                     bool check_freshness,
                     int freshness_threshold,
                     bool obsess_over,
                     const std::string& timezone,
                     uint64_t icon_id)
    : _name{name},
      _display_name{display_name.empty() ? name : display_name},
      _check_command{check_command},
      _check_interval{check_interval},
      _retry_interval{retry_interval},
      _max_attempts{max_attempts},
      _check_period{check_period},
      _event_handler{event_handler},
      _event_handler_enabled{event_handler_enabled},
      _action_url{action_url},
      _icon_image{icon_image},
      _icon_image_alt{icon_image_alt},
      _notes{notes},
      _notes_url{notes_url},
      _flap_detection_enabled{flap_detection_enabled},
      _low_flap_threshold{low_flap_threshold},
      _high_flap_threshold{high_flap_threshold},
      _obsess_over{obsess_over},
      _timezone{timezone},
      _checks_enabled{checks_enabled},
      _accept_passive_checks{accept_passive_checks},
      _check_freshness{check_freshness},
      _freshness_threshold{freshness_threshold},
      _check_type{check_active},
      _current_attempt{0},
      _has_been_checked{false},
      _scheduled_downtime_depth{0},
      _execution_time{0.0},
      _is_flapping{false},
      _last_check{0},
      _latency{0.0},
      _next_check{0L},
      _should_be_scheduled{true},
      _state_history_index{0},
      _last_state_change{0},
      _last_hard_state_change{0},
      _state_type{soft},
      _percent_state_change{0.0},
      _event_handler_ptr{nullptr},
      _check_command_ptr{nullptr},
      _is_executing{false},
      _icon_id{icon_id},
      check_period_ptr{nullptr} {
  if (max_attempts <= 0 || retry_interval <= 0 || freshness_threshold < 0) {
    std::ostringstream oss;
    bool empty{true};
    oss << "Error: In checkable '" << display_name << "' - ";
    if (max_attempts <= 0) {
      empty = false;
      oss << "Invalid max_attempts: value should be positive";
    }
    if (retry_interval <= 0) {
      if (!empty)
        oss << " - ";
      empty = false;
      oss << "Invalid retry_interval: value should be positive";
    }
    if (freshness_threshold < 0) {
      if (!empty)
        oss << " - ";
      oss << "Invalid freshness_threshold: value should be positive or 0";
    }
    engine_logger(log_config_error, basic) << oss.str();
    config_logger->error(oss.str());
    throw engine_error() << "Could not register checkable '" << display_name
                         << "'";
  }
}

const std::string& checkable::get_display_name() const {
  return _display_name;
}

void checkable::set_display_name(const std::string& display_name) {
  _display_name = display_name.empty() ? _name : display_name;
}

const std::string& checkable::check_command() const {
  return _check_command;
}

void checkable::set_check_command(const std::string& check_command) {
  _check_command = check_command;
}

uint32_t checkable::check_interval() const {
  return _check_interval;
}

void checkable::set_check_interval(uint32_t check_interval) {
  _check_interval = check_interval;
}

double checkable::retry_interval() const {
  return _retry_interval;
}

void checkable::set_retry_interval(double retry_interval) {
  _retry_interval = retry_interval;
}

time_t checkable::get_last_state_change() const {
  return _last_state_change;
}

void checkable::set_last_state_change(time_t last_state_change) {
  _last_state_change = last_state_change;
}

time_t checkable::get_last_hard_state_change() const {
  return _last_hard_state_change;
}

void checkable::set_last_hard_state_change(time_t last_hard_state_change) {
  _last_hard_state_change = last_hard_state_change;
}

int checkable::max_check_attempts() const {
  return _max_attempts;
}

void checkable::set_max_attempts(int max_attempts) {
  _max_attempts = max_attempts;
}

const std::string& checkable::check_period() const {
  return _check_period;
}

void checkable::set_check_period(const std::string& check_period) {
  _check_period = check_period;
}

const std::string& checkable::get_action_url() const {
  return _action_url;
}

void checkable::set_action_url(const std::string& action_url) {
  _action_url = action_url;
}

const std::string& checkable::get_icon_image() const {
  return _icon_image;
}

void checkable::set_icon_image(const std::string& icon_image) {
  _icon_image = icon_image;
}

const std::string& checkable::get_icon_image_alt() const {
  return _icon_image_alt;
}

void checkable::set_icon_image_alt(const std::string& icon_image_alt) {
  _icon_image_alt = icon_image_alt;
}

const std::string& checkable::event_handler() const {
  return _event_handler;
}

void checkable::set_event_handler(const std::string& event_handler) {
  _event_handler = event_handler;
}

const std::string& checkable::get_notes() const {
  return _notes;
}

void checkable::set_notes(const std::string& notes) {
  _notes = notes;
}

const std::string& checkable::get_notes_url() const {
  return _notes_url;
}

void checkable::set_notes_url(const std::string& notes_url) {
  _notes_url = notes_url;
}

const std::string& checkable::get_plugin_output() const {
  return _plugin_output;
}

void checkable::set_plugin_output(const std::string& plugin_output) {
  _plugin_output = plugin_output;
}

const std::string& checkable::get_long_plugin_output() const {
  return _long_plugin_output;
}

void checkable::set_long_plugin_output(const std::string& long_plugin_output) {
  _long_plugin_output = long_plugin_output;
}

const std::string& checkable::get_perf_data() const {
  return _perf_data;
}

void checkable::set_perf_data(const std::string& perf_data) {
  _perf_data = perf_data;
}

bool checkable::flap_detection_enabled() const {
  return _flap_detection_enabled;
}

void checkable::set_flap_detection_enabled(bool flap_detection_enabled) {
  _flap_detection_enabled = flap_detection_enabled;
}

double checkable::get_low_flap_threshold() const {
  return _low_flap_threshold;
}

void checkable::set_low_flap_threshold(double low_flap_threshold) {
  _low_flap_threshold = low_flap_threshold;
}

double checkable::get_high_flap_threshold() const {
  return _high_flap_threshold;
}

void checkable::set_high_flap_threshold(double high_flap_threshold) {
  _high_flap_threshold = high_flap_threshold;
}

const std::string& checkable::get_timezone() const {
  return _timezone;
}

void checkable::set_timezone(const std::string& timezone) {
  _timezone = timezone;
}

uint32_t checkable::get_state_history_index() const {
  return _state_history_index;
}

void checkable::set_state_history_index(uint32_t state_history_index) {
  _state_history_index = state_history_index;
}

bool checkable::active_checks_enabled() const {
  return _checks_enabled;
}

void checkable::set_checks_enabled(bool checks_enabled) {
  _checks_enabled = checks_enabled;
}

bool checkable::check_freshness_enabled() const {
  return _check_freshness;
}

void checkable::set_check_freshness(bool check_freshness) {
  _check_freshness = check_freshness;
}

enum checkable::check_type checkable::get_check_type() const {
  return _check_type;
}

void checkable::set_check_type(checkable::check_type check_type) {
  _check_type = check_type;
}

void checkable::set_current_attempt(int attempt) {
  _current_attempt = attempt;
}

int checkable::get_current_attempt() const {
  return _current_attempt;
}

void checkable::add_current_attempt(int num) {
  _current_attempt += num;
}

bool checkable::has_been_checked() const {
  return _has_been_checked;
}

void checkable::set_has_been_checked(bool has_been_checked) {
  _has_been_checked = has_been_checked;
}

bool checkable::event_handler_enabled() const {
  return _event_handler_enabled;
}

void checkable::set_event_handler_enabled(bool event_handler_enabled) {
  _event_handler_enabled = event_handler_enabled;
}

bool checkable::passive_checks_enabled() const {
  return _accept_passive_checks;
}

void checkable::set_accept_passive_checks(bool accept_passive_checks) {
  _accept_passive_checks = accept_passive_checks;
}

int checkable::get_scheduled_downtime_depth() const {
  return _scheduled_downtime_depth;
}

void checkable::set_scheduled_downtime_depth(
    int scheduled_downtime_depth) noexcept {
  _scheduled_downtime_depth = scheduled_downtime_depth;
}

void checkable::inc_scheduled_downtime_depth() noexcept {
  ++_scheduled_downtime_depth;
}

void checkable::dec_scheduled_downtime_depth() noexcept {
  --_scheduled_downtime_depth;
}

double checkable::get_execution_time() const {
  return _execution_time;
}

void checkable::set_execution_time(double execution_time) {
  _execution_time = execution_time;
}

int checkable::get_freshness_threshold() const {
  return _freshness_threshold;
}

void checkable::set_freshness_threshold(int freshness_threshold) {
  _freshness_threshold = freshness_threshold;
}

bool checkable::get_is_flapping() const {
  return _is_flapping;
}

void checkable::set_is_flapping(bool is_flapping) {
  _is_flapping = is_flapping;
}

std::time_t checkable::get_last_check() const {
  return _last_check;
}

void checkable::set_last_check(time_t last_check) {
  _last_check = last_check;
}

double checkable::get_latency() const {
  return _latency;
}

void checkable::set_latency(double latency) {
  _latency = latency;
}

std::time_t checkable::get_next_check() const {
  return _next_check;
}

void checkable::set_next_check(std::time_t next_check) {
  _next_check = next_check;
}

enum checkable::state_type checkable::get_state_type() const {
  return _state_type;
}

void checkable::set_state_type(enum checkable::state_type state_type) {
  _state_type = state_type;
}

double checkable::get_percent_state_change() const {
  return _percent_state_change;
}

void checkable::set_percent_state_change(double percent_state_change) {
  _percent_state_change = percent_state_change;
}

bool checkable::obsess_over() const {
  return _obsess_over;
}

void checkable::set_obsess_over(bool obsess_over) {
  _obsess_over = obsess_over;
}

bool checkable::get_should_be_scheduled() const {
  return _should_be_scheduled;
}

void checkable::set_should_be_scheduled(bool should_be_scheduled) {
  _should_be_scheduled = should_be_scheduled;
}

commands::command* checkable::get_event_handler_ptr() const {
  return _event_handler_ptr;
}

void checkable::set_event_handler_ptr(commands::command* cmd) {
  _event_handler_ptr = cmd;
}

void checkable::set_check_command_ptr(
    const std::shared_ptr<commands::command>& cmd) {
  _check_command_ptr = cmd;
}

bool checkable::get_is_executing() const {
  return _is_executing;
}

void checkable::set_is_executing(bool is_executing) {
  _is_executing = is_executing;
}

/**
 * @brief Set the severity. And we can also set nullptr if we don't want any
 * severity.
 *
 * @param severity The severity to associate to the resource.
 */
void checkable::set_severity(std::shared_ptr<severity> severity) {
  _severity = severity;
}

/**
 * @brief Set the icon_id.
 *
 * @param icon_id An unsigned long long integer.
 */
void checkable::set_icon_id(uint64_t icon_id) {
  _icon_id = icon_id;
}

/**
 * @brief Accessor to the icon_id.
 *
 * @return resource icon_id
 */
uint64_t checkable::get_icon_id() const {
  return _icon_id;
}

/**
 * @brief Accessor to the severity of the resource.
 *
 * @param severity The severity or nullptr if none.
 */
const std::shared_ptr<severity>& checkable::get_severity() const {
  return _severity;
}

/**
 * @brief Accessor to the icon_id of the resource.
 *
 * @return The resource icon_id.
 */
uint64_t checkable::icon_id() const {
  return _icon_id;
}

std::forward_list<std::shared_ptr<tag>>& checkable::mut_tags() {
  return _tags;
}

const std::forward_list<std::shared_ptr<tag>>& checkable::tags() const {
  return _tags;
}

const std::string& checkable::name() const {
  return _name;
}

void checkable::set_name(const std::string& name) {
  _name = name;
}

/**
 * @brief check if a command is allowed by whitelist
 * it tries to use last whitelist check result
 *
 * @param process_cmd final command line (macros replaced)
 * @param cached_cmd the static cached command (in case of a cache stored
 * in another place than in this). The cached command is writable to be updated
 * if needed. This method should be used with care, usually the other method
 * with the same name should be preferred.
 * @return A boolean true if allowed, false otherwise.
 */
bool checkable::command_is_allowed_by_whitelist(
    const std::string& process_cmd,
    static_whitelist_last_result& cached_cmd) {
  if (process_cmd == cached_cmd.command.process_cmd &&
      configuration::whitelist::instance().instance_id() ==
          cached_cmd.whitelist_instance_id) {
    return cached_cmd.command.allowed;
  }

  // something has changed => call whitelist
  cached_cmd.command.process_cmd = process_cmd;
  cached_cmd.command.allowed =
      configuration::whitelist::instance().is_allowed_engine(process_cmd);
  cached_cmd.whitelist_instance_id =
      configuration::whitelist::instance().instance_id();
  return cached_cmd.command.allowed;
}

/**
 * @brief check if a command is allowed by whitelist
 * it tries to use last whitelist check result
 *
 * @param process_cmd final command line (macros replaced)
 * @param typ a value among CHECK_TYPE, NOTIF_TYPE, EVH_TYPE or OBSESS_TYPE.
 * @return true allowed
 * @return false
 */
bool checkable::command_is_allowed_by_whitelist(const std::string& process_cmd,
                                                command_type typ) {
  auto& cmd = _whitelist_last_result.command[typ];
  if (process_cmd == cmd.process_cmd &&
      configuration::whitelist::instance().instance_id() ==
          _whitelist_last_result.whitelist_instance_id) {
    return cmd.allowed;
  }

  // something has changed => call whitelist
  cmd.process_cmd = process_cmd;
  cmd.allowed =
      configuration::whitelist::instance().is_allowed_engine(process_cmd);
  _whitelist_last_result.whitelist_instance_id =
      configuration::whitelist::instance().instance_id();
  return cmd.allowed;
}
