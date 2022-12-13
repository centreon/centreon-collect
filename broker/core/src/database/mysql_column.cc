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

mysql_column::mysql_column(int type, size_t row_count)
    : _type(type),
      _row_count{row_count},
      _vector(nullptr),
      _vector_buffer(nullptr),
      _is_null(row_count),
      _error(row_count),
      _length(row_count) {}

mysql_column::~mysql_column() {
  if (_vector)
    _free_vector();
}

mysql_column::mysql_column(mysql_column&& other)
    : _type(other._type),
      _row_count(other._row_count),
      _vector(other._vector),
      _vector_buffer(other._vector_buffer),
      _is_null(other._is_null),
      _error(other._error),
      _length(other._length) {
  other._vector = nullptr;
  other._vector_buffer = nullptr;
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
  _free_vector();
  _vector = other._vector;
  _vector_buffer = other._vector_buffer;
  other._vector = nullptr;
  other._vector_buffer = nullptr;
  return *this;
}

void mysql_column::_free_vector() {
  switch (_type) {
    case MYSQL_TYPE_STRING: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      for (auto& s : *vector) {
        if (s)
          free(s);
      }
      delete vector;
    } break;
    case MYSQL_TYPE_FLOAT: {
      std::vector<float>* vector = static_cast<std::vector<float>*>(_vector);
      delete vector;
    } break;
    case MYSQL_TYPE_LONG: {
      std::vector<int>* vector = static_cast<std::vector<int>*>(_vector);
      delete vector;
    } break;
    case MYSQL_TYPE_TINY: {
      std::vector<char>* vector = static_cast<std::vector<char>*>(_vector);
      delete vector;
    } break;
    case MYSQL_TYPE_DOUBLE: {
      std::vector<double>* vector = static_cast<std::vector<double>*>(_vector);
      delete vector;
    } break;
    case MYSQL_TYPE_LONGLONG: {
      std::vector<long long>* vector =
          static_cast<std::vector<long long>*>(_vector);
      delete vector;
    } break;
    case MYSQL_TYPE_NULL: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      delete vector;
    } break;
    default:
      assert(1 == 0);
  }
  _vector = nullptr;
  _vector_buffer = nullptr;
}

int mysql_column::get_type() const {
  return _type;
}

void* mysql_column::get_buffer() {
  return _vector_buffer;
}

/**
 * @brief Resize the number of rows of the column.
 *
 * @param s the new size to set.
 */
void mysql_column::resize_column(int32_t s) {
  _row_count = s;
  _is_null.resize(s);
  _error.resize(s);
  _length.resize(s);
  switch (_type) {
    case MYSQL_TYPE_STRING: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      vector->resize(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_FLOAT: {
      std::vector<float>* vector = static_cast<std::vector<float>*>(_vector);
      vector->resize(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_LONG: {
      std::vector<int>* vector = static_cast<std::vector<int>*>(_vector);
      vector->resize(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_TINY: {
      std::vector<char>* vector = static_cast<std::vector<char>*>(_vector);
      vector->resize(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_DOUBLE: {
      std::vector<double>* vector = static_cast<std::vector<double>*>(_vector);
      vector->resize(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_LONGLONG: {
      std::vector<long long>* vector =
          static_cast<std::vector<long long>*>(_vector);
      vector->resize(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_NULL: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      vector->resize(s);
      _vector_buffer = vector->data();
    } break;
    default:
      assert(1 == 0);
  }
}

void mysql_column::set_value(size_t row, const fmt::string_view& str) {
  assert(_type == MYSQL_TYPE_STRING);
  size_t size = str.size();
  std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
  if (row >= _row_count)
    resize_column(row + 1);
  if ((*vector)[row]) {
    if (_length[row] >= str.size()) {
      strncpy((*vector)[row], str.data(), size + 1);
      (*vector)[row][size] = 0;
      _length[row] = size;
      return;
    } else
      free((*vector)[row]);
  }
  (*vector)[row] = strndup(str.data(), size);
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
    case MYSQL_TYPE_STRING: {
      auto* vector = new std::vector<char*>(_row_count);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_FLOAT: {
      auto* vector = new std::vector<float>(_row_count);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_LONG: {
      auto* vector = new std::vector<int>(_row_count);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_TINY: {
      auto* vector = new std::vector<char>(_row_count);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_DOUBLE: {
      auto* vector = new std::vector<double>(_row_count);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_LONGLONG: {
      auto* vector = new std::vector<long long>(_row_count);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_NULL: {
      auto* vector = new std::vector<char*>(_row_count);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    default:
      assert(1 == 0);
  }
}

uint32_t mysql_column::array_size() const {
  return _row_count;
}
