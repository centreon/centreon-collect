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
#ifndef CCE_CONFIGURATION_APPLIER_STATE_HH
#define CCE_CONFIGURATION_APPLIER_STATE_HH

#include "com/centreon/engine/configuration/applier/difference.hh"
#include "com/centreon/engine/configuration/applier/pb_difference.hh"
#include "com/centreon/engine/servicedependency.hh"
#include "common/engine_conf/indexed_state.hh"

namespace com::centreon::engine {

// Forward declaration.
namespace commands {
class command;
class connector;
}  // namespace commands

namespace retention {
class state;
}

namespace configuration {
struct error_cnt;

namespace applier {
/**
 *  @class state state.hh
 *  @brief Simple configuration applier for state class.
 *
 *  Simple configuration applier for state class.
 */
class state {
 public:
  void apply(configuration::indexed_state& new_cfg,
             error_cnt& err,
             retention::state* state = nullptr);
  void apply_diff(configuration::DiffState& diff_conf,
                  error_cnt& err,
                  retention::state* state = nullptr);
  void apply_log_config(configuration::State& new_cfg);
  static state& instance();
  void clear();

  servicedependency_mmap const& servicedependencies() const throw();
  servicedependency_mmap& servicedependencies() throw();
  absl::flat_hash_map<std::string, std::string>& user_macros();
  absl::flat_hash_map<std::string, std::string>::const_iterator
  user_macros_find(std::string const& key) const;
  absl::flat_hash_map<std::string, std::string>::const_iterator
  user_macros_find(const std::string_view& key) const;
  void lock();
  void unlock();

 private:
  enum processing_state {
    state_waiting,
    state_apply,
    state_error,
    state_ready
  };

  state();
  state(state const&);
  ~state() noexcept;

#ifdef DEBUG_CONFIG
  void _check_serviceescalations() const;
  void _check_hostescalations() const;
  void _check_contacts() const;
  void _check_contactgroups() const;
  void _check_services() const;
  void _check_hosts() const;
#endif

  state& operator=(state const&);
  void _apply(const configuration::indexed_state& new_cfg, error_cnt& err);
  template <typename ConfigurationType, typename Key, typename ApplierType>
  void _apply(const pb_difference<ConfigurationType, Key>& diff,
              error_cnt& err);
  template <typename Applier,
            typename DiffType,
            typename KeyType,
            typename ObjType,
            typename ProtoKeyType>
  void _apply_ng(
      const DiffType& diff,
      absl::flat_hash_map<KeyType, std::unique_ptr<ObjType>>& current_list,
      std::function<KeyType(const ObjType&)>&& build_key,
      std::function<KeyType(const ProtoKeyType&)>&& convert_key) {
    Applier aplyr;

    // Modify objects.
    for (auto& m : diff.modified()) {
      KeyType key = build_key(m);
      auto* current_obj = current_list.at(key).get();
      aplyr.modify_object(current_obj, m);
    }

    // Erase objects.
    for (auto& key : diff.removed()) {
      aplyr.remove_object(convert_key(key));
    }

    // Add objects.
    for (auto& obj : diff.added()) {
      aplyr.add_object(obj);
    }
  }

  template <typename Applier,
            typename DiffType,
            typename KeyType,
            typename ObjType>
  void _apply_ng(
      const DiffType& diff,
      absl::flat_hash_map<KeyType, std::unique_ptr<ObjType>>& current_list,
      std::function<KeyType(const ObjType&)>&& build_key) {
    Applier aplyr;

    // Modify objects.
    for (auto& m : diff.modified()) {
      KeyType key = build_key(m);
      auto* current_obj = current_list.at(key).get();
      aplyr.modify_object(current_obj, m);
    }

    // Erase objects.
    for (auto& key : diff.removed()) {
      aplyr.remove_object(key);
    }

    // Add objects.
    for (auto& obj : diff.added()) {
      aplyr.add_object(obj);
    }
  }
  void _apply(configuration::indexed_state& new_cfg,
              retention::state& state,
              error_cnt& err);
  void _processing(configuration::indexed_state& new_cfg,
                   error_cnt& err,
                   retention::state* state = nullptr);
  void _processing_diff(configuration::DiffState& diff_conf,
                        error_cnt& err,
                        retention::state* state = nullptr);
  template <typename ConfigurationType, typename ApplierType>
  void _resolve(
      const ::google::protobuf::RepeatedPtrField<ConfigurationType>& cfg,
      error_cnt& err);
  template <typename ConfigurationType, typename KeyType, typename ApplierType>
  void _resolve(
      const absl::flat_hash_map<KeyType, std::unique_ptr<ConfigurationType>>&
          cfg,
      error_cnt& err);

  std::mutex _apply_lock;
  processing_state _processing_state;

  servicedependency_mmap _servicedependencies;
  absl::flat_hash_map<std::string, std::string> _user_macros;
};

}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_STATE_HH
