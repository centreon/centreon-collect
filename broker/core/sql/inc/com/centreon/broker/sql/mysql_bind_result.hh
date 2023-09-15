/*
** Copyright 2023 Centreon
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

#ifndef CCB_MYSQL_BIND_RESULT_HH
#define CCB_MYSQL_BIND_RESULT_HH

#include <absl/types/variant.h>
#include "com/centreon/broker/sql/mysql_bind_base.hh"
#include "com/centreon/broker/sql/mysql_column.hh"

namespace com::centreon::broker {

// Forward declarations
class mysql;

namespace database {
class mysql_bind_result : public mysql_bind_base {
  // Each data in the bind.
  int _length;
  std::vector<my_bool> _is_null;
  std::vector<char> _buffer;

 public:
  /**
   * @brief Default constructor
   */
  mysql_bind_result() = delete;
  /**
   * @brief Constructor
   *
   * @param size Number of columns in this bind
   * @param length Size to reserve for each column's buffer. This is useful when
   *               the column contains strings. By default, this value is 0 and
   *               no reservation is made.
   * @param logger The logger to use by this object.
   */
  mysql_bind_result(int size,
                    int length,
                    const std::shared_ptr<spdlog::logger>& logger);
  ~mysql_bind_result() noexcept = default;

  /**
   * @brief getter to the int32 value at index range. The type of the column
   * must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   *
   * @return An int32 integer.
   */
  int value_as_i32(size_t range) const;
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   * @param value The integer value to set.
   */
  // void set_value_as_i32(size_t range, int32_t value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   */
  // void set_null_i32(size_t range);

  /**
   * @brief getter to the uint32 value at index range. The type of the column
   * must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   *
   * @return An uint32 integer.
   */
  uint32_t value_as_u32(size_t range) const;
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   * @param value The unsigned integer value to set.
   */
  // void set_value_as_u32(size_t range, uint32_t value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   */
  // void set_null_u32(size_t range);

  /**
   * @brief getter to the int64 value at index range. The type of the column
   * must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   *
   * @return An int64 integer.
   */
  int64_t value_as_i64(size_t range) const;
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   * @param value The long integer value to set.
   */
  // void set_value_as_i64(size_t range, int64_t value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   */
  // void set_null_i64(size_t range);

  /**
   * @brief getter to the uint64 value at index range. The type of the column
   * must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   *
   * @return An uint64 integer.
   */
  uint64_t value_as_u64(size_t range) const;
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   * @param value The unsigned long integer value to set.
   */
  // void set_value_as_u64(size_t range, uint64_t value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   */
  // void set_null_u64(size_t range);

  /**
   * @brief getter to the bool value at index range. The type of the column
   * must be MYSQL_TYPE_TINY.
   *
   * @param range A non negative integer.
   *
   * @return A boolean.
   */
  bool value_as_bool(size_t range) const;
  //  /**
  //   * @brief Setter of the value at the column at index range and at the
  //   current
  //   * row. The type of the column must be MYSQL_TYPE_TINY.
  //   *
  //   * @param range A non negative integer.
  //   * @param value The boolean value to set.
  //   */
  //  void set_value_as_bool(size_t range, bool value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_TINY.
   *
   * @param range A non negative integer.
   */
  // void set_null_bool(size_t range);

  /**
   * @brief getter to the float value at index range. The type of the column
   * must be MYSQL_TYPE_FLOAT.
   *
   * @param range A non negative integer.
   *
   * @return A float.
   */
  float value_as_f32(size_t range) const;
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_FLOAT.
   *
   * @param range A non negative integer.
   * @param value The float value to set.
   */
  // void set_value_as_f32(size_t range, float value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_FLOAT.
   *
   * @param range A non negative integer.
   */
  // void set_null_f32(size_t range);

  /**
   * @brief getter to the double value at index range. The type of the column
   * must be MYSQL_TYPE_DOUBLE.
   *
   * @param range A non negative integer.
   *
   * @return A double.
   */
  double value_as_f64(size_t range) const;
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_DOUBLE.
   *
   * @param range A non negative integer.
   * @param value The double value to set.
   */
  // void set_value_as_f64(size_t range, double value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_DOUBLE.
   *
   * @param range A non negative integer.
   */
  // void set_null_f64(size_t range);

  /**
   * @brief getter to the string value at index range. The type of the column
   * must be MYSQL_TYPE_STRING.
   *
   * @param range A non negative integer.
   *
   * @return A const char* pointer.
   */
  const char* value_as_str(size_t range) const;
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_STRING.
   *
   * @param range A non negative integer.
   * @param value The string to set.
   */
  // void set_value_as_str(size_t range, const fmt::string_view& value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_STRING.
   *
   * @param range A non negative integer.
   */
  // void set_null_str(size_t range);

  /**
   * @brief getter to the char value at index range. The type of the column
   * must be MYSQL_TYPE_TINY.
   *
   * @param range A non negative integer.
   *
   * @return A char.
   */
  char value_as_tiny(size_t range) const;
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_TINY.
   *
   * @param range A non negative integer.
   * @param value The char to set.
   */
  // void set_value_as_tiny(size_t range, char value);
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_TINY.
   *
   * @param range A non negative integer.
   */
  // void set_null_tiny(size_t range);

  bool value_is_null(size_t range) const;
  size_t rows_count() const;

  size_t current_row() const;
  void next_row();
  void reserve(size_t size);
};

}  // namespace database

}

#endif  // CCB_MYSQL_BIND_RESULT_HH
