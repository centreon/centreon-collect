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

#ifndef CCE_CONFIGURATION_STATE_HH
#define CCE_CONFIGURATION_STATE_HH

#include <rapidjson/document.h>
#include <ryml/ryml.hpp>
#include "anomalydetection.hh"
#include "command.hh"
#include "connector.hh"
#include "contact.hh"
#include "contactgroup.hh"
#include "host.hh"
#include "hostdependency.hh"
#include "hostescalation.hh"
#include "hostgroup.hh"
#include "service.hh"
#include "servicedependency.hh"
#include "serviceescalation.hh"
#include "servicegroup.hh"
#include "severity.hh"
#include "tag.hh"
#include "timeperiod.hh"

namespace com::centreon::engine::configuration {

class state;

class setter_base {
 protected:
  const std::string_view _field_name;

 public:
  setter_base(const std::string_view& field_name) : _field_name(field_name) {}

  virtual ~setter_base() = default;
  virtual bool apply_from_cfg(state& obj, const char* value) = 0;
  virtual bool apply_from_json(state& obj, const rapidjson::Document& doc) = 0;
};

/**
 *  @class state state.hh
 *  @brief Simple configuration state class.
 *
 *  Simple configuration state class used by Centreon Engine
 *  to manage configuration data.
 */
class state {
  enum logger_type {
    LOG_ALL = 2096895ull,
    DBG_ALL = (4095ull << 32),
    ALL = LOG_ALL | DBG_ALL,
  };
  enum logger_verbosity_level {
    BASIC = 0u,
    MORE = 1u,
    MOST = 2u,
  };

  std::shared_ptr<spdlog::logger> _logger;

 public:
  struct sched_info_config {
    double host_inter_check_delay;
    double service_inter_check_delay;
    int32_t service_interleave_factor;
  } _scheduling_info;

  /**
   *  @enum state::date_format
   *  Date format types
   */
  enum date_type {
    us = 0,         // U.S. (MM-DD-YYYY HH:MM:SS)
    euro,           // European (DD-MM-YYYY HH:MM:SS)
    iso8601,        // ISO8601 (YYYY-MM-DD HH:MM:SS)
    strict_iso8601  // ISO8601 (YYYY-MM-DDTHH:MM:SS)
  };

  /**
   *  @enum state::inter_check_delay
   *  Inter-check delay calculation types
   */
  enum inter_check_delay {
    icd_none = 0,  // no inter-check delay
    icd_dumb,      // dumb delay of 1 second
    icd_smart,     // smart delay
    icd_user       // user-specified delay
  };

  /**
   *  @enum state::interleave_factor
   *  Interleave factor calculation types
   */
  enum interleave_factor {
    ilf_user = 0,  // user-specified interleave factor
    ilf_smart      // smart interleave
  };

