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

using namespace com::centreon::engine::configuration;

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
    for (auto& [k, v] : other._severities) {
      _severities.emplace(k, std::make_unique<Severity>(*v));
    }
    for (auto& [k, v] : other._hosts) {
      _hosts.emplace(k, std::make_unique<Host>(*v));
    }
    for (auto& [k, v] : other._services) {
      _services.emplace(k, std::make_unique<Service>(*v));
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
  for (auto& [_, severity] : _severities) {
    _state->mutable_severities()->AddAllocated(severity.release());
  }

  for (auto& [_, host] : _hosts) {
    _state->mutable_hosts()->AddAllocated(host.release());
  }

  for (auto& [_, service] : _services) {
    _state->mutable_services()->AddAllocated(service.release());
  }
}

void indexed_state::_clear_containers() {
  _severities.clear();
  _hosts.clear();
  _services.clear();
}

/**
 * @brief Index the items stored in the containers of the State object.
 */
void indexed_state::_index() {
  _severities.clear();
  while (!_state->severities().empty()) {
    Severity* severity = _state->mutable_severities()->ReleaseLast();
    _severities.emplace(
        std::make_pair(severity->key().id(), severity->key().type()),
        std::unique_ptr<Severity>(severity));
  }
  _hosts.clear();
  while (!_state->hosts().empty()) {
    Host* host = _state->mutable_hosts()->ReleaseLast();
    _hosts.emplace(host->host_id(), std::unique_ptr<Host>(host));
  }
  _services.clear();
  while (!_state->services().empty()) {
    Service* service = _state->mutable_services()->ReleaseLast();
    assert(service->host_id() > 0 && service->service_id() > 0);
    _services.emplace(std::make_pair(service->host_id(), service->service_id()),
                      std::unique_ptr<Service>(service));
  }
}
