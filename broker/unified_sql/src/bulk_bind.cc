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

#include "com/centreon/broker/unified_sql/bulk_bind.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::unified_sql;

/**
 * @brief Constructor
 *
 * @param connections_count Number of connections to the DB used for this bind.
 * @param max_interval Interval in seconds to wait before sending a new query.
 * @param max_rows Max rows to insert/update before sending a new query.
 * @param stmt The stmt that is executed with binds modified in bulk_bind.
 */
bulk_bind::bulk_bind(const size_t connections_count,
                     const uint32_t max_interval,
                     const uint32_t max_rows,
                     database::mysql_stmt& stmt)
    : _interval{max_interval},
      _max_size{max_rows},
      _stmt(stmt),
      _bind(connections_count),
      _next_time(connections_count) {}

/**
 * @brief Return true when the time limit or the row counter are reached with
 * the condition that there is something to write, the queue must not be empty.
 *
 * @param conn The connection to check (there is one bind per connection).
 * @return a boolean true if ready.
 */
bool bulk_bind::ready(int32_t conn) {
  std::lock_guard<std::mutex> lck(_queue_m);
  auto* b = _bind[conn].get();
  if (!b)
    return false;

  if (b->row_count() >= _max_size)
    return true;

  std::time_t now = time(nullptr);
  if (_next_time[conn] <= now) {
    if (b->current_row() == 0) {
      _next_time[conn] = std::time(nullptr) + _interval;
      return false;
    }
    return true;
  }
  return false;
}

/**
 * @brief Size of the queue
 *
 * @param conn The connection to determine the bind whose size we want.
 * @return a size.
 */
size_t bulk_bind::size(int32_t conn) const {
  std::lock_guard<std::mutex> lck(_queue_m);
  if (!_bind[conn])
    return 0;
  return _bind[conn]->row_count();
}

/**
 * @brief The next time when we will be able to try to execute the statement
 *
 * @return a timestamp.
 */
std::time_t bulk_bind::next_time() const {
  std::lock_guard<std::mutex> lck(_queue_m);
  auto it = std::min_element(_next_time.begin(), _next_time.end());
  return *it;
}

/**
 * @brief The bind given by conn is applied to the stmt used to construct the
 * bulk_bind. Then it is possible to execute it.
 *
 * @param conn The connection to choose the bind.
 */
void bulk_bind::apply_to_stmt(int32_t conn) {
  std::lock_guard<std::mutex> lck(_queue_m);
  _stmt.set_bind(std::move(_bind[conn]));
  _next_time[conn] = std::time(nullptr) + _interval;
}

void bulk_bind::init_from_stmt(int32_t conn) {
  std::lock_guard<std::mutex> lck(_queue_m);
  _bind[conn] = _stmt.create_bind();
}

/**
 * @brief Number of connections defined in the bulk_bind.
 *
 * @return A size_t.
 */
std::size_t bulk_bind::connections_count() const {
  return _bind.size();
}

std::unique_ptr<database::mysql_bind>& bulk_bind::bind(int32_t conn) {
  return _bind[conn];
}
