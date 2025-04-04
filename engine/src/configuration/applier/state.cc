/**
 * Copyright 2011-2025 Centreon
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

#include <absl/container/fixed_array.h>
#include <fmt/chrono.h>
#include <sys/resource.h>
#include <chrono>
#include <cstdint>

#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/commands/otel_connector.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/anomalydetection.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/connector.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/globals.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostdependency.hh"
#include "com/centreon/engine/configuration/applier/hostescalation.hh"
#include "com/centreon/engine/configuration/applier/hostgroup.hh"
#include "com/centreon/engine/configuration/applier/logging.hh"
#include "com/centreon/engine/configuration/applier/macros.hh"
#include "com/centreon/engine/configuration/applier/scheduler.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicedependency.hh"
#include "com/centreon/engine/configuration/applier/serviceescalation.hh"
#include "com/centreon/engine/configuration/applier/servicegroup.hh"
#include "com/centreon/engine/configuration/applier/severity.hh"
#include "com/centreon/engine/configuration/applier/tag.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/configuration/whitelist.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/broker_sink.hh"
#include "com/centreon/engine/retention/applier/state.hh"
#include "com/centreon/engine/version.hh"
#include "com/centreon/engine/xsddefault.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::logging;
using com::centreon::common::log_v2::log_v2;
using com::centreon::engine::logging::broker_sink_mt;

static bool has_already_been_loaded = false;

/**
 * @brief increase soft limit of opened file descriptors
 *
 * @param soft_fd_limit
 */
static void increase_fd_limit(uint32_t soft_fd_limit) {
  struct rlimit rlim;
  if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
    if (soft_fd_limit > rlim.rlim_cur) {
      if (soft_fd_limit > rlim.rlim_max) {
        SPDLOG_LOGGER_ERROR(
            process_logger,
            "Soft limit of opened file descriptors {} is greater than hard "
            "limit {}, we will set it to hard limit",
            soft_fd_limit, rlim.rlim_max);
        soft_fd_limit = rlim.rlim_max;
      }
      rlim.rlim_cur = soft_fd_limit;
      if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
        SPDLOG_LOGGER_ERROR(
            process_logger,
            "Failed to set soft limit of opened file descriptors "
            "to {}",
            soft_fd_limit);
      } else {
        SPDLOG_LOGGER_INFO(process_logger,
                           "Soft limit of opened file descriptors set to {}",
                           soft_fd_limit);
      }
    }
  } else {
    SPDLOG_LOGGER_ERROR(
        process_logger,
        "Failed to get current limit of opened file descriptors");
  }
}

/**
 *  Apply new protobuf configuration.
 *
 *  @param[in] new_cfg        The new protobuf configuration.
 *  @param[in] state          The retention to use.
 */
void applier::state::apply(configuration::State& new_cfg,
                           error_cnt& err,
                           retention::state* state) {
  configuration::indexed_state save(pb_indexed_config);
  try {
    _processing_state = state_ready;
    _processing(new_cfg, err, state);
  } catch (const std::exception& e) {
    // If is the first time to load configuration, we don't
    // have a valid configuration to restore.
    if (!has_already_been_loaded)
      throw;

    // If is not the first time, we can restore the old one.
    config_logger->error("Cannot apply new configuration: {}", e.what());

    // Check if we need to restore old configuration.
    if (_processing_state == state_error) {
      config_logger->debug("configuration: try to restore old configuration");
      auto old_state =
          std::unique_ptr<configuration::State>(pb_indexed_config.release());
      _processing(*old_state, err, state);
    }
  }
}

void applier::state::apply_diff(configuration::DiffState& diff_conf,
                                error_cnt& err,
                                retention::state* state [[maybe_unused]]) {
  configuration::indexed_state save(pb_indexed_config);
  try {
    _processing_state = state_ready;
    _processing_diff(diff_conf, err);
    std::ofstream f(proto_conf / "state.prot", std::ios::binary);
    pb_indexed_config.serialize_to_ostream(&f);
    f.close();
  } catch (const std::exception& e) {
    // If is the first time to load configuration, we don't
    // have a valid configuration to restore.
    if (!has_already_been_loaded)
      throw;

    // If is not the first time, we can restore the old one.
    config_logger->error("Cannot apply new configuration: {}", e.what());

    // Check if we need to restore old configuration.
    if (_processing_state == state_error) {
      auto old_state = std::unique_ptr<configuration::State>(save.release());
      config_logger->debug("configuration: try to restore old configuration");
      _processing(*old_state, err);
    }
  }
}

/**
 *  Get the singleton instance of state applier.
 *
 *  @return Singleton instance.
 */
applier::state& applier::state::instance() {
  static applier::state instance;
  return instance;
}

void applier::state::clear() {
  engine::contact::contacts.clear();
  engine::contactgroup::contactgroups.clear();
  engine::servicegroup::servicegroups.clear();
  engine::hostgroup::hostgroups.clear();
  engine::commands::command::commands.clear();
  engine::commands::connector::connectors.clear();
  engine::commands::otel_connector::clear();
  engine::service::services.clear();
  engine::service::services_by_id.clear();
  engine::servicedependency::servicedependencies.clear();
  engine::serviceescalation::serviceescalations.clear();
  engine::host::hosts.clear();
  engine::host::hosts_by_id.clear();
  engine::hostdependency::hostdependencies.clear();
  engine::hostescalation::hostescalations.clear();
  engine::timeperiod::timeperiods.clear();
  engine::comment::comments.clear();
  engine::comment::set_next_comment_id(1llu);

  applier::scheduler::instance().clear();
  applier::macros::instance().clear();
  applier::globals::instance().clear();
  applier::logging::instance().clear();

  _processing_state = state_ready;
}

/**
 *  Default constructor.
 */
applier::state::state() : _processing_state(state_ready) {}

/**
 *  Destructor.
 */
applier::state::~state() noexcept {
  engine::contact::contacts.clear();
  engine::contactgroup::contactgroups.clear();
  engine::servicegroup::servicegroups.clear();
  engine::hostgroup::hostgroups.clear();
  engine::commands::command::commands.clear();
  engine::commands::connector::connectors.clear();
  engine::commands::otel_connector::clear();
  engine::service::services.clear();
  engine::service::services_by_id.clear();
  engine::servicedependency::servicedependencies.clear();
  engine::serviceescalation::serviceescalations.clear();
  engine::host::hosts.clear();
  engine::host::hosts_by_id.clear();
  engine::hostdependency::hostdependencies.clear();
  engine::hostescalation::hostescalations.clear();
  engine::timeperiod::timeperiods.clear();
  engine::comment::comments.clear();
  engine::comment::set_next_comment_id(1llu);
}

/**
 *  Return the user macros.
 *
 *  @return  The user macros.
 */
absl::flat_hash_map<std::string, std::string>& applier::state::user_macros() {
  return _user_macros;
}

/**
 *  Find a user macro.
 *
 *  @param[in] key  The key.
 *
 *  @return  Iterator to user macros.
 */
absl::flat_hash_map<std::string, std::string>::const_iterator
applier::state::user_macros_find(std::string const& key) const {
  return _user_macros.find(key);
}

absl::flat_hash_map<std::string, std::string>::const_iterator
applier::state::user_macros_find(const std::string_view& key) const {
  return _user_macros.find(key);
}

/**
 *  Lock state
 *
 */
void applier::state::lock() {
  _apply_lock.lock();
}

/**
 *  Unlock state
 *
 */
void applier::state::unlock() {
  _apply_lock.unlock();
}

/*
 *  Update all new globals.
 *
 *  @param[in]  new_cfg The new configuration state.
 */
