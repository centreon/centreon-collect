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
#include "common/engine_conf/indexed_diff_state.hh"
#include "common/engine_conf/hostdependency_helper.hh"
#include "common/engine_conf/hostescalation_helper.hh"
#include "common/engine_conf/servicedependency_helper.hh"
#include "common/engine_conf/serviceescalation_helper.hh"
#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::configuration {

/**
 * @brief merge the content of the given diff state with this global diff
 * state.
 *
 * @param diff_state DiffState to merge with this global diff state. Once
 * merged, this diff state is almost empty.
 */
void indexed_diff_state::add_diff_state(configuration::DiffState& diff_state) {
  _add_diff_message<DiffTimeperiod, Timeperiod, std::string>(
      diff_state.mutable_timeperiods(), _added_timeperiods,
      _modified_timeperiods, _removed_timeperiods,
      [](Timeperiod* obj) { return obj->timeperiod_name(); });

  _add_diff_message<DiffCommand, Command, std::string>(
      diff_state.mutable_commands(), _added_commands, _modified_commands,
      _removed_commands, [](Command* obj) { return obj->command_name(); });

  _add_diff_message<DiffConnector, Connector, std::string>(
      diff_state.mutable_connectors(), _added_connectors, _modified_connectors,
      _removed_connectors,
      [](Connector* obj) { return obj->connector_name(); });

  _add_diff_message<DiffSeverity, Severity, std::pair<uint64_t, uint32_t>,
                    KeyType>(
      diff_state.mutable_severities(), _added_severities, _modified_severities,
      _removed_severities,
      [](Severity* obj) {
        return std::make_pair(obj->key().id(), obj->key().type());
      },
      [](const KeyType& proto_key) {
        return std::make_pair(proto_key.id(), proto_key.type());
      });

  _add_diff_message<DiffTag, Tag, std::pair<uint64_t, uint32_t>, KeyType>(
      diff_state.mutable_tags(), _added_tags, _modified_tags, _removed_tags,
      [](Tag* obj) {
        return std::make_pair(obj->key().id(), obj->key().type());
      },
      [](const KeyType& proto_key) {
        return std::make_pair(proto_key.id(), proto_key.type());
      });

  _add_diff_message<DiffContact, Contact, std::string>(
      diff_state.mutable_contacts(), _added_contacts, _modified_contacts,
      _removed_contacts, [](Contact* obj) { return obj->contact_name(); });

  _add_diff_message<DiffContactgroup, Contactgroup, std::string>(
      diff_state.mutable_contactgroups(), _added_contactgroups,
      _modified_contactgroups, _removed_contactgroups,
      [](Contactgroup* obj) { return obj->contactgroup_name(); });

  _add_diff_message<DiffHost, Host, uint64_t>(
      diff_state.mutable_hosts(), _added_hosts, _modified_hosts, _removed_hosts,
      [](Host* obj) { return obj->host_id(); });

  _add_diff_message<DiffHostgroup, Hostgroup, std::string>(
      diff_state.mutable_hostgroups(), _added_hostgroups, _modified_hostgroups,
      _removed_hostgroups,
      [](Hostgroup* obj) { return obj->hostgroup_name(); });

  _add_diff_message<DiffService, Service, std::pair<uint64_t, uint64_t>,
                    HostServiceId>(
      diff_state.mutable_services(), _added_services, _modified_services,
      _removed_services,
      [](Service* obj) {
        return std::make_pair(obj->host_id(), obj->service_id());
      },
      [](const HostServiceId& proto_key) {
        return std::make_pair(proto_key.host_id(), proto_key.service_id());
      });

  _add_diff_message<DiffAnomalydetection, Anomalydetection,
                    std::pair<uint64_t, uint64_t>, HostServiceId>(
      diff_state.mutable_anomalydetections(), _added_anomalydetections,
      _modified_anomalydetections, _removed_anomalydetections,
      [](Anomalydetection* obj) {
        return std::make_pair(obj->host_id(), obj->service_id());
      },
      [](const HostServiceId& proto_key) {
        return std::make_pair(proto_key.host_id(), proto_key.service_id());
      });

  _add_diff_message<DiffServicegroup, Servicegroup, std::string>(
      diff_state.mutable_servicegroups(), _added_servicegroups,
      _modified_servicegroups, _removed_servicegroups,
      [](Servicegroup* obj) { return obj->servicegroup_name(); });

  _add_diff_message<DiffHostdependency, Hostdependency, uint64_t>(
      diff_state.mutable_hostdependencies(), _added_hostdependencies,
      _modified_hostdependencies, _removed_hostdependencies,
      [](Hostdependency* obj) { return hostdependency_key(*obj); });

  _add_diff_message<DiffHostescalation, Hostescalation, uint64_t>(
      diff_state.mutable_hostescalations(), _added_hostescalations,
      _modified_hostescalations, _removed_hostescalations,
      [](Hostescalation* obj) { return hostescalation_key(*obj); });

  _add_diff_message<DiffServicedependency, Servicedependency, uint64_t>(
      diff_state.mutable_servicedependencies(), _added_servicedependencies,
      _modified_servicedependencies, _removed_servicedependencies,
      [](Servicedependency* obj) { return servicedependency_key(*obj); });

  _add_diff_message<DiffServiceescalation, Serviceescalation, uint64_t>(
      diff_state.mutable_serviceescalations(), _added_serviceescalations,
      _modified_serviceescalations, _removed_serviceescalations,
      [](Serviceescalation* obj) { return serviceescalation_key(*obj); });
}

