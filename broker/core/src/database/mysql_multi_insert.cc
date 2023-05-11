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
    statements.emplace_back(query);

    database::mysql_stmt& last = *statements.rbegin();
    stmt_binder binder(last);
    for (; row_iter != _rows.end() && nb_row_to_bind;
         ++row_iter, --nb_row_to_bind) {
      (*row_iter)->fill_row(binder);
      binder.inc_stmt_first_column(_nb_column);
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

/**
 * @brief bulk constructor to use when bulk queries are available
 *
 * @param connexion
 * @param request
 * @param bulk_row
 */
bulk_or_multi::bulk_or_multi(mysql& connexion,
                             const std::string& request,
                             unsigned bulk_row)
    : _bulk_stmt(std::make_unique<mysql_bulk_stmt>(request)),
      _bulk_bind(_bulk_stmt->create_bind()),
      _bulk_row(bulk_row) {
  connexion.prepare_statement(*_bulk_stmt);
  _bulk_bind->reserve(_bulk_row);
}

/**
 * @brief multi_insert constructor to use when bulk queries aren't available
 *
 * @param query
 * @param nb_column
 * @param on_duplicate_key_part
 */
bulk_or_multi::bulk_or_multi(const std::string& query,
                             unsigned nb_column,
                             const std::string& on_duplicate_key_part)
    : _mult_insert(
          std::make_unique<mysql_multi_insert>(query,
                                               nb_column,
                                               on_duplicate_key_part)) {}

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
    _mult_insert->push_stmt(connexion);
    _mult_insert =
        std::make_unique<database::mysql_multi_insert>(*_mult_insert);
  }
}

/**
 * @brief should not be used only override ones
 *
 * @param event
 */
void bulk_or_multi::add_event(const std::shared_ptr<io::data>& event) {
  assert(false);
}
