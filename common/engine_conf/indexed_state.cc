/**
 * Copyright 2025 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/indexed_state.hh"
#include <absl/container/btree_map.h>
#include <google/protobuf/util/message_differencer.h>
#include "common/engine_conf/hostdependency_helper.hh"
#include "common/engine_conf/hostescalation_helper.hh"
#include "common/engine_conf/servicedependency_helper.hh"
#include "common/engine_conf/serviceescalation_helper.hh"
#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::configuration {
/**
 * @brief Constructor from a State object. The State containers are emptied
 * and their items are stored directly in the indexed_state.
 *
 * @param state
 */
indexed_state::indexed_state(std::unique_ptr<State>&& state)
    : _state{std::move(state)} {
  _index();
}

indexed_state::indexed_state(const indexed_state& other) {
  if (other._state) {
    _state.reset(new State(*other._state));
    for (auto& [k, v] : other._timeperiods) {
      _timeperiods.emplace(k, std::make_unique<Timeperiod>(*v));
    }
    for (auto& [k, v] : other._commands) {
      _commands.emplace(k, std::make_unique<Command>(*v));
    }
    for (auto& [k, v] : other._connectors) {
      _connectors.emplace(k, std::make_unique<Connector>(*v));
    }
    for (auto& [k, v] : other._severities) {
      _severities.emplace(k, std::make_unique<Severity>(*v));
    }
    for (auto& [k, v] : other._tags) {
      _tags.emplace(k, std::make_unique<Tag>(*v));
    }
    for (auto& [k, v] : other._contacts) {
      _contacts.emplace(k, std::make_unique<Contact>(*v));
    }
    for (auto& [k, v] : other._contactgroups) {
      _contactgroups.emplace(k, std::make_unique<Contactgroup>(*v));
    }
    for (auto& [k, v] : other._hosts) {
      _hosts.emplace(k, std::make_unique<Host>(*v));
    }
    for (auto& [k, v] : other._services) {
      _services.emplace(k, std::make_unique<Service>(*v));
    }
    for (auto& [k, v] : other._anomalydetections) {
      _anomalydetections.emplace(k, std::make_unique<Anomalydetection>(*v));
    }
    for (auto& [k, v] : other._servicegroups) {
      _servicegroups.emplace(k, std::make_unique<Servicegroup>(*v));
    }
    for (auto& [k, v] : other._hostdependencies) {
      _hostdependencies.emplace(k, std::make_unique<Hostdependency>(*v));
    }
    for (auto& [k, v] : other._servicedependencies) {
      _servicedependencies.emplace(k, std::make_unique<Servicedependency>(*v));
    }
    for (auto& [k, v] : other._hostescalations) {
      _hostescalations.emplace(k, std::make_unique<Hostescalation>(*v));
    }
    for (auto& [k, v] : other._serviceescalations) {
      _serviceescalations.emplace(k, std::make_unique<Serviceescalation>(*v));
    }
  }
}

/**
 * @brief Set the State object to this. The State containers are emptied
 * and their items are stored directly in the indexed_state.
 *
 * @param state
 */
void indexed_state::set_state(std::unique_ptr<State>&& state) {
  _state = std::move(state);
  _index();
}

/**
 * @brief Reset the indexed_state. The contained State has its containers
 * filled with the items stored in the indexed_state. And the contained State
 * is returned. If there is no State object, nullptr is returned.
 *
 * @return State* The State object contained in the indexed_state.
 */
State* indexed_state::release() {
  State* retval;
  if (_state) {
    _apply_containers();
    retval = _state.release();
  } else {
    retval = nullptr;
  }
  return retval;
}

/**
 * @brief Reset the indexed_state. The contained State is deleted.
 */
void indexed_state::reset() {
  if (_state)
    _state.reset();
  _clear_containers();
}

/**
 * @brief Move indexed objects to the appropriate containers.
 *
 * @param state If we have to save the state, we have to move the Indexed
 * objects to the appropriate containers. This state is the owner of these
 * containers.
 */
