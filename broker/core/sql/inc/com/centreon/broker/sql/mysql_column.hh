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

#ifndef CCB_DATABASE_MYSQL_COLUMN_HH
#define CCB_DATABASE_MYSQL_COLUMN_HH

#include <mysql.h>

#include <cmath>

namespace com::centreon::broker {

namespace database {
class mysql_column {
  int _type = MYSQL_TYPE_NULL;
  size_t _rows_to_reserve = 0;
  size_t _row_count = 0;
  int32_t _current_row = 0;
  /** A vector with data of type _type
   * Its content corresponds to a database table column.
   */
  void* _vector = nullptr;

  /** A vector of indicators.
   * Indicators are used in prepared statements. See the MariadDB C connector
   * documentation for more information.
   */
  std::vector<char> _indicator;
  /** A vector to specify if the row contains errors.
   * This vector is used when sending prepared statement.
   */
  std::vector<my_bool> _error;
  /** A vector containing lengths.
   * It gives the length in characters of each item of the column. It is useful
   * when items are strings, otherwise we just have 0.
   */
  std::vector<unsigned long> _length;

  void _free_vector();
  void _push_value_i32(int32_t value);
  void _push_null_i32();
  void _push_value_u32(uint32_t value);
  void _push_null_u32();
  void _push_value_i64(int64_t value);
  void _push_null_i64();
  void _push_value_u64(uint64_t value);
  void _push_null_u64();
  void _push_value_f32(float value);
  void _push_null_f32();
  void _push_value_f64(double value);
  void _push_null_f64();
  void _push_value_tiny(char value);
  void _push_null_tiny();
  void _push_value_bool(bool value);
  void _push_null_bool();
  void _push_value_str(const fmt::string_view& str);
  void _push_null_str();

 public:
  mysql_column() = default;
  mysql_column(mysql_column&& other) = delete;
  mysql_column& operator=(mysql_column&& other) = delete;
  ~mysql_column() noexcept;
  int get_type() const;
  void* get_buffer();
  void set_type(int type);
  void clear();
  void reserve(size_t s);

  /**
   * @brief Set the value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_LONG. The greatest row value is the current size of the column,
   * this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   * @param value The content to set into the column.
   */
  void set_value_i32(size_t row, int32_t value);
  /**
   * @brief Set the null value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_LONG. The greatest row value is the current size of the column,
   * this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   */
  void set_null_i32(size_t row);

  /**
   * @brief Set the value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * unsigned MYSQL_TYPE_LONG. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   * @param value The content to set into the column.
   */
  void set_value_u32(size_t row, uint32_t value);
  /**
   * @brief Set the null value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is unsigned
   * MYSQL_TYPE_LONG. The greatest row value is the current size of the column,
   * this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   */
  void set_null_u32(size_t row);

  /**
   * @brief Set the value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_LONGLONG. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   * @param value The content to set into the column.
   */
  void set_value_i64(size_t row, int64_t value);
  /**
   * @brief Set the null value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_LONGLONG. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   */
  void set_null_i64(size_t row);

  /**
   * @brief Set the value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is unsigned
   * MYSQL_TYPE_LONGLONG. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   * @param value The content to set into the column.
   */
  void set_value_u64(size_t row, uint64_t value);
  /**
   * @brief Set the null value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is unsigned
   * MYSQL_TYPE_LONGLONG. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   */
  void set_null_u64(size_t row);

  /**
   * @brief Set the value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_FLOAT. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   * @param value The content to set into the column.
   */
  void set_value_f32(size_t row, float value);
  /**
   * @brief Set the null value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_FLOAT. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   */
  void set_null_f32(size_t row);

  /**
   * @brief Set the value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_DOUBLE. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   * @param value The content to set into the column.
   */
  void set_value_f64(size_t row, double value);
  /**
   * @brief Set the null value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_DOUBLE. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   */
  void set_null_f64(size_t row);

  /**
   * @brief Set the value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_TINY. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   * @param value The content to set into the column.
   */
  void set_value_tiny(size_t row, char value);
  /**
   * @brief Set the null value into the column at the row specified (we start at
   * index 0). This method can only be called when the content type is
   * MYSQL_TYPE_TINY. The greatest row value is the current size of the
   * column, this allows to increment its size by 1.
   *
   * @param row The row concerned by the insertion.
   */
  void set_null_tiny(size_t row);

  void set_value_bool(size_t row, bool value);
  void set_null_bool(size_t row);

  void set_value_str(size_t row, const fmt::string_view& str);
  void set_null_str(size_t row);

  char* indicator_buffer();
  bool is_null() const;
  my_bool* error_buffer();
  unsigned long* length_buffer();
  uint32_t array_size() const;
};

}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_DATABASE_MYSQL_COLUMN_HH
