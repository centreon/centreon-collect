/**
 * Copyright 2018-2024 Centreon
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

#include <absl/strings/str_split.h>

#include "com/centreon/broker/sql/mysql_manager.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] db_cfg  Database configuration.
 */
mysql::mysql(database_config const& db_cfg)
    : _db_cfg(db_cfg),
      _pending_queries(0),
      _current_connection(0),
      _logger{log_v2::instance().get(log_v2::SQL)} {
  mysql_manager& mgr(mysql_manager::instance());
  _connection = mgr.get_connections(db_cfg);
  _logger->info("mysql connector configured with {} connection(s)",
                db_cfg.get_connections_count());
  _get_server_infos();
}

/**
 *  Destructor
 */
mysql::~mysql() {
  _logger->trace("mysql: destruction");
  try {
    commit();
  } catch (const std::exception& e) {
    _logger->warn(
        "Unable to commit on the database server. Probably not connected: {}",
        e.what());
  }
  _connection.clear();
  mysql_manager::instance().update_connections();
  _logger->trace("mysql object destroyed");
}

/**
 *  Commit all pending queries of all the threads.
 *  This function waits for all the threads to be committed.
 *
 *  @param thread_id: -1 to commit all the threads or the index of the thread to
 *                    commit.
 *  If an error occures, an exception is thrown.
 */
void mysql::commit(int thread_id) {
  std::string error;
  if (thread_id < 0) {
    std::vector<std::future<void>> futures;
    futures.reserve(_connection.size());
    for (auto& c : _connection) {
      std::promise<void> p;
      futures.push_back(p.get_future());
      c->commit(std::move(p));
    }
    for (auto& f : futures) {
      /* We must handle exceptions thrown only in case of cbd stopped. Errors
       * are then all the same. We get the first one and send it to the caller.
       */
      try {
        f.wait();
      } catch (const std::exception& e) {
        error = e.what();
      }
    }
  } else {
    std::promise<void> p;
    std::future<void> f = p.get_future();
    _connection[thread_id]->commit(std::move(p));
    try {
      f.wait();
    } catch (const std::exception& e) {
      error = e.what();
    }
  }
  if (!error.empty()) {
    throw msg_fmt(error);
  }
}

/**
 *  fetch_row
 *
 * @param[out] res A mysql_result reference that will get the next row of
 *                 a result.
 *
 * @return A boolean telling if there are another row after.
 */
bool mysql::fetch_row(mysql_result& res) {
  _check_errors();
  return res.get_connection()->fetch_row(res);
}

/**
 *  Checks for previous errors. As queries are made asynchronously, errors
 *  may arise after the calls. This function is generally called just before
 *  adding a new query and throw an exception if an error is active.
 *
 */
void mysql::_check_errors() {
  for (auto it = _connection.begin(), end = _connection.end(); it != end; ++it)
    if ((*it)->is_in_error())
      throw msg_fmt((*it)->get_error_message());
}

/**
 *  The simplest way to execute a query. Only the first arg is mandatory.
 *
 * @param query The query to execute.
 * @param error_msg An error message to complete the error message returned
 *                  by the mysql connector.
 * @param thread A thread id or 0 to keep the library choosing which one.
 *
 * @return The thread id that executed the query.
 */
int mysql::run_query(std::string const& query,
                     my_error::code ec,
                     int thread_id) {
  _check_errors();
  if (thread_id < 0)
    // Here, we use _current_thread
    thread_id = choose_best_connection(-1);

  _connection[thread_id]->run_query(query, ec);
  return thread_id;
}

/**
 * This method looks like run_query but it is used when we need a result given
 * by the query.
 *
 * @param query The query to execute.
 * @param promise A promise that will contain the result when it will be
 *                available.
 * @param thread A thread id or 0 to keep the library choosing which one.
 *
 * With this function, the query is done. The promise will provide the result
 * if available and it will contain an exception if the query failed.
 *
 * @return The thread id that executed the query.
 */
int mysql::run_query_and_get_result(std::string const& query,
                                    std::promise<mysql_result>&& promise,
                                    int thread_id) {
  _check_errors();
  if (thread_id < 0)
    // Here, we use _current_thread
    thread_id = choose_best_connection(-1);

  _connection[thread_id]->run_query_and_get_result(query, std::move(promise));
  return thread_id;
}

/**
 * This method looks like run_query but it is used when we need to get back
 * an integer such as an inserted id or a number of rows.
 *
 * @param query The query to execute.
 * @param promise A promise that will contain the integer when it will be
 *                available.
 * @param type An int_type to tell what kind of integer the promise will
 *             contain.
 * @param thread_id A thread id or 0 to keep the library choosing which one.
 *
 * With this function, the query is done. The promise will provide an integer
 * corresponding to the type given in parameter.
 *
 * @return The thread id that executed the query.
 */
int mysql::run_query_and_get_int(std::string const& query,
                                 std::promise<int>&& promise,
                                 mysql_task::int_type type,
                                 int thread_id) {
  _check_errors();
  if (thread_id < 0)
    // Here, we use _current_thread
    thread_id = choose_best_connection(-1);

  _connection[thread_id]->run_query_and_get_int(query, std::move(promise),
                                                type);
  return thread_id;
}

/**
 * This method executes a previously prepared statement. It works almost
 * like the run_query() method.
 *
 * @param stmt The statement to execute.
 * @param error_msg An error message to complete the error message returned
 *                  by the mysql connector.
 * @param thread A thread id or -1 to keep the library choosing which one.
 *
 * @return The thread id that executed the query.
 */