void applier::state::_apply(const configuration::State& new_cfg,
                            error_cnt& err) {
  // Check variables should not be change after the first execution.
  if (has_already_been_loaded) {
    if (!std::equal(pb_indexed_config.state().broker_module().begin(),
                    pb_indexed_config.state().broker_module().end(),
                    new_cfg.broker_module().begin(),
                    new_cfg.broker_module().end())) {
      config_logger->warn(
          "Warning: Broker modules cannot be changed nor reloaded");
      ++err.config_warnings;
    }
    if (pb_indexed_config.state().broker_module_directory() !=
        new_cfg.broker_module_directory()) {
      config_logger->warn("Warning: Broker module directory cannot be changed");
      ++err.config_warnings;
    }
    if (pb_indexed_config.state().command_file() != new_cfg.command_file()) {
      config_logger->warn("Warning: Command file cannot be changed");
      ++err.config_warnings;
    }
    if (pb_indexed_config.state().external_command_buffer_slots() !=
        new_cfg.external_command_buffer_slots()) {
      config_logger->warn(
          "Warning: External command buffer slots cannot be changed");
      ++err.config_warnings;
    }
    if (pb_indexed_config.state().use_timezone() != new_cfg.use_timezone()) {
      config_logger->warn("Warning: Timezone can not be changed");
      ++err.config_warnings;
    }
  }

  // Initialize status file.
  bool modify_status(false);
  if (!has_already_been_loaded ||
      pb_indexed_config.state().status_file() != new_cfg.status_file())
    modify_status = true;

  // Cleanup.
  //  if (modify_perfdata)
  //    xpddefault_cleanup_performance_data();
  if (modify_status)
    xsddefault_cleanup_status_data(true);

  // Set new values.
  pb_indexed_config.mut_state().set_accept_passive_host_checks(
      new_cfg.accept_passive_host_checks());
  pb_indexed_config.mut_state().set_accept_passive_service_checks(
      new_cfg.accept_passive_service_checks());
  pb_indexed_config.mut_state().set_additional_freshness_latency(
      new_cfg.additional_freshness_latency());
  pb_indexed_config.mut_state().set_admin_email(new_cfg.admin_email());
  pb_indexed_config.mut_state().set_admin_pager(new_cfg.admin_pager());
  pb_indexed_config.mut_state().set_allow_empty_hostgroup_assignment(
      new_cfg.allow_empty_hostgroup_assignment());
  pb_indexed_config.mut_state().set_cached_host_check_horizon(
      new_cfg.cached_host_check_horizon());
  pb_indexed_config.mut_state().set_cached_service_check_horizon(
      new_cfg.cached_service_check_horizon());
  pb_indexed_config.mut_state().set_cfg_main(new_cfg.cfg_main());
  pb_indexed_config.mut_state().set_check_external_commands(
      new_cfg.check_external_commands());
  pb_indexed_config.mut_state().set_check_host_freshness(
      new_cfg.check_host_freshness());
  pb_indexed_config.mut_state().set_check_orphaned_hosts(
      new_cfg.check_orphaned_hosts());
  pb_indexed_config.mut_state().set_check_orphaned_services(
      new_cfg.check_orphaned_services());
  pb_indexed_config.mut_state().set_check_reaper_interval(
      new_cfg.check_reaper_interval());
  pb_indexed_config.mut_state().set_check_service_freshness(
      new_cfg.check_service_freshness());
  pb_indexed_config.mut_state().set_command_check_interval(
      new_cfg.command_check_interval());
  pb_indexed_config.mut_state().set_command_check_interval_is_seconds(
      new_cfg.command_check_interval_is_seconds());
  pb_indexed_config.mut_state().set_date_format(new_cfg.date_format());
  pb_indexed_config.mut_state().set_debug_file(new_cfg.debug_file());
  pb_indexed_config.mut_state().set_debug_level(new_cfg.debug_level());
  pb_indexed_config.mut_state().set_debug_verbosity(new_cfg.debug_verbosity());
  pb_indexed_config.mut_state().set_enable_environment_macros(
      new_cfg.enable_environment_macros());
  pb_indexed_config.mut_state().set_enable_event_handlers(
      new_cfg.enable_event_handlers());
  pb_indexed_config.mut_state().set_enable_flap_detection(
      new_cfg.enable_flap_detection());
  pb_indexed_config.mut_state().set_enable_notifications(
      new_cfg.enable_notifications());
  pb_indexed_config.mut_state().set_enable_predictive_host_dependency_checks(
      new_cfg.enable_predictive_host_dependency_checks());
  pb_indexed_config.mut_state().set_enable_predictive_service_dependency_checks(
      new_cfg.enable_predictive_service_dependency_checks());
  pb_indexed_config.mut_state().set_event_broker_options(
      new_cfg.event_broker_options());
  pb_indexed_config.mut_state().set_event_handler_timeout(
      new_cfg.event_handler_timeout());
  pb_indexed_config.mut_state().set_execute_host_checks(
      new_cfg.execute_host_checks());
  pb_indexed_config.mut_state().set_execute_service_checks(
      new_cfg.execute_service_checks());
  pb_indexed_config.mut_state().set_global_host_event_handler(
      new_cfg.global_host_event_handler());
  pb_indexed_config.mut_state().set_global_service_event_handler(
      new_cfg.global_service_event_handler());
  pb_indexed_config.mut_state().set_high_host_flap_threshold(
      new_cfg.high_host_flap_threshold());
  pb_indexed_config.mut_state().set_high_service_flap_threshold(
      new_cfg.high_service_flap_threshold());
  pb_indexed_config.mut_state().set_host_check_timeout(
      new_cfg.host_check_timeout());
  pb_indexed_config.mut_state().set_host_freshness_check_interval(
      new_cfg.host_freshness_check_interval());
  pb_indexed_config.mut_state()
      .mutable_host_inter_check_delay_method()
      ->CopyFrom(new_cfg.host_inter_check_delay_method());
  pb_indexed_config.mut_state().set_illegal_object_chars(
      new_cfg.illegal_object_chars());
  pb_indexed_config.mut_state().set_illegal_output_chars(
      new_cfg.illegal_output_chars());
  pb_indexed_config.mut_state().set_interval_length(new_cfg.interval_length());
  pb_indexed_config.mut_state().set_log_event_handlers(
      new_cfg.log_event_handlers());
  pb_indexed_config.mut_state().set_log_external_commands(
      new_cfg.log_external_commands());
  pb_indexed_config.mut_state().set_log_file(new_cfg.log_file());
  pb_indexed_config.mut_state().set_log_host_retries(
      new_cfg.log_host_retries());
  pb_indexed_config.mut_state().set_log_notifications(
      new_cfg.log_notifications());
  pb_indexed_config.mut_state().set_log_passive_checks(
      new_cfg.log_passive_checks());
  pb_indexed_config.mut_state().set_log_service_retries(
      new_cfg.log_service_retries());
  pb_indexed_config.mut_state().set_low_host_flap_threshold(
      new_cfg.low_host_flap_threshold());
  pb_indexed_config.mut_state().set_low_service_flap_threshold(
      new_cfg.low_service_flap_threshold());
  pb_indexed_config.mut_state().set_max_debug_file_size(
      new_cfg.max_debug_file_size());
  pb_indexed_config.mut_state().set_max_host_check_spread(
      new_cfg.max_host_check_spread());
  pb_indexed_config.mut_state().set_max_log_file_size(
      new_cfg.max_log_file_size());
  pb_indexed_config.mut_state().set_max_parallel_service_checks(
      new_cfg.max_parallel_service_checks());
  pb_indexed_config.mut_state().set_max_service_check_spread(
      new_cfg.max_service_check_spread());
  pb_indexed_config.mut_state().set_notification_timeout(
      new_cfg.notification_timeout());
  pb_indexed_config.mut_state().set_obsess_over_hosts(
      new_cfg.obsess_over_hosts());
  pb_indexed_config.mut_state().set_obsess_over_services(
      new_cfg.obsess_over_services());
  pb_indexed_config.mut_state().set_ochp_command(new_cfg.ochp_command());
  pb_indexed_config.mut_state().set_ochp_timeout(new_cfg.ochp_timeout());
  pb_indexed_config.mut_state().set_ocsp_command(new_cfg.ocsp_command());
  pb_indexed_config.mut_state().set_ocsp_timeout(new_cfg.ocsp_timeout());
  pb_indexed_config.mut_state().set_perfdata_timeout(
      new_cfg.perfdata_timeout());
  pb_indexed_config.mut_state().set_process_performance_data(
      new_cfg.process_performance_data());
  pb_indexed_config.mut_state().mutable_resource_file()->CopyFrom(
      new_cfg.resource_file());
  pb_indexed_config.mut_state().set_retain_state_information(
      new_cfg.retain_state_information());
  pb_indexed_config.mut_state().set_retained_contact_host_attribute_mask(
      new_cfg.retained_contact_host_attribute_mask());
  pb_indexed_config.mut_state().set_retained_contact_service_attribute_mask(
      new_cfg.retained_contact_service_attribute_mask());
  pb_indexed_config.mut_state().set_retained_host_attribute_mask(
      new_cfg.retained_host_attribute_mask());
  pb_indexed_config.mut_state().set_retained_process_host_attribute_mask(
      new_cfg.retained_process_host_attribute_mask());
  pb_indexed_config.mut_state().set_retention_scheduling_horizon(
      new_cfg.retention_scheduling_horizon());
  pb_indexed_config.mut_state().set_retention_update_interval(
      new_cfg.retention_update_interval());
  pb_indexed_config.mut_state().set_service_check_timeout(
      new_cfg.service_check_timeout());
  pb_indexed_config.mut_state().set_service_freshness_check_interval(
      new_cfg.service_freshness_check_interval());
  pb_indexed_config.mut_state()
      .mutable_service_inter_check_delay_method()
      ->CopyFrom(new_cfg.service_inter_check_delay_method());
  pb_indexed_config.mut_state()
      .mutable_service_interleave_factor_method()
      ->CopyFrom(new_cfg.service_interleave_factor_method());
  pb_indexed_config.mut_state().set_sleep_time(new_cfg.sleep_time());
  pb_indexed_config.mut_state().set_soft_state_dependencies(
      new_cfg.soft_state_dependencies());
  pb_indexed_config.mut_state().set_state_retention_file(
      new_cfg.state_retention_file());
  pb_indexed_config.mut_state().set_status_file(new_cfg.status_file());
  pb_indexed_config.mut_state().set_status_update_interval(
      new_cfg.status_update_interval());
  pb_indexed_config.mut_state().set_time_change_threshold(
      new_cfg.time_change_threshold());
  pb_indexed_config.mut_state().set_use_large_installation_tweaks(
      new_cfg.use_large_installation_tweaks());
  pb_indexed_config.mut_state().set_instance_heartbeat_interval(
      new_cfg.instance_heartbeat_interval());
  pb_indexed_config.mut_state().set_use_regexp_matches(
      new_cfg.use_regexp_matches());
  pb_indexed_config.mut_state().set_use_retained_program_state(
      new_cfg.use_retained_program_state());
  pb_indexed_config.mut_state().set_use_retained_scheduling_info(
      new_cfg.use_retained_scheduling_info());
  pb_indexed_config.mut_state().set_use_setpgid(new_cfg.use_setpgid());
  pb_indexed_config.mut_state().set_use_syslog(new_cfg.use_syslog());
  pb_indexed_config.mut_state().set_log_v2_enabled(new_cfg.log_v2_enabled());
  pb_indexed_config.mut_state().set_log_legacy_enabled(
      new_cfg.log_legacy_enabled());
  pb_indexed_config.mut_state().set_log_v2_logger(new_cfg.log_v2_logger());
  pb_indexed_config.mut_state().set_log_level_functions(
      new_cfg.log_level_functions());
  pb_indexed_config.mut_state().set_log_level_config(
      new_cfg.log_level_config());
  pb_indexed_config.mut_state().set_log_level_events(
      new_cfg.log_level_events());
  pb_indexed_config.mut_state().set_log_level_checks(
      new_cfg.log_level_checks());
  pb_indexed_config.mut_state().set_log_level_notifications(
      new_cfg.log_level_notifications());
  pb_indexed_config.mut_state().set_log_level_eventbroker(
      new_cfg.log_level_eventbroker());
  pb_indexed_config.mut_state().set_log_level_external_command(
      new_cfg.log_level_external_command());
  pb_indexed_config.mut_state().set_log_level_commands(
      new_cfg.log_level_commands());
  pb_indexed_config.mut_state().set_log_level_downtimes(
      new_cfg.log_level_downtimes());
  pb_indexed_config.mut_state().set_log_level_comments(
      new_cfg.log_level_comments());
  pb_indexed_config.mut_state().set_log_level_macros(
      new_cfg.log_level_macros());
  pb_indexed_config.mut_state().set_log_level_otl(new_cfg.log_level_otl());
  pb_indexed_config.mut_state().set_log_level_runtime(
      new_cfg.log_level_runtime());
  pb_indexed_config.mut_state().set_use_true_regexp_matching(
      new_cfg.use_true_regexp_matching());
  pb_indexed_config.mut_state().set_send_recovery_notifications_anyways(
      new_cfg.send_recovery_notifications_anyways());
  pb_indexed_config.mut_state().set_host_down_disable_service_checks(
      new_cfg.host_down_disable_service_checks());
  pb_indexed_config.mut_state().clear_user();
  for (auto& p : new_cfg.user())
    pb_indexed_config.mut_state().mutable_user()->at(p.first) = p.second;

  if (pb_indexed_config.state().max_file_descriptors() !=
      new_cfg.max_file_descriptors()) {
    pb_indexed_config.mut_state().set_max_file_descriptors(
        new_cfg.max_file_descriptors());
    increase_fd_limit(new_cfg.max_file_descriptors());
  }

  // Set this variable just the first time.
  if (!has_already_been_loaded) {
    pb_indexed_config.mut_state().mutable_broker_module()->CopyFrom(
        new_cfg.broker_module());
    pb_indexed_config.mut_state().set_broker_module_directory(
        new_cfg.broker_module_directory());
    pb_indexed_config.mut_state().set_command_file(new_cfg.command_file());
    pb_indexed_config.mut_state().set_external_command_buffer_slots(
        new_cfg.external_command_buffer_slots());
    pb_indexed_config.mut_state().set_use_timezone(new_cfg.use_timezone());
  }

  // Initialize.
  if (modify_status)
    xsddefault_initialize_status_data();

  // Check global event handler commands...
  if (verify_config) {
    events_logger->info("Checking global event handlers...");
  }
  if (!pb_indexed_config.state().global_host_event_handler().empty()) {
    // Check the event handler command.
    std::string temp_command_name(
        pb_indexed_config.state().global_host_event_handler().substr(
            0,
            pb_indexed_config.state().global_host_event_handler().find_first_of(
                '!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      config_logger->error(
          "Error: Global host event handler command '{}' is not defined "
          "anywhere!",
          temp_command_name);
      ++err.config_errors;
      global_host_event_handler_ptr = nullptr;
    } else
      global_host_event_handler_ptr = found->second.get();
  }
  if (!pb_indexed_config.state().global_service_event_handler().empty()) {
    // Check the event handler command.
    std::string temp_command_name(
        pb_indexed_config.state().global_service_event_handler().substr(
            0, pb_indexed_config.state()
                   .global_service_event_handler()
                   .find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      config_logger->error(
          "Error: Global service event handler command '{}' is not defined "
          "anywhere!",
          temp_command_name);
      ++err.config_errors;
      global_service_event_handler_ptr = nullptr;
    } else
      global_service_event_handler_ptr = found->second.get();
  }

  // Check obsessive processor commands...
  if (verify_config) {
    events_logger->info("Checking obsessive compulsive processor commands...");
  }
  if (!pb_indexed_config.state().ocsp_command().empty()) {
    std::string temp_command_name(
        pb_indexed_config.state().ocsp_command().substr(
            0, pb_indexed_config.state().ocsp_command().find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Obsessive compulsive service processor command '"
          << temp_command_name << "' is not defined anywhere!";
      config_logger->error(
          "Error: Obsessive compulsive service processor command '{}' is not "
          "defined anywhere!",
          temp_command_name);
      ++err.config_errors;
      ocsp_command_ptr = nullptr;
    } else
      ocsp_command_ptr = found->second.get();
  }
  if (!pb_indexed_config.state().ochp_command().empty()) {
    std::string temp_command_name(
        pb_indexed_config.state().ochp_command().substr(
            0, pb_indexed_config.state().ochp_command().find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      config_logger->error(
          "Error: Obsessive compulsive host processor command '{}' is not "
          "defined anywhere!",
          temp_command_name);
      ++err.config_errors;
      ochp_command_ptr = nullptr;
    } else
      ochp_command_ptr = found->second.get();
  }
}

/**
 *  @brief Apply protobuf configuration of a specific object type.
 *
 *  This method will perform a diff on cur_cfg and new_cfg to create the
 *  three element sets : added, modified and removed. The type applier
 *  will then be called to:
 *  * 1) modify existing objects (the modification must be done in first since
 * remove and create changes indices).
 *  * 2) remove old objects
 *  * 3) create new objects
 *
 *  @param[in] cur_cfg Current configuration set.
 *  @param[in] new_cfg New configuration set.
 */
// template <typename ConfigurationType, typename Key, typename ApplierType>
// void applier::state::_apply(const pb_difference<ConfigurationType, Key>&
// diff,
//                             error_cnt& err) {
//   // Applier.
//   ApplierType aplyr;
//
//   // Modify objects.
//   for (auto& p : diff.modified()) {
//     if (!verify_config)
//       aplyr.modify_object(p.first, *p.second);
//     else {
//       try {
//         aplyr.modify_object(p.first, *p.second);
//       } catch (const std::exception& e) {
//         ++err.config_errors;
//         config_logger->info(e.what());
//       }
//     }
//   }
//
//   // Erase objects.
//   for (auto it = diff.deleted().rbegin(); it != diff.deleted().rend(); ++it)
//   {
//     if (!verify_config)
//       aplyr.remove_object(it->second);
//     else {
//       try {
//         aplyr.remove_object(it->second);
//       } catch (const std::exception& e) {
//         ++err.config_errors;
//         config_logger->info(e.what());
//       }
//     }
//   }
//
//   // Add objects.
//   for (auto& obj : diff.added()) {
//     if (!verify_config)
//       aplyr.add_object(*obj);
//     else {
//       try {
//         aplyr.add_object(*obj);
//       } catch (const std::exception& e) {
//         ++err.config_errors;
//         config_logger->info(e.what());
//       }
//     }
//   }
// }

#ifdef DEBUG_CONFIG
/**
 *  A method to check service escalations pointers of each service are well
 *  defined.
 *
 *  If something wrong is found, an exception is thrown.
 */
void applier::state::_check_serviceescalations() const {
  for (auto const& p : engine::service::services) {
    engine::service const* srv{p.second.get()};

    std::unordered_set<uint64_t> s;
    for (auto const& escalation : srv->get_escalations()) {
      s.insert((uint64_t) static_cast<void*>(escalation));
      bool found = false;
      for (auto const& e : engine::serviceescalation::serviceescalations) {
        if (e.second.get() == escalation) {
          found = true;
          break;
        }
      }
      if (!found) {
        engine_logger(log_config_error, basic)
            << "Error on serviceescalation !!! The service "
            << srv->get_hostname() << "/" << srv->get_description()
            << " contains a non existing service escalation";
        config_logger->error(
            "Error on serviceescalation !!! The service {}/{} contains a non "
            "existing service escalation",
            srv->get_hostname(), srv->get_description());
        throw engine_error() << "This is a bug";
      }
    }
    if (s.size() != srv->get_escalations().size()) {
      engine_logger(log_config_error, basic)
          << "Error on serviceescalation !!! Some escalations are stored "
             "several times in service "
          << srv->get_hostname() << "/" << srv->get_description()
          << "set size: " << s.size()
          << " ; list size: " << srv->get_escalations().size();
      config_logger->error(
          "Error on serviceescalation !!! Some escalations are stored "
          "several times in service {}/{} set size: {} ; list size: {}",
          srv->get_hostname(), srv->get_description(), s.size(),
          srv->get_escalations().size());
      throw engine_error() << "This is a bug";
    }
  }

  for (auto const& e : engine::serviceescalation::serviceescalations) {
    engine::serviceescalation const* se{e.second.get()};
    bool found = false;

    for (auto const& p : engine::service::services) {
      if (p.second.get() == se->notifier_ptr) {
        found = true;
        if (se->get_hostname() != p.second->get_hostname()) {
          engine_logger(log_config_error, basic)
              << "Error on serviceescalation !!! The notifier seen by the "
                 "escalation is wrong. "
              << "Host name given by the escalation is " << se->get_hostname()
              << " whereas the hostname from the notifier is "
              << p.second->get_hostname() << ".";
          config_logger->error(
              "Error on serviceescalation !!! The notifier seen by the "
              "escalation is wrong. Host name given by the escalation is {} "
              "whereas the hostname from the notifier is {}.",
              se->get_hostname(), p.second->get_hostname());
          throw engine_error() << "This is a bug";
        }
        if (se->get_description() != p.second->get_description()) {
          engine_logger(log_config_error, basic)
              << "Error on serviceescalation !!! The notifier seen by the "
                 "escalation is wrong. "
              << "Service description given by the escalation is "
              << se->get_description()
              << " whereas the service description from the notifier is "
              << p.second->get_description() << ".";
          config_logger->error(
              "Error on serviceescalation !!! The notifier seen by the "
              "escalation is wrong. Service description given by the "
              "escalation is {} whereas the service description from the "
              "notifier is {}.",
              se->get_description(), p.second->get_description());
          throw engine_error() << "This is a bug";
        }
        break;
      }
    }
    if (!found) {
      engine_logger(log_config_error, basic)
          << "Error on serviceescalation !!! The notifier seen by the "
             "escalation is wrong "
          << "The bug is detected on escalation concerning host "
          << se->get_hostname() << " and service " << se->get_description();
      config_logger->error(
          "Error on serviceescalation !!! The notifier seen by the "
          "escalation is wrong The bug is detected on escalation concerning "
          "host {} and service {}",
          se->get_hostname(), se->get_description());
      throw engine_error() << "This is a bug";
    }
  }
}

/**
 *  A method to check host escalations pointers of each host are well
 *  defined.
 *
 *  If something wrong is found, an exception is thrown.
 */
void applier::state::_check_hostescalations() const {
  for (auto const& p : engine::host::hosts) {
    engine::host const* hst{p.second.get()};

    for (auto const& escalation : hst->get_escalations()) {
      bool found = false;
      for (auto const& e : engine::hostescalation::hostescalations) {
        if (e.second.get() == escalation) {
          found = true;
          break;
        }
      }
      if (!found) {
        engine_logger(log_config_error, basic)
            << "Error on hostescalation !!! The host " << hst->get_name()
            << " contains a non existing host escalation";
        config_logger->error(
            "Error on hostescalation !!! The host {} contains a non existing "
            "host escalation",
            hst->get_name());
        throw engine_error() << "This is a bug";
      }
    }
  }

  for (auto const& e : engine::hostescalation::hostescalations) {
    engine::hostescalation const* he{e.second.get()};
    bool found = false;

    for (auto const& p : engine::host::hosts) {
      if (p.second.get() == he->notifier_ptr) {
        found = true;
        if (he->get_hostname() != p.second->get_name()) {
          engine_logger(log_config_error, basic)
              << "Error on hostescalation !!! The notifier seen by the "
                 "escalation is wrong. "
              << "Host name given by the escalation is " << he->get_hostname()
              << " whereas the hostname from the notifier is "
              << p.second->get_name() << ".";
          config_logger->error(
              "Error on hostescalation !!! The notifier seen by the escalation "
              "is wrong. Host name given by the escalation is {} whereas the "
              "hostname from the notifier is {}.",
              he->get_hostname(), p.second->get_name());
          throw engine_error() << "This is a bug";
        }
        break;
      }
    }
    if (!found) {
      engine_logger(log_config_error, basic)
          << "Error on hostescalation !!! The notifier seen by the escalation "
             "is wrong "
          << "The bug is detected on escalation concerning host "
          << he->get_hostname();
      config_logger->error(
          "Error on hostescalation !!! The notifier seen by the escalation is "
          "wrong The bug is detected on escalation concerning host {}",
          he->get_hostname());
      throw engine_error() << "This is a bug";
    }
  }
}

/**
 *  A method to check contacts pointers of each possible container are well
 *  defined.
 *
 *  If something wrong is found, an exception is thrown.
 */
void applier::state::_check_contacts() const {
  for (auto const& p : engine::contactgroup::contactgroups) {
    engine::contactgroup const* cg{p.second.get()};
    for (auto const& pp : cg->get_members()) {
      contact_map::iterator found{engine::contact::contacts.find(pp.first)};
      if (found == engine::contact::contacts.end() ||
          found->second.get() != pp.second) {
        engine_logger(log_config_error, basic)
            << "Error on contact !!! The contact " << pp.first
            << " used in contactgroup " << p.first
            << " is not or badly defined";
        config_logger->error(
            "Error on contact !!! The contact {} used in contactgroup {} is "
            "not or badly defined",
            pp.first, p.first);
        throw engine_error() << "This is a bug";
      }
    }
  }

  for (auto const& p : engine::service::services) {
    for (auto const& pp : p.second->get_contacts()) {
      contact_map::iterator found{engine::contact::contacts.find(pp.first)};
      if (found == engine::contact::contacts.end() ||
          found->second.get() != pp.second) {
        engine_logger(log_config_error, basic)
            << "Error on contact !!! The contact " << pp.first
            << " used in service " << p.second->get_hostname() << '/'
            << p.second->get_description() << " is not or badly defined";
        config_logger->error(
            "Error on contact !!! The contact {} used in service {}/{} is not "
            "or badly defined",
            pp.first, p.second->get_hostname(), p.second->get_description());
        throw engine_error() << "This is a bug";
      }
    }
  }

  for (auto const& p : engine::host::hosts) {
    for (auto const& pp : p.second->get_contacts()) {
      contact_map::iterator found{engine::contact::contacts.find(pp.first)};
      if (found == engine::contact::contacts.end() ||
          found->second.get() != pp.second) {
        engine_logger(log_config_error, basic)
            << "Error on contact !!! The contact " << pp.first
            << " used in service " << p.second->get_name()
            << " is not or badly defined";
        config_logger->error(
            "Error on contact !!! The contact {} used in service {} is not or "
            "badly defined",
            pp.first, p.second->get_name());
        throw engine_error() << "This is a bug";
      }
    }
  }
}

/**
 *  A method to check contactgroups pointers of each possible container are well
 *  defined.
 *
 *  If something wrong is found, an exception is thrown.
 */
void applier::state::_check_contactgroups() const {
  for (auto const& p : engine::service::services) {
    engine::service const* svc{p.second.get()};
    for (auto const& pp : svc->get_contactgroups()) {
      contactgroup_map::iterator found{
          engine::contactgroup::contactgroups.find(pp.first)};
      if (found == engine::contactgroup::contactgroups.end() ||
          found->second.get() != pp.second) {
        engine_logger(log_config_error, basic)
            << "Error on contactgroup !!! The contactgroup " << pp.first
            << " used in service " << p.first.first << '/' << p.first.second
            << " is not or badly defined";
        config_logger->error(
            "Error on contactgroup !!! The contactgroup {} used in service "
            "{}/{} is not or badly defined",
            pp.first, p.first.first, p.first.second);
        throw engine_error() << "This is a bug";
      }
    }
  }

  for (auto const& p : engine::host::hosts) {
    for (auto const& pp : p.second->get_contactgroups()) {
      contactgroup_map::iterator found{
          engine::contactgroup::contactgroups.find(pp.first)};
      if (found == engine::contactgroup::contactgroups.end() ||
          found->second.get() != pp.second) {
        engine_logger(log_config_error, basic)
            << "Error on contactgroup !!! The contactgroup " << pp.first
            << " used in host " << p.first << " is not or badly defined";
        config_logger->error(
            "Error on contactgroup !!! The contactgroup {} used in host {} is "
            "not or badly defined",
            pp.first, p.first);
        throw engine_error() << "This is a bug";
      }
    }
  }

  for (auto const& p : engine::serviceescalation::serviceescalations) {
    for (auto const& pp : p.second->get_contactgroups()) {
      contactgroup_map::iterator found{
          engine::contactgroup::contactgroups.find(pp.first)};
      if (found == engine::contactgroup::contactgroups.end() ||
          found->second.get() != pp.second) {
        engine_logger(log_config_error, basic)
            << "Error on contactgroup !!! The contactgroup " << pp.first
            << " used in serviceescalation " << p.second->internal_key()
            << " is not or badly defined";
        config_logger->error(
            "Error on contactgroup !!! The contactgroup {} used in "
            "serviceescalation {} is not or badly defined",
            pp.first, p.second->internal_key());
        throw engine_error() << "This is a bug";
      }
    }
  }

  for (auto const& p : engine::hostescalation::hostescalations) {
    for (auto const& pp : p.second->get_contactgroups()) {
      contactgroup_map::iterator found{
          engine::contactgroup::contactgroups.find(pp.first)};
      if (found == engine::contactgroup::contactgroups.end() ||
          found->second.get() != pp.second) {
        engine_logger(log_config_error, basic)
            << "Error on contactgroup !!! The contactgroup " << pp.first
            << " used in hostescalation " << p.second->internal_key()
            << " is not or badly defined";
        config_logger->error(
            "Error on contactgroup !!! The contactgroup {} used in "
            "hostescalation {} is not or badly defined",
            pp.first, p.second->internal_key());
        throw engine_error() << "This is a bug";
      }
    }
  }
}

/**
 *  A method to check services pointers of each possible container are well
 *  defined.
 *
 *  If something wrong is found, an exception is thrown.
 */
void applier::state::_check_services() const {
  for (auto const& p : engine::servicedependency::servicedependencies) {
    engine::servicedependency const* sd{p.second.get()};
    std::list<engine::service*> svcs{sd->master_service_ptr,
                                     sd->dependent_service_ptr};
    for (engine::service const* svc : svcs) {
      service_id_map::const_iterator found{engine::service::services_by_id.find(
          {svc->get_host_id(), svc->get_service_id()})};
      if (found == engine::service::services_by_id.end() ||
          found->second.get() != svc) {
        engine_logger(log_config_error, basic)
            << "Error on service !!! The service " << p.first.first << '/'
            << p.first.second << " used in service dependency " << p.first.first
            << '/' << p.first.second << " is not or badly defined";
        config_logger->error(
            "Error on service !!! The service {}/{} used in service dependency "
            "{}/{} is not or badly defined",
            p.first.first, p.first.second, p.first.first, p.first.second);
        throw engine_error() << "This is a bug";
      }
    }
  }

  for (auto const& p : engine::service::services_by_id) {
    service_map::const_iterator found{engine::service::services.find(
        {p.second->get_hostname(), p.second->get_description()})};
    if (found == engine::service::services.end() ||
        found->second.get() != p.second.get()) {
      engine_logger(log_config_error, basic)
          << "Error on service !!! The service " << p.first.first << '/'
          << p.first.second
          << " defined in services is not defined in services_by_id";
      config_logger->error(
          "Error on service !!! The service {}/{} defined in services is not "
          "defined in services_by_id",
          p.first.first, p.first.second);
      throw engine_error() << "This is a bug";
    }
  }

  for (auto const& p : engine::service::services) {
    std::array<commands::command*, 2> arr{
        p.second->get_check_command_ptr(),
        p.second->get_event_handler_ptr(),
    };
    for (auto cc : arr) {
      if (cc) {
        bool found = false;
        for (auto& c : engine::commands::command::commands) {
          if (c.second.get() == cc) {
            found = true;
            break;
          }
        }
        if (!found) {
          engine_logger(log_config_error, basic)
              << "Error on service !!! The service " << p.first.first << '/'
              << p.first.second
              << " defined in services has a wrong check command";
          config_logger->error(
              "Error on service !!! The service {}/{} defined in services has "
              "a wrong check command",
              p.first.first, p.first.second);
          throw engine_error() << "This is a bug";
        }
      }
    }
  }

  if (engine::service::services_by_id.size() !=
      engine::service::services.size()) {
    engine_logger(log_config_error, basic)
        << "Error on service !!! services_by_id contains ices that are not in "
           "services. The first one size is "
        << engine::service::services.size() << "  the second size is "
        << engine::service::services.size();
    config_logger->error(
        "Error on service !!! services_by_id contains ices that are not in "
        "services. The first one size is {}  the second size is {}",
        engine::service::services.size(), engine::service::services.size());
    throw engine_error() << "This is a bug";
  }
}

/**
 *  A method to check hosts pointers of each possible container are well
 *  defined.
 *
 *  If something wrong is found, an exception is thrown.
 */
void applier::state::_check_hosts() const {
  auto find_host_by_name = [](engine::host const* hst,
                              std::string const& where) {
    host_map::const_iterator found{engine::host::hosts.find(hst->get_name())};
    if (found == engine::host::hosts.end() || found->second.get() != hst) {
      engine_logger(log_config_error, basic)
          << "Error on host !!! The host " << hst->get_name() << " used in "
          << where << " is not defined or badly defined in hosts";
      config_logger->error(
          "Error on host !!! The host {} used in {} is not defined or badly "
          "defined in hosts",
          hst->get_name(), where);
      throw engine_error() << "This is a bug";
    }
  };

  for (auto const& p : engine::hostdependency::hostdependencies) {
    engine::hostdependency const* sd{p.second.get()};
    std::list<engine::host*> hsts{sd->master_host_ptr, sd->dependent_host_ptr};
    for (engine::host const* hst : hsts)
      find_host_by_name(hst, "hostdependency");
  }

  for (auto const& p : engine::host::hosts_by_id) {
    find_host_by_name(p.second.get(), "hosts_by_id");
  }

  for (auto const& p : engine::host::hosts) {
    std::array<commands::command*, 2> arr{
        p.second->get_check_command_ptr(),
        p.second->get_event_handler_ptr(),
    };
    for (auto cc : arr) {
      if (cc) {
        bool found = false;
        for (auto& c : engine::commands::command::commands) {
          if (c.second.get() == cc) {
            found = true;
            break;
          }
        }
        if (!found) {
          engine_logger(log_config_error, basic)
              << "Error on host !!! The host " << p.first
              << " defined in hosts has a wrong check command";
          config_logger->error(
              "Error on host !!! The host {} defined in hosts has a wrong "
              "check command",
              p.first);
          throw engine_error() << "This is a bug";
        }
      }
    }
  }

  if (engine::host::hosts_by_id.size() != engine::host::hosts.size()) {
    engine_logger(log_config_error, basic)
        << "Error on host !!! hosts_by_id contains hosts that are not in "
           "hosts. The first one size is "
        << engine::service::services.size() << " whereas the second size is "
        << engine::service::services.size();
    config_logger->error(
        "Error on host !!! hosts_by_id contains hosts that are not in "
        "hosts. The first one size is {} whereas the second size is {}",
        engine::service::services.size(), engine::service::services.size());
    throw engine_error() << "This is a bug";
  }

  for (auto const& p : engine::service::services)
    find_host_by_name(p.second->get_host_ptr(), "service");
}

#endif

void applier::state::apply_log_config(configuration::State& new_cfg) {
  /* During the verification, loggers write to stdout. After this step, they
   * will log as it is written in their configurations. So if we check the
   * configuration, we don't want to change them. */
  if (verify_config || test_scheduling)
    return;

  using log_v2_config = com::centreon::common::log_v2::config;
  log_v2_config::logger_type log_type;
  if (new_cfg.log_v2_enabled()) {
    if (new_cfg.log_v2_logger() == "file") {
      if (!new_cfg.log_file().empty())
        log_type = log_v2_config::logger_type::LOGGER_FILE;
      else
        log_type = log_v2_config::logger_type::LOGGER_STDOUT;
    } else
      log_type = log_v2_config::logger_type::LOGGER_SYSLOG;

    log_v2_config log_cfg("centengine", log_type, new_cfg.log_flush_period(),
                          new_cfg.log_pid(), new_cfg.log_file_line());
    if (log_type == log_v2_config::logger_type::LOGGER_FILE) {
      log_cfg.set_log_path(new_cfg.log_file());
      log_cfg.set_max_size(new_cfg.max_log_file_size());
    }
    auto broker_sink = std::make_shared<broker_sink_mt>();
    broker_sink->set_level(spdlog::level::info);
    log_cfg.add_custom_sink(broker_sink);

    log_cfg.apply_custom_sinks(
        {"functions", "config", "events", "checks", "notifications",
         "eventbroker", "external_command", "commands", "downtimes", "comments",
         "macros", "otl", "process", "runtime"});
    log_cfg.set_level("functions",
                      LogLevel_Name(new_cfg.log_level_functions()));
    log_cfg.set_level("config", LogLevel_Name(new_cfg.log_level_config()));
    log_cfg.set_level("events", LogLevel_Name(new_cfg.log_level_events()));
    log_cfg.set_level("checks", LogLevel_Name(new_cfg.log_level_checks()));
    log_cfg.set_level("notifications",
                      LogLevel_Name(new_cfg.log_level_notifications()));
    log_cfg.set_level("eventbroker",
                      LogLevel_Name(new_cfg.log_level_eventbroker()));
    log_cfg.set_level("external_command",
                      LogLevel_Name(new_cfg.log_level_external_command()));
    log_cfg.set_level("commands", LogLevel_Name(new_cfg.log_level_commands()));
    log_cfg.set_level("downtimes",
                      LogLevel_Name(new_cfg.log_level_downtimes()));
    log_cfg.set_level("comments", LogLevel_Name(new_cfg.log_level_comments()));
    log_cfg.set_level("macros", LogLevel_Name(new_cfg.log_level_macros()));
    log_cfg.set_level("otl", LogLevel_Name(new_cfg.log_level_otl()));
    log_cfg.set_level("process", LogLevel_Name(new_cfg.log_level_process()));
    log_cfg.set_level("runtime", LogLevel_Name(new_cfg.log_level_runtime()));
    if (has_already_been_loaded)
      log_cfg.allow_only_atomic_changes(true);
    log_v2::instance().apply(log_cfg);
  } else {
    if (!new_cfg.log_file().empty())
      log_type = log_v2_config::logger_type::LOGGER_FILE;
    else
      log_type = log_v2_config::logger_type::LOGGER_STDOUT;
    log_v2_config log_cfg("centengine", log_type, new_cfg.log_flush_period(),
                          new_cfg.log_pid(), new_cfg.log_file_line());
    if (!new_cfg.log_file().empty()) {
      log_cfg.set_log_path(new_cfg.log_file());
      log_cfg.set_max_size(new_cfg.max_log_file_size());
    }
    log_v2::instance().apply(log_cfg);
    log_v2::instance().disable(
        {log_v2::CORE, log_v2::CONFIG, log_v2::PROCESS, log_v2::FUNCTIONS,
         log_v2::EVENTS, log_v2::CHECKS, log_v2::NOTIFICATIONS,
         log_v2::EVENTBROKER, log_v2::EXTERNAL_COMMAND, log_v2::COMMANDS,
         log_v2::DOWNTIMES, log_v2::COMMENTS, log_v2::MACROS, log_v2::RUNTIME,
         log_v2::OTL});
  }
  init_loggers();
}

/**
 *  Apply retention.
 *
 *  @param[in] new_cfg New configuration set.
 *  @param[in] state   The retention state to use.
 */
void applier::state::_apply(configuration::State& new_cfg,
                            retention::state& state,
                            error_cnt& err) {
  retention::applier::state app_state;
  if (!verify_config)
    app_state.apply(new_cfg, state);
  else {
    try {
      app_state.apply(new_cfg, state);
    } catch (std::exception const& e) {
      ++err.config_errors;
      std::cout << e.what();
    }
  }
}

void applier::state::_apply_diff_conf(
    DiffState& diff,
    absl::FixedArray<std::chrono::system_clock::time_point, 5>* tv,
    error_cnt& err) {
  // Timing.
  tv->at(1) = std::chrono::system_clock::now();

#define apply_diff(field) \
  if (diff.has_##field()) \
    pb_indexed_config.mut_state().set_##field(diff.field());

#define apply_repeated_diff(field)                     \
  if (!diff.field().empty()) {                         \
    pb_indexed_config.mut_state().clear_##field();     \
    for (auto& item : diff.field())                    \
      pb_indexed_config.mut_state().add_##field(item); \
  }

  apply_diff(cfg_main);
  apply_repeated_diff(cfg_file);
  apply_repeated_diff(resource_file);
  apply_diff(instance_heartbeat_interval);
  apply_diff(check_service_freshness);
  apply_diff(enable_flap_detection);
  apply_diff(rpc_listen_address);
  apply_diff(grpc_port);
  // apply_diff(users);
  apply_repeated_diff(cfg_dir);
  apply_diff(state_retention_file);
  apply_repeated_diff(broker_module);
  apply_diff(broker_module_directory);
  apply_diff(enable_macros_filter);
  // apply_diff(macros_filter);

  apply_diff(log_v2_enabled);
  apply_diff(log_legacy_enabled);
  apply_diff(use_syslog);
  apply_diff(log_v2_logger);
  apply_diff(log_file);
  apply_diff(debug_file);
  apply_diff(debug_level);
  apply_diff(debug_verbosity);
  apply_diff(max_debug_file_size);
  apply_diff(log_pid);
  apply_diff(log_file_line);
  apply_diff(log_flush_period);
  apply_diff(log_level_checks);
  apply_diff(log_level_commands);
  apply_diff(log_level_comments);
  apply_diff(log_level_config);
  apply_diff(log_level_downtimes);
  apply_diff(log_level_eventbroker);
  apply_diff(log_level_events);
  apply_diff(log_level_external_command);
  apply_diff(log_level_functions);
  apply_diff(log_level_macros);
  apply_diff(log_level_notifications);
  apply_diff(log_level_process);
  apply_diff(log_level_runtime);
  apply_diff(log_level_otl);
  apply_diff(global_host_event_handler);
  apply_diff(global_service_event_handler);
  apply_diff(illegal_object_chars);
  apply_diff(illegal_output_chars);
  apply_diff(interval_length);
  apply_diff(ochp_command);
  apply_diff(ocsp_command);
  apply_diff(use_timezone);
  apply_diff(accept_passive_host_checks);
  apply_diff(accept_passive_service_checks);
  apply_diff(additional_freshness_latency);
  apply_diff(cached_host_check_horizon);
  apply_diff(check_external_commands);
  apply_diff(check_host_freshness);
  apply_diff(check_reaper_interval);
  apply_diff(enable_event_handlers);
  apply_diff(enable_notifications);
  apply_diff(execute_host_checks);
  apply_diff(execute_service_checks);
  apply_diff(max_host_check_spread);
  apply_diff(max_service_check_spread);
  apply_diff(notification_timeout);
  apply_diff(obsess_over_hosts);
  apply_diff(obsess_over_services);
  apply_diff(process_performance_data);
  apply_diff(soft_state_dependencies);
  apply_diff(use_large_installation_tweaks);
  apply_diff(admin_email);
  apply_diff(admin_pager);
  apply_diff(allow_empty_hostgroup_assignment);
  apply_diff(command_file);
  apply_diff(status_file);
  apply_diff(poller_name);
  apply_diff(poller_id);
  apply_diff(cached_service_check_horizon);
  apply_diff(check_orphaned_hosts);
  apply_diff(check_orphaned_services);
  apply_diff(command_check_interval);
  apply_diff(command_check_interval_is_seconds);
  apply_diff(enable_environment_macros);
  apply_diff(event_broker_options);
  apply_diff(event_handler_timeout);
  apply_diff(external_command_buffer_slots);
  apply_diff(high_host_flap_threshold);
  apply_diff(high_service_flap_threshold);
  apply_diff(host_check_timeout);
  apply_diff(host_freshness_check_interval);
  apply_diff(service_freshness_check_interval);
  apply_diff(log_event_handlers);
  apply_diff(log_external_commands);
  apply_diff(log_notifications);
  apply_diff(log_passive_checks);
  apply_diff(log_host_retries);
  apply_diff(log_service_retries);
  apply_diff(max_log_file_size);
  apply_diff(low_host_flap_threshold);
  apply_diff(low_service_flap_threshold);
  apply_diff(max_parallel_service_checks);
  apply_diff(ochp_timeout);
  apply_diff(ocsp_timeout);
  apply_diff(perfdata_timeout);
  apply_diff(retained_host_attribute_mask);
  apply_diff(retained_process_host_attribute_mask);
  apply_diff(retained_contact_host_attribute_mask);
  apply_diff(retained_contact_service_attribute_mask);
  apply_diff(retain_state_information);
  apply_diff(retention_scheduling_horizon);
  apply_diff(retention_update_interval);
  apply_diff(service_check_timeout);
  apply_diff(sleep_time);
  apply_diff(status_update_interval);
  apply_diff(time_change_threshold);
  apply_diff(use_regexp_matches);
  apply_diff(use_retained_program_state);
  apply_diff(use_retained_scheduling_info);
  apply_diff(use_setpgid);
  apply_diff(use_true_regexp_matching);
  apply_diff(date_format);
  // apply_repeated_diff(host_inter_check_delay_method);
  // apply_repeated_diff(service_inter_check_delay_method);
  // apply_repeated_diff(service_interleave_factor_method);
  apply_diff(enable_predictive_host_dependency_checks);
  apply_diff(enable_predictive_service_dependency_checks);
  apply_diff(send_recovery_notifications_anyways);
  apply_diff(host_down_disable_service_checks);
  // Apply logging configurations.

  applier::logging::instance().apply(pb_indexed_config.mut_state());

  // Apply globals configurations.
  applier::globals::instance().apply(pb_indexed_config.mut_state());

  // Apply macros configurations.
  applier::macros::instance().apply(pb_indexed_config.mut_state());

  // Timing.
  tv->at(2) = std::chrono::system_clock::now();

  if (!has_already_been_loaded && !verify_config && !test_scheduling) {
    // This must be logged after we read config data,
    // as user may have changed location of main log file.
    process_logger->info(
        "Centreon Engine {} starting ... (PID={}) (Protobuf configuration)",
        CENTREON_ENGINE_VERSION_STRING, getpid());

    // Log the local time - may be different than clock
    // time due to timezone offset.
    process_logger->info("Local time is {}", string::ctime(program_start));
    process_logger->info("LOG VERSION: {}", LOG_VERSION_2);
  }

  //
  //  Apply and resolve all objects.
  //

  // Apply timeperiods.
  _apply_ng<configuration::applier::timeperiod, DiffTimeperiod, std::string,
            Timeperiod>(*diff.mutable_timeperiods(),
                        pb_indexed_config.mut_timeperiods(),
                        [](const Timeperiod& obj) -> std::string {
                          return obj.timeperiod_name();
                        });
  _resolve<configuration::Timeperiod, std::string, applier::timeperiod>(
      pb_indexed_config.timeperiods(), err);

  // Apply connectors.
  _apply_ng<configuration::applier::connector, DiffConnector, std::string,
            Connector>(
      *diff.mutable_connectors(), pb_indexed_config.mut_connectors(),
      [](const Connector& obj) -> std::string { return obj.connector_name(); });
  _resolve<configuration::Connector, std::string, applier::connector>(
      pb_indexed_config.connectors(), err);

  // Apply commands.
  _apply_ng<configuration::applier::command, DiffCommand, std::string, Command>(
      *diff.mutable_commands(), pb_indexed_config.mut_commands(),
      [](const Command& obj) -> std::string { return obj.command_name(); });
  _resolve<configuration::Command, std::string, applier::command>(
      pb_indexed_config.commands(), err);

  // Apply contacts and contactgroups.
  _apply_ng<configuration::applier::contact, DiffContact, std::string, Contact>(
      *diff.mutable_contacts(), pb_indexed_config.mut_contacts(),
      [](const Contact& obj) -> std::string { return obj.contact_name(); });
  _apply_ng<configuration::applier::contactgroup, DiffContactgroup, std::string,
            Contactgroup>(*diff.mutable_contactgroups(),
                          pb_indexed_config.mut_contactgroups(),
                          [](const Contactgroup& obj) -> std::string {
                            return obj.contactgroup_name();
                          });
  _resolve<configuration::Contact, std::string, applier::contact>(
      pb_indexed_config.contacts(), err);
  _resolve<configuration::Contactgroup, std::string, applier::contactgroup>(
      pb_indexed_config.contactgroups(), err);

  // Apply severities.
  _apply_ng<configuration::applier::severity, DiffSeverity,
            std::pair<uint64_t, uint32_t>, Severity, KeyType>(
      *diff.mutable_severities(), pb_indexed_config.mut_severities(),
      [](const Severity& obj) {
        return std::make_pair(obj.key().id(), obj.key().type());
      },
      [](const KeyType& key) { return std::make_pair(key.id(), key.type()); });

  // Apply tags.
  _apply_ng<configuration::applier::tag, DiffTag, std::pair<uint64_t, uint32_t>,
            Tag, KeyType>(
      *diff.mutable_tags(), pb_indexed_config.mut_tags(),
      [](const Tag& obj) {
        return std::make_pair(obj.key().id(), obj.key().type());
      },
      [](const KeyType& key) { return std::make_pair(key.id(), key.type()); });

  // Apply hosts and hostgroups.
  _apply_ng<configuration::applier::host, DiffHost, uint64_t, Host>(
      *diff.mutable_hosts(), pb_indexed_config.mut_hosts(),
      [](const Host& obj) -> uint64_t { return obj.host_id(); });
  _apply_ng<configuration::applier::hostgroup, DiffHostgroup, std::string,
            Hostgroup>(
      *diff.mutable_hostgroups(), pb_indexed_config.mut_hostgroups(),
      [](const Hostgroup& obj) -> std::string { return obj.hostgroup_name(); });

  // Apply services.
  _apply_ng<configuration::applier::service, DiffService,
            std::pair<uint64_t, uint64_t>, Service, HostServiceId>(
      *diff.mutable_services(), pb_indexed_config.mut_services(),
      [](const Service& obj) -> std::pair<uint64_t, uint64_t> {
        return std::make_pair(obj.host_id(), obj.service_id());
      },
      [](const HostServiceId& key) {
        return std::make_pair(key.host_id(), key.service_id());
      });

  // Apply anomalydetections.
  _apply_ng<configuration::applier::anomalydetection, DiffAnomalydetection,
            std::pair<uint64_t, uint64_t>, Anomalydetection, HostServiceId>(
      *diff.mutable_anomalydetections(),
      pb_indexed_config.mut_anomalydetections(),
      [](const Anomalydetection& obj) -> std::pair<uint64_t, uint64_t> {
        return std::make_pair(obj.host_id(), obj.service_id());
      },
      [](const HostServiceId& key) {
        return std::make_pair(key.host_id(), key.service_id());
      });

  // Apply servicegroups.
  _apply_ng<configuration::applier::servicegroup, DiffServicegroup, std::string,
            Servicegroup>(*diff.mutable_servicegroups(),
                          pb_indexed_config.mut_servicegroups(),
                          [](const Servicegroup& obj) -> std::string {
                            return obj.servicegroup_name();
                          });

  // Resolve hosts, services, host groups.
  _resolve<configuration::Host, uint64_t, applier::host>(
      pb_indexed_config.hosts(), err);
  _resolve<configuration::Hostgroup, std::string, applier::hostgroup>(
      pb_indexed_config.hostgroups(), err);

  // Resolve services.
  _resolve<configuration::Service, std::pair<uint64_t, uint64_t>,
           applier::service>(pb_indexed_config.services(), err);

  // Resolve anomalydetections.
  _resolve<configuration::Anomalydetection, std::pair<uint64_t, uint64_t>,
           applier::anomalydetection>(pb_indexed_config.anomalydetections(),
                                      err);

  // Resolve service groups.
  _resolve<configuration::Servicegroup, std::string, applier::servicegroup>(
      pb_indexed_config.servicegroups(), err);

  // Apply host dependencies.
  _apply_ng<configuration::applier::hostdependency, DiffHostdependency, size_t,
            Hostdependency>(*diff.mutable_hostdependencies(),
                            pb_indexed_config.mut_hostdependencies(),
                            configuration::hostdependency_key);
  _resolve<configuration::Hostdependency, uint64_t, applier::hostdependency>(
      pb_indexed_config.hostdependencies(), err);

  // Apply service dependencies.
  _apply_ng<configuration::applier::servicedependency, DiffServicedependency,
            size_t, Servicedependency>(
      *diff.mutable_servicedependencies(),
      pb_indexed_config.mut_servicedependencies(),
      configuration::servicedependency_key);
  _resolve<configuration::Servicedependency, uint64_t,
           applier::servicedependency>(pb_indexed_config.servicedependencies(),
                                       err);

  // Apply host escalations.
  _apply_ng<configuration::applier::hostescalation, DiffHostescalation, size_t,
            Hostescalation>(*diff.mutable_hostescalations(),
                            pb_indexed_config.mut_hostescalations(),
                            configuration::hostescalation_key);
  _resolve<configuration::Hostescalation, uint64_t, applier::hostescalation>(
      pb_indexed_config.hostescalations(), err);

  // Apply service escalations.
  _apply_ng<configuration::applier::serviceescalation, DiffServiceescalation,
            size_t, Serviceescalation>(
      *diff.mutable_serviceescalations(),
      pb_indexed_config.mut_serviceescalations(),
      configuration::serviceescalation_key);
  _resolve<configuration::Serviceescalation, uint64_t,
           applier::serviceescalation>(pb_indexed_config.serviceescalations(),
                                       err);

#ifdef DEBUG_CONFIG
  std::cout << "WARNING!! You are using a version of Centreon Engine for "
               "developers!!! This is not a production version.";
  // Checks on configuration
  _check_serviceescalations();
  _check_hostescalations();
  _check_contacts();
  _check_contactgroups();
  _check_services();
  _check_hosts();
#endif
}

/**
 *  Process new configuration and apply it.
 *
 *  @param[in] new_cfg        The new configuration.
 *  @param[in] state          The retention to use.
 */
void applier::state::_processing(configuration::State& new_cfg,
                                 error_cnt& err,
                                 retention::state* state) {
  // Timing.
  absl::FixedArray<std::chrono::system_clock::time_point, 5> tv{
      {}, {}, {}, {}, {}};

  // Call prelauch broker event the first time to run applier state.
  if (!has_already_been_loaded)
    broker_program_state(NEBTYPE_PROCESS_PRELAUNCH, NEBFLAG_NONE);

  //
  // Expand all objects.
  //
  tv[0] = std::chrono::system_clock::now();

  //
  //  Build difference for all objects.
  //

  DiffState diff;
  pb_indexed_config.diff_with_new_config(new_cfg, config_logger, &diff);

  try {
    std::lock_guard<std::mutex> locker(_apply_lock);
    _apply_diff_conf(diff, &tv, err);
    // Load retention.
    if (state)
      _apply(new_cfg, *state, err);

    // Apply scheduler.
    if (!verify_config)
      applier::scheduler::instance().apply(new_cfg, diff);

    // Apply new global on the current state.
    if (!verify_config) {
      _apply(new_cfg, err);
      whitelist::reload();
    } else {
      try {
        _apply(new_cfg, err);
      } catch (std::exception const& e) {
        ++err.config_errors;
        events_logger->info(e.what());
      }
    }

    // Timing.
    tv[3] = std::chrono::system_clock::now();

    // Check for circular paths between hosts.
    pre_flight_circular_check(&err.config_warnings, &err.config_errors);

    // Call start broker event the first time to run applier state.
    if (!has_already_been_loaded) {
      neb_load_all_modules();

      broker_program_state(NEBTYPE_PROCESS_START, NEBFLAG_NONE);
    } else {
      apply_log_config(new_cfg);
      cbm->reload();
      neb_reload_all_modules();
    }

    // Print initial states of new hosts and services.
    if (!verify_config && !test_scheduling) {
      for (auto a : diff.hosts().added()) {
        auto it_hst = engine::host::hosts_by_id.find(a.host_id());
        if (it_hst != engine::host::hosts_by_id.end())
          log_host_state(INITIAL_STATES, it_hst->second.get());
      }
      for (auto a : diff.services().added()) {
        auto it_svc =
            engine::service::services_by_id.find({a.host_id(), a.service_id()});
        if (it_svc != engine::service::services_by_id.end())
          log_service_state(INITIAL_STATES, it_svc->second.get());
      }
    }

    // Timing.
    tv[4] = std::chrono::system_clock::now();
    if (test_scheduling) {
      absl::FixedArray<double, 5> runtimes{0, 0, 0, 0, 0};
      for (uint32_t i = 0; i < runtimes.size() - 1; i++) {
        runtimes[i] = (tv[i + 1] - tv[i]).count();
        runtimes[4] += runtimes[i];
      }
      std::cout
          << "\nTiming information on configuration verification is listed "
             "below.\n\n"
             "CONFIG VERIFICATION TIMES          (* = Potential for speedup "
             "with -x option)\n"
             "----------------------------------\n"
             "Template Resolutions: "
          << runtimes[0]
          << " sec\n"
             "Object Relationships: "
          << runtimes[2]
          << " sec\n"
             "Circular Paths:       "
          << runtimes[3]
          << " sec  *\n"
             "Misc:                 "
          << runtimes[1]
          << " sec\n"
             "                      ============\n"
             "TOTAL:                "
          << runtimes[4] << " sec  * = " << runtimes[3] << " sec ("
          << (runtimes[3] * 100.0 / runtimes[4]) << "%) estimated savings\n";
    }
  } catch (...) {
    _processing_state = state_error;
    throw;
  }

  has_already_been_loaded = true;
  _processing_state = state_ready;
}

void applier::state::_processing_diff(configuration::DiffState& diff_conf,
                                      error_cnt& err,
                                      retention::state* state) {
  /* Particular case with a diff containing the full state */
  if (diff_conf.has_state()) {
    /* The previous version wasn't known by broker, the diff contains the full
     * state. So let's parse it as usual. */
    config_logger->debug("Processing full configuration from diff.");

    _processing(*diff_conf.mutable_state(), err, state);
    return;
  }

  // Timing.
  absl::FixedArray<std::chrono::system_clock::time_point, 5> tv{
      {}, {}, {}, {}, {}};

  // Call prelaunch broker event the first time to run applier state.
  if (!has_already_been_loaded)
    broker_program_state(NEBTYPE_PROCESS_PRELAUNCH, NEBFLAG_NONE);

  //
  // Expand all objects.
  //
  tv[0] = std::chrono::system_clock::now();

  try {
    std::lock_guard<std::mutex> lock(_apply_lock);
    _apply_diff_conf(diff_conf, &tv, err);

    config_logger->debug("Duration to apply the diff state configuration {}",
                         tv[2] - tv[1]);
    // Apply scheduler
    applier::scheduler::instance().apply(pb_indexed_config.mut_state(),
                                         diff_conf);
    whitelist::reload();

    // Timing.
    tv[3] = std::chrono::system_clock::now();

    config_logger->debug("Duration to reload the whitelist {}", tv[3] - tv[2]);
    // Check for circular paths between hosts.
    pre_flight_circular_check(&err.config_warnings, &err.config_errors);

    // Call start broker event the first time to run applier state.
    if (!has_already_been_loaded) {
      neb_load_all_modules();

      broker_program_state(NEBTYPE_PROCESS_START, NEBFLAG_NONE);
    } else {
      apply_log_config(pb_indexed_config.mut_state());
      cbm->reload();
      neb_reload_all_modules();
    }

    // Print initial states of new hosts and services.
    for (auto a : diff_conf.hosts().added()) {
      auto it_hst = engine::host::hosts_by_id.find(a.host_id());
      if (it_hst != engine::host::hosts_by_id.end())
        log_host_state(INITIAL_STATES, it_hst->second.get());
    }
    for (auto a : diff_conf.services().added()) {
      auto it_svc =
          engine::service::services_by_id.find({a.host_id(), a.service_id()});
      if (it_svc != engine::service::services_by_id.end())
        log_service_state(INITIAL_STATES, it_svc->second.get());
    }

    // Timing.
    tv[4] = std::chrono::system_clock::now();
    config_logger->debug("Duration to apply resources change {}",
                         tv[4] - tv[3]);
  } catch (...) {
    _processing_state = state_error;
    throw;
  }

  has_already_been_loaded = true;
  _processing_state = state_ready;
}

template <typename ConfigurationType, typename KeyType, typename ApplierType>
void applier::state::_resolve(
    const absl::flat_hash_map<KeyType, std::unique_ptr<ConfigurationType>>& cfg,
    error_cnt& err) {
  ApplierType aplyr;
  // remove all the child backlinks of all the hosts.
  // It is necessary to do it only once to prevent the removal
  // of valid child backlinks.
  for (const auto& [_, sptr_host] : engine::host::hosts)
    sptr_host->child_hosts.clear();

  for (auto& [key, obj] : cfg) {
    try {
      aplyr.resolve_object(*obj.get(), err);
    } catch (const std::exception& e) {
      if (verify_config) {
        ++err.config_errors;
        std::cout << e.what() << std::endl;
      } else
        throw;
    }
  }
}
