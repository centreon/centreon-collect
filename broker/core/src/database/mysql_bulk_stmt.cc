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

#include "com/centreon/broker/database/mysql_bulk_stmt.hh"

#include <cfloat>
#include <cmath>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/misc/string.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

/**
 * @brief Default constructor.
 */
mysql_bulk_stmt::mysql_bulk_stmt() : mysql_stmt_base(true), _hist_size(10) {}

/**
 * @brief Constructor of a mysql_bulk_stmt from a SQL query template. This
 * template can be named or not, i.e. respectively like UPDATE foo SET
 * a=:a_value or UPDATE foo SET a=?
 *
 * @param query The query template
 * @param named a boolean telling if the query is named or not (with ?).
 */
mysql_bulk_stmt::mysql_bulk_stmt(std::string const& query, bool named)
    : mysql_stmt_base(query, named), _hist_size(10) {}

/**
 * @brief Constructor of a mysql_bulk_stmt from a not named query template and a
 * correspondance table making the relation between column names and their
 * indices.
 *
 * @param query
 * @param bind_mapping
 */
mysql_bulk_stmt::mysql_bulk_stmt(const std::string& query,
                                 const mysql_bind_mapping& bind_mapping)
    : mysql_stmt_base(query, true, bind_mapping), _hist_size(10) {}

/**
 * @brief Create a bind compatible with this mysql_bulk_stmt. It is then
 * possible to work with it and once finished apply it to the statement for the
 * execution.
 *
 * @return An unique pointer to a mysql_bulk_bind.
 */
std::unique_ptr<mysql_bulk_bind> mysql_bulk_stmt::create_bind() {
  if (!_hist_size.empty()) {
    int avg = 0;
    for (int v : _hist_size) {
      avg += v;
    }
    _reserved_size = avg / _hist_size.size() + 1;
  }
  log_v2::sql()->trace("new mysql bind of stmt {} reserved with {} rows",
                       get_id(), _reserved_size);
  auto retval =
      std::make_unique<mysql_bulk_bind>(get_param_count(), 0, _reserved_size);
  return retval;
}

/**
 * @brief Apply a mysql_bulk_bind to the statement.
 *
 * @param bind the bind to move into the mysql_bulk_stmt.
 */
void mysql_bulk_stmt::set_bind(std::unique_ptr<mysql_bulk_bind>&& bind) {
  _bind = std::move(bind);
}

/**
 *  Move constructor
 */
mysql_bulk_stmt::mysql_bulk_stmt(mysql_bulk_stmt&& other)
    : mysql_stmt_base(std::move(other)),
      _reserved_size(std::move(other._reserved_size)),
      _bind(std::move(other._bind)) {}

/**
 * @brief Move copy
 *
 * @param other the statement to move.
 *
 * @return a reference to the self statement.
 */
mysql_bulk_stmt& mysql_bulk_stmt::operator=(mysql_bulk_stmt&& other) {
  if (this != &other) {
    mysql_stmt_base::operator=(std::move(other));
    _reserved_size = std::move(other._reserved_size);
    _bind = std::move(other._bind);
  }
  return *this;
}

/**

/**
 * @brief Return an unique pointer to the bind contained inside the statement.
 * Since the bind is in an unique pointer, it is removed from the statement when
 * returned.
 *
 * @return A std::unique_ptr<mysql_bulk_bind>
 */
std::unique_ptr<mysql_bulk_bind> mysql_bulk_stmt::get_bind() {
  if (_bind) {
    log_v2::sql()->trace("mysql bind of stmt {} returned with {} rows",
                         get_id(), _bind->rows_count());
    _hist_size.push_back(_bind->rows_count());
  }
  return std::move(_bind);
}

void mysql_bulk_stmt::bind_value_as_f64(size_t range, double value) {
  if (!_bind)
    _bind = std::make_unique<database::mysql_bulk_bind>(get_param_count(), 0,
                                                        _reserved_size);
  if (std::isinf(value) || std::isnan(value))
    _bind->set_null_f64(range);
  else
    _bind->set_value_as_f64(range, value);
}

void mysql_bulk_stmt::bind_null_f64(size_t range) {
  if (!_bind)
    _bind = std::make_unique<database::mysql_bulk_bind>(get_param_count(), 0,
                                                        _reserved_size);
  _bind->set_null_f64(range);
}

void mysql_bulk_stmt::bind_value_as_f32(size_t range, float value) {
  if (!_bind)
    _bind = std::make_unique<database::mysql_bulk_bind>(get_param_count(), 0,
                                                        _reserved_size);
  if (std::isinf(value) || std::isnan(value))
    _bind->set_null_f32(range);
  else
    _bind->set_value_as_f32(range, value);
}

void mysql_bulk_stmt::bind_null_f32(size_t range) {
  if (!_bind)
    _bind = std::make_unique<database::mysql_bulk_bind>(get_param_count(), 0,
                                                        _reserved_size);
  _bind->set_null_f32(range);
}

#define BIND_VALUE(ftype, vtype)                                              \
  void mysql_bulk_stmt::bind_value_as_##ftype(size_t range, vtype value) {    \
    if (!_bind)                                                               \
      _bind = std::make_unique<database::mysql_bulk_bind>(get_param_count(),  \
                                                          0, _reserved_size); \
    _bind->set_value_as_##ftype(range, value);                                \
  }                                                                           \
                                                                              \
  void mysql_bulk_stmt::bind_null_##ftype(size_t range) {                     \
    if (!_bind)                                                               \
      _bind = std::make_unique<database::mysql_bulk_bind>(get_param_count(),  \
                                                          0, _reserved_size); \
    _bind->set_null_##ftype(range);                                           \
  }

BIND_VALUE(i32, int32_t)
BIND_VALUE(u32, uint32_t)
BIND_VALUE(i64, int64_t)
BIND_VALUE(u64, uint64_t)
BIND_VALUE(tiny, char)
BIND_VALUE(bool, bool)
BIND_VALUE(str, const fmt::string_view&)

#undef BIND_VALUE
