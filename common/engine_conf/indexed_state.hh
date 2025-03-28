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
  std::unique_ptr<State> _state;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Host>> _hosts;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, std::unique_ptr<Service>>
      _services;
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Severity>>
      _severities;
  void _index();
  void _apply_containers();
  void _clear_containers();

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
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, std::unique_ptr<Severity>>&
  severities() {
    return _severities;
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
};
}  // namespace com::centreon::engine::configuration
#endif /* !CCE_CONFIGURATION_INDEXED_STATE */
