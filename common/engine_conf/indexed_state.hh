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
#ifndef CCE_CONFIGURATION_INDEXED_STATE
#define CCE_CONFIGURATION_INDEXED_STATE
#include <spdlog/spdlog.h>
#include "common/engine_conf/state_helper.hh"

using google::protobuf::util::MessageDifferencer;

namespace com::centreon::engine::configuration {
/**
 * @brief This class is used to index the configuration state. Many objects
 * in the state are stored in vectors and so not indexed. This class is used
 * to index these objects. So it is easier to access to any service or host.
 *
 * Indexed objects are moved to the appropriate containers. The state has its
 * containers set to empty.
 */
class indexed_state {
  std::unique_ptr<State> _state;
  absl::flat_hash_map<std::string, std::unique_ptr<Timeperiod>> _timeperiods;
  absl::flat_hash_map<std::string, std::unique_ptr<Command>> _commands;
  absl::flat_hash_map<std::string, std::unique_ptr<Connector>> _connectors;
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Severity>>
      _severities;
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Tag>>
      _tags;
  absl::flat_hash_map<std::string, std::unique_ptr<Contact>> _contacts;
  absl::flat_hash_map<std::string, std::unique_ptr<Contactgroup>>
      _contactgroups;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Host>> _hosts;
  absl::flat_hash_map<std::string, std::unique_ptr<Hostgroup>> _hostgroups;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, std::unique_ptr<Service>>
      _services;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      std::unique_ptr<Anomalydetection>>
      _anomalydetections;
  absl::flat_hash_map<std::string, std::unique_ptr<Servicegroup>>
      _servicegroups;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Hostdependency>>
      _hostdependencies;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Servicedependency>>
      _servicedependencies;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Hostescalation>>
      _hostescalations;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Serviceescalation>>
      _serviceescalations;
  void _index();
  void _apply_containers();
  void _clear_containers();

  template <typename ObjType, typename DiffType, typename SimpleKeyType>
  void _diff(::google::protobuf::RepeatedPtrField<ObjType>* new_obj,
             const absl::flat_hash_map<SimpleKeyType, std::unique_ptr<ObjType>>&
                 old_obj,
             const std::shared_ptr<spdlog::logger>& logger,
             std::function<SimpleKeyType(ObjType*)>&& key_builder,
             DiffType* result) {
    logger->trace("Diff on {}", typeid(ObjType).name());
    absl::flat_hash_set<SimpleKeyType> old_keys;
    for (const auto& [k, _] : old_obj)
      old_keys.insert(k);

    while (!new_obj->empty()) {
      ObjType* obj = new_obj->ReleaseLast();
      auto key = key_builder(obj);
      auto found = old_obj.find(key);
      if (found == old_obj.end()) {
        result->mutable_added()->AddAllocated(obj);
      } else {
        old_keys.erase(key);
        if (!MessageDifferencer::Equals(*obj, *found->second))
          result->mutable_modified()->AddAllocated(obj);
        else
          delete obj;
      }
    }
    for (auto& key : old_keys)
      result->add_removed(key);
  }

  template <typename ObjType,
            typename DiffType,
            typename KeyType,
            typename ProtoKeyType>
  void _diff(
      ::google::protobuf::RepeatedPtrField<ObjType>* new_obj,
      const absl::flat_hash_map<KeyType, std::unique_ptr<ObjType>>& old_obj,
      const std::shared_ptr<spdlog::logger>& logger,
      std::function<KeyType(ObjType*)>&& key_builder,
      std::function<void(ProtoKeyType*, const KeyType&)>&& proto_key_setter,
      DiffType* result) {
    /* Diff on services */
    logger->trace("Diff on {}", typeid(ObjType).name());
    absl::flat_hash_set<KeyType> old_keys;
    for (const auto& [k, _] : old_obj)
      old_keys.insert(k);

    while (!new_obj->empty()) {
      ObjType* obj = new_obj->ReleaseLast();
      auto key = key_builder(obj);
      auto found = old_obj.find(key);
      if (found == old_obj.end()) {
        result->mutable_added()->AddAllocated(obj);
      } else {
        old_keys.erase(key);
        if (!MessageDifferencer::Equals(*obj, *found->second))
          result->mutable_modified()->AddAllocated(obj);
        else
          delete obj;
      }
    }
    for (auto& key : old_keys) {
      auto* key_type = result->add_removed();
      proto_key_setter(key_type, key);
    }
  }

 public:
  indexed_state() = default;
  indexed_state(std::unique_ptr<State>&& state);
  indexed_state(const indexed_state& other);
  ~indexed_state() noexcept = default;
  void set_state(std::unique_ptr<State>&& state);
  void reset();
  State* release();
  const State& state() const { return *_state; }
  State& mut_state() { return *_state; }
  const absl::flat_hash_map<std::string, std::unique_ptr<Timeperiod>>&
  timeperiods() const {
    return _timeperiods;
  }
  absl::flat_hash_map<std::string, std::unique_ptr<Timeperiod>>&
  mut_timeperiods() {
    return _timeperiods;
  }
  const absl::flat_hash_map<std::string, std::unique_ptr<Connector>>&
  connectors() const {
    return _connectors;
  }
  absl::flat_hash_map<std::string, std::unique_ptr<Connector>>&
  mut_connectors() {
    return _connectors;
  }
  const absl::flat_hash_map<std::string, std::unique_ptr<Command>>& commands()
      const {
    return _commands;
  }
  absl::flat_hash_map<std::string, std::unique_ptr<Command>>& mut_commands() {
    return _commands;
  }
  const absl::flat_hash_map<std::pair<uint64_t, uint32_t>,
                            std::unique_ptr<Severity>>&
  severities() const {
    return _severities;
  }
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Severity>>&
  mut_severities() {
    return _severities;
  }
  const absl::flat_hash_map<std::pair<uint64_t, uint32_t>,
                            std::unique_ptr<Tag>>&
  tags() const {
    return _tags;
  }
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Tag>>&
  mut_tags() {
    return _tags;
  }
  const absl::flat_hash_map<uint64_t, std::unique_ptr<Host>>& hosts() const {
    return _hosts;
  }
  absl::flat_hash_map<uint64_t, std::unique_ptr<Host>>& mut_hosts() {
    return _hosts;
  }
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, std::unique_ptr<Service>>&
  mut_services() {
    return _services;
  }
  const absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                            std::unique_ptr<Service>>&
  services() const {
    return _services;
  }
  void diff_with_new_config(State& new_state,
                            const std::shared_ptr<spdlog::logger>& logger,
                            DiffState* result);
};
// template <>
// void indexed_state::_diff<Service, DiffService, std::pair<uint64_t,
// uint64_t>, HostServiceId>(
//     ::google::protobuf::RepeatedPtrField<Service>* new_obj,
//     const absl::flat_hash_map<KeyType, std::unique_ptr<Service>>& old_obj,
//     const std::shared_ptr<spdlog::logger>& logger,
//     std::function<KeyType(Service*)>&& key_builder,
//     std::function<void(HostServiceId*, const std::pair<uint64_t,
//     uint64_t>&)>&&
//         key_setter,
//     DiffService* result);
}  // namespace com::centreon::engine::configuration
#endif /* !CCE_CONFIGURATION_INDEXED_STATE */
