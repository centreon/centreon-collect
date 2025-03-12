/**
 * Copyright 2018-2023 Centreon
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

#ifndef CCB_MYSQL_STMT_HH
#define CCB_MYSQL_STMT_HH

#include "com/centreon/broker/sql/mysql_stmt_base.hh"

namespace com::centreon::broker {

namespace database {
class mysql_stmt : public mysql_stmt_base {
  std::unique_ptr<database::mysql_bind> _bind;

 public:
  /**
   * @brief Default constructor.
   */
  mysql_stmt(const std::shared_ptr<spdlog::logger>& logger);
  mysql_stmt(const std::string& query,
             bool named,
             const std::shared_ptr<spdlog::logger>& logger);
  mysql_stmt(const std::string& query,
             const std::shared_ptr<spdlog::logger>& logger,
             mysql_bind_mapping const& bind_mapping = mysql_bind_mapping());
  mysql_stmt(mysql_stmt&& other);
  mysql_stmt& operator=(const mysql_stmt&) = delete;
  mysql_stmt& operator=(mysql_stmt&& other);
  std::unique_ptr<database::mysql_bind> get_bind();
  void operator<<(io::data const& d);

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

  std::unique_ptr<mysql_bind> create_bind();
  void set_bind(std::unique_ptr<mysql_bind>&& bind);
};

}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_STMT_HH
