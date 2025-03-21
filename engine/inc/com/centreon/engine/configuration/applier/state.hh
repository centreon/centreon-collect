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
  void apply(configuration::State& new_cfg,
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
  std::unordered_map<std::string, std::string>& user_macros();
  std::unordered_map<std::string, std::string>::const_iterator user_macros_find(
      std::string const& key) const;
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
  void _apply(const configuration::State& new_cfg, error_cnt& err);
  template <typename ConfigurationType, typename Key, typename ApplierType>
  void _apply(const pb_difference<ConfigurationType, Key>& diff,
              error_cnt& err);
  void _apply_ng(const DiffSeverity& diff, error_cnt& err);
  void _apply(configuration::State& new_cfg,
              retention::state& state,
              error_cnt& err);
  template <typename ConfigurationType, typename ApplierType>
  void _expand(configuration::State& new_state, error_cnt& err);
  void _processing(configuration::State& new_cfg,
                   error_cnt& err,
                   retention::state* state = nullptr);
  void _processing_diff(configuration::DiffState& diff_conf,
                        error_cnt& err,
                        retention::state* state = nullptr);
  template <typename ConfigurationType, typename ApplierType>
  void _resolve(
      const ::google::protobuf::RepeatedPtrField<ConfigurationType>& cfg,
      error_cnt& err);

  std::mutex _apply_lock;
  processing_state _processing_state;

  servicedependency_mmap _servicedependencies;
  std::unordered_map<std::string, std::string> _user_macros;
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_STATE_HH