int mysql::run_statement(database::mysql_stmt_base& stmt,
                         my_error::code ec,
                         int thread_id) {
  _check_errors();
  if (thread_id < 0)
    // Here, we use _current_thread
    thread_id = choose_best_connection(-1);

  _connection[thread_id]->run_statement(stmt, ec);
  return thread_id;
}

/**
 * This method looks like run_query_and_get_result but it is used to execute a
 * prepared statement.
 *
 * @param stmt The statement to execute.
 * @param promise A promise that will contain the result when it will be
 *                available.
 * @param error_msg An error message to complete the error message returned
 *                  by the mysql connector.
 * @param thread_id A thread id or -1 to keep the library choosing which one.
 * @param length The max length of a column in characters. If it is too short,
 * the query will fail by returning nothing with an error message telling
 * this value is too short.
 *
 * With this function, the query is done. The promise will provide the result
 * if available and it will contain an exception if the query failed.
 *
 * @return The thread id that executed the query.
 */
int mysql::run_statement_and_get_result(database::mysql_stmt& stmt,
                                        std::promise<mysql_result>&& promise,
                                        int thread_id,
                                        size_t length) {
  _check_errors();
  if (thread_id < 0)
    // Here, we use _current_thread
    thread_id = choose_best_connection(-1);

  _connection[thread_id]->run_statement_and_get_result(stmt, std::move(promise),
                                                       length);
  return thread_id;
}

/**
 *  This method prepares a statement.
 *
 * @param stmt The statement to prepare.
 */
void mysql::prepare_statement(const mysql_stmt_base& stmt) {
  _check_errors();
  for (std::vector<std::shared_ptr<mysql_connection>>::const_iterator
           it(_connection.begin()),
       end(_connection.end());
       it != end; ++it)
    (*it)->prepare_query(stmt.get_id(), stmt.get_query());
}

/**
 *  This method prepares a statement from a query string and a bind mapping.
 *  The bind_mapping is not mandatory, it is almost a correspondance between
 *  positions and column names. If the query is made with only positions,
 *  it is not needed.
 *
 * @param query The query string.
 * @param bind_mapping The bind mapping.
 *
 * @return A mysql_stmt prepared and ready to use.
 */
mysql_stmt mysql::prepare_query(const std::string& query,
                                const mysql_bind_mapping& bind_mapping) {
  mysql_stmt retval(query, bind_mapping);
  prepare_statement(retval);

  return retval;
}

/**
 *  Returns the connections count used by this mysql object.
 *
 * @return an integer.
 */
int mysql::connections_count() const {
  return _connection.size();
}

/**
 *  choose_best_connection
 *
 * This method compares the connections activity and returns the index of
 * the best one to execute a new query.
 *
 * @return an integer.
 */
int mysql::choose_best_connection(int32_t type) {
  /* We work with _current_connection to avoid always working with the same
   * connections. */
  static int last_type = -1;
  static int previous_retval = 0;
  if (type >= 0 && last_type == type)
    return previous_retval;

  int retval(_current_connection);
  int task_count(std::numeric_limits<int>::max());
  int count(_connection.size());
  for (int i(0); i < count; i++) {
    ++_current_connection;
    if (_current_connection >= count)
      _current_connection = 0;
    if (_connection[_current_connection]->get_tasks_count() < task_count) {
      retval = _current_connection;
      task_count = _connection[_current_connection]->get_tasks_count();
    }
  }
  last_type = type;
  previous_retval = retval;
  return retval;
}

/**
 *  Return a connection index from a name. The same name will give the same
 *  index.
 *
 *  @param name The name to give
 *
 *  @return an integer
 */
int mysql::choose_connection_by_name(std::string const& name) {
  static int connection(0);
  int retval;
  std::unordered_map<std::string, int>::iterator it(
      _connection_by_name.find(name));

  if (it == _connection_by_name.end()) {
    retval = (connection++) % connections_count();
    _connection_by_name.insert(std::make_pair(name, retval));
  } else
    retval = it->second;
  return retval;
}

/**
 *  Return a connection index from an instance id. Each time a same instance
 *  is choosen, the same integer is returned.
 *
 *  @param instance_id The instance id we work with.
 *
 *  @return an integer
 */
int mysql::choose_connection_by_instance(int instance_id) const {
  return instance_id % connections_count();
}

/**
 * @brief Accessor to the database configuration
 *
 * @return the running database configuration.
 */
const database_config& mysql::get_config() const {
  return _db_cfg;
}

void mysql::_get_server_infos() {
  _support_bulk_statement = false;
  if (_connection.empty())
    return;
  else {
    std::promise<const char*> p;
    auto future = p.get_future();
    _connection[0]->get_server_version(std::move(p));
    _server_version = future.get();
  }

  std::vector<std::string_view> v =
      absl::StrSplit(_server_version, absl::ByAnyChar(".-"));
  if (v.size() >= 4) {
    int32_t major;
    int32_t minor;
    int32_t patch;
    std::string_view server = v[3];
    if (absl::SimpleAtoi(v[0], &major) && absl::SimpleAtoi(v[1], &minor) &&
        absl::SimpleAtoi(v[2], &patch)) {
      _logger->info("connected to '{}' Server, version {}.{}.{}",
                    fmt::string_view(server.data(), server.size()), major,
                    minor, patch);
      if (server == "MariaDB" && (major > 10 || (major == 10 && minor >= 2))) {
        _logger->info("it supports column-wise binding in prepared statements");
        _support_bulk_statement = true;
      }
    }
  } else {
    _logger->info("connected to '{}' Server", _server_version);
  }
}
