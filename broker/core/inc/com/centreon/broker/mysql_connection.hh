/*
** Copyright 2018 - 2021 Centreon
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

#ifndef CCB_MYSQL_CONNECTION_HH
#define CCB_MYSQL_CONNECTION_HH

#include <boost/circular_buffer.hpp>

#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/database/mysql_bulk_stmt.hh"
#include "com/centreon/broker/database/mysql_error.hh"
#include "com/centreon/broker/database/mysql_result.hh"
#include "com/centreon/broker/database/mysql_stmt.hh"
#include "com/centreon/broker/database/mysql_task.hh"
#include "com/centreon/broker/database_config.hh"
#include "com/centreon/broker/stats/center.hh"

CCB_BEGIN()

using my_error = database::mysql_error;

/**
 *  @class mysql_connection mysql_connection.hh
 * "com/centreon/broker/mysql_connection.hh"
 *  @brief Class representing a thread connected to the mysql server
 *
 *  mysql_connection classes are instanciated by the mysql_manager and then
 *  shared with the mysql objects asking for them. The developer has not to
 *  deal directly with mysql_connection. The are private objects of the mysql
 *  object.
 *
 *  When a query is asked through the mysql object, the developer sets a
 *  connection number (an index of the connection indexed from 0) or -1. With
 *  -1, the mysql object chooses the connection that has the least tasks.
 *  Then it sends the query to the good connection.
 *
 *  Queries in a connection are done asynchronously. We know when we ask for
 *  them, we don't know when we will have them. If a query A is sent before
 *  a query B, then A will be sent to the database before B.
 *
 *  A connection works with a list. Each query is pushed back on it. An internal
 *  thread pops them one by one to send to the database.
 */
class mysql_connection {
 public:
  enum connection_state { not_started, running, finished };

 private:
  std::unique_ptr<std::thread> _thread;
  MYSQL* _conn;

  // Mutex and condition working on _tasks_list.
  mutable std::mutex _tasks_m;
  std::condition_variable _tasks_condition;
  std::atomic<bool> _finish_asked;
  std::list<std::unique_ptr<database::mysql_task>> _tasks_list;
  std::atomic_int _tasks_count;
  bool _need_commit;
  std::time_t _last_access;

  std::unordered_map<uint32_t, MYSQL_STMT*> _stmt;
  std::unordered_map<uint32_t, std::string> _stmt_query;

  // Mutex and condition working on start.
  std::mutex _start_m;
  std::condition_variable _start_condition;

  // Mutex to access the configuration
  mutable std::mutex _cfg_mutex;
  std::string _host;
  std::string _socket;
  std::string _user;
  std::string _pwd;
  std::string _name;
  int _port;
  std::atomic<connection_state> _state;

  /**
   * Stats variables:
   * * _connected tells if this is connected and working or not
   * * _switch_point holds the timestamp of the last time _connected switched.
   * * _last_stats holds the last timestamp stats have been updated, they are
   * not updated more than one time per second.
   */
  bool _connected;
  std::time_t _switch_point;
  size_t _stats_idx;
  std::time_t _last_stats;
  uint32_t _qps;

  class stats_loop_span {
    mysql_connection* const _parent;
    const std::chrono::system_clock::time_point _start_time;
    bool _in_activity = false;
    std::chrono::system_clock::time_point _start_activity_time;

