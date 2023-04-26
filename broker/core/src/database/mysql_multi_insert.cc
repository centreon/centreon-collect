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

#include "com/centreon/broker/database/mysql_multi_insert.hh"
#include "com/centreon/broker/mysql.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

constexpr unsigned max_query_total_length = 1024 * 1024;

unsigned mysql_multi_insert::push_stmt(mysql& pool, int thread_id) const {
  unsigned row_index;

  // compute (?,?,...) per row
  std::string row_values;
  row_values.reserve(_nb_column * 2 + 2);
  row_values.push_back('(');
  if (_nb_column) {
    row_values.push_back('?');
  }
  for (unsigned col_index = 1; col_index < _nb_column; ++col_index) {
    row_values.push_back(',');
    row_values.push_back('?');
  }
  row_values.push_back(')');

  size_t remain = _rows.size();
  size_t fixed_part_length =
      _query.length() + _on_duplicate_key_part.length() + 2 /*spaces*/;
  size_t max_rows_per_query = (max_query_total_length - fixed_part_length) /
                              (row_values.length() + 2);  // add ,\n
  // mariadb limit of place holders
  if (max_rows_per_query * _nb_column > 65535) {
    max_rows_per_query = 65535 / _nb_column;
  }
  if (max_rows_per_query > remain) {
    max_rows_per_query = remain;
  }

  auto compute_value_part = [&](std::string& to_append, unsigned nb_row) {
    to_append += row_values;
    for (row_index = 1; row_index < nb_row; ++row_index) {
      to_append.push_back(',');
      to_append.push_back('\n');
      to_append += row_values;
    }
  };

  // compute max (?,?,?,..),\n(?,?,?,..),,\n... to reuse it
  std::string max_query_values_part;
  max_query_values_part.reserve(max_rows_per_query * (row_values.length() * 2));
  compute_value_part(max_query_values_part, max_rows_per_query);

  std::string query;
  query.reserve(fixed_part_length + max_query_values_part.length());

  // construction of the first string query
  query = _query;
  query.push_back(' ');
  query += max_query_values_part;
  query.push_back(' ');
  query += _on_duplicate_key_part;

  std::list<database::mysql_stmt> statements;
  std::list<std::future<unsigned>> to_wait;
  for (auto row_iter = _rows.begin(); row_iter != _rows.end();) {
    unsigned nb_row_to_bind =
        remain < max_rows_per_query ? remain : max_rows_per_query;
    if (nb_row_to_bind <
        max_rows_per_query) {  // next query shorter than first one => compute
      query = _query;
      query.push_back(' ');
      compute_value_part(query, remain);
      query.push_back(' ');
      query += _on_duplicate_key_part;
    }

    remain -= nb_row_to_bind;
    unsigned bind_first_column_index = 0;
    statements.emplace_back(query);

    database::mysql_stmt& last = *statements.rbegin();
    for (; row_iter != _rows.end() && nb_row_to_bind;
         ++row_iter, --nb_row_to_bind, bind_first_column_index += _nb_column) {
      (*row_iter)->fill_row(bind_first_column_index, last);
    }
    std::promise<unsigned> prom;
    to_wait.push_back(prom.get_future());
    pool.prepare_run_statement<unsigned>(
        last, std::move(prom), database::mysql_task::int_type::LAST_INSERT_ID,
        thread_id);
  }

  for (auto& fut : to_wait) {
    fut.get();
  }
  return to_wait.size();
}