void indexed_state::_apply_containers() {
  for (auto& [_, timeperiod] : _timeperiods) {
    _state->mutable_timeperiods()->AddAllocated(timeperiod.release());
  }
  for (auto& [_, command] : _commands) {
    _state->mutable_commands()->AddAllocated(command.release());
  }
  for (auto& [_, connector] : _connectors) {
    _state->mutable_connectors()->AddAllocated(connector.release());
  }
  for (auto& [_, severity] : _severities) {
    _state->mutable_severities()->AddAllocated(severity.release());
  }
  for (auto& [_, tag] : _tags) {
    _state->mutable_tags()->AddAllocated(tag.release());
  }
  for (auto& [_, contact] : _contacts) {
    _state->mutable_contacts()->AddAllocated(contact.release());
  }
  for (auto& [_, contactgroup] : _contactgroups) {
    _state->mutable_contactgroups()->AddAllocated(contactgroup.release());
  }
  for (auto& [_, host] : _hosts) {
    _state->mutable_hosts()->AddAllocated(host.release());
  }
  for (auto& [_, hostgroup] : _hostgroups) {
    _state->mutable_hostgroups()->AddAllocated(hostgroup.release());
  }
  for (auto& [_, service] : _services) {
    _state->mutable_services()->AddAllocated(service.release());
  }
  for (auto& [_, anomalydetection] : _anomalydetections) {
    _state->mutable_anomalydetections()->AddAllocated(
        anomalydetection.release());
  }
  for (auto& [_, servicegroup] : _servicegroups) {
    _state->mutable_servicegroups()->AddAllocated(servicegroup.release());
  }
  for (auto& [_, hostdependency] : _hostdependencies) {
    _state->mutable_hostdependencies()->AddAllocated(hostdependency.release());
  }
  for (auto& [_, servicedependency] : _servicedependencies) {
    _state->mutable_servicedependencies()->AddAllocated(
        servicedependency.release());
  }
  for (auto& [_, hostescalation] : _hostescalations) {
    _state->mutable_hostescalations()->AddAllocated(hostescalation.release());
  }
  for (auto& [_, serviceescalation] : _serviceescalations) {
    _state->mutable_serviceescalations()->AddAllocated(
        serviceescalation.release());
  }
}

void indexed_state::_clear_containers() {
  _timeperiods.clear();
  _commands.clear();
  _connectors.clear();
  _severities.clear();
  _tags.clear();
  _contacts.clear();
  _contactgroups.clear();
  _hosts.clear();
  _hostgroups.clear();
  _services.clear();
  _anomalydetections.clear();
  _servicegroups.clear();
  _hostdependencies.clear();
  _servicedependencies.clear();
  _hostescalations.clear();
  _serviceescalations.clear();
}

/**
 * @brief Index the items stored in the containers of the State object.
 */
