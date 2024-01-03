/*
** Copyright 2018-2023 Centreon
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
#include <errmsg.h>

#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/mysql_manager.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

constexpr const char* mysql_error::msg[];

const int MAX_ATTEMPTS = 10;

void (mysql_connection::*const mysql_connection::_task_processing_table[])(
    mysql_task* task) = {
    &mysql_connection::_query,
    &mysql_connection::_query_res,
    &mysql_connection::_query_int,
    &mysql_connection::_commit,
    &mysql_connection::_prepare,
    &mysql_connection::_statement,
    &mysql_connection::_statement_res,
    &mysql_connection::_statement_int<int>,
    &mysql_connection::_statement_int<int64_t>,
    &mysql_connection::_statement_int<uint32_t>,
    &mysql_connection::_statement_int<uint64_t>,
    &mysql_connection::_fetch_row_sync,
    &mysql_connection::_get_version,
};

/******************************************************************************/
/*                      Methods executed by this thread                       */
/******************************************************************************/

/**
 * @brief check if the error code is a server error. At the moment, we only
 * check two errors. Maybe we will need to add some.
 *
 * @param code the code to check
 *
 * @return a boolean telling if the error is fatal (a server error).
 */
bool mysql_connection::_server_error(int code) const {
  switch (code) {
    case CR_SERVER_GONE_ERROR:
    case CR_SERVER_LOST:
      return true;
    default:
      return false;
  }
}

/**
 * @brief Once the connection established, we set several parameters, this is
 * done by this function.
 */
void mysql_connection::_prepare_connection() {
  mysql_set_character_set(_conn, "utf8mb4");

  /* This is to set a timeout for the mysql_ping() function that can hang
   * sometimes */
  uint32_t timeout = 5;
  mysql_optionsv(_conn, MYSQL_OPT_READ_TIMEOUT, (void*)&timeout);

  if (_qps > 1)
    mysql_autocommit(_conn, 0);
  else
    mysql_autocommit(_conn, 1);
}

/**
 * @brief Function executed to close correctly the MYSQL connection.
 */
void mysql_connection::_clear_connection() {
  for (std::unordered_map<uint32_t, MYSQL_STMT*>::iterator it = _stmt.begin(),
                                                           end = _stmt.end();
       it != end; ++it) {
    mysql_stmt_close(it->second);
    it->second = nullptr;
  }
  _stmt.clear();
  mysql_close(_conn);
  _conn = nullptr;
  if (_connected) {
    _connected = false;
    _switch_point = std::time(nullptr);
  }
  _update_stats();
}

/**
 * @brief Fill statistics if it happened more than 1 second ago
 */
void mysql_connection::_update_stats() noexcept {
  auto now = std::time(nullptr);
  if (now > _last_stats + 2) {
    _last_stats = now;

    database::stats::loop avg_loop = _stats.average_loop();

    float stmt_avg = _stats.average_stmt_duration();
    float query_avg = _stats.average_query_duration();

    {
      std::lock_guard<stats::center> lck(stats::center::instance());
      _proto_stats->set_waiting_tasks(static_cast<int32_t>(_tasks_count));
      if (static_cast<bool>(_connected)) {
        _proto_stats->set_up_since(_switch_point);

        _proto_stats->set_average_statement_duration(stmt_avg);
        _proto_stats->set_average_query_duration(query_avg);

        _proto_stats->clear_slowest_statements();
        auto& ss = _stats.get_stat_stmt();
        _proto_stats->mutable_slowest_statements()->Reserve(ss.size());
        for (auto& l_ss : ss) {
          auto* ss = _proto_stats->add_slowest_statements();
          ss->set_rows_count(l_ss.rows_count);
          ss->set_duration(l_ss.duration);
          ss->set_start_time(l_ss.start_time);
          ss->set_statement_id(l_ss.statement_id);
          ss->set_statement_query(l_ss.statement_query);
        }

        _proto_stats->clear_slowest_queries();
        auto& sq = _stats.get_stat_query();
        _proto_stats->mutable_slowest_queries()->Reserve(sq.size());
        for (auto& l_sq : sq) {
          auto* sq = _proto_stats->add_slowest_queries();
          sq->set_length(l_sq.length);
          sq->set_duration(l_sq.duration);
          sq->set_start_time(l_sq.start_time);
          sq->set_query(l_sq.query);
        }
      } else {
        _proto_stats->set_down_since(_switch_point);
        _proto_stats->clear_slowest_statements();
        _proto_stats->clear_slowest_queries();
        _proto_stats->set_average_statement_duration(0);
        _proto_stats->set_average_query_duration(0);
      }
    }
    _proto_stats->set_activity_percent(avg_loop.activity_percent);
    _proto_stats->set_average_loop_duration(avg_loop.duration);
  }
}

