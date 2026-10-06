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

#include <google/protobuf/util/message_differencer.h>

#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/persistent_cache.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;

/**
 * @brief Construct an empty store
 *
 * @param cache where records are saved, may be null
 */
host_metadata_store::host_metadata_store(
    std::shared_ptr<persistent_cache> cache)
    : _cache(std::move(cache)) {}

/**
 * @brief store the information of an AgentHostInfo event, it replaces the
 * whole record of its host. The poller, host name and reception time are not
 * kept: they don't describe the machine, so the same information re-sent by
 * engine or received from another poller is not a change.
 *
 * @param info
 * @return true if the record is new or changed
 */
bool host_metadata_store::set(const AgentHostInfo& info) {
  AgentHostInfo received(info);
  received.clear_poller_id();
  received.clear_host_name();
  received.clear_observed_at();

  absl::MutexLock l(_protect);
  auto found = _data.find(info.host_id());
  if (found != _data.end() &&
      google::protobuf::util::MessageDifferencer::Equals(found->second,
                                                         received))
    return false;
  _data.insert_or_assign(info.host_id(), std::move(received));
  _modified = true;
  return true;
}

std::optional<AgentHostInfo> host_metadata_store::get(uint64_t host_id) const {
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

/**
 * @brief store the records saved in the cache, other events are ignored. The
 * loaded records are not considered as changes.
 *
 * @return number of records read
 */
size_t host_metadata_store::load() {
  if (!_cache)
    return 0;
  size_t count = 0;
  std::shared_ptr<io::data> d;
  for (_cache->get(d); d; _cache->get(d)) {
    if (d->type() == neb::pb_agent_host_info::static_type()) {
      set(std::static_pointer_cast<neb::pb_agent_host_info>(d)->obj());
      ++count;
    }
  }
  absl::MutexLock l(_protect);
  _modified = false;
  return count;
}

/**
 * @brief replace the content of the cache by one AgentHostInfo event per
 * host, if a record changed since the last save and that save is at least
 * min_interval old. The interval bounds the saves during a burst of changes,
 * as every save writes all the records.
 *
 * @param min_interval
 * @return true if the cache was written
 */
bool host_metadata_store::save(std::chrono::seconds min_interval) {
  const auto now = std::chrono::steady_clock::now();
  absl::MutexLock l(_protect);
  if (!_cache || !_modified || now < _last_save + min_interval)
    return false;
  /* a failing cache is retried after min_interval, not on each call */
  _last_save = now;
  _cache->transaction();
  for (const auto& host : _data)
    _cache->add(std::make_shared<neb::pb_agent_host_info>(host.second));
  _cache->commit();
  _modified = false;
  return true;
}
