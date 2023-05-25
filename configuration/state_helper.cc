/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "configuration/state_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

/**
 * @brief Constructor from a State object.
 *
 * @param obj The State object on which this helper works. The helper is not the
 * owner of this object.
 */
state_helper::state_helper(State* obj)
    : message_helper(
          object_type::state,
          obj,
          {
              {"service_interleave_factor", "service_interleave_factor_method"},
              {"xcddefault_comment_file", "comment_file"},
              {"xdddefault_downtime_file", "downtime_file"},
          },
          2) {
  _init();
}

/**
 * @brief For several keys, the parser of State objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool state_helper::hook(absl::string_view key, const absl::string_view& value) {
  State* obj = static_cast<State*>(mut_obj());
  key = validate_key(key);

  if (key == "date_format") {
    if (value == "euro")
      obj->set_date_format(DateType::euro);
    else if (value == "iso8601")
      obj->set_date_format(DateType::iso8601);
    else if (value == "strict-iso8601")
      obj->set_date_format(DateType::strict_iso8601);
    else if (value == "us")
      obj->set_date_format(DateType::us);
    else
      return false;
    return true;
  } else if (key == "host_inter_check_delay_method") {
    if (value == "n")
      obj->mutable_host_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_none);
    else if (value == "d")
      obj->mutable_host_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_dumb);
    else if (value == "s")
      obj->mutable_host_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_smart);
    else {
      obj->mutable_host_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_user);
      double user_value;
      if (!absl::SimpleAtod(value, &user_value) || user_value <= 0.0)
        throw msg_fmt(
            "Invalid value for host_inter_check_delay_method, must be one of "
            "'n' (none), 'd' (dumb), 's' (smart) or a stricly positive value "
            "({} provided)",
            user_value);
      obj->mutable_host_inter_check_delay_method()->set_user_value(user_value);
    }
    return true;
  } else if (key == "service_inter_check_delay_method") {
    if (value == "n")
      obj->mutable_service_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_none);
    else if (value == "d")
      obj->mutable_service_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_dumb);
    else if (value == "s")
      obj->mutable_service_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_smart);
    else {
      obj->mutable_service_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_user);
      double user_value;
      if (!absl::SimpleAtod(value, &user_value) || user_value <= 0.0)
        throw msg_fmt(
            "Invalid value for service_inter_check_delay_method, must be one "
            "of 'n' (none), 'd' (dumb), 's' (smart) or a stricly positive "
            "value ({} provided)",
            user_value);
      obj->mutable_service_inter_check_delay_method()->set_user_value(
          user_value);
    }
    return true;
  }
  return false;
}

/**
 * @brief Check the validity of the State object.
 */
void state_helper::check_validity() const {
  const State* o = static_cast<const State*>(obj());
}

/**
 * @brief Initializer of the State object, in other words set its default
 * values.
 */