/**
 * @brief Try to reconnect to the database when a server error arised.
 *
 * @return True on success, false otherwise.
 */
bool mysql_connection::_try_to_reconnect() {
  std::lock_guard<std::mutex> lck(_start_m);

  _clear_connection();
  log_v2::sql()->info(
      "mysql_connection: server has gone away, attempt to reconnect");
  _conn = mysql_init(nullptr);
  if (!_conn) {
    log_v2::sql()->error("mysql_connection: reconnection failed.");
    return false;
  }

  uint32_t timeout = 10;
  mysql_options(_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

  if (!mysql_real_connect(_conn, _host.c_str(), _user.c_str(), _pwd.c_str(),
                          _name.c_str(), _port,
                          (_socket == "" ? nullptr : _socket.c_str()),
                          CLIENT_FOUND_ROWS)) {
    log_v2::sql()->error(
        "mysql_connection: The mysql/mariadb database seems not started.");
    return false;
  }
  _last_access = std::time(nullptr);

  _prepare_connection();

  /* Re-prepare all statements */
  bool fail = false;
  for (auto itq = _stmt_query.begin(), endq = _stmt_query.end(); itq != endq;
       ++itq) {
    MYSQL_STMT* s = mysql_stmt_init(_conn);
    if (!s) {
      log_v2::sql()->error(
          "mysql_connection: impossible to reset prepared statements");
      fail = true;
      break;
    } else {
      if (mysql_stmt_prepare(s, itq->second.c_str(), itq->second.size())) {
        log_v2::sql()->error("mysql_connection: {}", mysql_stmt_error(s));
        fail = true;
        break;
      } else
        _stmt[itq->first] = s;
    }
  }
  if (!fail) {
    _switch_point = std::time(nullptr);
    _connected = true;
    _update_stats();
    return true;
  }
  return false;
}

void mysql_connection::_query(mysql_task* t) {
  mysql_task_run* task(static_cast<mysql_task_run*>(t));

  database::stats::query_span stats(&_stats, task->query);

  SPDLOG_LOGGER_DEBUG(log_v2::sql(), "mysql_connection {:p}: run query: {}",
                      static_cast<const void*>(this), task->query);
  if (mysql_query(_conn, task->query.c_str())) {
    const char* m = mysql_error::msg[task->error_code];
    std::string err_msg(fmt::format("{} {}", m, ::mysql_error(_conn)));
    log_v2::sql()->error("mysql_connection: {}", err_msg);
    if (task->fatal || _server_error(::mysql_errno(_conn)))
      set_error_message(err_msg);
  } else
    _need_commit = true;
}

void mysql_connection::_query_res(mysql_task* t) {
  mysql_task_run_res* task = static_cast<mysql_task_run_res*>(t);

  database::stats::query_span stats(&_stats, task->query);

  SPDLOG_LOGGER_DEBUG(log_v2::sql(), "mysql_connection {:p}: run query: {}",
                      static_cast<const void*>(this), task->query);
  if (mysql_query(_conn, task->query.c_str())) {
    std::string err_msg(::mysql_error(_conn));
    log_v2::sql()->error("mysql_connection: {}", err_msg);
    if (_server_error(mysql_errno(_conn)))
      set_error_message(err_msg);

    msg_fmt e(err_msg);
    task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
  } else {
    /* All is good here */
    _need_commit = true;
    task->promise.set_value(mysql_result(this, mysql_store_result(_conn)));
  }
}

void mysql_connection::_query_int(mysql_task* t) {
  mysql_task_run_int* task = static_cast<mysql_task_run_int*>(t);
  database::stats::query_span stats(&_stats, task->query);
  SPDLOG_LOGGER_DEBUG(log_v2::sql(), "mysql_connection {:p}: run query: {}",
                      static_cast<const void*>(this), task->query);
  if (mysql_query(_conn, task->query.c_str())) {
    std::string err_msg(::mysql_error(_conn));
    log_v2::sql()->error("mysql_connection: {}", err_msg);
    if (_server_error(::mysql_errno(_conn)))
      set_error_message(err_msg);

    msg_fmt e(err_msg);
    task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
  } else {
    /* All is good here */
    _need_commit = true;
    if (task->return_type == mysql_task::AFFECTED_ROWS)
      task->promise.set_value(mysql_affected_rows(_conn));
    else /* LAST_INSERT_ID */
      task->promise.set_value(mysql_insert_id(_conn));
  }
}

void mysql_connection::_commit(mysql_task* t) {
  mysql_task_commit* task(static_cast<mysql_task_commit*>(t));
  database::stats::query_span stats(&_stats, "COMMIT");
  int32_t attempts = 0;
  int res;
  std::string err_msg;
  if (_need_commit) {
    log_v2::sql()->debug("mysql_connection: commit");
    while (attempts++ < MAX_ATTEMPTS && (res = mysql_commit(_conn))) {
      err_msg = ::mysql_error(_conn);
      if (_server_error(::mysql_errno(_conn))) {
        set_error_message(err_msg);
        break;
      }
      log_v2::sql()->error("mysql_connection: {}", err_msg);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (res == 0)
      _last_access = std::time(nullptr);
  } else
    res = 0;

  if (res) {
    std::string err_msg(
        fmt::format("Error during commit: {}", ::mysql_error(_conn)));
    log_v2::sql()->error("mysql_connection: {}", err_msg);
    set_error_message(err_msg);
  } else {
    /* No more queries are waiting for a commit now. */
    _need_commit = false;
  }
  task->set_value(true);
}

void mysql_connection::_prepare(mysql_task* t) {
  mysql_task_prepare* task(static_cast<mysql_task_prepare*>(t));
  if (_stmt.find(task->id) != _stmt.end()) {
    log_v2::sql()->error(
        "mysql_connection: Statement already prepared: {} ({})", task->id,
        task->query);
    return;
  }

  _stmt_query[task->id] = task->query;
  log_v2::sql()->debug("mysql_connection: prepare statement {}: {}", task->id,
                       task->query);
  MYSQL_STMT* stmt(mysql_stmt_init(_conn));
  if (!stmt)
    set_error_message("statement initialization failed: insuffisant memory");
  else {
    if (mysql_stmt_prepare(stmt, task->query.c_str(), task->query.size())) {
      std::string err_msg(::mysql_stmt_error(stmt));
      log_v2::sql()->error("mysql_connection: {}", err_msg);
      set_error_message(err_msg);
    } else
      _stmt[task->id] = stmt;
  }
}

void mysql_connection::_statement(mysql_task* t) {
  mysql_task_statement* task(static_cast<mysql_task_statement*>(t));
  std::string& query = _stmt_query[task->statement_id];
  database::stats::stmt_span stats(&_stats, task->statement_id, query);
  log_v2::sql()->debug("mysql_connection: execute statement {}: {}",
                       task->statement_id, query);
  MYSQL_STMT* stmt(_stmt[task->statement_id]);
  if (!stmt) {
    log_v2::sql()->error("mysql_connection: no statement to execute");
    set_error_message("statement {} not prepared", task->statement_id);
    return;
  }
  MYSQL_BIND* bb = nullptr;
  if (task->bind) {
    bb = const_cast<MYSQL_BIND*>(task->bind->get_bind());
    if (task->bulk) {
      mysql_bulk_bind* bind = static_cast<mysql_bulk_bind*>(task->bind.get());
      uint32_t array_size = bind->rows_count();
      stats.set_rows_count(array_size);
      mysql_stmt_attr_set(stmt, STMT_ATTR_ARRAY_SIZE, &array_size);
    }
  }
  if (bb && mysql_stmt_bind_param(stmt, bb)) {
    std::string err_msg(::mysql_stmt_error(stmt));
    log_v2::sql()->error("mysql_connection: {}", err_msg);
    if (task->fatal)
      set_error_message(err_msg);
    else {
      log_v2::sql()->error(
          "mysql_connection: Error while binding values in statement: {}",
          err_msg);
    }
  } else {
    int32_t attempts = 0;
    for (;;) {
      if (mysql_stmt_execute(stmt)) {
        std::string err_msg(fmt::format("{} {}",
                                        mysql_error::msg[task->error_code],
                                        ::mysql_stmt_error(stmt)));
        if (_server_error(::mysql_stmt_errno(stmt))) {
          set_error_message(err_msg);
          break;
        }
        if (mysql_stmt_errno(stmt) != 1213 &&
            mysql_stmt_errno(stmt) != 1205)  // Dead Lock error
          attempts = MAX_ATTEMPTS;

        if (mysql_commit(_conn)) {
          set_error_message("Commit failed after execute statement");
          break;
        }

        log_v2::sql()->error("mysql_connection: {}", err_msg);
        if (++attempts >= MAX_ATTEMPTS) {
          if (task->fatal || _server_error(::mysql_stmt_errno(stmt)))
            set_error_message("{} {}", mysql_error::msg[task->error_code],
                              ::mysql_stmt_error(stmt));
          break;
        }
      } else {
        _need_commit = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

void mysql_connection::_statement_res(mysql_task* t) {
  mysql_task_statement_res* task(static_cast<mysql_task_statement_res*>(t));
  const std::string& query = _stmt_query[task->statement_id];
  database::stats::stmt_span stats(&_stats, task->statement_id, query);
  log_v2::sql()->debug("mysql_connection: execute statement {}: {}",
                       task->statement_id, query);
  MYSQL_STMT* stmt(_stmt[task->statement_id]);
  if (!stmt) {
    log_v2::sql()->error("mysql_connection: no statement to execute");
    msg_fmt e("statement not prepared");
    task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
    return;
  }
  MYSQL_BIND* bb = nullptr;
  if (task->bind) {
    bb = const_cast<MYSQL_BIND*>(task->bind->get_bind());
    if (task->bulk) {
      mysql_bulk_bind* bind = static_cast<mysql_bulk_bind*>(task->bind.get());
      uint32_t array_size = bind->rows_count();
      stats.set_rows_count(array_size);
      mysql_stmt_attr_set(stmt, STMT_ATTR_ARRAY_SIZE, &array_size);
    }
  }
  if (bb && mysql_stmt_bind_param(stmt, bb)) {
    std::string err_msg(::mysql_stmt_error(stmt));
    log_v2::sql()->error("mysql_connection: {}", err_msg);
    msg_fmt e("statement and get result failed: {}", err_msg);
    task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
  } else {
    int32_t attempts = 0;
    for (;;) {
      if (mysql_stmt_execute(stmt)) {
        std::string err_msg(::mysql_stmt_error(stmt));
        if (_server_error(mysql_stmt_errno(stmt))) {
          set_error_message(err_msg);
          msg_fmt e(err_msg);
          task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
          break;
        }
        if (mysql_stmt_errno(stmt) != 1213 &&
            mysql_stmt_errno(stmt) != 1205)  // Dead Lock error
          attempts = MAX_ATTEMPTS;

        if (mysql_commit(_conn)) {
          set_error_message("Commit failed after execute statement");
          break;
        }

        log_v2::sql()->error("mysql_connection: {}", err_msg);
        if (++attempts >= MAX_ATTEMPTS) {
          msg_fmt e(err_msg);
          task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
          break;
        }
      } else {
        _need_commit = true;
        mysql_result res(this, task->statement_id);
        MYSQL_STMT* stmt(_stmt[task->statement_id]);
        MYSQL_RES* prepare_meta_result(mysql_stmt_result_metadata(stmt));
        if (prepare_meta_result == nullptr) {
          if (mysql_stmt_errno(stmt)) {
            std::string err_msg(::mysql_stmt_error(stmt));
            msg_fmt e(err_msg);
            task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
          } else
            task->promise.set_value(nullptr);
        } else {
          int size(mysql_num_fields(prepare_meta_result));
          auto bind = std::make_unique<mysql_bind_result>(size, task->length);

          if (mysql_stmt_bind_result(stmt, bind->get_bind())) {
            std::string err_msg(::mysql_stmt_error(stmt));
            msg_fmt e(err_msg);
            task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
            return;
          } else {
            if (mysql_stmt_store_result(stmt)) {
              std::string err_msg(::mysql_stmt_error(stmt));
              if (_server_error(::mysql_stmt_errno(stmt)))
                set_error_message(err_msg);
              msg_fmt e(err_msg);
              task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
              return;
            }
            // Here, we have the first row.
            res.set(prepare_meta_result);
            bind->set_empty();
          }
          res.set_bind(move(bind));
          task->promise.set_value(std::move(res));
        }
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

template <typename T>
void mysql_connection::_statement_int(mysql_task* t) {
  mysql_task_statement_int<T>* task(
      static_cast<mysql_task_statement_int<T>*>(t));
  const std::string& query = _stmt_query[task->statement_id];
  database::stats::stmt_span stats(&_stats, task->statement_id, query);
  log_v2::sql()->debug("mysql_connection: execute statement {}: {}",
                       task->statement_id, query);
  MYSQL_STMT* stmt(_stmt[task->statement_id]);
  if (!stmt) {
    log_v2::sql()->error("mysql_connection: no statement to execute");
    msg_fmt e("statement not prepared");
    task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
    return;
  }
  MYSQL_BIND* bb(nullptr);
  if (task->bind) {
    bb = const_cast<MYSQL_BIND*>(task->bind->get_bind());
    if (task->bulk) {
      mysql_bulk_bind* bind = static_cast<mysql_bulk_bind*>(task->bind.get());
      uint32_t array_size = bind->rows_count();
      stats.set_rows_count(array_size);
      mysql_stmt_attr_set(stmt, STMT_ATTR_ARRAY_SIZE, &array_size);
    }
  }
  if (bb && mysql_stmt_bind_param(stmt, bb)) {
    std::string err_msg(::mysql_stmt_error(stmt));
    log_v2::sql()->error("mysql_connection: {}", err_msg);
    msg_fmt e(err_msg);
    task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
  } else {
    int32_t attempts = 0;
    for (;;) {
      if (mysql_stmt_execute(stmt)) {
        std::string err_msg(::mysql_stmt_error(stmt));
        if (_server_error(mysql_stmt_errno(stmt))) {
          set_error_message(err_msg);
          msg_fmt e(err_msg);
          task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
          break;
        }
        if (mysql_stmt_errno(stmt) != 1213 &&
            mysql_stmt_errno(stmt) != 1205)  // Dead Lock error
          attempts = MAX_ATTEMPTS;

        mysql_commit(_conn);

        if (++attempts >= MAX_ATTEMPTS) {
          msg_fmt e("run statement and get result failed: {}", err_msg);
          task->promise.set_exception(std::make_exception_ptr<msg_fmt>(e));
          break;
        }
      } else {
        _need_commit = true;
        if (task->return_type == mysql_task::AFFECTED_ROWS)
          task->promise.set_value(
              mysql_stmt_affected_rows(_stmt[task->statement_id]));
        else /* LAST_INSERT_ID */
          task->promise.set_value(
              mysql_stmt_insert_id(_stmt[task->statement_id]));
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

void mysql_connection::_fetch_row_sync(mysql_task* t) {
  mysql_task_fetch* task(static_cast<mysql_task_fetch*>(t));
  int stmt_id(task->result->get_statement_id());
  if (stmt_id) {
    MYSQL_STMT* stmt(_stmt[stmt_id]);
    int res(mysql_stmt_fetch(stmt));
    if (res != 0) {
      if (res == MYSQL_DATA_TRUNCATED)
        log_v2::sql()->error(
            "columns in the current row are too long, data would be truncated");
      task->result->get_bind()->set_empty();
    }
    task->promise.set_value(res == 0);
  } else {
    MYSQL_ROW r(mysql_fetch_row(task->result->get()));
    task->result->set_row(r);
    task->promise.set_value(r != nullptr);
  }
}

void mysql_connection::_get_version(mysql_task* t) {
  mysql_task_get_version* task(static_cast<mysql_task_get_version*>(t));
  const char* res = mysql_get_server_info(_conn);
  task->promise.set_value(res);
}

/**
 * @brief If the connection has encountered an error, this method returns true.
 *
 * @return a boolean True on error, False otherwise.
 */
bool mysql_connection::is_in_error() const {
  return _error.is_active();
}

std::string mysql_connection::get_error_message() {
  std::lock_guard<std::mutex> lck(_error_m);
  return _error.get_message();
}

/**
 * @brief Disable the connection's error. Therefore, the connection is no more
 * in error.
 */
void mysql_connection::clear_error() {
  _error.clear();
}

std::string mysql_connection::_get_stack() {
  std::string retval;
  for (auto& t : _tasks_list) {
    switch (t->type) {
      case mysql_task::RUN:
        retval += "RUN ; ";
        break;
      case mysql_task::RUN_RES:
        retval += "RUN with Result; ";
        break;
      case mysql_task::RUN_INT:
        retval += "RUN with int return; ";
        break;
      case mysql_task::COMMIT:
        retval += "COMMIT ; ";
        break;
      case mysql_task::PREPARE:
        retval += "PREPARE ; ";
        break;
      case mysql_task::STATEMENT:
        retval += "STATEMENT ; ";
        break;
      case mysql_task::STATEMENT_RES:
        retval += "STATEMENT with result; ";
        break;
      case mysql_task::STATEMENT_INT:
        retval += "STATEMENT with int return; ";
        break;
      case mysql_task::STATEMENT_UINT:
        retval += "STATEMENT with uint return; ";
        break;
      case mysql_task::STATEMENT_INT64:
        retval += "STATEMENT with int64 return; ";
        break;
      case mysql_task::STATEMENT_UINT64:
        retval += "STATEMENT with uint64 return; ";
        break;
      case mysql_task::FETCH_ROW:
        retval += "FETCH_ROW ; ";
        break;
      case mysql_task::GET_VERSION:
        retval += "GET_VERSION ; ";
        break;
    }
  }
  return retval;
}

void mysql_connection::_run() {
  std::unique_lock<std::mutex> lck(_start_m);
  _conn = mysql_init(nullptr);
  if (!_conn) {
    set_error_message(::mysql_error(_conn));
    _state = finished;
    _start_condition.notify_all();
    return;
  } else {
    uint32_t timeout = 10;
    mysql_options(_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    while (config::applier::mode != config::applier::finished &&
           !mysql_real_connect(_conn, _host.c_str(), _user.c_str(),
                               _pwd.c_str(), _name.c_str(), _port,
                               (_socket == "" ? nullptr : _socket.c_str()),
                               CLIENT_FOUND_ROWS)) {
      set_error_message(fmt::format(
          "mysql_connection: The mysql/mariadb database seems not started. "
          "Waiting before attempt to connect again: {}",
          ::mysql_error(_conn)));
      _state = finished;
      _start_condition.notify_all();
      return;
    }
    _last_access = std::time(nullptr);
  }

  if (config::applier::mode == config::applier::finished) {
    log_v2::sql()->debug("Connection over.");
    _state = finished;
    _start_condition.notify_all();
    lck.unlock();
  } else {
    _prepare_connection();

    _state = running;
    _start_condition.notify_all();

    _connected = true;
    _switch_point = std::time(nullptr);
    _update_stats();
    lck.unlock();

    std::unique_lock<std::mutex> lock(_tasks_m);
    while (_state == running || !_tasks_list.empty()) {
      /* inactive loop concerning queries/statements */
      database::stats::loop_span stats(&_stats);
      std::list<std::unique_ptr<database::mysql_task>> tasks_list;
      if (!_tasks_list.empty()) {
        std::swap(_tasks_list, tasks_list);
        assert(_tasks_list.empty());
      } else {
        /* We are waiting for some activity, nothing to do for now it is time
         * to make some ping */
        while (!_tasks_condition.wait_for(
            lock, std::chrono::seconds(10),
            [this] { return _finish_asked || !_tasks_list.empty(); })) {
          _update_stats();
        }

        std::time_t now = std::time(nullptr);

        if (_tasks_list.empty())
          _state = finished;
        else if (now >= _last_access + 30) {
          lock.unlock();
          log_v2::sql()->trace("SQL: performing mysql_ping.");
          if (mysql_ping(_conn)) {
            if (!_try_to_reconnect())
              log_v2::sql()->error("SQL: Reconnection failed.");
          } else {
            log_v2::sql()->trace("SQL: connection always alive");
            _last_access = now;
          }
          lock.lock();
        } else
          log_v2::sql()->trace(
              "SQL: last access to the database for this connection for {}s",
              now - _last_access);
        continue;
      }

      lock.unlock();
      stats.start_activity();
      for (auto& task : tasks_list) {
        --_tasks_count;
        _update_stats();

        if (_task_processing_table[task->type])
          (this->*(_task_processing_table[task->type]))(task.get());
        else {
          log_v2::sql()->error("mysql_connection: Error type not managed...");
        }
      }

      lock.lock();
    }
  }
  _clear_connection();
  mysql_thread_end();
  log_v2::core()->trace("mysql connection main loop finished.");
}

/******************************************************************************/
/*                    Methods executed by the main thread                     */
/******************************************************************************/

mysql_connection::mysql_connection(const database_config& db_cfg,
                                   SqlConnectionStats* stats)
    : _conn(nullptr),
      _finish_asked(false),
      _tasks_count{0},
      _need_commit(false),
      _last_access{0},
      _host(db_cfg.get_host()),
      _socket(db_cfg.get_socket()),
      _user(db_cfg.get_user()),
      _pwd(db_cfg.get_password()),
      _name(db_cfg.get_name()),
      _port(db_cfg.get_port()),
      _state(not_started),
      _connected{false},
      _switch_point{std::time(nullptr)},
      _proto_stats{stats},
      _last_stats{std::time(nullptr)},
      _qps(db_cfg.get_queries_per_transaction()) {
  std::unique_lock<std::mutex> lck(_start_m);
  log_v2::sql()->info("mysql_connection: starting connection");
  _thread = std::make_unique<std::thread>(&mysql_connection::_run, this);
  _start_condition.wait(lck, [this] { return _state != not_started; });
  if (_state == finished) {
    _thread->join();
    log_v2::sql()->error("mysql_connection: error while starting connection");
    throw msg_fmt("mysql_connection: error while starting connection");
  }
  pthread_setname_np(_thread->native_handle(), "mysql_connect");
  log_v2::sql()->info("mysql_connection: connection started");
  stats::center::instance().update(&SqlConnectionStats::set_waiting_tasks,
                                   _proto_stats, 0);
}

/**
 * @brief Destructor. When called, the finish task is post on the stack. So
 * the end will occur only when all the queries will be played.
 */
mysql_connection::~mysql_connection() {
  log_v2::sql()->info("mysql_connection: finished");
  finish();
  stats::center::instance().remove_connection(_proto_stats);
  _thread->join();
}

void mysql_connection::_push(std::unique_ptr<mysql_task>&& q) {
  std::lock_guard<std::mutex> locker(_tasks_m);
  if (_finish_asked || is_finished())
    throw msg_fmt("This connection is closed and does not accept any query");

  _tasks_list.push_back(std::move(q));
  _update_stats();
  ++_tasks_count;
  _tasks_condition.notify_all();
}

/**
 *  This method finishes to send current tasks and then commits. The commited
 * variable is then incremented of the queries committed count. This function is
 * called by mysql::commit whom goal is to commit on each of the connections.
 *  So, this last method waits all the commits to be done ; the semaphore is
 * there for that purpose.
 *
 *  @param commit_data synchronisation data
 */
void mysql_connection::commit(
    const mysql_task_commit::mysql_task_commit_data::pointer& commit_data) {
  _push(std::make_unique<mysql_task_commit>(commit_data));
}

void mysql_connection::prepare_query(int stmt_id, std::string const& query) {
  _push(std::make_unique<mysql_task_prepare>(stmt_id, query));
}

/**
 *  This method is used from the main thread to execute asynchronously a query.
 *  No exception is thrown in case of error since this query is made
 * asynchronously.
 *
 *  @param query The SQL query
 *  @param error_msg The error message to return in case of error.
 *  @param p A pointer to a promise.
 */
void mysql_connection::run_query(std::string const& query,
                                 my_error::code ec,
                                 bool fatal) {
  _push(std::make_unique<mysql_task_run>(query, ec, fatal));
}

void mysql_connection::run_query_and_get_result(
    const std::string& query,
    std::promise<mysql_result>&& promise) {
  _push(std::make_unique<mysql_task_run_res>(query, std::move(promise)));
}

void mysql_connection::run_query_and_get_int(std::string const& query,
                                             std::promise<int>&& promise,
                                             mysql_task::int_type type) {
  _push(std::make_unique<mysql_task_run_int>(query, std::move(promise), type));
}

void mysql_connection::run_statement(database::mysql_stmt_base& stmt,
                                     my_error::code ec,
                                     bool fatal) {
  _push(std::make_unique<mysql_task_statement>(stmt, ec, fatal));
}

void mysql_connection::run_statement_and_get_result(
    database::mysql_stmt& stmt,
    std::promise<mysql_result>&& promise,
    size_t length) {
  _push(std::make_unique<mysql_task_statement_res>(stmt, length,
                                                   std::move(promise)));
}

void mysql_connection::finish() {
  std::lock_guard<std::mutex> lock(_tasks_m);
  _finish_asked = true;
  _tasks_condition.notify_all();
}

bool mysql_connection::fetch_row(mysql_result& result) {
  std::promise<bool> promise;
  auto future = promise.get_future();
  _push(std::make_unique<mysql_task_fetch>(&result, std::move(promise)));
  return future.get();
}

bool mysql_connection::match_config(database_config const& db_cfg) const {
  std::lock_guard<std::mutex> lock(_cfg_mutex);
  return db_cfg.get_host() == _host && db_cfg.get_socket() == _socket &&
         db_cfg.get_user() == _user && db_cfg.get_password() == _pwd &&
         db_cfg.get_name() == _name && db_cfg.get_port() == _port &&
         db_cfg.get_queries_per_transaction() == _qps;
}

int mysql_connection::get_tasks_count() const {
  return _tasks_count;
}

bool mysql_connection::is_finish_asked() const {
  return _finish_asked;
}

bool mysql_connection::is_finished() const {
  return _state == finished;
}

void mysql_connection::get_server_version(std::promise<const char*>&& promise) {
  _push(std::make_unique<mysql_task_get_version>(std::move(promise)));
}