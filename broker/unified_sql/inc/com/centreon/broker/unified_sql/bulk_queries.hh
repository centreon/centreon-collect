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
#ifndef CCB_UNIFIED_SQL_BULK_QUERIES_HH
#define CCB_UNIFIED_SQL_BULK_QUERIES_HH

#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()
namespace unified_sql {
/**
 * @class bulk_queries bulk_queries.hh
 * "com/centreon/broker/unified_sql/bulk_queries.hh"
 * @brief Container for queries. They are stacked in a queue. When the limit
 * size or the limit time is reached, the method ready() becomes true and it
 * is time to execute the execute_queries() method.
 */
class bulk_queries {
  const uint32_t _interval;
  const uint32_t _max_size;
  const std::string _query;
  mutable std::mutex _queue_m;
  std::time_t _next_queries;
  uint32_t _max_queries = 0u;
  std::deque<std::string> _queue;

 public:
  bulk_queries(const uint32_t max_interval,
               const uint32_t max_queries,
               const std::string& query);
  std::string get_query();
  void push_query(const std::string& query);
  void push_query(std::string&& query);
  bool ready() const;
  size_t size() const;
};
}  // namespace unified_sql
CCB_END()

#endif /* !CCB_UNIFIED_SQL_BULK_QUERIES_HH */
