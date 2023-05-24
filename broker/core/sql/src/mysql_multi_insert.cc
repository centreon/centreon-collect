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
    : _query(query),
      _on_duplicate_key_part(on_duplicate_key_part),
      _max_data_length(max_query_total_length - query.length() -
                       on_duplicate_key_part.length() - 4096) {}

unsigned mysql_multi_insert::execute_queries(mysql& pool,
                                             my_error::code ec,
                                             int thread_id) const {
  std::string query;
  query.reserve(max_query_total_length);
  for (const std::string query_data : _queries_data) {
    query = _query;
    query.push_back(' ');
    query.append(query_data);
    query.push_back(' ');
    query.append(_on_duplicate_key_part);
    thread_id = pool.run_query(query, ec, thread_id);
  }
  return _queries_data.size();
}

/**
 * @brief push data into the request
 * data will be bound to query during push_request call
 *
 * @param data data that will be bound it must be like that "data1,
 * 'data_string2', data3,..." without ()
 */
void mysql_multi_insert::push(const std::string& data) {
  std::string* to_add;
  if (_queries_data.empty() ||
      _queries_data.rbegin()->length() >= _max_data_length) {
    _queries_data.emplace_back();
    to_add = &*_queries_data.rbegin();
  } else {
    to_add = &*_queries_data.rbegin();
    to_add->push_back(',');
  }
  to_add->push_back('(');
  to_add->append(data);
  to_add->push_back(')');
}

/**
 * @brief bulk constructor to use when bulk queries are available
 *
 * @param connexion
 * @param request query placeholders included
 * @param bulk_row  row reserved in mysql_bulk_bind
 * @param execute_delay_ready when first event add is older than
 * execute_delay_ready, ready() return true
 * @param row_count_ready when row_count_ready have been added to the query,
 * ready() return true
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
 * @brief multi_insert constructor to use when bulk queries aren't available
 *
 * @param query query with VALUES at the end
 * @param on_duplicate_key_part on duplicate key query part
 * @param execute_delay_ready when first event add is older than
 * execute_delay_ready, ready() return true
 * @param row_count_ready when row_count_ready have been added to the query,
 * ready() return true
 */
bulk_or_multi::bulk_or_multi(
    const std::string& query,
    const std::string& on_duplicate_key_part,
    const std::chrono::system_clock::duration execute_delay_ready,
    unsigned row_count_ready)
    : _mult_insert(
          std::make_unique<mysql_multi_insert>(query, on_duplicate_key_part)),
      _row_count(0),
      _first_row_add_time(std::chrono::system_clock::time_point::max()),
      _execute_delay_ready(execute_delay_ready),
      _row_count_ready(row_count_ready) {}

/**
 * @brief execute _bulk or multi insert query
 *
 * @param connexion
 */
void bulk_or_multi::execute(mysql& connexion) {
  if (_bulk_bind) {
    if (!_bulk_bind->empty()) {
      _bulk_stmt->set_bind(std::move(_bulk_bind));
      connexion.run_statement(*_bulk_stmt);
      _bulk_bind = _bulk_stmt->create_bind();
      _bulk_bind->reserve(_bulk_row);
    }
  } else {
    _mult_insert->execute_queries(connexion);
    _mult_insert->clear_queries();
  }
  _row_count = 0;
  _first_row_add_time = std::chrono::system_clock::time_point::max();
}

/**
 * @brief refresh _row_counter and _first_row_add_time
 *
 * @param event
 */
void bulk_or_multi::on_add_event() {
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
