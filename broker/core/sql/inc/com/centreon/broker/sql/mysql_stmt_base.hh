/**
 * Copyright 2023-2024 Centreon
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

#ifndef CCB_MYSQL_STMT_BASE_HH
#define CCB_MYSQL_STMT_BASE_HH

#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/sql/mysql_bind.hh"

namespace com::centreon::broker {

typedef absl::flat_hash_map<std::string, int> mysql_bind_mapping;

namespace database {
class mysql_stmt_base {
  const bool _bulk;
  uint32_t _id = 0u;
  size_t _param_count = 0u;
  std::string _query;

  mysql_bind_mapping _bind_mapping;

  /* This vector represents how we map bbdo protobuf items into the DB table.
   * Each index in the vector corresponds to the index in the protobuf object.
   * And of each item in the vector we keep its name, its length (when it is
   * a string or 0), its attributes (always_valid, invalid_on_zero,
   * invalid_on_minus_one)
   */
  std::vector<std::tuple<std::string, uint32_t, uint16_t>> _pb_mapping;

 protected:
  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

 private:
  size_t _compute_param_count(const std::string& query);

 public:
  mysql_stmt_base(bool bulk, const std::shared_ptr<spdlog::logger>& logger);
  mysql_stmt_base(const std::string& query,
                  bool named,
                  bool bulk,
                  const std::shared_ptr<spdlog::logger>& logger);
  mysql_stmt_base(
      const std::string& query,
      bool bulk,
      const std::shared_ptr<spdlog::logger>& logger,
      mysql_bind_mapping const& bind_mapping = mysql_bind_mapping());
  mysql_stmt_base(mysql_stmt_base&& other);
  virtual ~mysql_stmt_base() noexcept = default;
  mysql_stmt_base& operator=(const mysql_stmt_base&) = delete;
  mysql_stmt_base& operator=(mysql_stmt_base&& other);
  bool prepared() const;
  uint32_t get_id() const;
  constexpr bool in_bulk() const { return _bulk; }

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_i32(size_t range, int value) = 0;
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_i32_k(const std::string& key, int value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_i32(size_t range) = 0;
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_i32_k(const std::string& key);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_u32(size_t range, uint32_t value) = 0;
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_u32_k(const std::string& key, uint32_t value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_u32(size_t range) = 0;
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_u32_k(const std::string& key);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_i64(size_t range, int64_t value) = 0;
  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG. The value must not match the invalid_on bitfield,
   * otherwise the value is set to NULL.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   * @param invalid_on A bit field with values mapping::entry::invalid_on_zero,
   * mapping::entry::invalid_minus_one or mapping::entry::invalid_on_negative.
   */
  void bind_value_as_i64_ext(size_t range, int64_t value, uint32_t invalid_on);
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_i64_k(const std::string& key, int64_t value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_i64(size_t range) = 0;
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_i64_k(const std::string& key);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_u64(size_t range, uint64_t value) = 0;
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_u64_k(const std::string& key, uint64_t value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_u64(size_t range) = 0;
  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   * @param invalid_on A bit field with values mapping::entry::invalid_on_zero,
   * mapping::entry::invalid_minus_one or mapping::entry::invalid_on_negative.
   */
  void bind_value_as_u64_ext(size_t range, uint64_t value, uint32_t invalid_on);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_u64_k(const std::string& key);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_f32(size_t range, float value) = 0;
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_f32_k(const std::string& key, float value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_f32(size_t range) = 0;
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_f32_k(const std::string& key);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_DOUBLE.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_f64(size_t range, double value) = 0;
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_DOUBLE.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_f64_k(const std::string& key, double value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_DOUBLE.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_f64(size_t range) = 0;
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_f64_k(const std::string& range);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_tiny(size_t range, char value) = 0;
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_tiny_k(const std::string& key, char value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_tiny(size_t range) = 0;
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_tiny_k(const std::string& key);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_bool(size_t range, bool value) = 0;
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_bool_k(const std::string& key, bool value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_bool(size_t range) = 0;
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_bool_k(const std::string& key);

  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  virtual void bind_value_as_str(size_t range,
                                 const fmt::string_view& value) = 0;
  /**
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_str_k(const std::string& key,
                           const fmt::string_view& value);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_str_k(const std::string& key);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param range Index of the column(from 0).
   */
  virtual void bind_null_str(size_t range) = 0;

  const std::string& get_query() const;
  size_t get_param_count() const;
  void set_pb_mapping(
      std::vector<std::tuple<std::string, uint32_t, uint16_t>>&& mapping);
  const std::vector<std::tuple<std::string, uint32_t, uint16_t>>&
  get_pb_mapping() const;
};

}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_STMT_BASE_HH
