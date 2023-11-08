/*
 * Copyright 2011-2023 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "com/centreon/engine/configuration/applier/state.hh"

#include <google/protobuf/util/message_differencer.h>

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/commands/connector.hh"
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
#include "com/centreon/engine/configuration/command.hh"
#include "com/centreon/engine/configuration/diff_state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging.hh"
#include "com/centreon/engine/logging/broker_sink.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/objects.hh"
#include "com/centreon/engine/retention/applier/state.hh"
#include "com/centreon/engine/retention/state.hh"
#include "com/centreon/engine/severity.hh"
#include "com/centreon/engine/version.hh"
#include "com/centreon/engine/xpddefault.hh"
#include "com/centreon/engine/xsddefault.hh"
#include "common/configuration/state-generated.pb.h"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::logging;
using com::centreon::common::log_v2::log_v2;
using com::centreon::engine::logging::broker_sink_mt;

static bool has_already_been_loaded(false);

/**
 *  Apply new configuration.
 *
 *  @param[in] new_cfg        The new configuration.
 */
void applier::state::apply(configuration::State& new_cfg) {
  configuration::State save;
  save.CopyFrom(pb_config);
  try {
    _processing_state = state_ready;
    _processing(new_cfg);
  } catch (const std::exception& e) {
    // If is the first time to load configuration, we don't
    // have a valid configuration to restore.
    if (!has_already_been_loaded)
      throw;

    // If is not the first time, we can restore the old one.
    config_logger->error("Error: Could not apply new configuration: {}",
                         e.what());

    // Check if we need to restore old configuration.
    if (_processing_state == state_error) {
      config_logger->debug("configuration: try to restore old configuration");
      _processing(save);
    }
  }
}

/**
 *  Apply new configuration.
 *
 *  @param[in] new_cfg        The new configuration.
 */
void applier::state::apply(configuration::state& new_cfg) {
  configuration::state save(*config);
  try {
    _processing_state = state_ready;
    _processing(new_cfg);
  } catch (std::exception const& e) {
    // If is the first time to load configuration, we don't
    // have a valid configuration to restore.
    if (!has_already_been_loaded)
      throw;

    // If is not the first time, we can restore the old one.
    engine_logger(log_config_error, basic)
        << "Error: Could not apply new configuration: " << e.what();
    config_logger->error("Error: Could not apply new configuration: {}",
                         e.what());

    // Check if we need to restore old configuration.
    if (_processing_state == state_error) {
      engine_logger(dbg_config, more)
          << "configuration: try to restore old configuration";
      config_logger->debug("configuration: try to restore old configuration");
      _processing(save);
    }
  }
}

/**
 *  Apply new protobuf configuration.
 *
 *  @param[in] new_cfg        The new protobuf configuration.
 *  @param[in] state          The retention to use.
 */
void applier::state::apply(configuration::State& new_cfg,
                           retention::state& state) {
  configuration::State save;
  save.CopyFrom(pb_config);
  try {
    _processing_state = state_ready;
    _processing(new_cfg, &state);
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
      _processing(save, &state);
    }
  }
}

/**
 *  Apply new configuration.
 *
 *  @param[in] new_cfg        The new configuration.
 *  @param[in] state          The retention to use.
 */