  state();
  state(state const& right);
  ~state() noexcept = default;
  state& operator=(state const& right);
  bool operator==(state const& right) const noexcept;
  bool operator!=(state const& right) const noexcept;
  bool accept_passive_host_checks() const noexcept;
  void accept_passive_host_checks(bool value);
  bool accept_passive_service_checks() const noexcept;
  void accept_passive_service_checks(bool value);
  int additional_freshness_latency() const noexcept;
  void additional_freshness_latency(int value);
  std::string const& admin_email() const noexcept;
  void admin_email(std::string const& value);
  std::string const& admin_pager() const noexcept;
  void admin_pager(std::string const& value);
  bool allow_empty_hostgroup_assignment() const noexcept;
  void allow_empty_hostgroup_assignment(bool value);
  bool auto_reschedule_checks() const noexcept;
  void auto_reschedule_checks(bool value);
  unsigned int auto_rescheduling_interval() const noexcept;
  void auto_rescheduling_interval(unsigned int value);
  unsigned int auto_rescheduling_window() const noexcept;
  void auto_rescheduling_window(unsigned int value);
  std::list<std::string> const& broker_module() const noexcept;
  void broker_module(std::list<std::string> const& value);
  std::string const& broker_module_directory() const noexcept;
  void broker_module_directory(std::string const& value);
  unsigned long cached_host_check_horizon() const noexcept;
  void cached_host_check_horizon(unsigned long value);
  unsigned long cached_service_check_horizon() const noexcept;
  void cached_service_check_horizon(unsigned long value);
  std::list<std::string> const& cfg_dir() const noexcept;
  std::list<std::string> const& cfg_file() const noexcept;
  std::string const& cfg_main() const noexcept;
  void cfg_main(std::string const& value);
  bool check_external_commands() const noexcept;
  void check_external_commands(bool value);
  bool check_host_freshness() const noexcept;
  void check_host_freshness(bool value);
  bool check_orphaned_hosts() const noexcept;
  void check_orphaned_hosts(bool value);
  void check_orphaned_services(bool value);
  bool check_orphaned_services() const noexcept;
  unsigned int check_reaper_interval() const noexcept;
  void check_reaper_interval(unsigned int value);
  bool check_service_freshness() const noexcept;
  void check_service_freshness(bool value);
  const set_severity& severities() const noexcept;
  set_severity& mut_severities() noexcept;
  const set_tag& tags() const noexcept;
  set_tag& mut_tags() noexcept;
  set_command const& commands() const noexcept;
  set_command& commands() noexcept;
  set_command::const_iterator commands_find(command::key_type const& k) const;
  set_command::iterator commands_find(command::key_type const& k);
  int command_check_interval() const noexcept;
  void command_check_interval(int value);
  void command_check_interval(int value, bool is_second);
  bool command_check_interval_is_seconds() const noexcept;
  std::string const& command_file() const noexcept;
  void command_file(std::string const& value);
  set_connector const& connectors() const noexcept;
  set_connector& connectors() noexcept;
  set_connector::const_iterator connectors_find(
      connector::key_type const& k) const;
  set_connector::iterator connectors_find(connector::key_type const& k);
  set_contact const& contacts() const noexcept;
  set_contact& contacts() noexcept;
  set_contact::const_iterator contacts_find(contact::key_type const& k) const;
  set_contact::iterator contacts_find(contact::key_type const& k);
  set_contactgroup const& contactgroups() const noexcept;
  set_contactgroup& contactgroups() noexcept;
  set_contactgroup::const_iterator contactgroups_find(
      contactgroup::key_type const& k) const;
  set_contactgroup::iterator contactgroups_find(
      contactgroup::key_type const& k);
  date_type date_format() const noexcept;
  void date_format(date_type value);
  std::string const& debug_file() const noexcept;
  void debug_file(std::string const& value);
  int64_t debug_level() const noexcept;
  void debug_level(int64_t value);
  unsigned int debug_verbosity() const noexcept;
  void debug_verbosity(unsigned int value);
  bool enable_environment_macros() const noexcept;
  void enable_environment_macros(bool value);
  bool enable_event_handlers() const noexcept;
  void enable_event_handlers(bool value);
  bool enable_flap_detection() const noexcept;
  void enable_flap_detection(bool value);
  bool enable_macros_filter() const noexcept;
  void enable_macros_filter(bool value);
  bool enable_notifications() const noexcept;
  void enable_notifications(bool value);
  bool enable_predictive_host_dependency_checks() const noexcept;
  void enable_predictive_host_dependency_checks(bool value);
  bool enable_predictive_service_dependency_checks() const noexcept;
  void enable_predictive_service_dependency_checks(bool value);
  unsigned long event_broker_options() const noexcept;
  void event_broker_options(unsigned long value);
  unsigned int event_handler_timeout() const noexcept;
  void event_handler_timeout(unsigned int value);
  bool execute_host_checks() const noexcept;
  void execute_host_checks(bool value);
  bool execute_service_checks() const noexcept;
  void execute_service_checks(bool value);
  int external_command_buffer_slots() const noexcept;
  void external_command_buffer_slots(int value);
  std::string const& global_host_event_handler() const noexcept;
  void global_host_event_handler(std::string const& value);
  std::string const& global_service_event_handler() const noexcept;
  void global_service_event_handler(std::string const& value);
  float high_host_flap_threshold() const noexcept;
  void high_host_flap_threshold(float value);
  float high_service_flap_threshold() const noexcept;
  void high_service_flap_threshold(float value);
  set_hostdependency const& hostdependencies() const noexcept;
  set_hostdependency& hostdependencies() noexcept;
  set_hostescalation const& hostescalations() const noexcept;
  set_hostescalation& hostescalations() noexcept;
  set_hostgroup const& hostgroups() const noexcept;
  set_hostgroup& hostgroups() noexcept;
  set_hostgroup::const_iterator hostgroups_find(
      hostgroup::key_type const& k) const;
  set_hostgroup::iterator hostgroups_find(hostgroup::key_type const& k);
  set_host const& hosts() const noexcept;
  set_host& hosts() noexcept;
  set_host::const_iterator hosts_find(host::key_type const& k) const;
  set_host::iterator hosts_find(host::key_type const& k);
  set_host::const_iterator hosts_find(std::string const& name) const;
  unsigned int host_check_timeout() const noexcept;
  void host_check_timeout(unsigned int value);
  unsigned int host_freshness_check_interval() const noexcept;
  void host_freshness_check_interval(unsigned int value);
  inter_check_delay host_inter_check_delay_method() const noexcept;
  void host_inter_check_delay_method(inter_check_delay value);
  std::string const& illegal_object_chars() const noexcept;
  void illegal_object_chars(std::string const& value);
  std::string const& illegal_output_chars() const noexcept;
  void illegal_output_chars(std::string const& value);
  unsigned int interval_length() const noexcept;
  void interval_length(unsigned int value);
  bool log_event_handlers() const noexcept;
  void log_event_handlers(bool value);
  bool log_external_commands() const noexcept;
  void log_external_commands(bool value);
  std::string const& log_file() const noexcept;
  void log_file(std::string const& value);
  bool log_host_retries() const noexcept;
  void log_host_retries(bool value);
  bool log_notifications() const noexcept;
  void log_notifications(bool value);
  bool log_passive_checks() const noexcept;
  void log_passive_checks(bool value);
  bool log_pid() const noexcept;
  void log_pid(bool value);
  inline bool log_file_line() const { return _log_file_line; }
  void log_file_line(bool value);
  bool log_service_retries() const noexcept;
  void log_service_retries(bool value);
  float low_host_flap_threshold() const noexcept;
  void low_host_flap_threshold(float value);
  float low_service_flap_threshold() const noexcept;
  void low_service_flap_threshold(float value);
  void macros_filter(std::string const& value);
  std::set<std::string> const& macros_filter() const;
  unsigned long max_debug_file_size() const noexcept;
  void max_debug_file_size(unsigned long value);
  unsigned int max_host_check_spread() const noexcept;
  void max_host_check_spread(unsigned int value);
  unsigned long max_log_file_size() const noexcept;
  void max_log_file_size(unsigned long value);
  uint32_t log_flush_period() const noexcept;
  void log_flush_period(uint32_t value);
  unsigned int max_parallel_service_checks() const noexcept;
  void max_parallel_service_checks(unsigned int value);
  unsigned int max_service_check_spread() const noexcept;
  void max_service_check_spread(unsigned int value);
  unsigned int notification_timeout() const noexcept;
  void notification_timeout(unsigned int value);
  bool obsess_over_hosts() const noexcept;
  void obsess_over_hosts(bool value);
  bool obsess_over_services() const noexcept;
  void obsess_over_services(bool value);
  std::string const& ochp_command() const noexcept;
  void ochp_command(std::string const& value);
  unsigned int ochp_timeout() const noexcept;
  void ochp_timeout(unsigned int value);
  std::string const& ocsp_command() const noexcept;
  void ocsp_command(std::string const& value);
  unsigned int ocsp_timeout() const noexcept;
  void ocsp_timeout(unsigned int value);
  int perfdata_timeout() const noexcept;
  void perfdata_timeout(int value);
  std::string const& poller_name() const noexcept;
  void poller_name(std::string const& value);
  uint32_t poller_id() const noexcept;
  void poller_id(uint32_t value);
  uint16_t rpc_port() const noexcept;
  void rpc_port(uint16_t value);
  const std::string& rpc_listen_address() const noexcept;
  void rpc_listen_address(const std::string& listen_address);
  bool process_performance_data() const noexcept;
  void process_performance_data(bool value);
  std::list<std::string> const& resource_file() const noexcept;
  void resource_file(std::list<std::string> const& value);
  unsigned long retained_contact_host_attribute_mask() const noexcept;
  void retained_contact_host_attribute_mask(unsigned long value);
  unsigned long retained_contact_service_attribute_mask() const noexcept;
  void retained_contact_service_attribute_mask(unsigned long value);
  unsigned long retained_host_attribute_mask() const noexcept;
  void retained_host_attribute_mask(unsigned long value);
  unsigned long retained_process_host_attribute_mask() const noexcept;
  void retained_process_host_attribute_mask(unsigned long value);
  bool retain_state_information() const noexcept;
  void retain_state_information(bool value);
  unsigned int retention_scheduling_horizon() const noexcept;
  void retention_scheduling_horizon(unsigned int value);
  unsigned int retention_update_interval() const noexcept;
  void retention_update_interval(unsigned int value);
  set_servicedependency const& servicedependencies() const noexcept;
  set_servicedependency& servicedependencies() noexcept;
  set_serviceescalation const& serviceescalations() const noexcept;
  set_serviceescalation& serviceescalations() noexcept;
  set_servicegroup const& servicegroups() const noexcept;
  set_servicegroup& servicegroups() noexcept;
  set_servicegroup::const_iterator servicegroups_find(
      servicegroup::key_type const& k) const;
  set_servicegroup::iterator servicegroups_find(
      servicegroup::key_type const& k);
  // const set_anomalydetection& anomalydetections() const noexcept;
  set_anomalydetection& anomalydetections() noexcept;
  set_service const& services() const noexcept;
  set_service& mut_services() noexcept;
  set_anomalydetection::iterator anomalydetections_find(
      anomalydetection::key_type const& k);
  set_service::iterator services_find(service::key_type const& k);
  set_service::const_iterator services_find(
      std::string const& host_name,
      std::string const& service_desc) const;
  set_severity::iterator severities_find(const severity::key_type& k);
  set_tag::iterator tags_find(const tag::key_type& k);
  unsigned int service_check_timeout() const noexcept;
  void service_check_timeout(unsigned int value);
  unsigned int service_freshness_check_interval() const noexcept;
  void service_freshness_check_interval(unsigned int value);
  inter_check_delay service_inter_check_delay_method() const noexcept;
  void service_inter_check_delay_method(inter_check_delay value);
  interleave_factor service_interleave_factor_method() const noexcept;
  void service_interleave_factor_method(interleave_factor value);
  float sleep_time() const noexcept;
  void sleep_time(float value);
  bool soft_state_dependencies() const noexcept;
  void soft_state_dependencies(bool value);
  std::string const& state_retention_file() const noexcept;
  void state_retention_file(std::string const& value);
  std::string const& status_file() const noexcept;
  void status_file(std::string const& value);
  unsigned int status_update_interval() const noexcept;
  void status_update_interval(unsigned int value);
  bool set(char const* key, char const* value);
  set_timeperiod const& timeperiods() const noexcept;
  set_timeperiod& timeperiods() noexcept;
  set_timeperiod::const_iterator timeperiods_find(
      timeperiod::key_type const& k) const;
  set_timeperiod::iterator timeperiods_find(timeperiod::key_type const& k);
  unsigned int time_change_threshold() const noexcept;
  void time_change_threshold(unsigned int value);
  std::unordered_map<std::string, std::string> const& user() const noexcept;
  void user(std::unordered_map<std::string, std::string> const& value);
  void user(std::string const& key, std::string const& value);
  void user(unsigned int key, std::string const& value);
  void use_aggressive_host_checking(bool);
  bool use_large_installation_tweaks() const noexcept;
  void use_large_installation_tweaks(bool value);
  uint32_t instance_heartbeat_interval() const noexcept;
  void instance_heartbeat_interval(uint32_t value);
  bool use_regexp_matches() const noexcept;
  void use_regexp_matches(bool value);
  bool use_retained_program_state() const noexcept;
  void use_retained_program_state(bool value);
  bool use_retained_scheduling_info() const noexcept;
  void use_retained_scheduling_info(bool value);
  bool use_setpgid() const noexcept;
  void use_setpgid(bool value);
  bool use_syslog() const noexcept;
  void use_syslog(bool value);
  bool log_v2_enabled() const noexcept;
  void log_v2_enabled(bool value);
  bool log_legacy_enabled() const noexcept;
  void log_legacy_enabled(bool value);
  std::string const& log_v2_logger() const noexcept;
  void log_v2_logger(std::string const& value);
  std::string const& log_level_functions() const noexcept;
  void log_level_functions(std::string const& value);
  std::string const& log_level_config() const noexcept;
  void log_level_config(std::string const& value);
  std::string const& log_level_events() const noexcept;
  void log_level_events(std::string const& value);
  std::string const& log_level_checks() const noexcept;
  void log_level_checks(std::string const& value);
  std::string const& log_level_notifications() const noexcept;
  void log_level_notifications(std::string const& value);
  std::string const& log_level_eventbroker() const noexcept;
  void log_level_eventbroker(std::string const& value);
  std::string const& log_level_external_command() const noexcept;
  void log_level_external_command(std::string const& value);
  std::string const& log_level_commands() const noexcept;
  void log_level_commands(std::string const& value);
  std::string const& log_level_downtimes() const noexcept;
  void log_level_downtimes(std::string const& value);
  std::string const& log_level_comments() const noexcept;
  void log_level_comments(std::string const& value);
  std::string const& log_level_macros() const noexcept;
  void log_level_macros(std::string const& value);
  std::string const& log_level_process() const noexcept;
  void log_level_process(std::string const& value);
  std::string const& log_level_runtime() const noexcept;
  void log_level_runtime(std::string const& value);
  std::string const& log_level_otl() const noexcept;
  void log_level_otl(std::string const& value);
  std::string const& use_timezone() const noexcept;
  void use_timezone(std::string const& value);
  bool use_true_regexp_matching() const noexcept;
  void use_true_regexp_matching(bool value);
  sched_info_config& sched_info_config() { return _scheduling_info; }
  bool use_send_recovery_notifications_anyways() const;
  void use_send_recovery_notifications_anyways(bool value);
  bool use_host_down_disable_service_checks() const;
  void use_host_down_disable_service_checks(bool value);
  uint32_t max_file_descriptors() const;
  void max_file_descriptors(uint32_t value);

