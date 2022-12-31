/*
** Copyright 2018, 2021 Centreon
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

#ifndef CCB_MYSQL_BIND_HH
#define CCB_MYSQL_BIND_HH

#include "com/centreon/broker/database/mysql_column.hh"

CCB_BEGIN()

// Forward declarations
class mysql;

namespace database {
class mysql_bind {
  bool _prepared(size_t range) const;
  void _prepare_type(size_t range, enum enum_field_types type);

  std::vector<MYSQL_BIND> _bind;

  size_t _current_row = 0u;
  // The buffers contained by _bind
  std::vector<database::mysql_column> _column;

  // A vector telling if bindings are already typed or not.
  std::vector<bool> _typed;

 public:
  mysql_bind();
  mysql_bind(int size, int length = 0, size_t row_count = 1);
  ~mysql_bind() noexcept = default;
  void set_size(int size);

  int value_as_i32(size_t range) const;
  void set_value_as_i32(size_t range, int value);
  void set_null_i32(size_t range);

  uint32_t value_as_u32(size_t range) const;
  void set_value_as_u32(size_t range, uint32_t value);
  void set_null_u32(size_t range);

  int64_t value_as_i64(size_t range) const;
  void set_value_as_i64(size_t range, int64_t value);
  void set_null_i64(size_t range);

  uint64_t value_as_u64(size_t range) const;
  void set_value_as_u64(size_t range, uint64_t value);
  void set_null_u64(size_t range);

  bool value_as_bool(size_t range) const;
  void set_value_as_bool(size_t range, bool value);
  void set_null_bool(size_t range);

  float value_as_f32(size_t range) const;
  void set_value_as_f32(size_t range, float value);
  void set_null_f32(size_t range);

  double value_as_f64(size_t range) const;
  void set_value_as_f64(size_t range, double value);
  void set_null_f64(size_t range);

  const char* value_as_str(size_t range) const;
  void set_value_as_str(size_t range, const fmt::string_view& value);
  void set_null_str(size_t range);

  char value_as_tiny(size_t range) const;
  void set_value_as_tiny(size_t range, char value);
  void set_null_tiny(size_t range);

  int get_size() const;
  bool value_is_null(size_t range) const;
  bool empty() const;
  void set_empty();
  int get_rows_count() const;

  const MYSQL_BIND* get_bind() const;
  MYSQL_BIND* get_bind();
  size_t current_row() const;
  void next_row();
  void reserve(size_t size);
  size_t row_count() const;

  void debug();
};

}  // namespace database

CCB_END()

#endif  // CCB_MYSQL_BIND_HH
