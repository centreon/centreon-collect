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
#include "com/centreon/engine/configuration/indexed_state.hh"

using namespace com::centreon::engine::configuration;

State indexed_state::save() {
  State save;
  save.CopyFrom(_state);
  _apply_containers(save);
  return save;
}

/**
 * @brief Move indexed objects to the appropriate containers.
 *
 * @param state If we have to save the state, we have to move the Indexed
 * objects to the appropriate containers. This state is the owner of these
 * containers.
 */
void indexed_state::_apply_containers(State& state) {
  for (auto& [key, severity] : _severities) {
    state.mutable_severities()->AddAllocated(severity.release());
  }
}

/**
 * @brief Index the objects in the state.
 */
void indexed_state::index() {
  _severities.clear();
  while (!_state.severities().empty()) {
    Severity* severity = _state.mutable_severities()->ReleaseLast();
    _severities.emplace(
        std::make_pair(severity->key().id(), severity->key().type()),
        std::unique_ptr<Severity>(severity));
  }
}

/**
 * @brief Set the given State into the indexed state and index its objects.
 *
 * @param state The state to set.
 */
void indexed_state::restore(State& state) {
  _state.CopyFrom(state);
  index();
}
