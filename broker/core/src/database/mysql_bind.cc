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

#include "com/centreon/broker/database/mysql_bind.hh"

#include <cfloat>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/mysql.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

mysql_bind::mysql_bind(int size, int length)
    : mysql_bind_base(size), _buffer(size) {
  if (length) {
    for (int i = 0; i < size; ++i) {
      _bind[i].buffer_type = MYSQL_TYPE_STRING;
      _buffer[i] = "";
      absl::get<std::string>(_buffer[i]).reserve(length);
      std::string* s = absl::get_if<std::string>(&_buffer[i]);
      _bind[i].buffer = const_cast<char*>(s->data());
      _bind[i].buffer_length = length;
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
  _buffer.resize(size);
  for (int i = 0; i < size; ++i)
    _bind[i].buffer = get_value_pointer(i);
}

void* mysql_bind::get_value_pointer(size_t range) {
  switch (_bind[range].buffer_type) {
    case MYSQL_TYPE_LONG:
      if (_bind[range].is_unsigned)
        return absl::get_if<uint32_t>(&_buffer[range]);
      else
        return absl::get_if<int32_t>(&_buffer[range]);
      break;
    case MYSQL_TYPE_LONGLONG:
      if (_bind[range].is_unsigned)
        return absl::get_if<uint64_t>(&_buffer[range]);
      else
        return absl::get_if<int64_t>(&_buffer[range]);
      break;
    case MYSQL_TYPE_FLOAT:
      return absl::get_if<float>(&_buffer[range]);
      break;
    case MYSQL_TYPE_DOUBLE:
      return absl::get_if<double>(&_buffer[range]);
      break;
    case MYSQL_TYPE_STRING:
      return const_cast<char*>(
          absl::get_if<std::string>(&_buffer[range])->data());
      break;
    case MYSQL_TYPE_TINY:
      return absl::get_if<char>(&_buffer[range]);
      break;
    default:
      assert("mysql_bind::set_size(): unknown type..." != nullptr);
  }
  return nullptr;
}

void mysql_bind::set_value_as_bool(size_t range, bool value) {
  assert(range < _bind.size());
  _set_typed(range);
  _bind[range].buffer_type = MYSQL_TYPE_TINY;
  _bind[range].is_unsigned = false;
  _buffer[range] = static_cast<char>(value ? 1 : 0);
  _bind[range].buffer = get_value_pointer(range);
}

void mysql_bind::set_null_bool(size_t range) {
  assert(range < _bind.size());
  _set_typed(range);
  _bind[range].buffer_type = MYSQL_TYPE_NULL;
  _bind[range].is_unsigned = false;
}

#define SET_VALUE(ftype, vtype, sqltype, unsgn)                      \
  void mysql_bind::set_value_as_##ftype(size_t range, vtype value) { \
    assert(range < _bind.size());                                    \
    _set_typed(range);                                               \
    _bind[range].buffer_type = sqltype;                              \
    _bind[range].is_unsigned = unsgn;                                \
    _buffer[range] = value;                                          \
    _bind[range].buffer = get_value_pointer(range);                  \
  }                                                                  \
                                                                     \
  void mysql_bind::set_null_##ftype(size_t range) {                  \
    assert(range < _bind.size());                                    \
    _set_typed(range);                                               \
    _bind[range].buffer_type = MYSQL_TYPE_NULL;                      \
    _bind[range].is_unsigned = unsgn;                                \
  }

