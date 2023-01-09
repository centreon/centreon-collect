/*
** Copyright 2018, 2021-2022 Centreon
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

#include "com/centreon/broker/database/mysql_bind.hh"

#include <cassert>
#include <cfloat>
#include <cmath>

#include "com/centreon/broker/database/mysql_column.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/mysql.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

mysql_bind::mysql_bind(int size, int length, size_t row_count)
    : _bind(size), _column(size), _typed(size) {
  if (length) {
    for (int i = 0; i < size; ++i) {
      _bind[i].buffer_type = MYSQL_TYPE_STRING;
      _column[i] = mysql_column(MYSQL_TYPE_STRING, row_count);
      _bind[i].buffer = _column[i].get_buffer();
      _bind[i].u.indicator = _column[i].indicator_buffer();
      _bind[i].length = _column[i].length_buffer();
      _bind[i].buffer_length = length;
      _bind[i].error = _column[i].error_buffer();
    }
  }
}

/**
 * @brief Set the size of the bind, that is to say the number of columns.
 *
 * @param size An integer.
 */
void mysql_bind::set_size(int size) {
  _bind.resize(size);
  _column.resize(size);
  for (int i = 0; i < size; ++i)
    _bind[i].buffer = _column[i].get_buffer();
}

/**
 * @brief Return a boolean telling if the column at index range has been
 * prepared or not. A column is prepared when its column has a type defined.
 *
 * @param range a non negative integer.
 *
 * @return True if the column is prepared, False otherwise.
 */
bool mysql_bind::_prepared(size_t range) const {
  return _typed[range];
}

/**
 * @brief Prepare the column at index range by setting the given type.
 *
 * @param range
 * @param type
 */
void mysql_bind::_prepare_type(size_t range, enum enum_field_types type) {
  _typed[range] = true;
  _bind[range].buffer_type = type;
  _column[range].set_type(type);
}

#define SET_VALUE(ftype, vtype, sqltype, unsgn)                      \
  void mysql_bind::set_value_as_##ftype(size_t range, vtype value) { \
    assert(range < _bind.size());                                    \
    if (!_prepared(range)) {                                         \
      _prepare_type(range, sqltype);                                 \
      _bind[range].is_unsigned = unsgn;                              \
    }                                                                \
    _column[range].set_value_##ftype(_current_row, value);           \
    _bind[range].buffer = _column[range].get_buffer();               \
    _bind[range].u.indicator = _column[range].indicator_buffer();    \
    _bind[range].length = _column[range].length_buffer();            \
  }                                                                  \
                                                                     \
  void mysql_bind::set_null_##ftype(size_t range) {                  \
    assert(range < _bind.size());                                    \
    if (!_prepared(range)) {                                         \
      _prepare_type(range, sqltype);                                 \
      _bind[range].is_unsigned = unsgn;                              \
    }                                                                \
    _column[range].set_null_##ftype(_current_row);                   \
    _bind[range].buffer = _column[range].get_buffer();               \
    _bind[range].u.indicator = _column[range].indicator_buffer();    \
    _bind[range].length = _column[range].length_buffer();            \
  }

