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
      auto key = convert_key(proto_key);
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

 public:
  void add_diff_state(DiffState& state);
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_INDEXED_DIFF_STATE */
