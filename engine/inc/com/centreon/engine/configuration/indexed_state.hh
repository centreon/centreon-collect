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
#include "common/engine_conf/state_helper.hh"

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
  State _state;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Host>> _hosts;
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Severity>>
      _severities;

  void _apply_containers(State& state);

 public:
  indexed_state() = default;
  indexed_state(const indexed_state& other);
  const State& state() const { return _state; }
  State& mut_state() { return _state; }
  void index();
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Severity>>&
  severities() {
    return _severities;
  }
  void add_severity(std::unique_ptr<Severity> severity) {
    _severities.emplace(
        std::make_pair(severity->key().id(), severity->key().type()),
        std::move(severity));
  }
  Severity& severity(std::pair<uint64_t, uint32_t> key) {
    return *_severities[key];
  }
  void remove_severity(std::pair<uint64_t, uint32_t> key) {
    _severities.erase(key);
  }
  const absl::flat_hash_map<uint64_t, std::unique_ptr<Host>>& hosts() const {
    return _hosts;
  }
  absl::flat_hash_map<uint64_t, std::unique_ptr<Host>>& mut_hosts() {
    return _hosts;
  }
  State save();
  void restore(State& state);
};
}  // namespace com::centreon::engine::configuration
#endif /* !CCE_CONFIGURATION_INDEXED_STATE */