void applier::state::apply(configuration::state& new_cfg,
                           retention::state& state) {
  configuration::state save(*config);
  try {
    _processing_state = state_ready;
    _processing(new_cfg, &state);
  } catch (std::exception const& e) {
    // If is the first time to load configuration, we don't
    // have a valid configuration to restore.
    if (!has_already_been_loaded)
      throw;

    // If is not the first time, we can restore the old one.
    engine_logger(log_config_error, basic)
        << "Cannot apply new configuration: " << e.what();
    config_logger->error("Cannot apply new configuration: {}", e.what());

    // Check if we need to restore old configuration.
    if (_processing_state == state_error) {
      engine_logger(dbg_config, more)
          << "configuration: try to restore old configuration";
      config_logger->debug("configuration: try to restore old configuration");
      _processing(save, &state);
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
  engine::service::services.clear();
  engine::service::services_by_id.clear();
  engine::servicedependency::servicedependencies.clear();
  engine::serviceescalation::serviceescalations.clear();
  engine::host::hosts.clear();
  engine::host::hosts_by_id.clear();
  engine::hostdependency::hostdependencies.clear();
  engine::hostescalation::hostescalations.clear();
  engine::severity::severities.clear();
  engine::tag::tags.clear();
  engine::timeperiod::timeperiods.clear();
  engine::comment::comments.clear();
  engine::comment::set_next_comment_id(1llu);

  xpddefault_cleanup_performance_data();

  applier::scheduler::instance().clear();
  applier::macros::instance().clear();
  applier::globals::instance().clear();
  applier::logging::instance().clear();

  _processing_state = state_ready;
  _config = nullptr;
  has_already_been_loaded = false;
}

/**
 *  Default constructor.
 */
applier::state::state() : _config(nullptr), _processing_state(state_ready) {}

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
  engine::service::services.clear();
  engine::service::services_by_id.clear();
  engine::servicedependency::servicedependencies.clear();
  engine::serviceescalation::serviceescalations.clear();
  engine::host::hosts.clear();
  engine::host::hosts_by_id.clear();
  engine::hostdependency::hostdependencies.clear();
  engine::hostescalation::hostescalations.clear();
  engine::severity::severities.clear();
  engine::tag::tags.clear();
  engine::timeperiod::timeperiods.clear();
  engine::comment::comments.clear();
  engine::comment::set_next_comment_id(1llu);

  xpddefault_cleanup_performance_data();
}

/**
 *  Return the user macros.
 *
 *  @return  The user macros.
 */
std::unordered_map<std::string, std::string>& applier::state::user_macros() {
  return _user_macros;
}

/**
 *  Find a user macro.
 *
 *  @param[in] key  The key.
 *
 *  @return  Iterator to user macros.
 */
std::unordered_map<std::string, std::string>::const_iterator
applier::state::user_macros_find(std::string const& key) const {
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
void applier::state::_pb_apply(const configuration::State& new_cfg) {
  // Check variables should not be change after the first execution.
  if (has_already_been_loaded) {
    if (!std::equal(
            pb_config.broker_module().begin(), pb_config.broker_module().end(),
            new_cfg.broker_module().begin(), new_cfg.broker_module().end())) {
      config_logger->warn(
          "Warning: Broker modules cannot be changed nor reloaded");
      ++config_warnings;
    }
    if (pb_config.broker_module_directory() !=
        new_cfg.broker_module_directory()) {
      config_logger->warn("Warning: Broker module directory cannot be changed");
      ++config_warnings;
    }
    if (pb_config.command_file() != new_cfg.command_file()) {
      config_logger->warn("Warning: Command file cannot be changed");
      ++config_warnings;
    }
    if (pb_config.external_command_buffer_slots() !=
        new_cfg.external_command_buffer_slots()) {
      config_logger->warn(
          "Warning: External command buffer slots cannot be changed");
      ++config_warnings;
    }
    if (pb_config.use_timezone() != new_cfg.use_timezone()) {
      config_logger->warn("Warning: Timezone can not be changed");
      ++config_warnings;
    }
  }

  // Initialize status file.
  bool modify_status(false);
  if (!has_already_been_loaded ||
      pb_config.status_file() != new_cfg.status_file())
    modify_status = true;

  // Cleanup.
  //  if (modify_perfdata)
  //    xpddefault_cleanup_performance_data();
  if (modify_status)
    xsddefault_cleanup_status_data(true);

  // Set new values.
  pb_config.set_accept_passive_host_checks(
      new_cfg.accept_passive_host_checks());
  pb_config.set_accept_passive_service_checks(
      new_cfg.accept_passive_service_checks());
  pb_config.set_additional_freshness_latency(
      new_cfg.additional_freshness_latency());
  pb_config.set_admin_email(new_cfg.admin_email());
  pb_config.set_admin_pager(new_cfg.admin_pager());
  pb_config.set_allow_empty_hostgroup_assignment(
      new_cfg.allow_empty_hostgroup_assignment());
  pb_config.set_auto_reschedule_checks(new_cfg.auto_reschedule_checks());
  pb_config.set_auto_rescheduling_interval(
      new_cfg.auto_rescheduling_interval());
  pb_config.set_auto_rescheduling_window(new_cfg.auto_rescheduling_window());
  pb_config.set_cached_host_check_horizon(new_cfg.cached_host_check_horizon());
  pb_config.set_cached_service_check_horizon(
      new_cfg.cached_service_check_horizon());
  pb_config.set_cfg_main(new_cfg.cfg_main());
  pb_config.set_check_external_commands(new_cfg.check_external_commands());
  pb_config.set_check_host_freshness(new_cfg.check_host_freshness());
  pb_config.set_check_orphaned_hosts(new_cfg.check_orphaned_hosts());
  pb_config.set_check_orphaned_services(new_cfg.check_orphaned_services());
  pb_config.set_check_reaper_interval(new_cfg.check_reaper_interval());
  pb_config.set_check_service_freshness(new_cfg.check_service_freshness());
  pb_config.set_command_check_interval(new_cfg.command_check_interval());
  pb_config.set_command_check_interval_is_seconds(
      new_cfg.command_check_interval_is_seconds());
  pb_config.set_date_format(new_cfg.date_format());
  pb_config.set_debug_file(new_cfg.debug_file());
  pb_config.set_debug_level(new_cfg.debug_level());
  pb_config.set_debug_verbosity(new_cfg.debug_verbosity());
  pb_config.set_enable_environment_macros(new_cfg.enable_environment_macros());
  pb_config.set_enable_event_handlers(new_cfg.enable_event_handlers());
  pb_config.set_enable_flap_detection(new_cfg.enable_flap_detection());
  pb_config.set_enable_notifications(new_cfg.enable_notifications());
  pb_config.set_enable_predictive_host_dependency_checks(
      new_cfg.enable_predictive_host_dependency_checks());
  pb_config.set_enable_predictive_service_dependency_checks(
      new_cfg.enable_predictive_service_dependency_checks());
  pb_config.set_event_broker_options(new_cfg.event_broker_options());
  pb_config.set_event_handler_timeout(new_cfg.event_handler_timeout());
  pb_config.set_execute_host_checks(new_cfg.execute_host_checks());
  pb_config.set_execute_service_checks(new_cfg.execute_service_checks());
  pb_config.set_global_host_event_handler(new_cfg.global_host_event_handler());
  pb_config.set_global_service_event_handler(
      new_cfg.global_service_event_handler());
  pb_config.set_high_host_flap_threshold(new_cfg.high_host_flap_threshold());
  pb_config.set_high_service_flap_threshold(
      new_cfg.high_service_flap_threshold());
  pb_config.set_host_check_timeout(new_cfg.host_check_timeout());
  pb_config.set_host_freshness_check_interval(
      new_cfg.host_freshness_check_interval());
  pb_config.mutable_host_inter_check_delay_method()->CopyFrom(
      new_cfg.host_inter_check_delay_method());
  pb_config.set_host_perfdata_command(new_cfg.host_perfdata_command());
  pb_config.set_illegal_object_chars(new_cfg.illegal_object_chars());
  pb_config.set_illegal_output_chars(new_cfg.illegal_output_chars());
  pb_config.set_interval_length(new_cfg.interval_length());
  pb_config.set_log_event_handlers(new_cfg.log_event_handlers());
  pb_config.set_log_external_commands(new_cfg.log_external_commands());
  pb_config.set_log_file(new_cfg.log_file());
  pb_config.set_log_host_retries(new_cfg.log_host_retries());
  pb_config.set_log_notifications(new_cfg.log_notifications());
  pb_config.set_log_passive_checks(new_cfg.log_passive_checks());
  pb_config.set_log_service_retries(new_cfg.log_service_retries());
  pb_config.set_low_host_flap_threshold(new_cfg.low_host_flap_threshold());
  pb_config.set_low_service_flap_threshold(
      new_cfg.low_service_flap_threshold());
  pb_config.set_max_debug_file_size(new_cfg.max_debug_file_size());
  pb_config.set_max_host_check_spread(new_cfg.max_host_check_spread());
  pb_config.set_max_log_file_size(new_cfg.max_log_file_size());
  pb_config.set_max_parallel_service_checks(
      new_cfg.max_parallel_service_checks());
  pb_config.set_max_service_check_spread(new_cfg.max_service_check_spread());
  pb_config.set_notification_timeout(new_cfg.notification_timeout());
  pb_config.set_obsess_over_hosts(new_cfg.obsess_over_hosts());
  pb_config.set_obsess_over_services(new_cfg.obsess_over_services());
  pb_config.set_ochp_command(new_cfg.ochp_command());
  pb_config.set_ochp_timeout(new_cfg.ochp_timeout());
  pb_config.set_ocsp_command(new_cfg.ocsp_command());
  pb_config.set_ocsp_timeout(new_cfg.ocsp_timeout());
  pb_config.set_perfdata_timeout(new_cfg.perfdata_timeout());
  pb_config.set_process_performance_data(new_cfg.process_performance_data());
  pb_config.clear_resource_file();
  pb_config.mutable_resource_file()->CopyFrom(new_cfg.resource_file());
  pb_config.set_retain_state_information(new_cfg.retain_state_information());
  pb_config.set_retained_contact_host_attribute_mask(
      new_cfg.retained_contact_host_attribute_mask());
  pb_config.set_retained_contact_service_attribute_mask(
      new_cfg.retained_contact_service_attribute_mask());
  pb_config.set_retained_host_attribute_mask(
      new_cfg.retained_host_attribute_mask());
  pb_config.set_retained_process_host_attribute_mask(
      new_cfg.retained_process_host_attribute_mask());
  pb_config.set_retention_scheduling_horizon(
      new_cfg.retention_scheduling_horizon());
  pb_config.set_retention_update_interval(new_cfg.retention_update_interval());
  pb_config.set_service_check_timeout(new_cfg.service_check_timeout());
  pb_config.set_service_freshness_check_interval(
      new_cfg.service_freshness_check_interval());
  pb_config.mutable_service_inter_check_delay_method()->CopyFrom(
      new_cfg.service_inter_check_delay_method());
  pb_config.set_service_interleave_factor_method(
      new_cfg.service_interleave_factor_method());
  pb_config.set_service_perfdata_command(new_cfg.service_perfdata_command());
  pb_config.set_sleep_time(new_cfg.sleep_time());
  pb_config.set_soft_state_dependencies(new_cfg.soft_state_dependencies());
  pb_config.set_state_retention_file(new_cfg.state_retention_file());
  pb_config.set_status_file(new_cfg.status_file());
  pb_config.set_status_update_interval(new_cfg.status_update_interval());
  pb_config.set_time_change_threshold(new_cfg.time_change_threshold());
  pb_config.set_use_large_installation_tweaks(
      new_cfg.use_large_installation_tweaks());
  pb_config.set_instance_heartbeat_interval(
      new_cfg.instance_heartbeat_interval());
  pb_config.set_use_regexp_matches(new_cfg.use_regexp_matches());
  pb_config.set_use_retained_program_state(
      new_cfg.use_retained_program_state());
  pb_config.set_use_retained_scheduling_info(
      new_cfg.use_retained_scheduling_info());
  pb_config.set_use_setpgid(new_cfg.use_setpgid());
  pb_config.set_use_syslog(new_cfg.use_syslog());
  pb_config.set_log_v2_enabled(new_cfg.log_v2_enabled());
  pb_config.set_log_legacy_enabled(new_cfg.log_legacy_enabled());
  pb_config.set_log_v2_logger(new_cfg.log_v2_logger());
  pb_config.set_log_level_functions(new_cfg.log_level_functions());
  pb_config.set_log_level_config(new_cfg.log_level_config());
  pb_config.set_log_level_events(new_cfg.log_level_events());
  pb_config.set_log_level_checks(new_cfg.log_level_checks());
  pb_config.set_log_level_notifications(new_cfg.log_level_notifications());
  pb_config.set_log_level_eventbroker(new_cfg.log_level_eventbroker());
  pb_config.set_log_level_external_command(
      new_cfg.log_level_external_command());
  pb_config.set_log_level_commands(new_cfg.log_level_commands());
  pb_config.set_log_level_downtimes(new_cfg.log_level_downtimes());
  pb_config.set_log_level_comments(new_cfg.log_level_comments());
  pb_config.set_log_level_macros(new_cfg.log_level_macros());
  pb_config.set_use_true_regexp_matching(new_cfg.use_true_regexp_matching());
  pb_config.clear_user();
  for (auto& p : new_cfg.user())
    pb_config.mutable_user()->at(p.first) = p.second;

  // Set this variable just the first time.
  if (!has_already_been_loaded) {
    pb_config.mutable_broker_module()->CopyFrom(new_cfg.broker_module());
    pb_config.set_broker_module_directory(new_cfg.broker_module_directory());
    pb_config.set_command_file(new_cfg.command_file());
    pb_config.set_external_command_buffer_slots(
        new_cfg.external_command_buffer_slots());
    pb_config.set_use_timezone(new_cfg.use_timezone());
  }

  // Initialize.
  if (modify_status)
    xsddefault_initialize_status_data();

  // Check global event handler commands...
  if (verify_config) {
    events_logger->info("Checking global event handlers...");
  }
  if (!pb_config.global_host_event_handler().empty()) {
    // Check the event handler command.
    std::string temp_command_name(pb_config.global_host_event_handler().substr(
        0, pb_config.global_host_event_handler().find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      config_logger->error(
          "Error: Global host event handler command '{}' is not defined "
          "anywhere!",
          temp_command_name);
      ++config_errors;
      global_host_event_handler_ptr = nullptr;
    } else
      global_host_event_handler_ptr = found->second.get();
  }
  if (!pb_config.global_service_event_handler().empty()) {
    // Check the event handler command.
    std::string temp_command_name(
        pb_config.global_service_event_handler().substr(
            0, pb_config.global_service_event_handler().find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      config_logger->error(
          "Error: Global service event handler command '{}' is not defined "
          "anywhere!",
          temp_command_name);
      ++config_errors;
      global_service_event_handler_ptr = nullptr;
    } else
      global_service_event_handler_ptr = found->second.get();
  }

  // Check obsessive processor commands...
  if (verify_config) {
    events_logger->info("Checking obsessive compulsive processor commands...");
  }
  if (!pb_config.ocsp_command().empty()) {
    std::string temp_command_name(pb_config.ocsp_command().substr(
        0, pb_config.ocsp_command().find_first_of('!')));
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
      ++config_errors;
      ocsp_command_ptr = nullptr;
    } else
      ocsp_command_ptr = found->second.get();
  }
  if (!pb_config.ochp_command().empty()) {
    std::string temp_command_name(pb_config.ochp_command().substr(
        0, pb_config.ochp_command().find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      config_logger->error(
          "Error: Obsessive compulsive host processor command '{}' is not "
          "defined anywhere!",
          temp_command_name);
      ++config_errors;
      ochp_command_ptr = nullptr;
    } else
      ochp_command_ptr = found->second.get();
  }
}

/*
 *  Update all new globals.
 *
 *  @param[in]  new_cfg The new configuration state.
 */
void applier::state::_apply(configuration::state const& new_cfg) {
  // Check variables should not be change after the first execution.
  if (has_already_been_loaded) {
    if (config->broker_module() != new_cfg.broker_module()) {
      engine_logger(log_config_warning, basic)
          << "Warning: Broker modules cannot be changed nor reloaded";
      config_logger->warn(
          "Warning: Broker modules cannot be changed nor reloaded");
      ++config_warnings;
    }
    if (config->broker_module_directory() !=
        new_cfg.broker_module_directory()) {
      engine_logger(log_config_warning, basic)
          << "Warning: Broker module directory cannot be changed";
      config_logger->warn("Warning: Broker module directory cannot be changed");
      ++config_warnings;
    }
    if (config->command_file() != new_cfg.command_file()) {
      engine_logger(log_config_warning, basic)
          << "Warning: Command file cannot be changed";
      config_logger->warn("Warning: Command file cannot be changed");
      ++config_warnings;
    }
    if (config->external_command_buffer_slots() !=
        new_cfg.external_command_buffer_slots()) {
      engine_logger(log_config_warning, basic)
          << "Warning: External command buffer slots cannot be changed";
      config_logger->warn(
          "Warning: External command buffer slots cannot be changed");
      ++config_warnings;
    }
    if (config->use_timezone() != new_cfg.use_timezone()) {
      engine_logger(log_config_warning, basic)
          << "Warning: Timezone can not be changed";
      config_logger->warn("Warning: Timezone can not be changed");
      ++config_warnings;
    }
  }

  // Initialize status file.
  bool modify_status(false);
  if (!has_already_been_loaded ||
      config->status_file() != new_cfg.status_file())
    modify_status = true;

  // Cleanup.
  //  if (modify_perfdata)
  //    xpddefault_cleanup_performance_data();
  if (modify_status)
    xsddefault_cleanup_status_data(true);

  // Set new values.
  config->accept_passive_host_checks(new_cfg.accept_passive_host_checks());
  config->accept_passive_service_checks(
      new_cfg.accept_passive_service_checks());
  config->additional_freshness_latency(new_cfg.additional_freshness_latency());
  config->admin_email(new_cfg.admin_email());
  config->admin_pager(new_cfg.admin_pager());
  config->allow_empty_hostgroup_assignment(
      new_cfg.allow_empty_hostgroup_assignment());
  config->auto_reschedule_checks(new_cfg.auto_reschedule_checks());
  config->auto_rescheduling_interval(new_cfg.auto_rescheduling_interval());
  config->auto_rescheduling_window(new_cfg.auto_rescheduling_window());
  config->cached_host_check_horizon(new_cfg.cached_host_check_horizon());
  config->cached_service_check_horizon(new_cfg.cached_service_check_horizon());
  config->cfg_main(new_cfg.cfg_main());
  config->check_external_commands(new_cfg.check_external_commands());
  config->check_host_freshness(new_cfg.check_host_freshness());
  config->set_check_orphaned_hosts(new_cfg.check_orphaned_hosts());
  config->set_check_orphaned_services(new_cfg.check_orphaned_services());
  config->set_check_reaper_interval(new_cfg.check_reaper_interval());
  config->check_service_freshness(new_cfg.check_service_freshness());
  config->command_check_interval(new_cfg.command_check_interval(),
                                 new_cfg.command_check_interval_is_seconds());
  config->date_format(new_cfg.date_format());
  config->debug_file(new_cfg.debug_file());
  config->debug_level(new_cfg.debug_level());
  config->debug_verbosity(new_cfg.debug_verbosity());
  config->enable_environment_macros(new_cfg.enable_environment_macros());
  config->enable_event_handlers(new_cfg.enable_event_handlers());
  config->enable_flap_detection(new_cfg.enable_flap_detection());
  config->enable_notifications(new_cfg.enable_notifications());
  config->enable_predictive_host_dependency_checks(
      new_cfg.enable_predictive_host_dependency_checks());
  config->enable_predictive_service_dependency_checks(
      new_cfg.enable_predictive_service_dependency_checks());
  config->event_broker_options(new_cfg.event_broker_options());
  config->event_handler_timeout(new_cfg.event_handler_timeout());
  config->execute_host_checks(new_cfg.execute_host_checks());
  config->execute_service_checks(new_cfg.execute_service_checks());
  config->global_host_event_handler(new_cfg.global_host_event_handler());
  config->global_service_event_handler(new_cfg.global_service_event_handler());
  config->high_host_flap_threshold(new_cfg.high_host_flap_threshold());
  config->high_service_flap_threshold(new_cfg.high_service_flap_threshold());
  config->host_check_timeout(new_cfg.host_check_timeout());
  config->host_freshness_check_interval(
      new_cfg.host_freshness_check_interval());
  config->host_inter_check_delay_method(
      new_cfg.host_inter_check_delay_method());
  config->host_perfdata_command(new_cfg.host_perfdata_command());
  config->set_illegal_object_chars(new_cfg.illegal_object_chars());
  config->set_illegal_output_chars(new_cfg.illegal_output_chars());
  config->interval_length(new_cfg.interval_length());
  config->log_event_handlers(new_cfg.log_event_handlers());
  config->log_external_commands(new_cfg.log_external_commands());
  config->log_file(new_cfg.log_file());
  config->log_host_retries(new_cfg.log_host_retries());
  config->log_notifications(new_cfg.log_notifications());
  config->log_passive_checks(new_cfg.log_passive_checks());
  config->log_service_retries(new_cfg.log_service_retries());
  config->low_host_flap_threshold(new_cfg.low_host_flap_threshold());
  config->low_service_flap_threshold(new_cfg.low_service_flap_threshold());
  config->max_debug_file_size(new_cfg.max_debug_file_size());
  config->max_host_check_spread(new_cfg.max_host_check_spread());
  config->max_log_file_size(new_cfg.max_log_file_size());
  config->set_max_parallel_service_checks(
      new_cfg.max_parallel_service_checks());
  config->max_service_check_spread(new_cfg.max_service_check_spread());
  config->notification_timeout(new_cfg.notification_timeout());
  config->obsess_over_hosts(new_cfg.obsess_over_hosts());
  config->obsess_over_services(new_cfg.obsess_over_services());
  config->ochp_command(new_cfg.ochp_command());
  config->ochp_timeout(new_cfg.ochp_timeout());
  config->ocsp_command(new_cfg.ocsp_command());
  config->ocsp_timeout(new_cfg.ocsp_timeout());
  config->perfdata_timeout(new_cfg.perfdata_timeout());
  config->process_performance_data(new_cfg.process_performance_data());
  config->resource_file(new_cfg.resource_file());
  config->retain_state_information(new_cfg.retain_state_information());
  config->retained_contact_host_attribute_mask(
      new_cfg.retained_contact_host_attribute_mask());
  config->retained_contact_service_attribute_mask(
      new_cfg.retained_contact_service_attribute_mask());
  config->retained_host_attribute_mask(new_cfg.retained_host_attribute_mask());
  config->retained_process_host_attribute_mask(
      new_cfg.retained_process_host_attribute_mask());
  config->retention_scheduling_horizon(new_cfg.retention_scheduling_horizon());
  config->retention_update_interval(new_cfg.retention_update_interval());
  config->service_check_timeout(new_cfg.service_check_timeout());
  config->service_freshness_check_interval(
      new_cfg.service_freshness_check_interval());
  config->service_inter_check_delay_method(
      new_cfg.service_inter_check_delay_method());
  config->service_interleave_factor_method(
      new_cfg.service_interleave_factor_method());
  config->service_perfdata_command(new_cfg.service_perfdata_command());
  config->sleep_time(new_cfg.sleep_time());
  config->soft_state_dependencies(new_cfg.soft_state_dependencies());
  config->state_retention_file(new_cfg.state_retention_file());
  config->status_file(new_cfg.status_file());
  config->status_update_interval(new_cfg.status_update_interval());
  config->time_change_threshold(new_cfg.time_change_threshold());
  config->use_large_installation_tweaks(
      new_cfg.use_large_installation_tweaks());
  config->instance_heartbeat_interval(new_cfg.instance_heartbeat_interval());
  config->set_use_regexp_matches(new_cfg.use_regexp_matches());
  config->use_retained_program_state(new_cfg.use_retained_program_state());
  config->use_retained_scheduling_info(new_cfg.use_retained_scheduling_info());
  config->use_setpgid(new_cfg.use_setpgid());
  config->use_syslog(new_cfg.use_syslog());
  config->log_v2_enabled(new_cfg.log_v2_enabled());
  config->log_legacy_enabled(new_cfg.log_legacy_enabled());
  config->log_v2_logger(new_cfg.log_v2_logger());
  config->log_level_functions(new_cfg.log_level_functions());
  config->log_level_config(new_cfg.log_level_config());
  config->log_level_events(new_cfg.log_level_events());
  config->log_level_checks(new_cfg.log_level_checks());
  config->log_level_notifications(new_cfg.log_level_notifications());
  config->log_level_eventbroker(new_cfg.log_level_eventbroker());
  config->log_level_external_command(new_cfg.log_level_external_command());
  config->log_level_commands(new_cfg.log_level_commands());
  config->log_level_downtimes(new_cfg.log_level_downtimes());
  config->log_level_comments(new_cfg.log_level_comments());
  config->log_level_macros(new_cfg.log_level_macros());
  config->use_true_regexp_matching(new_cfg.use_true_regexp_matching());
  config->user(new_cfg.user());

  // Set this variable just the first time.
  if (!has_already_been_loaded) {
    config->broker_module(new_cfg.broker_module());
    config->broker_module_directory(new_cfg.broker_module_directory());
    config->command_file(new_cfg.command_file());
    config->external_command_buffer_slots(
        new_cfg.external_command_buffer_slots());
    config->use_timezone(new_cfg.use_timezone());
  }

  // Initialize.
  if (modify_status)
    xsddefault_initialize_status_data();

  // Check global event handler commands...
  if (verify_config) {
    engine_logger(log_info_message, basic)
        << "Checking global event handlers...";
    events_logger->info("Checking global event handlers...");
  }
  if (!config->global_host_event_handler().empty()) {
    // Check the event handler command.
    std::string temp_command_name(config->global_host_event_handler().substr(
        0, config->global_host_event_handler().find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Global host event handler command '" << temp_command_name
          << "' is not defined anywhere!";
      config_logger->error(
          "Error: Global host event handler command '{}' is not defined "
          "anywhere!",
          temp_command_name);
      ++config_errors;
      global_host_event_handler_ptr = nullptr;
    } else
      global_host_event_handler_ptr = found->second.get();
  }
  if (!config->global_service_event_handler().empty()) {
    // Check the event handler command.
    std::string temp_command_name(config->global_service_event_handler().substr(
        0, config->global_service_event_handler().find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Global service event handler command '"
          << temp_command_name << "' is not defined anywhere!";
      config_logger->error(
          "Error: Global service event handler command '{}' is not defined "
          "anywhere!",
          temp_command_name);
      ++config_errors;
      global_service_event_handler_ptr = nullptr;
    } else
      global_service_event_handler_ptr = found->second.get();
  }

  // Check obsessive processor commands...
  if (verify_config) {
    engine_logger(log_info_message, basic)
        << "Checking obsessive compulsive processor commands...";
    events_logger->info("Checking obsessive compulsive processor commands...");
  }
  if (!config->ocsp_command().empty()) {
    std::string temp_command_name(config->ocsp_command().substr(
        0, config->ocsp_command().find_first_of('!')));
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
      ++config_errors;
      ocsp_command_ptr = nullptr;
    } else
      ocsp_command_ptr = found->second.get();
  }
  if (!config->ochp_command().empty()) {
    std::string temp_command_name(config->ochp_command().substr(
        0, config->ochp_command().find_first_of('!')));
    command_map::iterator found{
        commands::command::commands.find(temp_command_name)};
    if (found == commands::command::commands.end() || !found->second) {
      engine_logger(log_verification_error, basic)
          << "Error: Obsessive compulsive host processor command '"
          << temp_command_name << "' is not defined anywhere!";
      config_logger->error(
          "Error: Obsessive compulsive host processor command '{}' is not "
          "defined anywhere!",
          temp_command_name);
      ++config_errors;
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
template <typename ConfigurationType, typename Key, typename ApplierType>
void applier::state::_pb_apply(
    const pb_difference<ConfigurationType, Key>& diff) {
  // Applier.
  ApplierType aplyr;

  // Modify objects.
  for (auto& p : diff.modified()) {
    if (!verify_config)
      aplyr.modify_object(p.first, p.second);
    else {
      try {
        aplyr.modify_object(p.first, p.second);
      } catch (const std::exception& e) {
        ++config_errors;
        events_logger->info(e.what());
      }
    }
  }

  // Erase objects.
  for (auto it = diff.deleted().rbegin(); it != diff.deleted().rend(); ++it) {
    ssize_t idx = it->first;
    if (!verify_config)
      aplyr.remove_object(idx);
    else {
      try {
        aplyr.remove_object(idx);
      } catch (const std::exception& e) {
        ++config_errors;
        events_logger->info(e.what());
      }
    }
  }

  // Add objects.
  for (auto& obj : diff.added()) {
    if (!verify_config)
      aplyr.add_object(obj);
    else {
      try {
        aplyr.add_object(obj);
      } catch (const std::exception& e) {
        ++config_errors;
        events_logger->info(e.what());
      }
    }
  }
}

/**
 *  @brief Apply configuration of a specific object type.
 *
 *  This method will perform a diff on cur_cfg and new_cfg to create the
 *  three element sets : added, modified and removed. The type applier
 *  will then be called to 1) remove old objects 2) create new objects
 *  3) modify existing objects, in this order.
 *
 *  @param[in] cur_cfg Current configuration set.
 *  @param[in] new_cfg New configuration set.
 */
template <typename ConfigurationType, typename ApplierType>
void applier::state::_apply(
    difference<std::set<ConfigurationType>> const& diff) {
  // Type alias.
  typedef std::set<ConfigurationType> cfg_set;

  /*
  ** Configuration application.
  */

  // Applier.
  ApplierType aplyr;

  // Erase objects.
  for (typename cfg_set::const_iterator it_delete(diff.deleted().begin()),
       end_delete(diff.deleted().end());
       it_delete != end_delete; ++it_delete) {
    if (!verify_config)
      aplyr.remove_object(*it_delete);
    else {
      try {
        aplyr.remove_object(*it_delete);
      } catch (std::exception const& e) {
        ++config_errors;
        engine_logger(log_info_message, basic) << e.what();
        events_logger->info(e.what());
      }
    }
  }

  // Add objects.
  for (typename cfg_set::const_iterator it_create = diff.added().begin(),
                                        end_create = diff.added().end();
       it_create != end_create; ++it_create) {
    if (!verify_config)
      aplyr.add_object(*it_create);
    else {
      try {
        aplyr.add_object(*it_create);
      } catch (std::exception const& e) {
        ++config_errors;
        engine_logger(log_info_message, basic) << e.what();
        events_logger->info(e.what());
      }
    }
  }

  // Modify objects.
  for (typename cfg_set::const_iterator it_modify = diff.modified().begin(),
                                        end_modify = diff.modified().end();
       it_modify != end_modify; ++it_modify) {
    if (!verify_config)
      aplyr.modify_object(*it_modify);
    else {
      try {
        aplyr.modify_object(*it_modify);
      } catch (std::exception const& e) {
        ++config_errors;
        engine_logger(log_info_message, basic) << e.what();
        events_logger->info(e.what());
      }
    }
  }
}

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
            << " used in serviceescalation " << p.second->uuid().to_string()
            << " is not or badly defined";
        config_logger->error(
            "Error on contactgroup !!! The contactgroup {} used in "
            "serviceescalation {} is not or badly defined",
            pp.first, p.second->uuid().to_string());
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
            << " used in hostescalation " << p.second->uuid().to_string()
            << " is not or badly defined";
        config_logger->error(
            "Error on contactgroup !!! The contactgroup {} used in "
            "hostescalation {} is not or badly defined",
            pp.first, p.second->uuid().to_string());
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

/**
 *  Apply retention.
 *
 *  @param[in] new_cfg New configuration set.
 *  @param[in] state   The retention state to use.
 */
void applier::state::_pb_apply(configuration::State& new_cfg,
                               retention::state& state) {
  retention::applier::state app_state;
  if (!verify_config)
    app_state.apply(new_cfg, state);
  else {
    try {
      app_state.apply(new_cfg, state);
    } catch (std::exception const& e) {
      ++config_errors;
      std::cout << e.what();
    }
  }
}

/**
 *  Apply retention.
 *
 *  @param[in] new_cfg New configuration set.
 *  @param[in] state   The retention state to use.
 */
void applier::state::_apply(configuration::state& new_cfg,
                            retention::state& state) {
  retention::applier::state app_state;
  if (!verify_config)
    app_state.apply(new_cfg, state);
  else {
    try {
      app_state.apply(new_cfg, state);
    } catch (std::exception const& e) {
      ++config_errors;
      std::cout << e.what();
    }
  }
}

/**
 *  Expand objects.
 *
 *  @param[in,out] new_state New configuration state.
 *  @param[in,out] cfg       Configuration objects.
 */
template <typename ConfigurationType, typename ApplierType>
void applier::state::_expand(configuration::State& new_state) {
  ApplierType aplyr;
  try {
    aplyr.expand_objects(new_state);
  } catch (std::exception const& e) {
    if (verify_config) {
      ++config_errors;
      std::cout << e.what();
    } else
      throw;
  }
}

/**
 *  Expand objects.
 *
 *  @param[in,out] new_state New configuration state.
 *  @param[in,out] cfg       Configuration objects.
 */
template <typename ConfigurationType, typename ApplierType>
void applier::state::_expand(configuration::state& new_state) {
  ApplierType aplyr;
  try {
    aplyr.expand_objects(new_state);
  } catch (std::exception const& e) {
    if (verify_config) {
      ++config_errors;
      std::cout << e.what();
    } else
      throw;
  }
}

/**
 *  Process new configuration and apply it.
 *
 *  @param[in] new_cfg        The new configuration.
 *  @param[in] state          The retention to use.
 */
void applier::state::_processing(configuration::State& new_cfg,
                                 retention::state* state) {
  // Timing.
  struct timeval tv[5];

  // Call prelauch broker event the first time to run applier state.
  if (!has_already_been_loaded)
    broker_program_state(NEBTYPE_PROCESS_PRELAUNCH, NEBFLAG_NONE);

  //
  // Expand all objects.
  //
  gettimeofday(tv, nullptr);

  // Expand timeperiods.
  _expand<configuration::Timeperiod, applier::timeperiod>(new_cfg);

  // Expand connectors.
  _expand<configuration::Connector, applier::connector>(new_cfg);

  // Expand commands.
  _expand<configuration::Command, applier::command>(new_cfg);

  // Expand contacts.
  _expand<configuration::Contact, applier::contact>(new_cfg);

  // Expand contactgroups.
  _expand<configuration::Contactgroup, applier::contactgroup>(new_cfg);

  // Expand hosts.
  _expand<configuration::Host, applier::host>(new_cfg);

  // Expand hostgroups.
  _expand<configuration::Hostgroup, applier::hostgroup>(new_cfg);

  // Expand services.
  _expand<configuration::Service, applier::service>(new_cfg);

  // Expand anomalydetections.
  _expand<configuration::Anomalydetection, applier::anomalydetection>(new_cfg);

  // Expand servicegroups.
  _expand<configuration::Servicegroup, applier::servicegroup>(new_cfg);

  // Expand hostdependencies.
  _expand<configuration::Hostdependency, applier::hostdependency>(new_cfg);

  // Expand servicedependencies.
  _expand<configuration::Servicedependency, applier::servicedependency>(
      new_cfg);

  // Expand hostescalations.
  _expand<configuration::Hostescalation, applier::hostescalation>(new_cfg);

  // Expand serviceescalations.
  _expand<configuration::Serviceescalation, applier::serviceescalation>(
      new_cfg);

  //
  //  Build difference for all objects.
  //

  // Build difference for timeperiods.
  pb_difference<configuration::Timeperiod, std::string> diff_timeperiods;
  diff_timeperiods.parse(
      pb_config.timeperiods().begin(), pb_config.timeperiods().end(),
      new_cfg.timeperiods().begin(), new_cfg.timeperiods().end(),
      &configuration::Timeperiod::timeperiod_name);

  // Build difference for connectors.
  pb_difference<configuration::Connector, std::string> diff_connectors;
  diff_connectors.parse(
      pb_config.connectors().begin(), pb_config.connectors().end(),
      new_cfg.connectors().begin(), new_cfg.connectors().end(),
      &configuration::Connector::connector_name);

  // Build difference for commands.
  pb_difference<configuration::Command, std::string> diff_commands;
  diff_commands.parse(pb_config.commands().begin(), pb_config.commands().end(),
                      new_cfg.commands().begin(), new_cfg.commands().end(),
                      &configuration::Command::command_name);

  // Build difference for severities.
  pb_difference<configuration::Severity, std::pair<uint64_t, uint32_t>>
      diff_severities;
  diff_severities.parse(
      pb_config.severities().begin(), pb_config.severities().end(),
      new_cfg.severities().begin(), new_cfg.severities().end(),
      [](const configuration::Severity& sev) -> std::pair<uint64_t, uint32_t> {
        return std::make_pair(sev.key().id(), sev.key().type());
      });

  // Build difference for tags.
  pb_difference<configuration::Tag, std::pair<uint64_t, uint32_t>> diff_tags;
  diff_tags.parse(
      pb_config.tags().begin(), pb_config.tags().end(), new_cfg.tags().begin(),
      new_cfg.tags().end(),
      [](const configuration::Tag& tg) -> std::pair<uint64_t, uint32_t> {
        return std::make_pair(tg.key().id(), tg.key().type());
      });

  // Build difference for contacts.
  pb_difference<configuration::Contact, std::string> diff_contacts;
  diff_contacts.parse(pb_config.contacts().begin(), pb_config.contacts().end(),
                      new_cfg.contacts().begin(), new_cfg.contacts().end(),
                      &configuration::Contact::contact_name);

  // Build difference for contactgroups.
  pb_difference<configuration::Contactgroup, std::string> diff_contactgroups;
  diff_contactgroups.parse(
      pb_config.contactgroups().begin(), pb_config.contactgroups().end(),
      new_cfg.contactgroups().begin(), new_cfg.contactgroups().end(),
      &configuration::Contactgroup::contactgroup_name);

  // Build difference for hosts.
  pb_difference<configuration::Host, uint64_t> diff_hosts;
  diff_hosts.parse(pb_config.hosts().begin(), pb_config.hosts().end(),
                   new_cfg.hosts().begin(), new_cfg.hosts().end(),
                   &configuration::Host::host_id);

  // Build difference for hostgroups.
  pb_difference<configuration::Hostgroup, std::string> diff_hostgroups;
  diff_hostgroups.parse(
      pb_config.hostgroups().begin(), pb_config.hostgroups().end(),
      new_cfg.hostgroups().begin(), new_cfg.hostgroups().end(),
      &configuration::Hostgroup::hostgroup_name);

  // Build difference for services.
  pb_difference<configuration::Service, std::pair<uint64_t, uint64_t>>
      diff_services;
  diff_services.parse(pb_config.services().begin(), pb_config.services().end(),
                      new_cfg.services().begin(), new_cfg.services().end(),
                      [](const configuration::Service& s) {
                        return std::make_pair(s.host_id(), s.service_id());
                      });

  // Build difference for anomalydetections.
  pb_difference<configuration::Anomalydetection, std::pair<uint64_t, uint64_t>>
      diff_anomalydetections;
  diff_anomalydetections.parse(
      pb_config.anomalydetections().begin(),
      pb_config.anomalydetections().end(), new_cfg.anomalydetections().begin(),
      new_cfg.anomalydetections().end(),
      [](const configuration::Anomalydetection& ad) {
        return std::make_pair(ad.host_id(), ad.service_id());
      });

  // Build difference for servicegroups.
  pb_difference<configuration::Servicegroup, std::string> diff_servicegroups;
  diff_servicegroups.parse(
      pb_config.servicegroups().begin(), pb_config.servicegroups().end(),
      new_cfg.servicegroups().begin(), new_cfg.servicegroups().end(),
      &configuration::Servicegroup::servicegroup_name);

  // Build difference for hostdependencies.
  pb_difference<configuration::Hostdependency, size_t> diff_hostdependencies;
  typedef size_t (*key_func)(const configuration::Hostdependency&);
  diff_hostdependencies.parse<key_func>(
      pb_config.hostdependencies().begin(), pb_config.hostdependencies().end(),
      new_cfg.hostdependencies().begin(), new_cfg.hostdependencies().end(),
      configuration::hostdependency_key);

  // Build difference for servicedependencies.
  pb_difference<configuration::Servicedependency, size_t>
      diff_servicedependencies;
  typedef size_t (*key_func_sd)(const configuration::Servicedependency&);
  diff_servicedependencies.parse<key_func_sd>(
      pb_config.servicedependencies().begin(),
      pb_config.servicedependencies().end(),
      new_cfg.servicedependencies().begin(),
      new_cfg.servicedependencies().end(),
      configuration::servicedependency_key);

  // Build difference for hostdependencies.
  pb_difference<configuration::Hostescalation, size_t> diff_hostescalations;
  typedef size_t (*key_func_he)(const configuration::Hostescalation&);
  diff_hostescalations.parse<key_func_he>(
      pb_config.hostescalations().begin(), pb_config.hostescalations().end(),
      new_cfg.hostescalations().begin(), new_cfg.hostescalations().end(),
      configuration::hostescalation_key);

  // Build difference for servicedependencies.
  pb_difference<configuration::Serviceescalation, size_t>
      diff_serviceescalations;
  typedef size_t (*key_func_se)(const configuration::Serviceescalation&);
  diff_serviceescalations.parse<key_func_se>(
      pb_config.serviceescalations().begin(),
      pb_config.serviceescalations().end(),
      new_cfg.serviceescalations().begin(), new_cfg.serviceescalations().end(),
      configuration::serviceescalation_key);

  // Timing.
  gettimeofday(tv + 1, nullptr);

  try {
    std::lock_guard<std::mutex> locker(_apply_lock);

    // Apply logging configurations.

    applier::logging::instance().apply(new_cfg);

    apply_log_config(new_cfg);

    // Apply globals configurations.
    applier::globals::instance().apply(new_cfg);

    // Apply macros configurations.
    applier::macros::instance().apply(new_cfg);

    // Timing.
    gettimeofday(tv + 2, nullptr);

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
    _pb_apply<configuration::Timeperiod, std::string, applier::timeperiod>(
        diff_timeperiods);
    _pb_resolve<configuration::Timeperiod, applier::timeperiod>(
        pb_config.timeperiods());

    // Apply connectors.
    _pb_apply<configuration::Connector, std::string, applier::connector>(
        diff_connectors);
    _pb_resolve<configuration::Connector, applier::connector>(
        pb_config.connectors());

    // Apply commands.
    _pb_apply<configuration::Command, std::string, applier::command>(
        diff_commands);
    _pb_resolve<configuration::Command, applier::command>(pb_config.commands());

    // Apply contacts and contactgroups.
    _pb_apply<configuration::Contact, std::string, applier::contact>(
        diff_contacts);
    _pb_apply<configuration::Contactgroup, std::string, applier::contactgroup>(
        diff_contactgroups);
    _pb_resolve<configuration::Contactgroup, applier::contactgroup>(
        pb_config.contactgroups());
    _pb_resolve<configuration::Contact, applier::contact>(pb_config.contacts());

    // Apply severities.
    _pb_apply<configuration::Severity, std::pair<uint64_t, uint32_t>,
              applier::severity>(diff_severities);

    // Apply tags.
    _pb_apply<configuration::Tag, std::pair<uint64_t, uint32_t>, applier::tag>(
        diff_tags);

    // Apply hosts and hostgroups.
    _pb_apply<configuration::Host, uint64_t, applier::host>(diff_hosts);
    _pb_apply<configuration::Hostgroup, std::string, applier::hostgroup>(
        diff_hostgroups);

    // Apply services.
    _pb_apply<configuration::Service, std::pair<uint64_t, uint64_t>,
              applier::service>(diff_services);

    // Apply anomalydetections.
    _pb_apply<configuration::Anomalydetection, std::pair<uint64_t, uint64_t>,
              applier::anomalydetection>(diff_anomalydetections);

    // Apply servicegroups.
    _pb_apply<configuration::Servicegroup, std::string, applier::servicegroup>(
        diff_servicegroups);

    // Resolve hosts, services, host groups.
    _pb_resolve<configuration::Host, applier::host>(pb_config.hosts());
    _pb_resolve<configuration::Hostgroup, applier::hostgroup>(
        pb_config.hostgroups());

    // Resolve services.
    _pb_resolve<configuration::Service, applier::service>(pb_config.services());

    // Resolve anomalydetections.
    _pb_resolve<configuration::Anomalydetection, applier::anomalydetection>(
        pb_config.anomalydetections());

    // Resolve service groups.
    _pb_resolve<configuration::Servicegroup, applier::servicegroup>(
        pb_config.servicegroups());

    // Apply host dependencies.
    _pb_apply<configuration::Hostdependency, size_t, applier::hostdependency>(
        diff_hostdependencies);
    _pb_resolve<configuration::Hostdependency, applier::hostdependency>(
        pb_config.hostdependencies());

    // Apply service dependencies.
    _pb_apply<configuration::Servicedependency, size_t,
              applier::servicedependency>(diff_servicedependencies);
    _pb_resolve<configuration::Servicedependency, applier::servicedependency>(
        pb_config.servicedependencies());

    // Apply host escalations.
    _pb_apply<configuration::Hostescalation, size_t, applier::hostescalation>(
        diff_hostescalations);
    _pb_resolve<configuration::Hostescalation, applier::hostescalation>(
        pb_config.hostescalations());

    // Apply service escalations.
    _pb_apply<configuration::Serviceescalation, size_t,
              applier::serviceescalation>(diff_serviceescalations);
    _pb_resolve<configuration::Serviceescalation, applier::serviceescalation>(
        pb_config.serviceescalations());

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

    // Load retention.
    if (state)
      _pb_apply(new_cfg, *state);

    // Apply scheduler.
    if (!verify_config)
      applier::scheduler::instance().apply(new_cfg, diff_hosts, diff_services,
                                           diff_anomalydetections);

    // Apply new global on the current state.
    if (!verify_config)
      _pb_apply(new_cfg);
    else {
      try {
        _pb_apply(new_cfg);
      } catch (std::exception const& e) {
        ++config_errors;
        events_logger->info(e.what());
      }
    }

    // Timing.
    gettimeofday(tv + 3, nullptr);

    // Check for circular paths between hosts.
    pre_flight_circular_check(&config_warnings, &config_errors);

    // Call start broker event the first time to run applier state.
    if (!has_already_been_loaded) {
      neb_load_all_modules();

      broker_program_state(NEBTYPE_PROCESS_START, NEBFLAG_NONE);
    } else
      neb_reload_all_modules();

    // Print initial states of new hosts and services.
    if (!verify_config && !test_scheduling) {
      for (auto a : diff_hosts.added()) {
        auto it_hst = engine::host::hosts_by_id.find(a.host_id());
        if (it_hst != engine::host::hosts_by_id.end())
          log_host_state(INITIAL_STATES, it_hst->second.get());
      }
      for (auto a : diff_services.added()) {
        auto it_svc =
            engine::service::services_by_id.find({a.host_id(), a.service_id()});
        if (it_svc != engine::service::services_by_id.end())
          log_service_state(INITIAL_STATES, it_svc->second.get());
      }
    }

    // Timing.
    gettimeofday(tv + 4, nullptr);
    if (test_scheduling) {
      double runtimes[5];
      runtimes[4] = 0.0;
      for (unsigned int i(0); i < (sizeof(runtimes) / sizeof(*runtimes) - 1);
           ++i) {
        runtimes[i] = tv[i + 1].tv_sec - tv[i].tv_sec +
                      (tv[i + 1].tv_usec - tv[i].tv_usec) / 1000000.0;
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

/**
 * @brief Compute the difference between the running configuration pb_config and
 * the new one new_cfg.
 *
 * @param new_cfg The new configuration to compare with.
 *
 * @return An object with differences.
 */
DiffState applier::state::build_difference(
    const configuration::State& cfg,
    const configuration::State& new_cfg) const {
  diff_state dstate;

  ::google::protobuf::util::MessageDifferencer differencer;
  differencer.set_report_matches(false);
  differencer.ReportDifferencesTo(&dstate);
  differencer.Compare(cfg, new_cfg);
  return dstate.report();
}

void applier::state::apply_log_config(configuration::State& new_cfg) {
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

    log_cfg.apply_custom_sinks({"functions", "configuration", "events",
                                "checks", "notifications", "eventbroker",
                                "external_command", "commands", "downtimes",
                                "comments", "macros", "process", "runtime"});
    log_cfg.set_level("functions", new_cfg.log_level_functions());
    log_cfg.set_level("configuration", new_cfg.log_level_config());
    log_cfg.set_level("events", new_cfg.log_level_events());
    log_cfg.set_level("checks", new_cfg.log_level_checks());
    log_cfg.set_level("notifications", new_cfg.log_level_notifications());
    log_cfg.set_level("eventbroker", new_cfg.log_level_eventbroker());
    log_cfg.set_level("external_command", new_cfg.log_level_external_command());
    log_cfg.set_level("commands", new_cfg.log_level_commands());
    log_cfg.set_level("downtimes", new_cfg.log_level_downtimes());
    log_cfg.set_level("comments", new_cfg.log_level_comments());
    log_cfg.set_level("macros", new_cfg.log_level_macros());
    log_cfg.set_level("process", new_cfg.log_level_process());
    log_cfg.set_level("runtime", new_cfg.log_level_runtime());
    log_v2::instance().apply(log_cfg);
  } else
    log_v2::instance().disable();
  init_loggers();
}

void applier::state::apply_log_config(configuration::state& new_cfg) {
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

    log_cfg.apply_custom_sinks({"functions", "configuration", "events",
                                "checks", "notifications", "eventbroker",
                                "external_command", "commands", "downtimes",
                                "comments", "macros", "process", "runtime"});
    log_cfg.set_level("functions", new_cfg.log_level_functions());
    log_cfg.set_level("configuration", new_cfg.log_level_config());
    log_cfg.set_level("events", new_cfg.log_level_events());
    log_cfg.set_level("checks", new_cfg.log_level_checks());
    log_cfg.set_level("notifications", new_cfg.log_level_notifications());
    log_cfg.set_level("eventbroker", new_cfg.log_level_eventbroker());
    log_cfg.set_level("external_command", new_cfg.log_level_external_command());
    log_cfg.set_level("commands", new_cfg.log_level_commands());
    log_cfg.set_level("downtimes", new_cfg.log_level_downtimes());
    log_cfg.set_level("comments", new_cfg.log_level_comments());
    log_cfg.set_level("macros", new_cfg.log_level_macros());
    log_cfg.set_level("process", new_cfg.log_level_process());
    log_cfg.set_level("runtime", new_cfg.log_level_runtime());
    log_v2::instance().apply(log_cfg);
  } else
    log_v2::instance().disable();
  init_loggers();
}

/**
 *  Process new configuration and apply it.
 *
 *  @param[in] new_cfg        The new configuration.
 *  @param[in] state          The retention to use.
 */
void applier::state::_processing(configuration::state& new_cfg,
                                 retention::state* state) {
  // Timing.
  struct timeval tv[5];

  // Call prelauch broker event the first time to run applier state.
  if (!has_already_been_loaded)
    broker_program_state(NEBTYPE_PROCESS_PRELAUNCH, NEBFLAG_NONE);

  //
  // Expand all objects.
  //
  gettimeofday(tv, nullptr);

  // Expand timeperiods.
  _expand<configuration::timeperiod, applier::timeperiod>(new_cfg);

  // Expand connectors.
  _expand<configuration::connector, applier::connector>(new_cfg);

  // Expand commands.
  _expand<configuration::command, applier::command>(new_cfg);

  // Expand contacts.
  _expand<configuration::contact, applier::contact>(new_cfg);

  // Expand contactgroups.
  _expand<configuration::contactgroup, applier::contactgroup>(new_cfg);

  // Expand hosts.
  _expand<configuration::host, applier::host>(new_cfg);

  // Expand hostgroups.
  _expand<configuration::hostgroup, applier::hostgroup>(new_cfg);

  // Expand services.
  _expand<configuration::service, applier::service>(new_cfg);

  // Expand anomalydetections.
  _expand<configuration::anomalydetection, applier::anomalydetection>(new_cfg);

  // Expand servicegroups.
  _expand<configuration::servicegroup, applier::servicegroup>(new_cfg);

  // Expand hostdependencies.
  _expand<configuration::hostdependency, applier::hostdependency>(new_cfg);

  // Expand servicedependencies.
  _expand<configuration::servicedependency, applier::servicedependency>(
      new_cfg);

  // Expand hostescalations.
  _expand<configuration::hostescalation, applier::hostescalation>(new_cfg);

  // Expand serviceescalations.
  _expand<configuration::serviceescalation, applier::serviceescalation>(
      new_cfg);

  //
  //  Build difference for all objects.
  //

  // Build difference for timeperiods.
  difference<set_timeperiod> diff_timeperiods;
  diff_timeperiods.parse(config->timeperiods(), new_cfg.timeperiods());

  // Build difference for connectors.
  difference<set_connector> diff_connectors;
  diff_connectors.parse(config->connectors(), new_cfg.connectors());

  // Build difference for commands.
  difference<set_command> diff_commands;
  diff_commands.parse(config->commands(), new_cfg.commands());

  // Build difference for severities.
  difference<set_severity> diff_severities;
  diff_severities.parse(config->severities(), new_cfg.severities());

  // Build difference for tags.
  difference<set_tag> diff_tags;
  diff_tags.parse(config->tags(), new_cfg.tags());

  // Build difference for contacts.
  difference<set_contact> diff_contacts;
  diff_contacts.parse(config->contacts(), new_cfg.contacts());

  // Build difference for contactgroups.
  difference<set_contactgroup> diff_contactgroups;
  diff_contactgroups.parse(config->contactgroups(), new_cfg.contactgroups());

  // Build difference for hosts.
  difference<set_host> diff_hosts;
  diff_hosts.parse(config->hosts(), new_cfg.hosts());

  // Build difference for hostgroups.
  difference<set_hostgroup> diff_hostgroups;
  diff_hostgroups.parse(config->hostgroups(), new_cfg.hostgroups());

  // Build difference for services.
  difference<set_service> diff_services;
  diff_services.parse(config->services(), new_cfg.services());

  // Build difference for anomalydetections.
  difference<set_anomalydetection> diff_anomalydetections;
  diff_anomalydetections.parse(config->anomalydetections(),
                               new_cfg.anomalydetections());

  // Build difference for servicegroups.
  difference<set_servicegroup> diff_servicegroups;
  diff_servicegroups.parse(config->servicegroups(), new_cfg.servicegroups());

  // Build difference for hostdependencies.
  difference<set_hostdependency> diff_hostdependencies;
  diff_hostdependencies.parse(config->hostdependencies(),
                              new_cfg.hostdependencies());

  // Build difference for servicedependencies.
  difference<set_servicedependency> diff_servicedependencies;
  diff_servicedependencies.parse(config->servicedependencies(),
                                 new_cfg.servicedependencies());

  // Build difference for hostescalations.
  difference<set_hostescalation> diff_hostescalations;
  diff_hostescalations.parse(config->hostescalations(),
                             new_cfg.hostescalations());

  // Build difference for serviceescalations.
  difference<set_serviceescalation> diff_serviceescalations;
  diff_serviceescalations.parse(config->serviceescalations(),
                                new_cfg.serviceescalations());

  // Timing.
  gettimeofday(tv + 1, nullptr);

  try {
    std::lock_guard<std::mutex> locker(_apply_lock);

    // Apply logging configurations.

    applier::logging::instance().apply(new_cfg);

    apply_log_config(new_cfg);

    // Apply globals configurations.
    applier::globals::instance().apply(new_cfg);

    // Apply macros configurations.
    applier::macros::instance().apply(new_cfg);

    // Timing.
    gettimeofday(tv + 2, nullptr);

    if (!has_already_been_loaded && !verify_config && !test_scheduling) {
      // This must be logged after we read config data,
      // as user may have changed location of main log file.
      engine_logger(log_process_info, basic)
          << "Centreon Engine " << CENTREON_ENGINE_VERSION_STRING
          << " starting ... (PID=" << getpid() << ")";
      process_logger->info(
          "Centreon Engine {} starting ... (PID={}) (Legacy configuration)",
          CENTREON_ENGINE_VERSION_STRING, getpid());

      // Log the local time - may be different than clock
      // time due to timezone offset.
      engine_logger(log_process_info, basic)
          << "Local time is " << string::ctime(program_start) << "\n"
          << "LOG VERSION: " << LOG_VERSION_2;
      process_logger->info("Local time is {}", string::ctime(program_start));
      process_logger->info("LOG VERSION: {}", LOG_VERSION_2);
    }

    //
    //  Apply and resolve all objects.
    //

    // Apply timeperiods.
    _apply<configuration::timeperiod, applier::timeperiod>(diff_timeperiods);
    _resolve<configuration::timeperiod, applier::timeperiod>(
        config->timeperiods());

    // Apply connectors.
    _apply<configuration::connector, applier::connector>(diff_connectors);
    _resolve<configuration::connector, applier::connector>(
        config->connectors());

    // Apply commands.
    _apply<configuration::command, applier::command>(diff_commands);
    _resolve<configuration::command, applier::command>(config->commands());

    // Apply contacts and contactgroups.
    _apply<configuration::contact, applier::contact>(diff_contacts);
    _apply<configuration::contactgroup, applier::contactgroup>(
        diff_contactgroups);
    _resolve<configuration::contactgroup, applier::contactgroup>(
        config->contactgroups());
    _resolve<configuration::contact, applier::contact>(config->contacts());

    // Apply severities.
    _apply<configuration::severity, applier::severity>(diff_severities);

    // Apply tags.
    _apply<configuration::tag, applier::tag>(diff_tags);

    // Apply hosts and hostgroups.
    _apply<configuration::host, applier::host>(diff_hosts);
    _apply<configuration::hostgroup, applier::hostgroup>(diff_hostgroups);

    // Apply services.
    _apply<configuration::service, applier::service>(diff_services);

    // Apply anomalydetections.
    _apply<configuration::anomalydetection, applier::anomalydetection>(
        diff_anomalydetections);

    // Apply servicegroups.
    _apply<configuration::servicegroup, applier::servicegroup>(
        diff_servicegroups);

    // Resolve hosts, services, host groups.
    _resolve<configuration::host, applier::host>(config->hosts());
    _resolve<configuration::hostgroup, applier::hostgroup>(
        config->hostgroups());

    // Resolve services.
    _resolve<configuration::service, applier::service>(config->services());

    // Resolve anomalydetections.
    _resolve<configuration::anomalydetection, applier::anomalydetection>(
        config->anomalydetections());

    // Resolve service groups.
    _resolve<configuration::servicegroup, applier::servicegroup>(
        config->servicegroups());

    // Apply host dependencies.
    _apply<configuration::hostdependency, applier::hostdependency>(
        diff_hostdependencies);
    _resolve<configuration::hostdependency, applier::hostdependency>(
        config->hostdependencies());

    // Apply service dependencies.
    _apply<configuration::servicedependency, applier::servicedependency>(
        diff_servicedependencies);
    _resolve<configuration::servicedependency, applier::servicedependency>(
        config->servicedependencies());

    // Apply host escalations.
    _apply<configuration::hostescalation, applier::hostescalation>(
        diff_hostescalations);
    _resolve<configuration::hostescalation, applier::hostescalation>(
        config->hostescalations());

    // Apply service escalations.
    _apply<configuration::serviceescalation, applier::serviceescalation>(
        diff_serviceescalations);
    _resolve<configuration::serviceescalation, applier::serviceescalation>(
        config->serviceescalations());

#ifdef DEBUG_CONFIG
    std::cout << "WARNING!! You are using a version of "
                 "centreon engine for developers!!!"
                 " This is not a production version.";
    // Checks on configuration
    _check_serviceescalations();
    _check_hostescalations();
    _check_contacts();
    _check_contactgroups();
    _check_services();
    _check_hosts();
#endif

    // Load retention.
    if (state)
      _apply(new_cfg, *state);

    // Apply scheduler.
    if (!verify_config)
      applier::scheduler::instance().apply(new_cfg, diff_hosts, diff_services,
                                           diff_anomalydetections);

    // Apply new global on the current state.
    if (!verify_config)
      _apply(new_cfg);
    else {
      try {
        _apply(new_cfg);
      } catch (std::exception const& e) {
        ++config_errors;
        engine_logger(log_info_message, basic) << e.what();
        events_logger->info(e.what());
      }
    }

    // Timing.
    gettimeofday(tv + 3, nullptr);

    // Check for circular paths between hosts.
    pre_flight_circular_check(&config_warnings, &config_errors);

    // Call start broker event the first time to run applier state.
    if (!has_already_been_loaded) {
      neb_load_all_modules();

      broker_program_state(NEBTYPE_PROCESS_START, NEBFLAG_NONE);
    } else
      neb_reload_all_modules();

    // Print initial states of new hosts and services.
    if (!verify_config && !test_scheduling) {
      for (set_host::iterator it(diff_hosts.added().begin()),
           end(diff_hosts.added().end());
           it != end; ++it) {
        host_id_map::const_iterator hst(
            engine::host::hosts_by_id.find(it->host_id()));
        if (hst != engine::host::hosts_by_id.end())
          log_host_state(INITIAL_STATES, hst->second.get());
      }
      for (set_service::iterator it(diff_services.added().begin()),
           end(diff_services.added().end());
           it != end; ++it) {
        service_id_map::const_iterator svc(engine::service::services_by_id.find(
            {it->host_id(), it->service_id()}));
        if (svc != engine::service::services_by_id.end())
          log_service_state(INITIAL_STATES, svc->second.get());
      }
    }

    // Timing.
    gettimeofday(tv + 4, nullptr);
    if (test_scheduling) {
      double runtimes[5];
      runtimes[4] = 0.0;
      for (unsigned int i(0); i < (sizeof(runtimes) / sizeof(*runtimes) - 1);
           ++i) {
        runtimes[i] = tv[i + 1].tv_sec - tv[i].tv_sec +
                      (tv[i + 1].tv_usec - tv[i].tv_usec) / 1000000.0;
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

/**
 *  Resolve objects.
 *
 *  @param[in] cfg Configuration objects.
 */
/**
 * @brief Resolve objects.
 *
 * @tparam ConfigurationType The protobuf object configuration.
 * @tparam ApplierType The applier used to handle the configuration.
 * @param cfg
 */
template <typename ConfigurationType, typename ApplierType>
void applier::state::_pb_resolve(
    const ::google::protobuf::RepeatedPtrField<ConfigurationType>& cfg) {
  ApplierType aplyr;
  for (auto& obj : cfg) {
    try {
      aplyr.resolve_object(obj);
    } catch (const std::exception& e) {
      if (verify_config) {
        ++config_errors;
        std::cout << e.what() << std::endl;
      } else
        throw;
    }
  }
}

/**
 *  Resolve objects.
 *
 *  @param[in] cfg Configuration objects.
 */
template <typename ConfigurationType, typename ApplierType>
void applier::state::_resolve(std::set<ConfigurationType>& cfg) {
  ApplierType aplyr;
  for (typename std::set<ConfigurationType>::const_iterator it(cfg.begin()),
       end(cfg.end());
       it != end; ++it) {
    try {
      aplyr.resolve_object(*it);
    } catch (std::exception const& e) {
      if (verify_config) {
        ++config_errors;
        std::cout << e.what() << std::endl;
      } else
        throw;
    }
  }
}
