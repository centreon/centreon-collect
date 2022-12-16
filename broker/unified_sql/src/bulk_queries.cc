/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/unified_sql/bulk_queries.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::unified_sql;

bulk_queries::bulk_queries(const uint32_t max_interval,
                           const uint32_t max_queries,
                           const std::string& query)
    : _interval{max_interval}, _max_size{max_queries}, _query(query) {}

std::string bulk_queries::get_query() {
  std::deque<std::string> queue;
  {
    std::lock_guard<std::mutex> lck(_queue_m);
    std::swap(queue, _queue);
  }

  std::string query;
  if (!queue.empty()) {
    /* Building the query */
    log_v2::sql()->debug("SQL: {} customvariables sent in bulk", queue.size());
    query = fmt::format(_query, fmt::join(queue, ","));
    log_v2::sql()->trace("sending query << {} >>", query);
  }
  _next_queries += _interval;
  return query;
}

void bulk_queries::push_query(const std::string& query) {
  std::lock_guard<std::mutex> lck(_queue_m);
  _queue.push_back(query);
}

void bulk_queries::push_query(std::string&& query) {
  std::lock_guard<std::mutex> lck(_queue_m);
  _queue.push_back(std::move(query));
}

bool bulk_queries::ready() const {
  std::lock_guard<std::mutex> lck(_queue_m);
  if (_queue.size() >= _max_size)
    return true;

  std::time_t now = time(nullptr);
  if (_next_queries <= now)
    return true;
  return false;
}

/**
 * @brief Size of the queue
 *
 * @return a size.
 */
size_t bulk_queries::size() const {
  std::lock_guard<std::mutex> lck(_queue_m);
  return _queue.size();
}
