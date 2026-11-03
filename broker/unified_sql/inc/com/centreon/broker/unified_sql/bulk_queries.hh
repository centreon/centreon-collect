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
#ifndef CCB_UNIFIED_SQL_BULK_QUERIES_HH
#define CCB_UNIFIED_SQL_BULK_QUERIES_HH

namespace com::centreon::broker {
namespace unified_sql {
/**
 * @class bulk_queries "com/centreon/broker/unified_sql/bulk_queries.hh"
 * @brief Container for big queries.
 *
 * If for example, we have many custom
 * variables to insert into the customvariables table, it is the good class to
 * use. The idea is very simple, the bulk is constructed with an interval in
 * seconds, a max queries count and a template of the query to execute.
 * Regularly, we call the method ready(), if it returns true then the limit
 * time or the max number of rows are reached. And we can get the query to
 * execute with the get_query() method. Once this last method called, the stack
 * is empty and ready() will return true once one of the two conditions are
 * reached again.
 *
 * Queries are not done directly by this class because it has no idea of the
 * connection to use nor the client configuration, it is only a container.
 *
 * How it works:
 * @code
 * bulk_queries bq(10, 15, "INSERT INTO test (col_a, col_b) VALUES {}");
 * bq.push_query("\"foo\", \"bar\"");
 * bq.push_query("\"foo1\", \"bar1\"");
 * ...
 * if (bq.ready()) {
 *   std::string q = bq.get_query();
 *   mysql->run_query(q);
 * }
 * @endcode
 * The query content is
 * `INSERT INTO test (col_a, col_b) VALUES ("foo", "bar"), ("foo1", "bar1")`
 */
class bulk_queries {
  const uint32_t _interval;
  const uint32_t _max_size;
  const std::string _query;
  mutable std::mutex _queue_m;
  std::time_t _next_time;
  std::deque<std::string> _queue;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  bulk_queries(const uint32_t max_interval,
               const uint32_t max_queries,
               const std::string& query,
               const std::shared_ptr<spdlog::logger>& logger);
  std::string get_query();
  void push_query(const std::string& query);
  void push_query(std::string&& query);
  bool ready();
  void force_ready();
  size_t size() const;
  std::time_t next_time() const;
};
}  // namespace unified_sql
}  // namespace com::centreon::broker

#endif /* !CCB_UNIFIED_SQL_BULK_QUERIES_HH */