#define VALUE(ftype, vtype, sqltype)                       \
  vtype mysql_bind::value_as_##ftype(size_t range) const { \
    if (_bind[range].buffer_type == sqltype)               \
      return *static_cast<vtype*>(_bind[range].buffer);    \
    else                                                   \
      assert("This field is not an " #sqltype == nullptr); \
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
bool mysql_bind::value_is_null(size_t range) const {
  return _column[range].is_null() ||
         _bind[range].buffer_type == MYSQL_TYPE_NULL;
}

/**
 * @brief A debug function to display the content of this bind.
 */
void mysql_bind::debug() {
  std::cout << "DEBUG BIND " << this << std::endl;
  int size = _bind.size();
  for (size_t j = 0; j < rows_count(); ++j) {
    for (int i = 0; i < size; ++i) {
      switch (_bind[i].buffer_type) {
        case MYSQL_TYPE_LONGLONG: {
          std::cout << "LONGLONG : "
                    << "BL: " << _bind[i].buffer_length << " NULL: "
                    << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
                                                                       : "0")
                    << " : "
                    << static_cast<long long*>(_column[i].get_buffer())[j]
                    << std::endl;
        } break;
        case MYSQL_TYPE_LONG: {
          std::cout << "LONG : "
                    << "BL: " << _bind[i].buffer_length << " NULL: "
                    << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
                                                                       : "0")
                    << " : " << static_cast<int*>(_column[i].get_buffer())[j]
                    << std::endl;
        } break;
        case MYSQL_TYPE_TINY: {
          std::cout << "TINY : "
                    << "BL: " << _bind[i].buffer_length << " NULL: "
                    << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
                                                                       : "0")
                    << " : " << static_cast<char*>(_column[i].get_buffer())[j]
                    << std::endl;
        } break;
        case MYSQL_TYPE_NULL:
          std::cout << "NULL : "
                    << "BL: " << _bind[i].buffer_length;
          break;
        case MYSQL_TYPE_ENUM:
          std::cout << "ENUM : "
                    << "BL: " << _bind[i].buffer_length << " : "
                    << static_cast<char**>(_column[i].get_buffer())[j]
                    << std::endl;
          break;
        case MYSQL_TYPE_STRING:
          std::cout << "STRING : "
                    << "BL: " << _bind[i].buffer_length << " NULL: "
                    << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
                                                                       : "0")
                    << " : " << static_cast<char**>(_column[i].get_buffer())[j]
                    << std::endl;
          break;
        case MYSQL_TYPE_DOUBLE: {
          std::cout << "DOUBLE : "
                       "BL: "
                    << _bind[i].buffer_length << " NULL: "
                    << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
                                                                       : "0")
                    << " : " << static_cast<double*>(_column[i].get_buffer())[j]
                    << std::endl;
        } break;
        case MYSQL_TYPE_FLOAT: {
          std::cout << "FLOAT : "
                       "BL: "
                    << _bind[i].buffer_length << " NULL: "
                    << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
                                                                       : "0")
                    << " : " << static_cast<float*>(_column[i].get_buffer())[j]
                    << std::endl;
        } break;
        default:
          std::cout << _bind[i].buffer_type
                    << " : BL: " << _bind[i].buffer_length << " : "
                    << "TYPE NOT MANAGED...\n";
          assert(1 == 0);  // Should not arrive...
          break;
      }
      std::cout << std::endl;
    }
  }
}

/**
 * @brief Return True if the bind contains no row, otherwise return False.
 *
 * @return A boolean.
 */
bool mysql_bind::empty() const {
  return _column[0].array_size() == 0;
}

/**
 * @brief Accessor to the MYSQL_BIND* contained in this mysql_bind. This is
 * useful to call the MariaDB C connector functions.
 *
 * @return A MYSQL_BIND* pointer.
 */
const MYSQL_BIND* mysql_bind::get_bind() const {
  return &_bind[0];
}

/**
 * @brief Accessor to the MYSQL_BIND* contained in this mysql_bind. This is
 * useful to call the MariaDB C connector functions.
 *
 * @return A MYSQL_BIND* pointer.
 */
MYSQL_BIND* mysql_bind::get_bind() {
  return &_bind[0];
}

/**
 * @brief Accessor to the number of columns in this bind.
 *
 * @return An integer.
 */
int mysql_bind::get_size() const {
  return _bind.size();
}

/**
 * @brief Accessor to the number of rows in this bind.
 *
 * @return an integer.
 */
size_t mysql_bind::rows_count() const {
  return _column[0].array_size();
}

/**
 * @brief Empty the bind. Actually, the current_row is set to -1.
 */
void mysql_bind::set_empty() {
  for (auto& c : _column)
    c.clear();
  _current_row = 0;
}

/**
 * @brief Accessor to the current row index.
 *
 * @return An integer.
 */
size_t mysql_bind::current_row() const {
  return _current_row;
}

/**
 * @brief Reserve size rows in this bind. The current size of the bind does
 * not change, but it will be faster to add rows if the reservation is big
 * enough.
 *
 * @param size The wanted number of rows.
 */
void mysql_bind::reserve(size_t size) {
  for (auto& c : _column)
    c.reserve(size);
}

/**
 * @brief Increment the current row index.
 */
void mysql_bind::next_row() {
  ++_current_row;
}