void indexed_diff_state::reset() {
#define CLEAR(name)         \
  _added_##name.clear();    \
  _modified_##name.clear(); \
  _removed_##name.clear();

  CLEAR(timeperiods);
  CLEAR(commands);
  CLEAR(connectors);
  CLEAR(severities);
  CLEAR(tags);
  CLEAR(contacts);
  CLEAR(contactgroups);
  CLEAR(hostdependencies);
  CLEAR(hostescalations);
  CLEAR(hostgroups);
  CLEAR(hosts);
  CLEAR(services);
  CLEAR(anomalydetections);
  CLEAR(servicegroups);
  CLEAR(servicedependencies);
  CLEAR(serviceescalations);
#undef CLEAR
}

void indexed_diff_state::release_diff_state(DiffState& state) {
  state.mutable_timeperiods()->clear_added();
  for (auto& [k, v] : _added_timeperiods)
    state.mutable_timeperiods()->mutable_added()->AddAllocated(v.release());
  state.mutable_timeperiods()->clear_modified();
  for (auto& [k, v] : _modified_timeperiods)
    state.mutable_timeperiods()->mutable_modified()->AddAllocated(v.release());
  state.mutable_timeperiods()->clear_removed();
  for (const std::string& k : _removed_timeperiods)
    state.mutable_timeperiods()->add_removed(k);

  state.mutable_commands()->clear_added();
  for (auto& [k, v] : _added_commands)
    state.mutable_commands()->mutable_added()->AddAllocated(v.release());
  state.mutable_commands()->clear_modified();
  for (auto& [k, v] : _modified_commands)
    state.mutable_commands()->mutable_modified()->AddAllocated(v.release());
  state.mutable_commands()->clear_removed();
  for (const std::string& k : _removed_commands)
    state.mutable_commands()->add_removed(k);

  state.mutable_connectors()->clear_added();
  for (auto& [k, v] : _added_connectors)
    state.mutable_connectors()->mutable_added()->AddAllocated(v.release());
  state.mutable_connectors()->clear_modified();
  for (auto& [k, v] : _modified_connectors)
    state.mutable_connectors()->mutable_modified()->AddAllocated(v.release());
  state.mutable_connectors()->clear_removed();
  for (const std::string& k : _removed_connectors)
    state.mutable_connectors()->add_removed(k);

  state.mutable_severities()->clear_added();
  for (auto& [k, v] : _added_severities)
    state.mutable_severities()->mutable_added()->AddAllocated(v.release());
  state.mutable_severities()->clear_modified();
  for (auto& [k, v] : _modified_severities)
    state.mutable_severities()->mutable_modified()->AddAllocated(v.release());
  state.mutable_severities()->clear_removed();
  for (const auto& k : _removed_severities) {
    auto key = state.mutable_severities()->add_removed();
    key->set_id(k.first);
    key->set_type(k.second);
  }

  state.mutable_tags()->clear_added();
  for (auto& [k, v] : _added_tags)
    state.mutable_tags()->mutable_added()->AddAllocated(v.release());
  state.mutable_tags()->clear_modified();
  for (auto& [k, v] : _modified_tags)
    state.mutable_tags()->mutable_modified()->AddAllocated(v.release());
  state.mutable_tags()->clear_removed();
  for (const auto& k : _removed_tags) {
    auto key = state.mutable_tags()->add_removed();
    key->set_id(k.first);
    key->set_type(k.second);
  }

  state.mutable_contacts()->clear_added();
  for (auto& [k, v] : _added_contacts)
    state.mutable_contacts()->mutable_added()->AddAllocated(v.release());
  state.mutable_contacts()->clear_modified();
  for (auto& [k, v] : _modified_contacts)
    state.mutable_contacts()->mutable_modified()->AddAllocated(v.release());
  state.mutable_contacts()->clear_removed();
  for (const std::string& k : _removed_contacts)
    state.mutable_contacts()->add_removed(k);

  state.mutable_contactgroups()->clear_added();
  for (auto& [k, v] : _added_contactgroups)
    state.mutable_contactgroups()->mutable_added()->AddAllocated(v.release());
  state.mutable_contactgroups()->clear_modified();
  for (auto& [k, v] : _modified_contactgroups)
    state.mutable_contactgroups()->mutable_modified()->AddAllocated(
        v.release());
  state.mutable_contactgroups()->clear_removed();
  for (const std::string& k : _removed_contactgroups)
    state.mutable_contactgroups()->add_removed(k);

  state.mutable_hostdependencies()->clear_added();
  for (auto& [k, v] : _added_hostdependencies)
    state.mutable_hostdependencies()->mutable_added()->AddAllocated(
        v.release());
  state.mutable_hostdependencies()->clear_modified();
  for (auto& [k, v] : _modified_hostdependencies)
    state.mutable_hostdependencies()->mutable_modified()->AddAllocated(
        v.release());
  state.mutable_hostdependencies()->clear_removed();
  for (uint64_t k : _removed_hostdependencies)
    state.mutable_hostdependencies()->add_removed(k);

  state.mutable_hostescalations()->clear_added();
  for (auto& [k, v] : _added_hostescalations)
    state.mutable_hostescalations()->mutable_added()->AddAllocated(v.release());
  state.mutable_hostescalations()->clear_modified();
  for (auto& [k, v] : _modified_hostescalations)
    state.mutable_hostescalations()->mutable_modified()->AddAllocated(
        v.release());
  state.mutable_hostescalations()->clear_removed();
  for (uint64_t k : _removed_hostescalations)
    state.mutable_hostescalations()->add_removed(k);

  state.mutable_hostgroups()->clear_added();
  for (auto& [k, v] : _added_hostgroups)
    state.mutable_hostgroups()->mutable_added()->AddAllocated(v.release());
  state.mutable_hostgroups()->clear_modified();
  for (auto& [k, v] : _modified_hostgroups)
    state.mutable_hostgroups()->mutable_modified()->AddAllocated(v.release());
  state.mutable_hostgroups()->clear_removed();
  for (const std::string& k : _removed_hostgroups)
    state.mutable_hostgroups()->add_removed(k);

  state.mutable_hosts()->clear_added();
  for (auto& [k, v] : _added_hosts)
    state.mutable_hosts()->mutable_added()->AddAllocated(v.release());
  state.mutable_hosts()->clear_modified();
  for (auto& [k, v] : _modified_hosts)
    state.mutable_hosts()->mutable_modified()->AddAllocated(v.release());
  state.mutable_hosts()->clear_removed();
  for (uint64_t k : _removed_hosts)
    state.mutable_hosts()->add_removed(k);

  state.mutable_services()->clear_added();
  for (auto& [k, v] : _added_services)
    state.mutable_services()->mutable_added()->AddAllocated(v.release());
  state.mutable_services()->clear_modified();
  for (auto& [k, v] : _modified_services)
    state.mutable_services()->mutable_modified()->AddAllocated(v.release());
  state.mutable_services()->clear_removed();
  for (const auto& k : _removed_services) {
    auto key = state.mutable_services()->add_removed();
    key->set_host_id(k.first);
    key->set_service_id(k.second);
  }

  state.mutable_anomalydetections()->clear_added();
  for (auto& [k, v] : _added_anomalydetections)
    state.mutable_anomalydetections()->mutable_added()->AddAllocated(
        v.release());
  state.mutable_anomalydetections()->clear_modified();
  for (auto& [k, v] : _modified_anomalydetections)
    state.mutable_anomalydetections()->mutable_modified()->AddAllocated(
        v.release());
  state.mutable_anomalydetections()->clear_removed();
  for (const auto& k : _removed_anomalydetections) {
    auto key = state.mutable_anomalydetections()->add_removed();
    key->set_host_id(k.first);
    key->set_service_id(k.second);
  }

  state.mutable_servicegroups()->clear_added();
  for (auto& [k, v] : _added_servicegroups)
    state.mutable_servicegroups()->mutable_added()->AddAllocated(v.release());
  state.mutable_servicegroups()->clear_modified();
  for (auto& [k, v] : _modified_servicegroups)
    state.mutable_servicegroups()->mutable_modified()->AddAllocated(
        v.release());
  state.mutable_servicegroups()->clear_removed();
  for (const std::string& k : _removed_servicegroups)
    state.mutable_servicegroups()->add_removed(k);

  state.mutable_servicedependencies()->clear_added();
  for (auto& [k, v] : _added_servicedependencies)
    state.mutable_servicedependencies()->mutable_added()->AddAllocated(
        v.release());
  state.mutable_servicedependencies()->clear_modified();
  for (auto& [k, v] : _modified_servicedependencies)
    state.mutable_servicedependencies()->mutable_modified()->AddAllocated(
        v.release());
  state.mutable_servicedependencies()->clear_removed();
  for (uint64_t k : _removed_servicedependencies)
    state.mutable_servicedependencies()->add_removed(k);

  state.mutable_serviceescalations()->clear_added();
  for (auto& [k, v] : _added_serviceescalations)
    state.mutable_serviceescalations()->mutable_added()->AddAllocated(
        v.release());
  state.mutable_serviceescalations()->clear_modified();
  for (auto& [k, v] : _modified_serviceescalations)
    state.mutable_serviceescalations()->mutable_modified()->AddAllocated(
        v.release());
  state.mutable_serviceescalations()->clear_removed();
  for (uint64_t k : _removed_serviceescalations)
    state.mutable_serviceescalations()->add_removed(k);
  reset();
}
}  // namespace com::centreon::engine::configuration
