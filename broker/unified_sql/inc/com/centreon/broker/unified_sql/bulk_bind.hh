/**
 * Copyright 2022-2023 Centreon
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
#ifndef CCB_UNIFIED_SQL_BULK_BIND_HH
#define CCB_UNIFIED_SQL_BULK_BIND_HH

#include "com/centreon/broker/sql/mysql_bulk_stmt.hh"

namespace com::centreon::broker::unified_sql {
/**
 * @class bulk_bind "com/centreon/broker/unified_sql/bulk_bind.hh"
 * @brief Container used for a multiline statement bind. It is threadsafe and
 * contains object to monitor its size and time since its construction.
 *
 * If for example, we have many prepared statements to update many rows in
 * the services table. We can add many rows in this container, each row
 * corresponding to one update.
 *
 * Regularly, we call the method ready(), if it returns true then the limit
 * time or the max number of rows are reached. And we can get the bind to
 * execute with the get_bind() method. Once this last method called, the stack
 * is empty and ready() will return true once one of the two conditions are
 * reached again.
 *
 * Statements are not executed directly by this class because it has no idea of
 * the connection to use nor the client configuration, it is only a container.
 *
 * How it works:
 * @code
 * bulk_bind bs(10, 15, 10000, stmt);
 * bs.bind()->set_value_as_str(0, "foo");  // first ? in statement
 * bs.bind()->set_value_as_u32(1, 12);  // second ? in statement
 * bs.bind()->next_row();               // Let's go to the next row
 * bs.bind()->set_value_as_str(0, "bar");  // first ? in statement
 * bs.bind()->set_value_as_u32(1, 13);  // second ? in statement
 * ...
 * // Is it time to execute the statement on connection 0?
 * if (bs.ready(0)) {
 *   bs.apply_to_stmt(0);
 *   mysql->execute_statement(stmt);
 * }
 * @endcode
 */
class bulk_bind {
  const uint32_t _interval;
  const uint32_t _max_size;
  database::mysql_bulk_stmt& _stmt;
  mutable absl::Mutex _queue_m;
  std::vector<std::unique_ptr<database::mysql_bulk_bind>> _bind
      ABSL_GUARDED_BY(_queue_m);
  std::vector<std::time_t> _next_time ABSL_GUARDED_BY(_queue_m);
  std::shared_ptr<spdlog::logger> _logger;

 public:
  bulk_bind(const size_t connections_count,
            const uint32_t max_interval,
            const uint32_t max_rows,
            database::mysql_bulk_stmt& stmt,
            const std::shared_ptr<spdlog::logger>& logger);
  bulk_bind(const bulk_bind&) = delete;
  std::unique_ptr<database::mysql_bulk_bind>& bind(int32_t conn)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_queue_m);
  void apply_to_stmt(int32_t conn) ABSL_LOCKS_EXCLUDED(_queue_m);
  bool ready(int32_t conn) ABSL_LOCKS_EXCLUDED(_queue_m);
  std::size_t size(int32_t conn = -1) const ABSL_LOCKS_EXCLUDED(_queue_m);
  std::time_t next_time() const ABSL_LOCKS_EXCLUDED(_queue_m);
  std::size_t connections_count() const ABSL_LOCKS_EXCLUDED(_queue_m);
  void init_from_stmt(int32_t conn) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_queue_m);
  void lock() ABSL_EXCLUSIVE_LOCK_FUNCTION(_queue_m);
  void unlock() ABSL_UNLOCK_FUNCTION(_queue_m);
};
}  // namespace com::centreon::broker::unified_sql

#endif /* !CCB_UNIFIED_SQL_BULK_BIND_HH */