void indexed_state::_index() {
  _clear_containers();
  while (!_state->timeperiods().empty()) {
    Timeperiod* timeperiod = _state->mutable_timeperiods()->ReleaseLast();
    _timeperiods.emplace(timeperiod->timeperiod_name(),
                         std::unique_ptr<Timeperiod>(timeperiod));
  }
  while (!_state->commands().empty()) {
    Command* command = _state->mutable_commands()->ReleaseLast();
    _commands.emplace(command->command_name(),
                      std::unique_ptr<Command>(command));
  }
  while (!_state->connectors().empty()) {
    Connector* connector = _state->mutable_connectors()->ReleaseLast();
    _connectors.emplace(connector->connector_name(),
                        std::unique_ptr<Connector>(connector));
  }
  while (!_state->severities().empty()) {
    Severity* severity = _state->mutable_severities()->ReleaseLast();
    _severities.emplace(
        std::make_pair(severity->key().id(), severity->key().type()),
        std::unique_ptr<Severity>(severity));
  }
  while (!_state->tags().empty()) {
    Tag* tag = _state->mutable_tags()->ReleaseLast();
    _tags.emplace(std::make_pair(tag->key().id(), tag->key().type()),
                  std::unique_ptr<Tag>(tag));
  }
  while (!_state->contacts().empty()) {
    Contact* contact = _state->mutable_contacts()->ReleaseLast();
    _contacts.emplace(contact->contact_name(),
                      std::unique_ptr<Contact>(contact));
  }
  while (!_state->contactgroups().empty()) {
    Contactgroup* contactgroup = _state->mutable_contactgroups()->ReleaseLast();
    _contactgroups.emplace(contactgroup->contactgroup_name(),
                           std::unique_ptr<Contactgroup>(contactgroup));
  }
  while (!_state->hosts().empty()) {
    Host* host = _state->mutable_hosts()->ReleaseLast();
    _hosts.emplace(host->host_id(), std::unique_ptr<Host>(host));
  }
  while (!_state->hostgroups().empty()) {
    Hostgroup* hostgroup = _state->mutable_hostgroups()->ReleaseLast();
    _hostgroups.emplace(hostgroup->hostgroup_name(),
                        std::unique_ptr<Hostgroup>(hostgroup));
  }
  while (!_state->services().empty()) {
    Service* service = _state->mutable_services()->ReleaseLast();
    assert(service->host_id() > 0 && service->service_id() > 0);
    _services.emplace(std::make_pair(service->host_id(), service->service_id()),
                      std::unique_ptr<Service>(service));
  }
  while (!_state->anomalydetections().empty()) {
    Anomalydetection* anomalydetection =
        _state->mutable_anomalydetections()->ReleaseLast();
    assert(anomalydetection->host_id() > 0 &&
           anomalydetection->service_id() > 0);
    _anomalydetections.emplace(
        std::make_pair(anomalydetection->host_id(),
                       anomalydetection->service_id()),
        std::unique_ptr<Anomalydetection>(anomalydetection));
  }
  while (!_state->servicegroups().empty()) {
    Servicegroup* servicegroup = _state->mutable_servicegroups()->ReleaseLast();
    _servicegroups.emplace(servicegroup->servicegroup_name(),
                           std::unique_ptr<Servicegroup>(servicegroup));
  }
  while (!_state->hostdependencies().empty()) {
    Hostdependency* hostdependency =
        _state->mutable_hostdependencies()->ReleaseLast();
    _hostdependencies.emplace(hostdependency_key(*hostdependency),
                              std::unique_ptr<Hostdependency>(hostdependency));
  }
  while (!_state->servicedependencies().empty()) {
    Servicedependency* servicedependency =
        _state->mutable_servicedependencies()->ReleaseLast();
    _servicedependencies.emplace(
        servicedependency_key(*servicedependency),
        std::unique_ptr<Servicedependency>(servicedependency));
  }
  while (!_state->hostescalations().empty()) {
    Hostescalation* hostescalation =
        _state->mutable_hostescalations()->ReleaseLast();
    _hostescalations.emplace(hostescalation_key(*hostescalation),
                             std::unique_ptr<Hostescalation>(hostescalation));
  }
  while (!_state->serviceescalations().empty()) {
    Serviceescalation* serviceescalation =
        _state->mutable_serviceescalations()->ReleaseLast();
    _serviceescalations.emplace(
        serviceescalation_key(*serviceescalation),
        std::unique_ptr<Serviceescalation>(serviceescalation));
  }
}