void state_helper::_init() {
  State* obj = static_cast<State*>(mut_obj());
  obj->set_accept_passive_service_checks(true);
  obj->set_additional_freshness_latency(15);
  obj->set_admin_email("");
  obj->set_admin_pager("");
  obj->set_auto_reschedule_checks(false);
  obj->set_auto_rescheduling_interval(30);
  obj->set_auto_rescheduling_window(180);
  obj->set_cached_host_check_horizon(15);
  obj->set_cached_service_check_horizon(15);
  obj->set_check_external_commands(true);
  obj->set_check_host_freshness(false);
  obj->set_check_orphaned_hosts(true);
  obj->set_check_orphaned_services(true);
  obj->set_check_reaper_interval(10);
  obj->set_check_service_freshness(true);
  obj->set_command_check_interval(-1);
  obj->set_command_file(DEFAULT_COMMAND_FILE);
  obj->set_date_format(DateType::us);
  obj->set_debug_file(DEFAULT_DEBUG_FILE);
  obj->set_debug_level(0);
  obj->set_debug_verbosity(1);
  obj->set_enable_environment_macros(false);
  obj->set_enable_event_handlers(true);
  obj->set_enable_flap_detection(false);
  obj->set_enable_macros_filter(false);
  obj->set_enable_notifications(true);
  obj->set_event_broker_options(std::numeric_limits<unsigned long>::max());
  obj->set_event_handler_timeout(30);
  obj->set_execute_host_checks(true);
  obj->set_execute_service_checks(true);
  obj->set_external_command_buffer_slots(4096);
  obj->set_global_host_event_handler("");
  obj->set_global_service_event_handler("");
  obj->set_high_host_flap_threshold(30.0);
  obj->set_high_service_flap_threshold(30.0);
  obj->set_host_check_timeout(30);
  obj->set_host_freshness_check_interval(60);
  obj->mutable_host_inter_check_delay_method()->set_type(
      InterCheckDelay_IcdType_smart);
  obj->set_illegal_object_chars("");
  obj->set_illegal_output_chars("`~$&|'\"<>");
  obj->set_interval_length(60);
  obj->set_log_event_handlers(true);
  obj->set_log_external_commands(true);
  obj->set_log_file(DEFAULT_LOG_FILE);
  obj->set_log_host_retries(false);
  obj->set_log_notifications(true);
  obj->set_log_passive_checks(true);
  obj->set_log_pid(true);
  obj->set_log_service_retries(false);
  obj->set_low_host_flap_threshold(20.0);
  obj->set_low_service_flap_threshold(20.0);
  obj->set_max_debug_file_size(1000000);
  obj->set_max_host_check_spread(5);
  obj->set_max_log_file_size(0);
  obj->set_max_parallel_service_checks(0);
  obj->set_max_service_check_spread(5);
  obj->set_notification_timeout(30);
  obj->set_obsess_over_hosts(false);
  obj->set_obsess_over_services(false);
  obj->set_ochp_command("");
  obj->set_ochp_timeout(15);
  obj->set_ocsp_command("");
  obj->set_ocsp_timeout(15);
  obj->set_perfdata_timeout(5);
  obj->set_poller_name("unknown");
  obj->set_process_performance_data(false);
  obj->set_retained_host_attribute_mask(0L);
  obj->set_retain_state_information(true);
  obj->set_retention_scheduling_horizon(900);
  obj->set_retention_update_interval(60);
  obj->set_service_check_timeout(60);
  obj->set_sleep_time(0.5);
  obj->set_soft_state_dependencies(false);
  obj->set_state_retention_file(DEFAULT_RETENTION_FILE);
  obj->set_status_file(DEFAULT_STATUS_FILE);
  obj->set_status_update_interval(60);
  obj->set_time_change_threshold(900);
  obj->set_use_large_installation_tweaks(false);
  obj->set_instance_heartbeat_interval(30);
  obj->set_use_regexp_matches(false);
  obj->set_use_retained_program_state(true);
  obj->set_use_retained_scheduling_info(false);
  obj->set_use_setpgid(true);
  obj->set_use_syslog(true);
  obj->set_log_v2_enabled(true);
  obj->set_log_legacy_enabled(true);
  obj->set_log_v2_logger("file");
  obj->set_log_level_functions("error");
  obj->set_log_level_config("info");
  obj->set_log_level_events("info");
  obj->set_log_level_checks("info");
  obj->set_log_level_notifications("error");
  obj->set_log_level_eventbroker("error");
  obj->set_log_level_external_command("error");
  obj->set_log_level_commands("error");
  obj->set_log_level_downtimes("error");
  obj->set_log_level_comments("error");
  obj->set_log_level_macros("error");
  obj->set_log_level_process("info");
  obj->set_log_level_runtime("error");
  obj->set_use_timezone("");
  obj->set_use_true_regexp_matching(false);
}
}  // namespace configuration
}  // namespace engine
}  // namespace centreon

}  // namespace com