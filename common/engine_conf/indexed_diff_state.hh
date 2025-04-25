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
#ifndef CCE_CONFIGURATION_INDEXED_DIFF_STATE
#define CCE_CONFIGURATION_INDEXED_DIFF_STATE
#include <type_traits>
#include "common/engine_conf/state_helper.hh"

namespace com::centreon::engine::configuration {
/**
 * @brief This class indexes DiffStates messages. But it is also able to
 * merge several DiffStates into one.
 *
 * The goal is if we have X new DiffStates for X different pollers, Broker
 * can have a single diff for all of them thanks to this class.
 *
 * The indexed side is to improve performance during lookups.
 */
class indexed_diff_state {
  absl::flat_hash_map<std::string, std::unique_ptr<Timeperiod>>
      _added_timeperiods;
  absl::flat_hash_map<std::string, std::unique_ptr<Timeperiod>>
      _modified_timeperiods;
  absl::flat_hash_set<std::string> _removed_timeperiods;

  absl::flat_hash_map<std::string, std::unique_ptr<Command>> _added_commands;
  absl::flat_hash_map<std::string, std::unique_ptr<Command>> _modified_commands;
  absl::flat_hash_set<std::string> _removed_commands;

  absl::flat_hash_map<std::string, std::unique_ptr<Connector>>
      _added_connectors;
  absl::flat_hash_map<std::string, std::unique_ptr<Connector>>
      _modified_connectors;
  absl::flat_hash_set<std::string> _removed_connectors;

  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Severity>>
      _added_severities;
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Severity>>
      _modified_severities;
  absl::flat_hash_set<std::pair<uint64_t, uint32_t>> _removed_severities;

  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Tag>>
      _added_tags;
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Tag>>
      _modified_tags;
  absl::flat_hash_set<std::pair<uint64_t, uint32_t>> _removed_tags;

  absl::flat_hash_map<std::string, std::unique_ptr<Contact>> _added_contacts;
  absl::flat_hash_map<std::string, std::unique_ptr<Contact>> _modified_contacts;
  absl::flat_hash_set<std::string> _removed_contacts;

  absl::flat_hash_map<std::string, std::unique_ptr<Contactgroup>>
      _added_contactgroups;
  absl::flat_hash_map<std::string, std::unique_ptr<Contactgroup>>
      _modified_contactgroups;
  absl::flat_hash_set<std::string> _removed_contactgroups;

  absl::flat_hash_map<uint64_t, std::unique_ptr<Hostdependency>>
      _added_hostdependencies;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Hostdependency>>
      _modified_hostdependencies;
  absl::flat_hash_set<uint64_t> _removed_hostdependencies;

  absl::flat_hash_map<uint64_t, std::unique_ptr<Hostescalation>>
      _added_hostescalations;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Hostescalation>>
      _modified_hostescalations;
  absl::flat_hash_set<uint64_t> _removed_hostescalations;

  absl::flat_hash_map<std::string, std::unique_ptr<Hostgroup>>
      _added_hostgroups;
  absl::flat_hash_map<std::string, std::unique_ptr<Hostgroup>>
      _modified_hostgroups;
  absl::flat_hash_set<std::string> _removed_hostgroups;

  absl::flat_hash_map<uint64_t, std::unique_ptr<Host>> _added_hosts;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Host>> _modified_hosts;
  absl::flat_hash_set<uint64_t> _removed_hosts;

  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, std::unique_ptr<Service>>
      _added_services;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, std::unique_ptr<Service>>
      _modified_services;
  absl::flat_hash_set<std::pair<uint64_t, uint64_t>> _removed_services;

  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      std::unique_ptr<Anomalydetection>>
      _added_anomalydetections;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      std::unique_ptr<Anomalydetection>>
      _modified_anomalydetections;
  absl::flat_hash_set<std::pair<uint64_t, uint64_t>> _removed_anomalydetections;

  absl::flat_hash_map<std::string, std::unique_ptr<Servicegroup>>
      _added_servicegroups;
  absl::flat_hash_map<std::string, std::unique_ptr<Servicegroup>>
      _modified_servicegroups;
  absl::flat_hash_set<std::string> _removed_servicegroups;

  absl::flat_hash_map<uint64_t, std::unique_ptr<Servicedependency>>
      _added_servicedependencies;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Servicedependency>>
      _modified_servicedependencies;
  absl::flat_hash_set<uint64_t> _removed_servicedependencies;

  absl::flat_hash_map<uint64_t, std::unique_ptr<Serviceescalation>>
      _added_serviceescalations;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Serviceescalation>>
      _modified_serviceescalations;
  absl::flat_hash_set<uint64_t> _removed_serviceescalations;

  template <typename DiffType, typename Type, typename Key>
  void _add_diff_message(
      DiffType* diff,
      absl::flat_hash_map<Key, std::unique_ptr<Type>>& added_map,
      absl::flat_hash_map<Key, std::unique_ptr<Type>>& modified_map,
      absl::flat_hash_set<Key>& removed_set,
      std::function<Key(Type*)>&& key_builder) {
    _add_diff_message<DiffType, Type, Key, Key>(
        diff, added_map, modified_map, removed_set, std::move(key_builder),
        nullptr);
  }

