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

#ifndef CCB_MYSQL_BULK_STMT_HH
#define CCB_MYSQL_BULK_STMT_HH

#include <boost/circular_buffer.hpp>
#include "com/centreon/broker/sql/mysql_bulk_bind.hh"
#include "com/centreon/broker/sql/mysql_stmt_base.hh"

namespace com::centreon::broker {

namespace database {
class mysql_bulk_stmt : public mysql_stmt_base {
  size_t _reserved_size = 0u;

  std::unique_ptr<database::mysql_bulk_bind> _bind;

  /**
   * We keep here an historical of the number of rows in the stmt. This is
   * useful when we create a new bin in the statement, it is already reserved
   * at the average size of this buffer. */
  boost::circular_buffer<size_t> _hist_size;

 public:
  mysql_bulk_stmt(
      const std::string& query,
      const std::shared_ptr<spdlog::logger>& logger,
      mysql_bind_mapping const& bind_mapping = mysql_bind_mapping());
  mysql_bulk_stmt(mysql_bulk_stmt&& other) = delete;
  mysql_bulk_stmt& operator=(const mysql_bulk_stmt&) = delete;
  mysql_bulk_stmt& operator=(mysql_bulk_stmt&&) = delete;
  std::unique_ptr<database::mysql_bulk_bind> get_bind();

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_i32(size_t range, int value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_i32(size_t range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_u32(size_t range, uint32_t value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_u32(size_t range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_i64(size_t range, int64_t value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_i64(size_t range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_u64(size_t range, uint64_t value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_u64(size_t range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_f32(size_t range, float value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_f32(size_t range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_DOUBLE.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_f64(size_t range, double value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_DOUBLE.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_f64(size_t range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_tiny(size_t range, char value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_tiny(size_t range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_bool(size_t range, bool value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_bool(size_t range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void bind_value_as_str(size_t range, const fmt::string_view& value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_str(size_t range);

  std::unique_ptr<mysql_bulk_bind> create_bind();
  void set_bind(std::unique_ptr<mysql_bulk_bind>&& bind);
};

}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_BULK_STMT_HH