void indexed_state::diff_with_new_config(
    State& new_state,
    const std::shared_ptr<spdlog::logger>& logger,
    configuration::DiffState* result) {
  /* ***** Build the diff ***** */

  /* Diff on timeperiods */
  _diff<Timeperiod, DiffTimeperiod, std::string>(
      new_state.mutable_timeperiods(), _timeperiods, logger,
      [](Timeperiod* obj) { return obj->timeperiod_name(); },
      result->mutable_timeperiods());

  /* Diff on commands */
  _diff<Command, DiffCommand, std::string>(
      new_state.mutable_commands(), _commands, logger,
      [](Command* obj) { return obj->command_name(); },
      result->mutable_commands());

  /* Diff on connectors */
  _diff<Connector, DiffConnector, std::string>(
      new_state.mutable_connectors(), _connectors, logger,
      [](Connector* obj) { return obj->connector_name(); },
      result->mutable_connectors());

  /* Diff on severities */
  _diff<Severity, DiffSeverity, std::pair<uint64_t, uint32_t>, KeyType>(
      new_state.mutable_severities(), _severities, logger,
      [](Severity* obj) {
        return std::make_pair(obj->key().id(), obj->key().type());
      },
      [](KeyType* key_type, const std::pair<uint64_t, uint32_t>& key) {
        key_type->set_id(key.first);
        key_type->set_type(key.second);
      },
      result->mutable_severities());

  /* Diff on tags */
  _diff<Tag, DiffTag, std::pair<uint64_t, uint32_t>, KeyType>(
      new_state.mutable_tags(), _tags, logger,
      [](Tag* obj) {
        return std::make_pair(obj->key().id(), obj->key().type());
      },
      [](KeyType* key_type, const std::pair<uint64_t, uint32_t>& key) {
        key_type->set_id(key.first);
        key_type->set_type(key.second);
      },
      result->mutable_tags());

  /* Diff on contacts */
  _diff<Contact, DiffContact, std::string>(
      new_state.mutable_contacts(), _contacts, logger,
      [](Contact* obj) { return obj->contact_name(); },
      result->mutable_contacts());

  /* Diff on contactgroups */
  _diff<Contactgroup, DiffContactgroup, std::string>(
      new_state.mutable_contactgroups(), _contactgroups, logger,
      [](Contactgroup* obj) { return obj->contactgroup_name(); },
      result->mutable_contactgroups());

  /* Diff on hosts */
  _diff<Host, DiffHost, uint64_t>(
      new_state.mutable_hosts(), _hosts, logger,
      [](Host* obj) { return obj->host_id(); }, result->mutable_hosts());

  /* Diff on hostgroups */
  _diff<Hostgroup, DiffHostgroup, std::string>(
      new_state.mutable_hostgroups(), _hostgroups, logger,
      [](Hostgroup* obj) { return obj->hostgroup_name(); },
      result->mutable_hostgroups());

  /* Diff on services */
  _diff<Service, DiffService, std::pair<uint64_t, uint64_t>, HostServiceId>(
      new_state.mutable_services(), _services, logger,
      [](Service* obj) {
        return std::make_pair(obj->host_id(), obj->service_id());
      },
      [](HostServiceId* key_type, const std::pair<uint64_t, uint64_t>& key) {
        key_type->set_host_id(key.first);
        key_type->set_service_id(key.second);
      },
      result->mutable_services());

  /* Diff on anomalydetections */
  _diff<Anomalydetection, DiffAnomalydetection, std::pair<uint64_t, uint64_t>,
        HostServiceId>(
      new_state.mutable_anomalydetections(), _anomalydetections, logger,
      [](Anomalydetection* obj) {
        return std::make_pair(obj->host_id(), obj->service_id());
      },
      [](HostServiceId* key_type, const std::pair<uint64_t, uint64_t>& key) {
        key_type->set_host_id(key.first);
        key_type->set_service_id(key.second);
      },
      result->mutable_anomalydetections());

  /* Diff on servicegroups */
  _diff<Servicegroup, DiffServicegroup, std::string>(
      new_state.mutable_servicegroups(), _servicegroups, logger,
      [](Servicegroup* obj) { return obj->servicegroup_name(); },
      result->mutable_servicegroups());

  /* Diff on hostdependencies */
  _diff<Hostdependency, DiffHostdependency, uint64_t>(
      new_state.mutable_hostdependencies(), _hostdependencies, logger,
      [](Hostdependency* obj) { return hostdependency_key(*obj); },
      result->mutable_hostdependencies());

  /* Diff on servicedependencies */
  _diff<Servicedependency, DiffServicedependency, uint64_t>(
      new_state.mutable_servicedependencies(), _servicedependencies, logger,
      [](Servicedependency* obj) { return servicedependency_key(*obj); },
      result->mutable_servicedependencies());

  /* Diff on hostescalations */
  _diff<Hostescalation, DiffHostescalation, uint64_t>(
      new_state.mutable_hostescalations(), _hostescalations, logger,
      [](Hostescalation* obj) { return hostescalation_key(*obj); },
      result->mutable_hostescalations());

  /* Diff on serviceescalations */
  _diff<Serviceescalation, DiffServiceescalation, uint64_t>(
      new_state.mutable_serviceescalations(), _serviceescalations, logger,
      [](Serviceescalation* obj) { return serviceescalation_key(*obj); },
      result->mutable_serviceescalations());

  /* Diff on state values */
#define SET_IF_CHANGED(field)               \
  if (_state->field() != new_state.field()) \
    result->set_##field(new_state.field());

#define SET_REPEATED_IF_CHANGED(field)                                   \
  if (!std::equal(_state->field().begin(), _state->field().end(),        \
                  new_state.field().begin(), new_state.field().end())) { \
    for (auto& item : new_state.field())                                 \
      result->add_##field(item);                                         \
  }

#define SET_STR_LST_IF_CHANGED(field)                                          \
  if (!std::equal(                                                             \
          _state->field().data().begin(), _state->field().data().end(),        \
          new_state.field().data().begin(), new_state.field().data().end())) { \
    for (auto& item : new_state.field().data())                                \
      result->mutable_##field()->add_data(item);                               \
  }

  SET_IF_CHANGED(cfg_main);
  for (auto& c : _state->cfg_file()) {
    logger->error("cfg_file: {}", c);
  }
  for (auto& c : new_state.cfg_file()) {
    logger->error("cfg_file: {}", c);
  }
  SET_REPEATED_IF_CHANGED(cfg_file);
  for (auto& c : result->cfg_file()) {
    logger->error("cfg_file: {}", c);
  }

  SET_REPEATED_IF_CHANGED(resource_file);
  SET_IF_CHANGED(instance_heartbeat_interval);
  SET_IF_CHANGED(check_service_freshness);
  SET_IF_CHANGED(enable_flap_detection);
  SET_IF_CHANGED(rpc_listen_address);
  SET_IF_CHANGED(grpc_port);
  {
    absl::btree_map<std::string, std::string> old_users;
    absl::btree_map<std::string, std::string> new_users;
    for (auto& user : _state->users())
      old_users.emplace(user.first, user.second);
    for (auto& user : new_state.users())
      new_users.emplace(user.first, user.second);
    if (old_users != new_users) {
      for (auto& user : new_users)
        (*result->mutable_users())[user.first] = user.second;
    }
  }
  SET_REPEATED_IF_CHANGED(cfg_dir);
  SET_IF_CHANGED(state_retention_file);
  SET_REPEATED_IF_CHANGED(broker_module);
  SET_IF_CHANGED(broker_module_directory);
  SET_IF_CHANGED(enable_macros_filter);
  SET_STR_LST_IF_CHANGED(macros_filter);
  SET_IF_CHANGED(log_v2_enabled);
  SET_IF_CHANGED(log_legacy_enabled);
  SET_IF_CHANGED(use_syslog);
  SET_IF_CHANGED(log_v2_logger);
  SET_IF_CHANGED(log_file);
  SET_IF_CHANGED(debug_file);
  SET_IF_CHANGED(debug_level);
  SET_IF_CHANGED(debug_verbosity);
  SET_IF_CHANGED(max_debug_file_size);
  SET_IF_CHANGED(log_pid);
  SET_IF_CHANGED(log_file_line);
  SET_IF_CHANGED(log_flush_period);
  SET_IF_CHANGED(log_level_checks);
  SET_IF_CHANGED(log_level_commands);
  SET_IF_CHANGED(log_level_comments);
  SET_IF_CHANGED(log_level_config);
  SET_IF_CHANGED(log_level_downtimes);
  SET_IF_CHANGED(log_level_eventbroker);
  SET_IF_CHANGED(log_level_events);
  SET_IF_CHANGED(log_level_external_command);
  SET_IF_CHANGED(log_level_functions);
  SET_IF_CHANGED(log_level_macros);
  SET_IF_CHANGED(log_level_notifications);
  SET_IF_CHANGED(log_level_process);
  SET_IF_CHANGED(log_level_runtime);
  SET_IF_CHANGED(log_level_otl);
  SET_IF_CHANGED(global_host_event_handler);
  SET_IF_CHANGED(global_service_event_handler);
  SET_IF_CHANGED(illegal_object_chars);
  SET_IF_CHANGED(illegal_output_chars);
  SET_IF_CHANGED(interval_length);
  SET_IF_CHANGED(ochp_command);
  SET_IF_CHANGED(ocsp_command);
  SET_IF_CHANGED(use_timezone);
  SET_IF_CHANGED(accept_passive_host_checks);
  SET_IF_CHANGED(accept_passive_service_checks);
  SET_IF_CHANGED(additional_freshness_latency);
  SET_IF_CHANGED(cached_host_check_horizon);
  SET_IF_CHANGED(check_external_commands);
  SET_IF_CHANGED(check_host_freshness);
  SET_IF_CHANGED(check_reaper_interval);
  SET_IF_CHANGED(enable_event_handlers);
  SET_IF_CHANGED(enable_notifications);
  SET_IF_CHANGED(execute_host_checks);
  SET_IF_CHANGED(execute_service_checks);
  SET_IF_CHANGED(max_host_check_spread);
  SET_IF_CHANGED(max_service_check_spread);
  SET_IF_CHANGED(notification_timeout);
  SET_IF_CHANGED(obsess_over_hosts);
  SET_IF_CHANGED(obsess_over_services);
  SET_IF_CHANGED(process_performance_data);
  SET_IF_CHANGED(soft_state_dependencies);
  SET_IF_CHANGED(use_large_installation_tweaks);
  SET_IF_CHANGED(admin_email);
  SET_IF_CHANGED(admin_pager);
  SET_IF_CHANGED(allow_empty_hostgroup_assignment);
  SET_IF_CHANGED(command_file);
  SET_IF_CHANGED(status_file);
  SET_IF_CHANGED(poller_name);
  SET_IF_CHANGED(poller_id);
  SET_IF_CHANGED(cached_service_check_horizon);
  SET_IF_CHANGED(check_orphaned_hosts);
  SET_IF_CHANGED(check_orphaned_services);
  SET_IF_CHANGED(command_check_interval);
  SET_IF_CHANGED(command_check_interval_is_seconds);
  SET_IF_CHANGED(enable_environment_macros);
  SET_IF_CHANGED(event_broker_options);
  SET_IF_CHANGED(event_handler_timeout);
  SET_IF_CHANGED(external_command_buffer_slots);
  SET_IF_CHANGED(high_host_flap_threshold);
  SET_IF_CHANGED(high_service_flap_threshold);
  SET_IF_CHANGED(host_check_timeout);
  SET_IF_CHANGED(host_freshness_check_interval);
  SET_IF_CHANGED(service_freshness_check_interval);
  SET_IF_CHANGED(log_event_handlers);
  SET_IF_CHANGED(log_external_commands);
  SET_IF_CHANGED(log_notifications);
  SET_IF_CHANGED(log_passive_checks);
  SET_IF_CHANGED(log_host_retries);
  SET_IF_CHANGED(log_service_retries);
  SET_IF_CHANGED(max_log_file_size);
  SET_IF_CHANGED(low_host_flap_threshold);
  SET_IF_CHANGED(low_service_flap_threshold);
  SET_IF_CHANGED(max_parallel_service_checks);
  SET_IF_CHANGED(ochp_timeout);
  SET_IF_CHANGED(ocsp_timeout);
  SET_IF_CHANGED(perfdata_timeout);
  SET_IF_CHANGED(retained_host_attribute_mask);
  SET_IF_CHANGED(retained_process_host_attribute_mask);
  SET_IF_CHANGED(retained_contact_host_attribute_mask);
  SET_IF_CHANGED(retained_contact_service_attribute_mask);
  SET_IF_CHANGED(retain_state_information);
  SET_IF_CHANGED(retention_scheduling_horizon);
  SET_IF_CHANGED(retention_update_interval);
  SET_IF_CHANGED(service_check_timeout);
  SET_IF_CHANGED(sleep_time);
  SET_IF_CHANGED(status_update_interval);
  SET_IF_CHANGED(time_change_threshold);
  SET_IF_CHANGED(use_regexp_matches);
  SET_IF_CHANGED(use_retained_program_state);
  SET_IF_CHANGED(use_retained_scheduling_info);
  SET_IF_CHANGED(use_setpgid);
  SET_IF_CHANGED(use_true_regexp_matching);
  SET_IF_CHANGED(date_format);
  if (!MessageDifferencer::Equals(_state->host_inter_check_delay_method(),
                                  new_state.host_inter_check_delay_method())) {
    *result->mutable_host_inter_check_delay_method() =
        new_state.host_inter_check_delay_method();
  }
  if (!MessageDifferencer::Equals(
          _state->service_inter_check_delay_method(),
          new_state.service_inter_check_delay_method())) {
    *result->mutable_service_inter_check_delay_method() =
        new_state.service_inter_check_delay_method();
  }
  if (!MessageDifferencer::Equals(
          _state->service_interleave_factor_method(),
          new_state.service_interleave_factor_method())) {
    *result->mutable_service_interleave_factor_method() =
        new_state.service_interleave_factor_method();
  }
  SET_IF_CHANGED(enable_predictive_host_dependency_checks);
  SET_IF_CHANGED(enable_predictive_service_dependency_checks);
  SET_IF_CHANGED(send_recovery_notifications_anyways);
  SET_IF_CHANGED(host_down_disable_service_checks);
  SET_IF_CHANGED(max_file_descriptors);
  SET_IF_CHANGED(config_version);
}

void indexed_state::serialize_to_ostream(std::ostream* os) {
  std::unique_ptr<State> state(release());
  if (state) {
    state->SerializeToOstream(os);
    set_state(std::move(state));
  }
}

}  // namespace com::centreon::engine::configuration
