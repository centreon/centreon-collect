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

#ifndef CCB_MYSQL_MULTI_INSERT_HH
#define CCB_MYSQL_MULTI_INSERT_HH

#include "com/centreon/broker/database/mysql_stmt.hh"

CCB_BEGIN()

namespace database {

/**
 * @brief this class only provide a similar interface to bulk_bind
 * it's used by row_filler class described above
 *
 */
class stmt_binder {
  unsigned _stmt_first_column;
  mysql_stmt& _to_bind;

 public:
  stmt_binder(unsigned stmt_first_column, mysql_stmt& to_bind)
      : _stmt_first_column(stmt_first_column), _to_bind(to_bind) {}

  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_DOUBLE.
   *
   * @param range A non negative integer.
   * @param value The double value to set.
   */
  void set_value_as_f64(size_t range, double value) {
    _to_bind.bind_value_as_f64(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   * @param value The integer value to set.
   */
  void set_value_as_i32(size_t range, int32_t value) {
    _to_bind.bind_value_as_i32(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   * @param value The unsigned integer value to set.
   */
  void set_value_as_u32(size_t range, uint32_t value) {
    _to_bind.bind_value_as_u32(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG. The value must not
   * match the invalid_on bitfield, otherwise the value is set to NULL.
   *
   * @param range A non negative integer.
   * @param value The unsigned long integer value to set.
   */
  void set_value_as_u64(size_t range, int64_t value) {
    _to_bind.bind_value_as_u64(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   * @param value The long integer value to set.
   */
  void set_value_as_i64(size_t range, int64_t value) {
    _to_bind.bind_value_as_i64(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   */
  void set_null_u64(size_t range) { _to_bind.bind_null_u64(range); }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_TINY.
   *
   * @param range A non negative integer.
   * @param value The boolean value to set.
   */
  void set_value_as_bool(size_t range, bool value) {
    _to_bind.bind_value_as_bool(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_STRING.
   *
   * @param range A non negative integer.
   * @param value The string to set.
   */
  void set_value_as_str(size_t range, const fmt::string_view& value) {
    _to_bind.bind_value_as_str(range + _stmt_first_column, value);
  }
  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void set_value_as_tiny(size_t range, char value) {
    _to_bind.bind_value_as_tiny(range + _stmt_first_column, value);
  }
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   */
  void set_null_tiny(size_t range) {
    _to_bind.bind_null_tiny(range + _stmt_first_column);
  }
};

/**
 * @brief this class used as a data provider is the data provider of
 * multi_insert_class.
 * It embeds two things: data and code to bind data to the
 * statement example of implementation:
 * @code {.c++}
 *   struct row {
 *    using pointer = std::shared_ptr<row>;
 *    std::string name;
 *    double value;
 *  };
 *
 *  struct row_filler : public database::row_filler {
 *    row::pointer data;
 *    row_filler(const row::pointer& dt) : data(dt) {}
 *
 *    inline void fill_row(stmt_binder& to_bind) const override {
 *      to_bind.set_value_as_str(0, data->name);
 *      to_bind.set_value_as_f64(1, data->value);
 *    }
 *  };
 * @endcode
 *
 *
 */
class row_filler {
 public:
  using pointer = std::unique_ptr<row_filler>;
  virtual ~row_filler() {}
  virtual void fill_row(stmt_binder&& to_bind) const = 0;
};

/**
 * @brief this class emulates a bulk insert by create as many multi insert
 * statements as necessary (INSERT INTO toto (col1, col2,..) VALUES (col0row0,
 * col1row0...) (col0row1, col1row1,...) )
 *
 */
class mysql_multi_insert {
  const std::string _query;
  const unsigned _nb_column;
  const std::string _on_duplicate_key_part;

  std::list<row_filler::pointer> _rows;

 public:
  mysql_multi_insert(const std::string& query,
                     unsigned nb_column,
                     const std::string& on_duplicate_key_part)
      : _query(query),
        _nb_column(nb_column),
        _on_duplicate_key_part(on_duplicate_key_part) {}

  mysql_multi_insert(const mysql_multi_insert&) = delete;
  mysql_multi_insert& operator=(const mysql_multi_insert&) = delete;

  /**
   * @brief push data into the request
   * data will be bound to query during push_request call
   *
   * @param data data that will be bound
   */
  inline void push(row_filler::pointer&& data) {
    _rows.emplace_back(std::move(data));
  }

  unsigned push_stmt(mysql& pool, int thread_id = -1) const;

  size_t rows_count() const { return _rows.size(); }
};

}  // namespace database

CCB_END()

#endif  // CCB_MYSQL_MULTI_INSERT_HH
