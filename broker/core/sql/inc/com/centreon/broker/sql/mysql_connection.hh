/**
 * Copyright 2018 - 2024 Centreon
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

#ifndef CCB_MYSQL_CONNECTION_HH
#define CCB_MYSQL_CONNECTION_HH

#include "com/centreon/broker/sql/database_config.hh"
#include "com/centreon/broker/sql/mysql_bulk_stmt.hh"
#include "com/centreon/broker/sql/mysql_error.hh"
#include "com/centreon/broker/sql/mysql_result.hh"
#include "com/centreon/broker/sql/mysql_stmt.hh"
#include "com/centreon/broker/sql/mysql_task.hh"
#include "com/centreon/broker/sql/stats.hh"
#include "com/centreon/broker/stats/center.hh"

namespace com::centreon::broker {

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

  const std::string _host;
  const std::string _socket;
  const std::string _user;
  const std::string _pwd;
  const std::string _name;
  const int _port;
  const std::string _extension_directory;
  const unsigned _max_second_commit_delay;
  time_t _last_commit;
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
  SqlConnectionStats* _proto_stats;
  std::time_t _last_stats;
  uint32_t _qps;
  unsigned _category;

  sql::stats _stats;

  /* mutex to protect the string access in _error */
  mutable std::mutex _error_m;
  database::mysql_error _error;

  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

  std::shared_ptr<stats::center> _center;

  /**************************************************************************/
  /*                    Methods executed by this thread                     */
  /**************************************************************************/
  bool _server_error(int code) const;
  void _run();
  void _process_tasks(std::list<std::unique_ptr<database::mysql_task>>& task);
  void _process_while_empty_task(
      std::list<std::unique_ptr<database::mysql_task>>& task);

  std::string _get_stack(
      const std::list<std::unique_ptr<database::mysql_task>>& task);
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
  bool _try_to_reconnect();

  static void (mysql_connection::*const _task_processing_table[])(
      database::mysql_task* task);

  void _prepare_connection();
  void _clear_connection();
  void _update_stats() noexcept;
  void _send_exceptions_to_task_futures(
      std::list<std::unique_ptr<database::mysql_task>>& tasks_list);

  inline void set_need_to_commit() { _need_commit = true; }
  void _finish();

 public:
  /**************************************************************************/
  /*                  Methods executed by the main thread                   */
  /**************************************************************************/

  mysql_connection(const database_config& db_cfg,
                   SqlConnectionStats* stats,
                   const std::shared_ptr<spdlog::logger>& logger,
                   std::shared_ptr<stats::center> center);
  ~mysql_connection();

  void prepare_query(int id, std::string const& query);
  void commit(std::promise<void>&& p);
  void run_query(std::string const& query, my_error::code ec);
  void run_query_and_get_result(std::string const& query,
                                std::promise<database::mysql_result>&& promise);
  void run_query_and_get_int(std::string const& query,
                             std::promise<int>&& promise,
                             database::mysql_task::int_type type);
  void get_server_version(std::promise<const char*>&& promise);

  void run_statement(database::mysql_stmt_base& stmt, my_error::code ec);
  void run_statement_and_get_result(
      database::mysql_stmt& stmt,
      std::promise<database::mysql_result>&& promise,
      size_t length);

  template <typename T>
  void run_statement_and_get_int(database::mysql_stmt_base& stmt,
                                 std::promise<T>&& promise,
                                 database::mysql_task::int_type type) {
    _push(std::make_unique<database::mysql_task_statement_int<T>>(
        stmt, std::move(promise), type));
  }

  bool fetch_row(database::mysql_result& result);
  mysql_bind_mapping get_stmt_mapping(int stmt_id) const;
  bool match_config(database_config const& db_cfg) const;
  int get_tasks_count() const;
  bool is_finish_asked() const;
  bool is_finished() const;
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
  void stop();
};

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_CONNECTION_HH
