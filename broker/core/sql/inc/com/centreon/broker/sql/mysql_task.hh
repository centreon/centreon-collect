/**
 * Copyright 2018 Centreon
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

#ifndef CCB_MYSQL_TASK_HH
#define CCB_MYSQL_TASK_HH

#include "com/centreon/broker/sql/mysql_bind_base.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com::centreon::broker {

typedef int (*mysql_callback)(MYSQL* conn, void* data);

namespace database {
class mysql_task {
 public:
  enum type {
    RUN,
    RUN_RES,
    RUN_INT,
    COMMIT,
    PREPARE,
    STATEMENT,
    STATEMENT_RES,
    STATEMENT_INT,
    STATEMENT_INT64,
    STATEMENT_UINT,
    STATEMENT_UINT64,
    FETCH_ROW,
    GET_VERSION,
  };

  enum int_type {
    AFFECTED_ROWS,
    LAST_INSERT_ID,
  };

  virtual ~mysql_task() {}

  mysql_task(type type) : type(type) {}
  type type;
};

class mysql_task_commit : public mysql_task {
 public:
  mysql_task_commit(std::promise<void>&& p)
      : mysql_task(mysql_task::COMMIT), promise(std::move(p)) {}

  std::promise<void> promise;
};

class mysql_task_fetch : public mysql_task {
 public:
  mysql_task_fetch(mysql_result* result, std::promise<bool>&& promise)
      : mysql_task(mysql_task::FETCH_ROW),
        result(result),
        promise(std::move(promise)) {}
  mysql_result* result;
  std::promise<bool> promise;
};

class mysql_task_prepare : public mysql_task {
 public:
  mysql_task_prepare(uint32_t id, std::string const& q)
      : mysql_task(mysql_task::PREPARE), id(id), query(q) {}
  uint32_t id;
  std::string query;
};

class mysql_task_run : public mysql_task {
 public:
  mysql_task_run(std::string const& q, mysql_error::code ec)
      : mysql_task(mysql_task::RUN), query(q), error_code(ec) {}
  std::string query;
  mysql_error::code error_code;
};

class mysql_task_run_res : public mysql_task {
 public:
  mysql_task_run_res(std::string const& q, std::promise<mysql_result>&& promise)
      : mysql_task(mysql_task::RUN_RES),
        query(q),
        promise(std::move(promise)) {}
  std::string query;
  std::promise<mysql_result> promise;
};

class mysql_task_get_version : public mysql_task {
 public:
  mysql_task_get_version(std::promise<const char*>&& promise)
      : mysql_task(mysql_task::GET_VERSION), promise(std::move(promise)) {}
  std::promise<const char*> promise;
};

class mysql_task_run_int : public mysql_task {
 public:
  mysql_task_run_int(std::string const& q,
                     std::promise<int>&& promise,
                     int_type type)
      : mysql_task(mysql_task::RUN_INT),
        query(q),
        promise(std::move(promise)),
        return_type(type) {}
  std::string query;
  std::promise<int> promise;
  int_type return_type;
};

class mysql_task_statement : public mysql_task {
 public:
  mysql_task_statement(database::mysql_stmt_base& stmt, mysql_error::code ec)
      : mysql_task(mysql_task::STATEMENT),
        statement_id(stmt.get_id()),
        param_count(stmt.get_param_count()),
        error_code(ec) {
    if (stmt.in_bulk()) {
      database::mysql_bulk_stmt& s =
          *static_cast<database::mysql_bulk_stmt*>(&stmt);
      bind.reset(s.get_bind().release());
      bulk = true;
    } else {
      database::mysql_stmt& s = *static_cast<database::mysql_stmt*>(&stmt);
      bind.reset(s.get_bind().release());
      bulk = false;
    }
  }
  uint32_t statement_id;
  int param_count;
  std::unique_ptr<database::mysql_bind_base> bind;
  mysql_error::code error_code;
  bool bulk;
};

class mysql_task_statement_res : public mysql_task {
 public:
  mysql_task_statement_res(database::mysql_stmt_base& stmt,
                           size_t length,
                           std::promise<mysql_result>&& promise)
      : mysql_task(mysql_task::STATEMENT_RES),
        length(length),
        promise(std::move(promise)),
        statement_id(stmt.get_id()),
        param_count(stmt.get_param_count()) {
    if (stmt.in_bulk()) {
      database::mysql_bulk_stmt& s =
          *static_cast<database::mysql_bulk_stmt*>(&stmt);
      bind.reset(s.get_bind().release());
      bulk = true;
    } else {
      database::mysql_stmt& s = *static_cast<database::mysql_stmt*>(&stmt);
      bind.reset(s.get_bind().release());
      bulk = false;
    }
  }

  size_t length;
  std::promise<mysql_result> promise;
  uint32_t statement_id;
  int param_count;
  std::unique_ptr<database::mysql_bind_base> bind;
  bool bulk;
};

template <typename T>
class mysql_task_statement_int : public mysql_task {
 public:
  mysql_task_statement_int(database::mysql_stmt_base& stmt,
                           std::promise<T>&& promise,
                           int_type type)
      : mysql_task((std::is_same<T, int>::value) ? mysql_task::STATEMENT_INT
                   : (std::is_same<T, int64_t>::value)
                       ? mysql_task::STATEMENT_INT64
                   : (std::is_same<T, uint32_t>::value)
                       ? mysql_task::STATEMENT_UINT
                       : mysql_task::STATEMENT_UINT64),
        promise(std::move(promise)),
        return_type(type),
        statement_id(stmt.get_id()),
        param_count(stmt.get_param_count()) {
    if (stmt.in_bulk()) {
      database::mysql_bulk_stmt& s =
          *static_cast<database::mysql_bulk_stmt*>(&stmt);
      bind.reset(s.get_bind().release());
      bulk = true;
    } else {
      database::mysql_stmt& s = *static_cast<database::mysql_stmt*>(&stmt);
      bind.reset(s.get_bind().release());
      bulk = false;
    }
  }
  std::promise<T> promise;
  int_type return_type;
  uint32_t statement_id;
  int param_count;
  std::unique_ptr<database::mysql_bind_base> bind;
  bool bulk;
};
}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_TASK_HH