#define VALUE(ftype, vtype, sqltype)                                      \
  vtype mysql_bind::value_as_##ftype(size_t range) const {                \
    if (_bind[range].buffer_type == sqltype)                              \
      return *static_cast<vtype*>(_bind[range].buffer);                   \
    else {                                                                \
      assert("This field is not an " #sqltype == nullptr);                \
      SPDLOG_LOGGER_CRITICAL(log_v2::sql(), " {} field is {} and not {}", \
                             __FUNCTION__, _bind[range].buffer_type,      \
                             #sqltype);                                   \
      return 0;                                                           \
    }                                                                     \
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
VALUE(str, const char*, MYSQL_TYPE_STRING)
SET_VALUE(tiny, char, MYSQL_TYPE_TINY, false)
VALUE(tiny, char, MYSQL_TYPE_TINY)
// SET_VALUE(bool, bool, MYSQL_TYPE_TINY, false)
VALUE(bool, bool, MYSQL_TYPE_TINY)

#undef SET_VALUE
#undef VALUE

void mysql_bind::set_value_as_str(size_t range, const fmt::string_view& value) {
  assert(range < _bind.size());
  _set_typed(range);
  _bind[range].buffer_type = MYSQL_TYPE_STRING;
  _buffer[range] = value.data();
  _bind[range].buffer_length = value.size();
  _bind[range].buffer = get_value_pointer(range);
}

void mysql_bind::set_null_str(size_t range) {
  assert(range < _bind.size());
  _set_typed(range);
  _bind[range].buffer_type = MYSQL_TYPE_NULL;
}

/**
 * @brief Return if the value at index range is NULL or not.
 *
 * @param range A non negative integer.
 *
 * @return A boolean True when the value is NULL.
 */
bool mysql_bind::value_is_null(size_t range) const {
  return _bind[range].buffer_type == MYSQL_TYPE_NULL;
}

/**
 * @brief A debug function to display the content of this bind.
 */
// void mysql_bind::debug() {
//   std::cout << "DEBUG BIND " << this << std::endl;
//   int size = _bind.size();
//     for (int i = 0; i < size; ++i) {
//       switch (_bind[i].buffer_type) {
//         case MYSQL_TYPE_LONGLONG: {
//           std::cout << "LONGLONG : "
//                     << "BL: " << _bind[i].buffer_length << " NULL: "
//                     << (_bind[i].buffer_type == MYSQL_TYPE_NULL ? "1" : "0")
//                     << " : "
//                     << get_value_pointer(i)
//                     << std::endl;
//         } break;
//         case MYSQL_TYPE_LONG: {
//           std::cout << "LONG : "
//                     << "BL: " << _bind[i].buffer_length << " NULL: "
//                     << (_bind[i].buffer_type == MYSQL_TYPE_NULL ? "1" : "0")
//                     << " : "
//                     << get_value_pointer(i)
//                     << std::endl;
//         } break;
//         case MYSQL_TYPE_TINY: {
//           std::cout << "TINY : "
//                     << "BL: " << _bind[i].buffer_length << " NULL: "
//                     << (_bind[i].buffer_type == MYSQL_TYPE_NULL ? "1" : "0")
//                     << " : "
//                     << get_value_pointer(i)
//                     << std::endl;
//         } break;
//         case MYSQL_TYPE_NULL:
//           std::cout << "NULL : "
//                     << "BL: " << _bind[i].buffer_length;
//           break;
//         case MYSQL_TYPE_ENUM:
//           std::cout << "ENUM : "
//                     << "BL: " << _bind[i].buffer_length << " : "
//                     << static_cast<char**>(_column[i].get_buffer())[j]
//                     << std::endl;
//           break;
//         case MYSQL_TYPE_STRING:
//           std::cout << "STRING : "
//                     << "BL: " << _bind[i].buffer_length << " NULL: "
//                     << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
//                                                                        : "0")
//                     << " : " <<
//                     static_cast<char**>(_column[i].get_buffer())[j]
//                     << std::endl;
//           break;
//         case MYSQL_TYPE_DOUBLE: {
//           std::cout << "DOUBLE : "
//                        "BL: "
//                     << _bind[i].buffer_length << " NULL: "
//                     << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
//                                                                        : "0")
//                     << " : " <<
//                     static_cast<double*>(_column[i].get_buffer())[j]
//                     << std::endl;
//         } break;
//         case MYSQL_TYPE_FLOAT: {
//           std::cout << "FLOAT : "
//                        "BL: "
//                     << _bind[i].buffer_length << " NULL: "
//                     << (_bind[i].u.indicator[j] == STMT_INDICATOR_NULL ? "1"
//                                                                        : "0")
//                     << " : " <<
//                     static_cast<float*>(_column[i].get_buffer())[j]
//                     << std::endl;
//         } break;
//         default:
//           std::cout << _bind[i].buffer_type
//                     << " : BL: " << _bind[i].buffer_length << " : "
//                     << "TYPE NOT MANAGED...\n";
//           assert(1 == 0);  // Should not arrive...
//           break;
//       }
//       std::cout << std::endl;
//     }
// }

/**
 * @brief Accessor to the number of columns in this bind.
 *
 * @return An integer.
 */
int mysql_bind::get_size() const {
  return _bind.size();
}
