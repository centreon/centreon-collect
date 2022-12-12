/*
** Copyright 2018-2020 Centreon
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

#include "com/centreon/broker/database/mysql_column.hh"

#include <cassert>

#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

mysql_column::mysql_column(int type, int row_count, int size)
    : _type(type),
      _row_count(row_count),
      _vector(nullptr),
      _is_null(row_count),
      _error(row_count),
      _length(row_count) {
  if (type == MYSQL_TYPE_STRING && row_count && size) {
    char** vector = static_cast<char**>(calloc(_row_count, sizeof(char*)));
    _vector = vector;
  }
}

mysql_column::~mysql_column() {
  if (_vector) {
    if (_type == MYSQL_TYPE_STRING) {
      char** vector = static_cast<char**>(_vector);
      for (int i = 0; i < _row_count; ++i) {
        if (vector[i])
          free(vector[i]);
      }
    }
    free(_vector);
  }
}

mysql_column::mysql_column(mysql_column&& other)
    : _type(other._type),
      _row_count(other._row_count),
      _vector(other._vector),
      _is_null(other._is_null),
      _error(other._error),
      _length(other._length) {
  other._vector = nullptr;
}

mysql_column& mysql_column::operator=(mysql_column&& other) {
  if (this == &other)
    return *this;

  _type = other._type;
  _row_count = other._row_count;
  _current_row = other._current_row;
  _length = std::move(other._length);
  _error = std::move(other._error);
  _is_null = std::move(other._is_null);
  if (_type == MYSQL_TYPE_STRING) {
    char** vector = static_cast<char**>(_vector);
    for (int i = 0; i < _row_count; ++i) {
      if (vector[i])
        free(vector[i]);
    }
  }
  free(_vector);
  _vector = other._vector;
  other._vector = nullptr;
  return *this;
}

int mysql_column::get_type() const {
  return _type;
}

void* mysql_column::get_buffer() {
  return _vector;
}

void mysql_column::set_value(int32_t row, const fmt::string_view& str) {
  assert(_type == MYSQL_TYPE_STRING);
  size_t size = str.size();
  char** vector = static_cast<char**>(_vector);
  if (vector[row]) {
    if (_length[row] >= str.size()) {
      strncpy(vector[row], str.data(), size + 1);
      vector[row][size] = 0;
      _length[row] = size;
      return;
    }
    else
      free(vector[row]);
  }
  vector[row] = strndup(str.data(), size);
  _length[row] = size;
}

bool mysql_column::is_null() const {
  return _is_null[_current_row];
}

my_bool* mysql_column::is_null_buffer() {
  return &_is_null[0];
}

my_bool* mysql_column::error_buffer() {
  return &_error[0];
}

unsigned long* mysql_column::length_buffer() {
  return &_length[0];
}

void mysql_column::set_type(int type) {
  _type = type;
  assert(_vector == nullptr);
  switch (type) {
    case MYSQL_TYPE_STRING:
      _vector = calloc(_row_count, sizeof(char*));
      break;
    case MYSQL_TYPE_FLOAT:
      _vector = calloc(_row_count, sizeof(float));
      break;
    case MYSQL_TYPE_LONG:
      _vector = calloc(_row_count, sizeof(int));
      break;
    case MYSQL_TYPE_TINY:
      _vector = calloc(_row_count, sizeof(char));
      break;
    case MYSQL_TYPE_DOUBLE:
      _vector = calloc(_row_count, sizeof(double));
      break;
    case MYSQL_TYPE_LONGLONG:
      _vector = calloc(_row_count, sizeof(long long));
      break;
    case MYSQL_TYPE_NULL:
      _vector = calloc(_row_count, sizeof(char*));
      break;
    default:
      assert(1 == 0);
  }
}