   public:
    stats_loop_span(mysql_connection* parent)
        : _parent{parent}, _start_time(std::chrono::system_clock::now()) {}
    ~stats_loop_span() noexcept {
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
        _parent->_stat_loop.push_back({
            .duration = total,
            .activity_percent = percent,
        });
      }
    }
    void start_activity() {
      _start_activity_time = std::chrono::system_clock::now();
      _in_activity = true;
    }
  };

  class stats_stmt_span {
    mysql_connection* const _parent;
    const std::chrono::system_clock::time_point _start_time;
    const uint32_t _statement_id;
    std::string _query;
    uint32_t _rows_count = 1;

   public:
    stats_stmt_span(mysql_connection* parent, uint32_t stmt_id)
        : _parent{parent},
          _start_time(std::chrono::system_clock::now()),
          _statement_id{stmt_id} {
      const std::string& query = parent->_stmt_query[stmt_id];
      _query = query.size() > 50
                   ? fmt::format("{}...", fmt::string_view(query.data(), 50))
                   : query;
    }
    ~stats_stmt_span() noexcept {
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

  class stats_query_span {
    mysql_connection* const _parent;
    const std::chrono::system_clock::time_point _start_time;
    const uint32_t _query_len;
    std::string _query;

   public:
    stats_query_span(mysql_connection* parent, const std::string& query)
        : _parent{parent},
          _start_time(std::chrono::system_clock::now()),
          _query_len(query.size()) {
      _query = query.size() > 50
                   ? fmt::format("{}...", fmt::string_view(query.data(), 50))
                   : query;
    }
    ~stats_query_span() noexcept {
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

  struct stat_loop {
    float duration;
    float activity_percent;
  };

  /* Statistics */
  boost::circular_buffer<float> _query_duration;
  std::vector<stat_query> _stat_query;
  boost::circular_buffer<float> _stmt_duration;
  std::vector<stat_statement> _stat_stmt;
  boost::circular_buffer<stat_loop> _stat_loop;

  /* mutex to protect the string access in _error */
  mutable std::mutex _error_m;
  database::mysql_error _error;

  /**************************************************************************/
  /*                    Methods executed by this thread                     */
  /**************************************************************************/
  bool _server_error(int code) const;
  void _run();
  std::string _get_stack();
  void _query(database::mysql_task* t);
  void _query_res(database::mysql_task* t);
  void _query_int(database::mysql_task* t);
  void _commit(database::mysql_task* t);
  void _prepare(database::mysql_task* t);
  void _statement(database::mysql_task* t);
  void _statement_res(database::mysql_task* t);
  template <typename T>
  void _statement_int(database::mysql_task* t);
  void _fetch_row_sync(database::mysql_task* task);
  void _get_version(database::mysql_task* t);
  void _push(std::unique_ptr<database::mysql_task>&& q);
  void _debug(MYSQL_BIND* bind, uint32_t size);
  bool _try_to_reconnect();

  static void (mysql_connection::*const _task_processing_table[])(
      database::mysql_task* task);

  void _prepare_connection();
  void _clear_connection();
  void _update_stats() noexcept;

 public:
  /**************************************************************************/
  /*                  Methods executed by the main thread                   */
  /**************************************************************************/

  mysql_connection(database_config const& db_cfg, size_t stats_idx);
  ~mysql_connection();

  void prepare_query(int id, std::string const& query);
  void commit(
      const database::mysql_task_commit::mysql_task_commit_data::pointer&
          commit_data);
  void run_query(std::string const& query, my_error::code ec, bool fatal);
  void run_query_and_get_result(std::string const& query,
                                std::promise<database::mysql_result>&& promise);
  void run_query_and_get_int(std::string const& query,
                             std::promise<int>&& promise,
                             database::mysql_task::int_type type);
  void get_server_version(std::promise<const char*>&& promise);

  void run_statement(database::mysql_stmt_base& stmt,
                     my_error::code ec,
                     bool fatal);
  void run_statement_and_get_result(
      database::mysql_stmt& stmt,
      std::promise<database::mysql_result>&& promise,
      size_t length);

  template <typename T>
  void run_statement_and_get_int(database::mysql_stmt& stmt,
                                 std::promise<T>&& promise,
                                 database::mysql_task::int_type type) {
    _push(std::make_unique<database::mysql_task_statement_int<T>>(
        stmt, std::move(promise), type));
  }

  void finish();
  bool fetch_row(database::mysql_result& result);
  mysql_bind_mapping get_stmt_mapping(int stmt_id) const;
  int get_stmt_size() const;
  bool match_config(database_config const& db_cfg) const;
  int get_tasks_count() const;
  bool is_finish_asked() const;
  bool is_finished() const;
  bool ping();
  bool is_in_error() const;
  void clear_error();
  std::string get_error_message();

  /**
   * @brief Create an error on the connection. All error created as this, is a
   * fatal error that will throw an exception later.
   */
  template <typename... Args>
  void set_error_message(std::string const& fmt, const Args&... args) {
    std::lock_guard<std::mutex> lck(_error_m);
    if (!_error.is_active())
      _error.set_message(fmt, args...);
  }
};

CCB_END()

#endif  // CCB_MYSQL_CONNECTION_HH