  template <typename DiffType, typename Type, typename Key, typename ProtoKey>
  void _add_diff_message(
      DiffType* diff,
      absl::flat_hash_map<Key, std::unique_ptr<Type>>& added_map,
      absl::flat_hash_map<Key, std::unique_ptr<Type>>& modified_map,
      absl::flat_hash_set<Key>& removed_set,
      std::function<Key(Type*)>&& key_builder,
      std::function<Key(const ProtoKey&)>&& convert_key) {
    while (diff->added_size() > 0) {
      auto obj = std::unique_ptr<Type>(diff->mutable_added()->ReleaseLast());
      auto found = removed_set.find(key_builder(obj.get()));
      if (found != removed_set.end()) {
        /* We have an added message that is also removed, so it is moved. */
        removed_set.erase(found);
        modified_map.emplace(key_builder(obj.get()), std::move(obj));
      } else {
        added_map.emplace(key_builder(obj.get()), std::move(obj));
      }
    }

    while (diff->modified_size() > 0) {
      auto obj = std::unique_ptr<Type>(diff->mutable_modified()->ReleaseLast());
      auto found = removed_set.find(key_builder(obj.get()));
      if (found != removed_set.end()) {
        /* We have an modified message that is also removed, so it is moved. */
        removed_set.erase(found);
      }
      modified_map.emplace(key_builder(obj.get()), std::move(obj));
    }

    for (const ProtoKey& proto_key : diff->removed()) {
      Key key;
      if (convert_key)
        key = convert_key(proto_key);
      else {
        if constexpr (std::is_same_v<Key, ProtoKey>) {
          key = proto_key;
        }
      }
      auto found1 = added_map.find(key);
      if (found1 != added_map.end()) {
        /* A poller wants to add this message but we want to remove it. So,
         * this message should be modified, not added... */
        modified_map.emplace(key, std::move(found1->second));
        added_map.erase(found1);
      } else {
        auto found2 = modified_map.find(key);
        if (found2 != modified_map.end()) {
          /* Some pollers already want to modify this message, so we can't
           * remove it. */
        } else {
          /* We can remove it, no body wants it. */
          removed_set.emplace(key);
        }
      }
    }
  }

  template <typename Type, typename Key>
  void _add_message(
      google::protobuf::RepeatedPtrField<Type>* container,
      absl::flat_hash_map<Key, std::unique_ptr<Type>>& added_map,
      absl::flat_hash_map<Key, std::unique_ptr<Type>>& modified_map,
      absl::flat_hash_set<Key>& removed_set,
      std::function<Key(Type*)>&& key_builder) {
    while (container->size() > 0) {
      auto obj = std::unique_ptr<Type>(container->ReleaseLast());
      auto found = removed_set.find(key_builder(obj.get()));
      if (found != removed_set.end()) {
        /* We have an added message that is also removed, so it is moved. */
        removed_set.erase(found);
        modified_map.emplace(key_builder(obj.get()), std::move(obj));
      } else {
        added_map.emplace(key_builder(obj.get()), std::move(obj));
      }
    }
  }

 public:
  void add_diff_state(DiffState& state);
  auto& added_timeperiods() const { return _added_timeperiods; }
  auto& modified_timeperiods() const { return _modified_timeperiods; }
  auto& removed_timeperiods() const { return _removed_timeperiods; }

  auto& added_commands() const { return _added_commands; }
  auto& modified_commands() const { return _modified_commands; }
  auto& removed_commands() const { return _removed_commands; }

  auto& added_connectors() const { return _added_connectors; }
  auto& modified_connectors() const { return _modified_connectors; }
  auto& removed_connectors() const { return _removed_connectors; }

  auto& added_severities() const { return _added_severities; }
  auto& modified_severities() const { return _modified_severities; }
  auto& removed_severities() const { return _removed_severities; }

  auto& added_tags() const { return _added_tags; }
  auto& modified_tags() const { return _modified_tags; }
  auto& removed_tags() const { return _removed_tags; }

  auto& added_contacts() const { return _added_contacts; }
  auto& modified_contacts() const { return _modified_contacts; }
  auto& removed_contacts() const { return _removed_contacts; }

  auto& added_contactgroups() const { return _added_contactgroups; }
  auto& modified_contactgroups() const { return _modified_contactgroups; }
  auto& removed_contactgroups() const { return _removed_contactgroups; }

  auto& added_hostdependencies() const { return _added_hostdependencies; }
  auto& modified_hostdependencies() const { return _modified_hostdependencies; }
  auto& removed_hostdependencies() const { return _removed_hostdependencies; }

  auto& added_hostescalations() const { return _added_hostescalations; }
  auto& modified_hostescalations() const { return _modified_hostescalations; }

  auto& removed_hostescalations() const { return _removed_hostescalations; }

  auto& added_hostgroups() const { return _added_hostgroups; }
  auto& modified_hostgroups() const { return _modified_hostgroups; }
  auto& removed_hostgroups() const { return _removed_hostgroups; }

  auto& added_hosts() const { return _added_hosts; }
  auto& modified_hosts() const { return _modified_hosts; }
  auto& removed_hosts() const { return _removed_hosts; }

  auto& added_services() const { return _added_services; }
  auto& modified_services() const { return _modified_services; }
  auto& removed_services() const { return _removed_services; }

  auto& added_anomalydetections() const { return _added_anomalydetections; }
  auto& modified_anomalydetections() const {
    return _modified_anomalydetections;
  }
  auto& removed_anomalydetections() const { return _removed_anomalydetections; }

  auto& added_servicegroups() const { return _added_servicegroups; }
  auto& modified_servicegroups() const { return _modified_servicegroups; }
  auto& removed_servicegroups() const { return _removed_servicegroups; }

  auto& added_servicedependencies() const { return _added_servicedependencies; }
  auto& modified_servicedependencies() const {
    return _modified_servicedependencies;
  }
  auto& removed_servicedependencies() const {
    return _removed_servicedependencies;
  }

  auto& added_serviceescalations() const { return _added_serviceescalations; }
  auto& modified_serviceescalations() const {
    return _modified_serviceescalations;
  }
  auto& removed_serviceescalations() const {
    return _removed_serviceescalations;
  }
  void release_diff_state(DiffState& state);
  void reset();
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_INDEXED_DIFF_STATE */
