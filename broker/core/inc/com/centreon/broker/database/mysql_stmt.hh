/*
** Copyright 2018 Centreon
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

#ifndef CCB_MYSQL_STMT_HH
#define CCB_MYSQL_STMT_HH

#include <boost/circular_buffer.hpp>
#include "com/centreon/broker/database/mysql_bind.hh"
#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/log_v2.hh"

CCB_BEGIN()

typedef absl::flat_hash_map<std::string, int> mysql_bind_mapping;

namespace database {
class mysql_stmt {
  size_t _compute_param_count(const std::string& query);

  uint32_t _id = 0u;
  size_t _param_count = 0;
  std::string _query;
  size_t _reserved_size = 0u;

  std::unique_ptr<database::mysql_bind> _bind;
  mysql_bind_mapping _bind_mapping;

  /* This vector represents how we map bbdo protobuf items into the DB table.
   * Each index in the vector corresponds to the index in the protobuf object.
   * And of each item in the vector we keep its name, its length (when it is
   * a string or 0), its attributes (always_valid, invalid_on_zero,
   * invalid_on_minus_one)
   */
  std::vector<std::tuple<std::string, uint32_t, uint16_t>> _pb_mapping;

  boost::circular_buffer<size_t> _hist_size;

 public:
  mysql_stmt();
  mysql_stmt(const std::string& query, bool named);
  mysql_stmt(const std::string& query,
             mysql_bind_mapping const& bind_mapping = mysql_bind_mapping());
  mysql_stmt(mysql_stmt&& other);
  mysql_stmt& operator=(const mysql_stmt&) = delete;
  mysql_stmt& operator=(mysql_stmt&& other);
  bool prepared() const;
  uint32_t get_id() const;
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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_i32(const std::string& key, int value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_i32(size_t range);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_i32(const std::string& key);

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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_u32(const std::string& key, uint32_t value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_u32(size_t range);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONG.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_u32(const std::string& key);

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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_i64(const std::string& key, int64_t value);
  template <typename not_null_predicate>
  void bind_value_as_i64(size_t range,
                         int64_t value,
                         const not_null_predicate& pred) {
    if (pred(value))
      bind_value_as_i64(range, value);
    else
      bind_null_i64(range);
  }
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_i64(size_t range);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_i64(const std::string& key);

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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_u64(const std::string& key, uint64_t value);
  template <typename not_null_predicate>
  void bind_value_as_u64(size_t range,
                         uint64_t value,
                         const not_null_predicate& pred) {
    if (pred(value))
      bind_value_as_u64(range, value);
    else
      bind_null_u64(range);
  }
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_u64(size_t range);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_LONGLONG.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_u64(const std::string& key);

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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_f32(const std::string& key, float value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_f32(size_t range);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_f32(const std::string& key);

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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_DOUBLE.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_f64(const std::string& key, double value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_DOUBLE.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_f64(size_t range);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_FLOAT.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_f64(const std::string& range);

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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_tiny(const std::string& key, char value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_tiny(size_t range);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_tiny(const std::string& key);

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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_bool(const std::string& key, bool value);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_bool(size_t range);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_bool(const std::string& key);

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
   * @brief Set the given value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param key the key of the parameter in the prepared statement.
   * @param value The value to set.
   */
  void bind_value_as_str(const std::string& key, const fmt::string_view& value);
  /**
   * @brief Set the NULL value at the column named key in the prepared
   * statement at the current row. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param key the key of the parameter in the prepared statement.
   */
  void bind_null_str(const std::string& key);
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_STRING.
   *
   * @param range Index of the column(from 0).
   */
  void bind_null_str(size_t range);

  const std::string& get_query() const;
  size_t get_param_count() const;
  void set_pb_mapping(
      std::vector<std::tuple<std::string, uint32_t, uint16_t>>&& mapping);
  std::unique_ptr<mysql_bind> create_bind();
  void set_bind(std::unique_ptr<mysql_bind>&& bind);
};

}  // namespace database

CCB_END()

#endif  // CCB_MYSQL_STMT_HH
