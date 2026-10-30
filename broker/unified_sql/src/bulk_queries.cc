/**
 * Copyright 2022-2024 Centreon
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

#include "com/centreon/broker/unified_sql/bulk_queries.hh"

#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker::unified_sql;

using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 * @brief Constructor
 *
 * @param max_interval Interval in seconds to wait before sending a new query.
 * @param max_queries Max rows to insert/update before sending a new query.
 * @param query The template of the query to execute, a string with {} telling
 * where all the contained rows will be inserted.
 */
bulk_queries::bulk_queries(const uint32_t max_interval,
                           const uint32_t max_queries,
                           const std::string& query,
                           const std::shared_ptr<spdlog::logger>& logger)
    : _interval{max_interval},
      _max_size{max_queries},
      _query(query),
      _next_time{std::time(nullptr) + max_interval},
      _logger{logger} {}

/**
 * @brief Compute the query to execute as a string and return it. The container
 * is then initialized.
 *
 * @return a string.
 */
std::string bulk_queries::get_query() {
  std::deque<std::string> queue;
  {
    std::lock_guard<std::mutex> lck(_queue_m);
    std::swap(queue, _queue);
  }

  std::string query;
  if (!queue.empty()) {
    /* Building the query */
    _logger->debug("SQL: {} items sent in bulk", queue.size());
    query = fmt::format(_query, fmt::join(queue, ","));
    _logger->trace("Sending query << {} >>", query);
  }
  _next_time = std::time(nullptr) + _interval;
  return query;
}

/**
 * @brief Add a new row represented by a string that will be part of the query.
 *
 * @param query A string.
 */
void bulk_queries::push_query(const std::string& query) {
  std::lock_guard<std::mutex> lck(_queue_m);
  _queue.push_back(query);
}

/**
 * @brief Add a new row represented by a string that will be part of the query.
 *
 * @param query A string.
 */
void bulk_queries::push_query(std::string&& query) {
  std::lock_guard<std::mutex> lck(_queue_m);
  _queue.push_back(std::move(query));
}

/**
 * @brief Return true when the time limit or the row counter are reached with
 * the condition that there is something to write, the queue must not be empty.
 *
 * @return a boolean
 */
bool bulk_queries::ready() {
  std::lock_guard<std::mutex> lck(_queue_m);
  if (_queue.size() >= _max_size)
    return true;

  std::time_t now = time(nullptr);
  if (_next_time <= now) {
    if (_queue.empty()) {
      _next_time = std::time(nullptr) + _interval;
      return false;
    }
    return true;
  }
  return false;
}

/**
 * @brief Force the bind to be ready in term of time. If there is nothing to
 * write, it won't be ready.
 */
void bulk_queries::force_ready() {
  std::lock_guard<std::mutex> lck(_queue_m);
  _next_time = 0;
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

std::time_t bulk_queries::next_time() const {
  std::lock_guard<std::mutex> lck(_queue_m);
  return _next_time;
}
