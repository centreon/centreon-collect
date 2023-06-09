/*
** Copyright 2023 Centreon
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

#ifndef CCB_STATS_HH
#define CCB_STATS_HH

#include <boost/circular_buffer.hpp>
#include "com/centreon/broker/config/applier/state.hh"

CCB_BEGIN()

namespace database {

/**
 * @class stats stats.hh
 * "com/centreon/broker/database/stats.hh"
 * @brief Class doing stats on mysql connections.
 *
 * Statistics made by this class are relatively easy:
 * * queries duration average.
 * * statements duration average.
 * * duration of connection loop.
 * * activity in connection loop (working time / total time).
 *
 * To aquieve this, we use three subclasses.
 * * query_span: this class is specialized to make measures on queries. Once
 *   it is instanciated, it stores a start_time, the realized query and its
 *   length in characters. At its destruction, it stores an end_time and stacks
 *   these data into a little struct stat_query.
 * * stmt_span: this class looks like query_span but stores the statement id,
 *   the content of the query, the number of lines (important for bulk
 *   statements). At its destruction, it stacks data into a little struct
 *   stat_statement.
 * * loop_span: it looks like the two previous one. When it is intanciated,
 *   it stores a start time, but the span is considered as inactive. There is
 *   a method start_activity() to set loop_span in active state. At its
 *   destruction, it stacks collected data into a little struct loop.
 */
class stats {
 public:
  class query_span {
    stats* const _parent;
    const std::chrono::system_clock::time_point _start_time;
    const uint32_t _query_len;
    std::string _query;

   public:
    query_span(stats* const parent, const std::string& query)
        : _parent{parent},
          _start_time(std::chrono::system_clock::now()),
          _query_len(query.size()) {
      _query = query.size() > 50
                   ? fmt::format("{}...", fmt::string_view(query.data(), 50))
                   : query;
    }
    ~query_span() noexcept {
      uint32_t top_q = config::applier::state::instance()
                           .stats_conf()
                           .sql_slowest_queries_count;
      auto end_time = std::chrono::system_clock::now();
      float s = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - _start_time)
                    .count() /
                1000.0f;
      if (s > 0) {
        stat_query sq{
            .query = std::move(_query),
            .start_time = std::chrono::system_clock::to_time_t(_start_time),
            .duration = s,
            .length = _query_len,
        };
        auto it = std::lower_bound(
            _parent->_stat_query.begin(), _parent->_stat_query.end(), sq,
            [](const stat_query& a, const stat_query& b) {
              return a.duration > b.duration;
            });
        if (_parent->_stat_query.size() < top_q)
          _parent->_stat_query.insert(it, std::move(sq));
        else if (it != _parent->_stat_query.end())
          *it = std::move(sq);
        _parent->_query_duration.push_back(s);
      }
    }
  };

  class stmt_span {
    stats* const _parent;
    const std::chrono::system_clock::time_point _start_time;
    const uint32_t _statement_id;
    std::string _query;
    uint32_t _rows_count = 1;

   public:
    stmt_span(stats* const parent,
              const uint32_t stmt_id,
              const std::string& query)
        : _parent{parent},
          _start_time(std::chrono::system_clock::now()),
          _statement_id{stmt_id},
          _query{query.size() > 50
                     ? fmt::format("{}...", fmt::string_view(query.data(), 50))
                     : query} {}
    ~stmt_span() noexcept {
      uint32_t top_s = config::applier::state::instance()
                           .stats_conf()
                           .sql_slowest_statements_count;
      auto end_time = std::chrono::system_clock::now();
      float s = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - _start_time)
                    .count() /
                1000.0f;
      if (s > 0) {
        stat_statement ss{
            .statement_query = std::move(_query),
            .statement_id = _statement_id,
            .start_time = std::chrono::system_clock::to_time_t(_start_time),
            .duration = s,
            .rows_count = _rows_count,
        };
        auto it = std::lower_bound(
            _parent->_stat_stmt.begin(), _parent->_stat_stmt.end(), ss,
            [](const stat_statement& a, const stat_statement& b) {
              return a.duration > b.duration;
            });
        if (_parent->_stat_stmt.size() < top_s)
          _parent->_stat_stmt.insert(it, std::move(ss));
        else if (it != _parent->_stat_stmt.end())
          *it = std::move(ss);
        _parent->_stmt_duration.push_back(s);
      }
    }
    void set_rows_count(uint32_t rows_count) { _rows_count = rows_count; }
  };

  struct loop {
    float duration;
    float activity_percent;
  };

  class loop_span {
    stats* const _parent;
    const std::chrono::system_clock::time_point _start_time;
    bool _in_activity = false;
    std::chrono::system_clock::time_point _start_activity_time;

   public:
    loop_span(stats* const parent)
        : _parent{parent}, _start_time(std::chrono::system_clock::now()) {}
    ~loop_span() noexcept {
      auto end_time = std::chrono::system_clock::now();
      float total = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - _start_time)
                        .count();
      float activity =
          _in_activity ? std::chrono::duration_cast<std::chrono::milliseconds>(
                             end_time - _start_activity_time)
                             .count()
                       : 0.0f;
      if (total > 0) {
        float percent = activity / total * 100;
        _parent->_loop.push_back({
            .duration = total / 1000,
            .activity_percent = percent,
        });
      }
    }
    void start_activity() {
      _start_activity_time = std::chrono::system_clock::now();
      _in_activity = true;
    }
  };

 private:
  struct stat_query {
    std::string query;
    time_t start_time;
    float duration;
    size_t length;
  };

  struct stat_statement {
    std::string statement_query;
    uint32_t statement_id;
    time_t start_time;
    float duration;
    uint32_t rows_count;
  };

  /* Stats for queries */
  std::vector<stat_query> _stat_query;
  boost::circular_buffer<float> _query_duration;

  /* Stats for statements */
  boost::circular_buffer<float> _stmt_duration;
  std::vector<stat_statement> _stat_stmt;

  /* Stats for the connection loop */
  boost::circular_buffer<loop> _loop;

 public:
  stats() : _query_duration(20), _stmt_duration(20), _loop(20) {}
  const std::vector<stat_query>& get_stat_query() const;
  float average_query_duration() const;
  const std::vector<stat_statement>& get_stat_stmt() const;
  float average_stmt_duration() const;
  loop average_loop() const;
};

}  // namespace sql

CCB_END()

#endif /* !CCB_STATS_HH */
