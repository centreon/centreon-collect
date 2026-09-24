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
 * @brief store the information of an AgentHostInfo event
 *
 * @param info
 * @param now broker time
 * @return what has been done, see update_result
 */
host_metadata_store::update_result host_metadata_store::update(
    const AgentHostInfo& info,
    std::time_t now) {
  host_metadata received;
  received.poller_id = info.poller_id();
  received.os_type = info.os_type();
  received.os_name = info.os_name();
  received.os_version = info.os_version();
  received.arch = info.arch();
  received.machine_id = info.machine_id();
  received.ips.assign(info.ips().begin(), info.ips().end());
  received.observed_at = static_cast<std::time_t>(info.observed_at());
  received.received_at = now;

  absl::MutexLock l(_protect);
  auto found = _data.find(info.host_id());
  if (found == _data.end() || found->second.received_at + _ttl.count() < now) {
    _data.insert_or_assign(info.host_id(), std::move(received));
    return update_result::updated;
  }

  host_metadata& stored = found->second;
  if (received.observed_at < stored.observed_at)
    return update_result::ignored;

  update_result ret;
  if (received.machine_id != stored.machine_id)
    ret = update_result::identity_changed;
  else if (received.poller_id != stored.poller_id ||
           received.os_type != stored.os_type ||
           received.os_name != stored.os_name ||
           received.os_version != stored.os_version ||
           received.arch != stored.arch || received.ips != stored.ips)
    ret = update_result::updated;
  else
    ret = update_result::refreshed;

  stored = std::move(received);
  return ret;
}

std::optional<host_metadata> host_metadata_store::get(uint64_t host_id,
                                                      std::time_t now) {
  absl::MutexLock l(_protect);
  auto found = _data.find(host_id);
  if (found == _data.end())
    return std::nullopt;
  if (found->second.received_at + _ttl.count() < now) {
    _data.erase(found);
    return std::nullopt;
  }
  return found->second;
}

size_t host_metadata_store::size() const {
  absl::MutexLock l(_protect);
  return _data.size();
}
