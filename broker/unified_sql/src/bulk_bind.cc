/**
 * Copyright 2022-2025 Centreon
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

#include "com/centreon/broker/unified_sql/bulk_bind.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::unified_sql;

using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 * @brief Constructor
 *
 * @param max_interval Interval in seconds to wait before sending a new query.
 * @param max_rows Max rows to insert/update before sending a new query.
 * @param stmt The stmt that is executed with binds modified in bulk_bind.
 * @param logger The logger to use.
 */
bulk_bind::bulk_bind(const uint32_t max_interval,
                     const uint32_t max_rows,
                     database::mysql_bulk_stmt& stmt,
                     const std::shared_ptr<spdlog::logger>& logger)
    : _interval{max_interval},
      _max_size{max_rows},
      _stmt(stmt),
      _next_time{0},
      _logger{logger} {}

/**
 * @brief Return true when the time limit or the row counter are reached with
 * the condition that there is something to write, the queue must not be empty.
 *
 * @return a boolean true if ready.
 */
bool bulk_bind::ready() {
  absl::MutexLock lck(&_queue_m);
  auto* b = _bind.get();
  if (!b)
    return false;

  if (b->rows_count() >= _max_size) {
    _logger->trace("The bind rows count {} reaches its max size {}",
                   b->rows_count(), _max_size);
    return true;
  }

  std::time_t now = time(nullptr);
  if (_next_time <= now) {
    _logger->trace(
        "The bind next time {} has been reached by the current time {}",
        _next_time, now);
    if (b->current_row() == 0) {
      _logger->trace("the rows count of the binding is 0 so nothing to do");
      _next_time = std::time(nullptr) + _interval;
      _logger->trace(" => bind not ready");
      return false;
    }
    _logger->trace(" => bind ready");
    return true;
  }
  _logger->trace(" => bind not ready");
  return false;
}

/**
 * @brief Force the bind to be ready in term of time. If there is nothing to
 * write, the bind won't be ready.
 */
void bulk_bind::force_ready() {
  absl::MutexLock lck(&_queue_m);
  _next_time = 0;
}

/**
 * @brief Size of the queue
 *
 * @return a size.
 */
size_t bulk_bind::size() const {
  absl::MutexLock lck(&_queue_m);
  if (!_bind)
    return 0;
  return _bind->rows_count();
}

/**
 * @brief The bind is applied to the stmt used to construct the bulk_bind. Then
 * it is possible to execute it.
 */
void bulk_bind::apply_to_stmt() {
  absl::MutexLock lck(&_queue_m);
  _stmt.set_bind(std::move(_bind));
  _next_time = std::time(nullptr) + _interval;
}

/**
 * @brief Initialize the bind from the associated statement. This function call
 * must be protected.
 */
void bulk_bind::init_from_stmt() {
  _bind = _stmt.create_bind();
}

/**
 * @brief accessor to the bind. This function call must be protected.
 *
 * @return An unique_ptr to a mysql_bulk_bind.
 */
std::unique_ptr<database::mysql_bulk_bind>& bulk_bind::bind() {
  return _bind;
}

void bulk_bind::lock() {
  _queue_m.Lock();
}

void bulk_bind::unlock() {
  _queue_m.Unlock();
}
