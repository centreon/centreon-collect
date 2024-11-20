/**
 * Copyright 2023 Centreon
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

#include "com/centreon/broker/sql/mysql_multi_insert.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

constexpr unsigned max_query_total_length = 8 * 1024 * 1024;

/**
 * @brief Construct a new mysql multi insert::mysql multi insert object
 *
 * @param query first part of the query as INSERT INTO TABLE my_table COLUMN
 * (col_1,col_2,col_3) VALUES
 * @param on_duplicate_key_part optional end of the query as ON DUPLICATE KEY
 * UPDATE col_1=VALUES(col_1)
 */
mysql_multi_insert::mysql_multi_insert(const std::string& query,
                                       const std::string& on_duplicate_key_part)
    : _query(query + ' '),
      _on_duplicate_key_part(' ' + on_duplicate_key_part),
      _max_query_begin_length(max_query_total_length -
                              on_duplicate_key_part.length()) {}

unsigned mysql_multi_insert::execute_queries(mysql& pool,
                                             my_error::code ec,
                                             int thread_id) {
  for (std::string& query_data : _queries) {
    thread_id =
        pool.run_query(query_data + _on_duplicate_key_part, ec, thread_id);
  }
  return _queries.size();
}

/**
 * @brief push data into the request
 * data will be bound to query during push_request call
 *
 * @param data data that will be bound it must be like that
 * "(data1,'data_string2', data3,...)"
 */
void mysql_multi_insert::push(const std::string& data) {
  std::string* to_add;
  if (_queries.empty() ||
      _queries.rbegin()->length() >= _max_query_begin_length) {
    _queries.emplace_back();
    to_add = &*_queries.rbegin();
    to_add->reserve(_max_query_begin_length);
    *to_add = _query;
  } else {
    to_add = &*_queries.rbegin();
    to_add->push_back(',');
  }
  to_add->append(data);
}

/**
 * @brief bulk_or_multi constructor to use when bulk prepared statements are
 * available.
 *
 * @param connexion Mysql object to interract with the database.
 * @param request SQL Query with placeholders included.
 * @param bulk_row  Reserved rows in the mysql_bulk_bind.
 * @param execute_delay_ready Max duration in seconds after what the
 * bulk_or_multi is ready.
 * @param row_count_ready Max row count after what the bulk_or_multi is
 * considered as ready.
 */
bulk_or_multi::bulk_or_multi(
    mysql& connexion,
    const std::string& request,
    unsigned bulk_row,
    const std::chrono::system_clock::duration execute_delay_ready,
    unsigned row_count_ready)
    : _bulk_stmt(std::make_unique<mysql_bulk_stmt>(request)),
      _bulk_bind(_bulk_stmt->create_bind()),
      _bulk_row(bulk_row),
      _execute_delay_ready(execute_delay_ready),
      _row_count_ready(row_count_ready) {
  connexion.prepare_statement(*_bulk_stmt);
  _bulk_bind->reserve(_bulk_row);
}

/**
 * @brief bulk_or_multi constructor to use when bulk prepared statement aren't
 * available.
 *
 * @param query SQL query with VALUES at the end.
 * @param on_duplicate_key_part Second part of the query, something beginning
 * with ON DUPLICATE KEY. This string can also be empty.
 * @param execute_delay_ready Max duration in seconds after what the
 * bulk_or_multi is ready.
 * @param row_count_ready Max row count after what the bulk_or_multi is
 * considered as ready.
 */
bulk_or_multi::bulk_or_multi(
    const std::string& query,
    const std::string& on_duplicate_key_part,
    const std::chrono::system_clock::duration execute_delay_ready,
    unsigned row_count_ready)
    : _row_count(0),
      _first_row_add_time(std::chrono::system_clock::time_point::max()),
      _execute_delay_ready(execute_delay_ready),
      _row_count_ready(row_count_ready),
      _mult_insert(
          std::make_unique<mysql_multi_insert>(query, on_duplicate_key_part)) {}

/**
 * @brief execute _bulk or multi insert query
 *
 * @param connexion Object to interract with the database.
 * @param ec The error code to use in case of error.
 * @param thread_id Index of the connection to use.
 */
void bulk_or_multi::execute(mysql& connexion,
                            my_error::code ec,
                            int thread_id) {
  if (_bulk_stmt) {
    /* If the database connection is lost, we can have this issue */
    if (!_bulk_bind) {
      _bulk_bind = _bulk_stmt->create_bind();
      _bulk_bind->reserve(_bulk_row);
    } else if (!_bulk_bind->empty()) {
      _bulk_stmt->set_bind(std::move(_bulk_bind));
      connexion.run_statement(*_bulk_stmt, ec, thread_id);
      _bulk_bind = _bulk_stmt->create_bind();
      _bulk_bind->reserve(_bulk_row);
    }
  } else {
    _mult_insert->execute_queries(connexion, ec, thread_id);
    _mult_insert->clear_queries();
  }
  _row_count = 0;
  _first_row_add_time = std::chrono::system_clock::time_point::max();
}

/**
 * @brief refresh _row_counter and _first_row_add_time
 *
 */
void bulk_or_multi::on_add_row() {
  if (!_row_count) {
    _first_row_add_time = std::chrono::system_clock::now();
  }
  ++_row_count;
}

/**
 * @brief return the delay between the time of first event added and now
 *
 * @return std::chrono::seconds
 */
std::chrono::seconds bulk_or_multi::get_oldest_waiting_event_delay() const {
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  if (_first_row_add_time < now)
    return std::chrono::duration_cast<std::chrono::seconds>(
        now - _first_row_add_time);
  else {
    return std::chrono::seconds(0);
  }
}

/**
 * @brief return true if the delay between the first data add and now >=
 * _execute_delay_ready or if the number of inserted data >= _row_count_ready
 *
 * @return true
 * @return false
 */
bool bulk_or_multi::ready() {
  return get_oldest_waiting_event_delay() >= _execute_delay_ready ||
         _row_count >= _row_count_ready;
}
