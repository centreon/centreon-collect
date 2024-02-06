/**
* Copyright 2018, 2021-2023 Centreon
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

#include "com/centreon/broker/sql/mysql_bulk_bind.hh"

#include <cfloat>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/sql/mysql.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

mysql_bulk_bind::mysql_bulk_bind(int size, size_t reserved_rows_count)
    : mysql_bind_base(size), _column(size) {
  for (auto& c : _column)
    c.reserve(reserved_rows_count);
}

/**
 * @brief Prepare the column at index range by setting the given type.
 *
 * @param range
 * @param type
 */
void mysql_bulk_bind::_prepare_type(size_t range, enum enum_field_types type) {
  _set_typed(range);
  _bind[range].buffer_type = type;
  _column[range].set_type(type);
}

void mysql_bulk_bind::set_value_as_u64(size_t range,
                                       int64_t value,
                                       uint32_t invalid_on) {
  if (value == 0 && (invalid_on & mapping::entry::invalid_on_zero))
    set_null_u64(range);
  else
    set_value_as_u64(range, value);
}

void mysql_bulk_bind::set_value_as_i64(size_t range,
                                       int64_t value,
                                       uint32_t invalid_on) {
  if (invalid_on & mapping::entry::invalid_on_zero) {
    if (value == 0) {
      set_null_i64(range);
      return;
    }
  }
  if (invalid_on & mapping::entry::invalid_on_minus_one) {
    if (value == -1) {
      set_null_i64(range);
      return;
    }
  }
  if (invalid_on & mapping::entry::invalid_on_negative) {
    if (value < 0) {
      set_null_i64(range);
      return;
    }
  }
  set_value_as_i64(range, value);
}

#define SET_VALUE(ftype, vtype, sqltype, unsgn)                           \
  void mysql_bulk_bind::set_value_as_##ftype(size_t range, vtype value) { \
    assert(range < _bind.size());                                         \
    if (!_prepared(range)) {                                              \
      _prepare_type(range, sqltype);                                      \
      _bind[range].is_unsigned = unsgn;                                   \
    }                                                                     \
    _column[range].set_value_##ftype(_current_row, value);                \
    _bind[range].buffer = _column[range].get_buffer();                    \
    _bind[range].u.indicator = _column[range].indicator_buffer();         \
    _bind[range].length = _column[range].length_buffer();                 \
  }                                                                       \
                                                                          \
  void mysql_bulk_bind::set_null_##ftype(size_t range) {                  \
    assert(range < _bind.size());                                         \
    if (!_prepared(range)) {                                              \
      _prepare_type(range, sqltype);                                      \
      _bind[range].is_unsigned = unsgn;                                   \
    }                                                                     \
    _column[range].set_null_##ftype(_current_row);                        \
    _bind[range].buffer = _column[range].get_buffer();                    \
    _bind[range].u.indicator = _column[range].indicator_buffer();         \
    _bind[range].length = _column[range].length_buffer();                 \
  }

#define VALUE(ftype, vtype, sqltype)                                        \
  vtype mysql_bulk_bind::value_as_##ftype(size_t range) const {             \
    if (_bind[range].buffer_type == sqltype)                                \
      return *static_cast<vtype*>(_bind[range].buffer);                     \
    else {                                                                  \
      assert("This field is not an " #sqltype == nullptr);                  \
      SPDLOG_LOGGER_CRITICAL(log_v2::sql(),                                 \
                             "{} This field is not an " #sqltype " but {}", \
                             __FUNCTION__, _bind[range].buffer_type);       \
      return 0;                                                             \
    }                                                                       \
  }

SET_VALUE(i32, int32_t, MYSQL_TYPE_LONG, false)
VALUE(i32, int32_t, MYSQL_TYPE_LONG)
SET_VALUE(u32, uint32_t, MYSQL_TYPE_LONG, true)
VALUE(u32, uint32_t, MYSQL_TYPE_LONG)
SET_VALUE(i64, int64_t, MYSQL_TYPE_LONGLONG, false)
VALUE(i64, int64_t, MYSQL_TYPE_LONGLONG)
SET_VALUE(u64, uint64_t, MYSQL_TYPE_LONGLONG, true)
VALUE(u64, uint64_t, MYSQL_TYPE_LONGLONG)
SET_VALUE(f32, float, MYSQL_TYPE_FLOAT, false)
VALUE(f32, float, MYSQL_TYPE_FLOAT)
SET_VALUE(f64, double, MYSQL_TYPE_DOUBLE, false)
VALUE(f64, double, MYSQL_TYPE_DOUBLE)
SET_VALUE(str, const fmt::string_view&, MYSQL_TYPE_STRING, false)
VALUE(str, const char*, MYSQL_TYPE_STRING)
SET_VALUE(tiny, char, MYSQL_TYPE_TINY, false)
VALUE(tiny, char, MYSQL_TYPE_TINY)
SET_VALUE(bool, bool, MYSQL_TYPE_TINY, false)
VALUE(bool, bool, MYSQL_TYPE_TINY)

#undef SET_VALUE
#undef VALUE

/**
 * @brief Return if the value at index range is NULL or not.
 *
 * @param range A non negative integer.
 *
 * @return A boolean True when the value is NULL.
 */
bool mysql_bulk_bind::value_is_null(size_t range) const {
  return _column[range].is_null() ||
         _bind[range].buffer_type == MYSQL_TYPE_NULL;
}

/**
 * @brief Return True if the bind contains no row, otherwise return False.
 *
 * @return A boolean.
 */
bool mysql_bulk_bind::empty() const {
  return _column[0].array_size() == 0;
}

/**
 * @brief Accessor to the number of rows in this bind.
 *
 * @return an integer.
 */
size_t mysql_bulk_bind::rows_count() const {
  return _column[0].array_size();
}

/**
 * @brief Accessor to the current row index.
 *
 * @return An integer.
 */
size_t mysql_bulk_bind::current_row() const {
  return _current_row;
}

/**
 * @brief Reserve size rows in this bind. The current size of the bind does
 * not change, but it will be faster to add rows if the reservation is big
 * enough.
 *
 * @param size The wanted number of rows.
 */
void mysql_bulk_bind::reserve(size_t size) {
  for (auto& c : _column)
    c.reserve(size);
}

/**
 * @brief Increment the current row index.
 */
void mysql_bulk_bind::next_row() {
  ++_current_row;
}
