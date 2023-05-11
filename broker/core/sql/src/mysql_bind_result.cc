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

#include "com/centreon/broker/sql/mysql_bind_result.hh"

#include <absl/strings/numbers.h>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/sql/mysql.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

mysql_bind_result::mysql_bind_result(int size, int length)
    : mysql_bind_base(size), _length(length), _is_null(size) {
  if (length) {
    int alloc = length + 1;
    _buffer.resize(size * alloc);
    char* tmp = _buffer.data();
    for (int i = 0; i < size; ++i) {
      _bind[i].buffer_type = MYSQL_TYPE_STRING;
      _bind[i].buffer = const_cast<char*>(tmp);
      _bind[i].buffer_length = length;
      _bind[i].is_null = &_is_null[i];
      tmp += alloc;
    }
  }
}

int32_t mysql_bind_result::value_as_i32(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  int32_t retval;
  if (absl::SimpleAtoi(static_cast<char*>(_bind[range].buffer), &retval))
    return retval;
  else {
    log_v2::sql()->debug(
        "mysql_bind_result: unable to parse an i32 at range {}", range);
    return 0;
  }
}

uint32_t mysql_bind_result::value_as_u32(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  uint32_t retval;
  if (absl::SimpleAtoi(static_cast<char*>(_bind[range].buffer), &retval))
    return retval;
  else {
    log_v2::sql()->debug(
        "mysql_bind_result: unable to parse an u32 at range {}", range);
    return 0;
  }
}

int64_t mysql_bind_result::value_as_i64(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  int64_t retval;
  if (absl::SimpleAtoi(static_cast<char*>(_bind[range].buffer), &retval))
    return retval;
  else {
    log_v2::sql()->debug(
        "mysql_bind_result: unable to parse an i64 at range {}", range);
    return 0;
  }
}

uint64_t mysql_bind_result::value_as_u64(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  uint64_t retval;
  if (absl::SimpleAtoi(static_cast<char*>(_bind[range].buffer), &retval))
    return retval;
  else {
    log_v2::sql()->debug(
        "mysql_bind_result: unable to parse an u64 at range {}", range);
    return 0;
  }
}

float mysql_bind_result::value_as_f32(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  float retval;
  if (absl::SimpleAtof(static_cast<char*>(_bind[range].buffer), &retval))
    return retval;
  else {
    log_v2::sql()->debug(
        "mysql_bind_result: unable to parse an f32 at range {}", range);
    return 0;
  }
}

double mysql_bind_result::value_as_f64(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  double retval;
  if (absl::SimpleAtod(static_cast<char*>(_bind[range].buffer), &retval))
    return retval;
  else {
    log_v2::sql()->debug(
        "mysql_bind_result: unable to parse an f64 at range {}", range);
    return 0;
  }
}

const char* mysql_bind_result::value_as_str(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  return static_cast<char*>(_bind[range].buffer);
}

char mysql_bind_result::value_as_tiny(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  int32_t retval;
  if (absl::SimpleAtoi(static_cast<char*>(_bind[range].buffer), &retval))
    return static_cast<char>(retval);
  else {
    log_v2::sql()->debug(
        "mysql_bind_result: unable to parse a tiny at range {}", range);
    return 0;
  }
}

bool mysql_bind_result::value_as_bool(size_t range) const {
  assert(_bind[range].buffer_type == MYSQL_TYPE_STRING);
  bool retval;
  if (absl::SimpleAtob(static_cast<char*>(_bind[range].buffer), &retval))
    return retval;
  else {
    log_v2::sql()->debug(
        "mysql_bind_result: unable to parse a bool at range {}", range);
    return false;
  }
}

/**
 * @brief Return if the value at index range is NULL or not.
 *
 * @param range A non negative integer.
 *
 * @return A boolean True when the value is NULL.
 */
bool mysql_bind_result::value_is_null(size_t range) const {
  return _is_null[range];
}

/***********************************************************************
 * all following methods are mandatory for inheritance but must never be called
 ***********************************************************************/

void mysql_bind_result::set_value_as_f64(size_t range, double value) {
  assert(false);
}
void mysql_bind_result::set_value_as_i32(size_t range, int32_t value) {
  assert(false);
}
void mysql_bind_result::set_value_as_u32(size_t range, uint32_t value) {
  assert(false);
}
void mysql_bind_result::set_value_as_i64(size_t range, int64_t value) {
  assert(false);
}
void mysql_bind_result::set_value_as_u64(size_t range, uint64_t value) {
  assert(false);
}
void mysql_bind_result::set_null_u64(size_t range) {
  assert(false);
}
void mysql_bind_result::set_value_as_bool(size_t range, bool value) {
  assert(false);
}
void mysql_bind_result::set_value_as_str(size_t range,
                                         const fmt::string_view& value) {
  assert(false);
}
void mysql_bind_result::set_value_as_tiny(size_t range, char value) {
  assert(false);
}
void mysql_bind_result::set_null_tiny(size_t range) {
  assert(false);
}