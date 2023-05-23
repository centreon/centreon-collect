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

unsigned mysql_multi_insert::push_stmt(mysql& pool, int thread_id) const {
  std::string query;
  query.reserve(max_query_total_length);

  query = _query;
  query.push_back(' ');

  char sep = ' ';
  unsigned nb_row_in_query = 0;

  unsigned nb_query = 0;
  for (auto row_iter = _rows.begin(); row_iter != _rows.end(); ++row_iter) {
    if (query.length() >= max_query_total_length) {
      query += ' ' + _on_duplicate_key_part;
      pool.run_query(query, my_error::empty, thread_id);
      query = _query;
      sep = ' ';
      nb_row_in_query = 0;
      ++nb_query;
    }

    query += sep;
    query += '(';
    (*row_iter)->fill_row(query);
    query += ')';
    sep = ',';
    ++nb_row_in_query;
  }

  // some remained data to push?
  if (query.length() > _query.length() + 1) {
    query += ' ' + _on_duplicate_key_part;
    pool.run_query(query, my_error::empty, thread_id);
    ++nb_query;
  }

  return nb_query;
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
                             const std::string& on_duplicate_key_part)
    : _mult_insert(
          std::make_unique<mysql_multi_insert>(query, on_duplicate_key_part)) {}

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
    _mult_insert->clear_rows();
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