  using setter_map =
      absl::flat_hash_map<std::string_view, std::unique_ptr<setter_base>>;
  static const setter_map& get_setters() { return _setters; }

  void apply_extended_conf(const std::string& file_path,
                           const rapidjson::Document& json_doc);

 private:
  static void _init_setter();
  void _set_broker_module(std::string const& value);
  void _set_cfg_dir(std::string const& value);
  void _set_cfg_file(std::string const& value);
  void _set_command_check_interval(std::string const& value);
  void _set_date_format(std::string const& value);
  void _set_event_broker_options(std::string const& value);
  void _set_host_inter_check_delay_method(std::string const& value);
  void _set_object_cache_file(std::string const& value);
  void _set_resource_file(std::string const& value);
  void _set_service_inter_check_delay_method(std::string const& value);
  void _set_service_interleave_factor_method(std::string const& value);

  bool _accept_passive_host_checks;
  bool _accept_passive_service_checks;
  int _additional_freshness_latency;
  std::string _admin_email;
  std::string _admin_pager;
  bool _allow_empty_hostgroup_assignment;
  bool _auto_reschedule_checks;
  unsigned int _auto_rescheduling_interval;
  unsigned int _auto_rescheduling_window;
  std::list<std::string> _broker_module;
  std::string _broker_module_directory;
  unsigned long _cached_host_check_horizon;
  unsigned long _cached_service_check_horizon;
  std::list<std::string> _cfg_dir;
  std::list<std::string> _cfg_file;
  std::string _cfg_main;
  bool _check_external_commands;
  bool _check_host_freshness;
  bool _check_orphaned_hosts;
  bool _check_orphaned_services;
  unsigned int _check_reaper_interval;
  bool _check_service_freshness;
  set_command _commands;
  set_severity _severities;
  set_tag _tags;
  int _command_check_interval;
  bool _command_check_interval_is_seconds;
  std::string _command_file;
  set_connector _connectors;
  set_contactgroup _contactgroups;
  set_contact _contacts;
  date_type _date_format;
  std::string _debug_file;
  int64_t _debug_level;
  unsigned int _debug_verbosity;
  bool _enable_environment_macros;
  bool _enable_event_handlers;
  bool _enable_flap_detection;
  bool _enable_macros_filter;
  bool _enable_notifications;
  bool _enable_predictive_host_dependency_checks;
  bool _enable_predictive_service_dependency_checks;
  unsigned long _event_broker_options;
  unsigned int _event_handler_timeout;
  bool _execute_host_checks;
  bool _execute_service_checks;
  int _external_command_buffer_slots;
  std::string _global_host_event_handler;
  std::string _global_service_event_handler;
  float _high_host_flap_threshold;
  float _high_service_flap_threshold;
  set_hostdependency _hostdependencies;
  set_hostescalation _hostescalations;
  set_hostgroup _hostgroups;
  set_host _hosts;
  unsigned int _host_check_timeout;
  unsigned int _host_freshness_check_interval;
  inter_check_delay _host_inter_check_delay_method;
  std::string _illegal_object_chars;
  std::string _illegal_output_chars;
  unsigned int _interval_length;
  bool _log_event_handlers;
  bool _log_external_commands;
  std::string _log_file;
  bool _log_host_retries;
  bool _log_notifications;
  bool _log_passive_checks;
  bool _log_pid;
  bool _log_file_line;
  bool _log_service_retries;
  float _low_host_flap_threshold;
  float _low_service_flap_threshold;
  std::set<std::string> _macros_filter;
  unsigned long _max_debug_file_size;
  unsigned int _max_host_check_spread;
  unsigned long _max_log_file_size;
  uint32_t _log_flush_period;
  unsigned int _max_parallel_service_checks;
  unsigned int _max_service_check_spread;
  unsigned int _notification_timeout;
  bool _obsess_over_hosts;
  bool _obsess_over_services;
  std::string _ochp_command;
  unsigned int _ochp_timeout;
  std::string _ocsp_command;
  unsigned int _ocsp_timeout;
  int _perfdata_timeout;
  std::string _poller_name;
  uint32_t _poller_id;
  uint16_t _rpc_port;
  std::string _rpc_listen_address;
  bool _process_performance_data;
  std::list<std::string> _resource_file;
  unsigned long _retained_contact_host_attribute_mask;
  unsigned long _retained_contact_service_attribute_mask;
  unsigned long _retained_host_attribute_mask;
  unsigned long _retained_process_host_attribute_mask;
  bool _retain_state_information;
  unsigned int _retention_scheduling_horizon;
  unsigned int _retention_update_interval;
  set_servicedependency _servicedependencies;
  set_serviceescalation _serviceescalations;
  set_servicegroup _servicegroups;
  set_anomalydetection _anomalydetections;
  set_service _services;
  unsigned int _service_check_timeout;
  unsigned int _service_freshness_check_interval;
  inter_check_delay _service_inter_check_delay_method;
  interleave_factor _service_interleave_factor_method;
  static setter_map _setters;
  float _sleep_time;
  bool _soft_state_dependencies;
  std::string _state_retention_file;
  std::string _status_file;
  unsigned int _status_update_interval;
  set_timeperiod _timeperiods;
  unsigned int _time_change_threshold;
  std::unordered_map<std::string, std::string> _users;
  bool _use_large_installation_tweaks;
  uint32_t _instance_heartbeat_interval;
  bool _use_regexp_matches;
  bool _use_retained_program_state;
  bool _use_retained_scheduling_info;
  bool _use_setpgid;
  bool _use_syslog;
  bool _log_v2_enabled;
  bool _log_legacy_enabled;
  std::string _log_v2_logger;
  std::string _log_level_functions;
  std::string _log_level_config;
  std::string _log_level_events;
  std::string _log_level_checks;
  std::string _log_level_notifications;
  std::string _log_level_eventbroker;
  std::string _log_level_external_command;
  std::string _log_level_commands;
  std::string _log_level_downtimes;
  std::string _log_level_comments;
  std::string _log_level_macros;
  std::string _log_level_process;
  std::string _log_level_runtime;
  std::string _log_level_otl;
  std::string _use_timezone;
  bool _use_true_regexp_matching;
  bool _send_recovery_notifications_anyways;
  bool _host_down_disable_service_checks;
  uint32_t _max_file_descriptors;
};

}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_STATE_HH
