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

/**
 * @brief Constructor of a mysql_column. The row_count parameter is a
 * reservation, not a predefined size.
 *
 * @param type The type of the column to allocate.
 * @param row_count The number of rows to reserve whereas the initial size is 0.
 */
mysql_column::mysql_column(int type, size_t row_count)
    : _type(type),
      _row_count{row_count},
      _vector{nullptr},
      _vector_buffer(nullptr) {
  if (row_count > 0) {
    set_type(type);
    _reserve(row_count);
  }
}

/**
 * @brief Destructor
 */
mysql_column::~mysql_column() noexcept {
  if (_vector)
    _free_vector();
}

/**
 * @brief Move constructor.
 *
 * @param other The column to move.
 */
mysql_column::mysql_column(mysql_column&& other)
    : _type(other._type),
      _row_count(other._row_count),
      _vector(other._vector),
      _vector_buffer(other._vector_buffer),
      _is_null(std::move(other._is_null)),
      _error(std::move(other._error)),
      _length(std::move(other._length)) {
  other._vector = nullptr;
  other._vector_buffer = nullptr;
}

/**
 * @brief Move operator
 *
 * @param other The column to move.
 *
 * @return a reference of this column.
 */
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

void mysql_column::clear() {
  _is_null.clear();
  _error.clear();
  _length.clear();
  switch (_type) {
    case MYSQL_TYPE_STRING: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      for (auto* c : *vector) {
        delete c;
        c = nullptr;
      }
      vector->clear();
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_FLOAT: {
      std::vector<float>* vector = static_cast<std::vector<float>*>(_vector);
      vector->clear();
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_LONG: {
      std::vector<int>* vector = static_cast<std::vector<int>*>(_vector);
      vector->clear();
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_TINY: {
      std::vector<char>* vector = static_cast<std::vector<char>*>(_vector);
      vector->clear();
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_DOUBLE: {
      std::vector<double>* vector = static_cast<std::vector<double>*>(_vector);
      vector->clear();
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_LONGLONG: {
      std::vector<long long>* vector =
          static_cast<std::vector<long long>*>(_vector);
      vector->clear();
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_NULL: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      vector->clear();
      _vector_buffer = vector->data();
    } break;
    default:
      assert(1 == 0);
  }
}

/**
 * @brief Reserve the size of the column, that is to say its number of rows.
 * It is not an allocation, the size itself does not change.
 *
 * @param s The size to reserve.
 */
void mysql_column::_reserve(size_t s) {
  assert(_vector && "set_type() has not been called before the reservation");
  _is_null.reserve(s);
  _error.reserve(s);
  _length.reserve(s);
  switch (_type) {
    case MYSQL_TYPE_STRING: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_FLOAT: {
      std::vector<float>* vector = static_cast<std::vector<float>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_LONG: {
      std::vector<int>* vector = static_cast<std::vector<int>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_TINY: {
      std::vector<char>* vector = static_cast<std::vector<char>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_DOUBLE: {
      std::vector<double>* vector = static_cast<std::vector<double>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_LONGLONG: {
      std::vector<long long>* vector =
          static_cast<std::vector<long long>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_NULL: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    default:
      assert(1 == 0);
  }
}

/**
 * @brief Resize the number of rows of the column.
 *
 * @param s the new size to set.
 */
void mysql_column::reserve(size_t s) {
  _is_null.reserve(s);
  _error.reserve(s);
  _length.reserve(s);
  switch (_type) {
    case MYSQL_TYPE_STRING: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_FLOAT: {
      std::vector<float>* vector = static_cast<std::vector<float>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_LONG: {
      std::vector<int>* vector = static_cast<std::vector<int>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_TINY: {
      std::vector<char>* vector = static_cast<std::vector<char>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_DOUBLE: {
      std::vector<double>* vector = static_cast<std::vector<double>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_LONGLONG: {
      std::vector<long long>* vector =
          static_cast<std::vector<long long>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    case MYSQL_TYPE_NULL: {
      std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
      vector->reserve(s);
      _vector_buffer = vector->data();
    } break;
    default:
      assert(1 == 0);
  }
}

void mysql_column::_push_row(const fmt::string_view& str) {
  std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
  assert(_is_null.size() == _row_count && _error.size() == _row_count &&
         _length.size() == _row_count && vector->size() == _row_count);
  _is_null.push_back(false);
  _error.push_back(false);
  _length.push_back(str.size());
  vector->push_back(strndup(str.data(), str.size()));
  _vector_buffer = vector->data();
  ++_row_count;
}

void mysql_column::set_value(size_t row, const fmt::string_view& str) {
  assert(_type == MYSQL_TYPE_STRING);
  size_t size = str.size();
  std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
  if (vector->size() <= row) {
    assert(vector->size() == row);
    _push_row(str);
  } else if ((*vector)[row]) {
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
  assert(_vector == nullptr && _row_count == 0);
  _type = type;
  switch (type) {
    case MYSQL_TYPE_STRING: {
      auto* vector = new std::vector<char*>();
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_FLOAT: {
      auto* vector = new std::vector<float>();
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_LONG: {
      auto* vector = new std::vector<int>();
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_TINY: {
      auto* vector = new std::vector<char>();
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_DOUBLE: {
      auto* vector = new std::vector<double>();
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_LONGLONG: {
      auto* vector = new std::vector<long long>();
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_NULL: {
      auto* vector = new std::vector<char*>();
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
