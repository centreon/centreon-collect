/**
 * Copyright 2026 Centreon
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

#include "com/centreon/broker/otlp/host_metadata_store.hh"

using namespace com::centreon::broker::otlp;

/**
 * @brief store the information of an AgentHostInfo event, it replaces the
 * whole record of its host
 *
 * @param info
 */
void host_metadata_store::set(const AgentHostInfo& info) {
  host_metadata received;
  received.os_type = info.os_type();
  received.os_name = info.os_name();
  received.os_version = info.os_version();
  received.arch = info.arch();
  received.machine_id = info.machine_id();
  received.ips.assign(info.ips().begin(), info.ips().end());

  absl::MutexLock l(_protect);
  _data.insert_or_assign(info.host_id(), std::move(received));
}

std::optional<host_metadata> host_metadata_store::get(uint64_t host_id) const {
  absl::MutexLock l(_protect);
  auto found = _data.find(host_id);
  if (found == _data.end())
    return std::nullopt;
  return found->second;
}

size_t host_metadata_store::size() const {
  absl::MutexLock l(_protect);
  return _data.size();
}
