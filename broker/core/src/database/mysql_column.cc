/*
** Copyright 2018-2022 Centreon
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
    reserve(row_count);
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
      _indicator(std::move(other._indicator)),
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
  _indicator = std::move(other._indicator);
  _free_vector();
  _vector = other._vector;
  _vector_buffer = other._vector_buffer;
  other._vector = nullptr;
  other._vector_buffer = nullptr;
  return *this;
}

/**
 * @brief Destroy the main vector of the column.
 */
void mysql_column::_free_vector() {
  if (!_vector)
    return;
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

/**
 * @brief Accessor to the type of items stored in the column.
 *
 * @return Integer equal to MYSQL_TYPE_STRING, MYSQL_TYPE_LONG, ...
 */
int mysql_column::get_type() const {
  return _type;
}

/**
 * @brief Accessor to the array of data contained in the column.
 * For example, in case of data of type MYSQL_TYPE_LONG, this buffer is of type
 * long*.
 *
 * @return A pointer.
 */
void* mysql_column::get_buffer() {
  return _vector_buffer;
}

/**
 * @brief Clear the column. Its size is reset to 0.
 */
void mysql_column::clear() {
  _indicator.clear();
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
void mysql_column::reserve(size_t s) {
  _indicator.reserve(s);
  _error.reserve(s);
  _length.reserve(s);
  if (_vector) {
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
        std::vector<double>* vector =
            static_cast<std::vector<double>*>(_vector);
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
}

/**
 * @brief Push the str value to the end of the column. The size of the column
 * is then incremented by 1.
 *
 * @param str The string to add to the column.
 */
void mysql_column::_push_value_str(const fmt::string_view& str) {
  std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
  assert(_indicator.size() == _row_count && _error.size() == _row_count &&
         _length.size() == _row_count && vector->size() == _row_count);
  _indicator.push_back(STMT_INDICATOR_NTS);
  _error.push_back(false);
  _length.push_back(str.size());
  vector->push_back(strndup(str.data(), str.size()));
  _vector_buffer = vector->data();
  ++_row_count;
}

/**
 * @brief Push a null value to the end of the column. The size of the column
 * is incremented by 1. This method is used when the type of the column is
 * MYSQL_TYPE_STRING.
 */
void mysql_column::_push_null_str() {
  std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
  assert(_indicator.size() == _row_count && _error.size() == _row_count &&
         _length.size() == _row_count && vector->size() == _row_count);
  _indicator.push_back(STMT_INDICATOR_NULL);
  _error.push_back(false);
  _length.push_back(0);
  vector->push_back(nullptr);
  _vector_buffer = vector->data();
  ++_row_count;
}

/**
 * @brief Set the str value into the column at the row specified (we start at
 * index 0). This method can only be called when the content type is
 * MYSQL_TYPE_STRING. The greatest row value is the current size of the column,
 * this allows to increment its size by 1.
 *
 * @param row The row concerned by the insertion.
 * @param str The content to set into the column.
 */
void mysql_column::set_value_str(size_t row, const fmt::string_view& str) {
  assert(_type == MYSQL_TYPE_STRING);
  size_t size = str.size();
  std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
  if (vector->size() <= row) {
    assert(vector->size() == row);
    _push_value_str(str);
    return;
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

/**
 * @brief Set the null value into the column at the row specified (we start at
 * index 0). This method can only be called when the content type is
 * MYSQL_TYPE_STRING. The greatest row value is the current size of the column,
 * this allows to increment its size by 1.
 *
 * @param row The row concerned by the insertion.
 */
void mysql_column::set_null_str(size_t row) {
  assert(_type == MYSQL_TYPE_STRING);
  std::vector<char*>* vector = static_cast<std::vector<char*>*>(_vector);
  if (vector->size() <= row) {
    assert(vector->size() == row);
    _push_null_str();
  } else if ((*vector)[row]) {
    free((*vector)[row]);
    (*vector)[row] = nullptr;
    _length[row] = 0;
    _indicator[row] = STMT_INDICATOR_NULL;
    _error[row] = false;
  }
}

/**
 * @brief Return true if the column is of type STMT_INDICATOR_NULL. In other
 * words, this function is useful after a SELECT when we fetch the result line
 * by line. It allows us to know if the value is NULL.
 *
 * It is useless in case of a prepared statement, or we want to set to NULL the
 * entire column.
 *
 * @return A boolean.
 */
bool mysql_column::is_null() const {
  return _indicator[_current_row] == STMT_INDICATOR_NULL;
}

/**
 * @brief Accessor to the content of the indicators vector given as a char*
 * array (needed by the C connector).
 *
 * @return A char* pointer.
 */
char* mysql_column::indicator_buffer() {
  return &_indicator[0];
}

/**
 * @brief Accessor to the content of the error vector given as a my_bool*
 * array (needed by the C connector).
 *
 * @return A my_bool* pointer.
 */
my_bool* mysql_column::error_buffer() {
  return &_error[0];
}

/**
 * @brief Accessor to the content of the lengths vector given as an unsigned
 * long* array (needed by the C connector).
 *
 * @return A unsigned long* pointer.
 */
unsigned long* mysql_column::length_buffer() {
  return &_length[0];
}

/**
 * @brief Set the type of the column with a value like MYSQL_TYPE_STRING,
 * MYSQL_TYPE_LONG, etc...
 *
 * @param type The type to specify.
 */
void mysql_column::set_type(int type) {
  assert(_vector == nullptr);
  assert(_row_count <= 1);
  assert(_current_row <= 1);
  _type = type;
  size_t reserved_size = _indicator.capacity();
  switch (type) {
    case MYSQL_TYPE_STRING: {
      auto* vector = new std::vector<char*>();
      if (reserved_size > 0)
        vector->reserve(reserved_size);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_FLOAT: {
      auto* vector = new std::vector<float>();
      if (reserved_size > 0)
        vector->reserve(reserved_size);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_LONG: {
      auto* vector = new std::vector<int>();
      if (reserved_size > 0)
        vector->reserve(reserved_size);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_TINY: {
      auto* vector = new std::vector<char>();
      if (reserved_size > 0)
        vector->reserve(reserved_size);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_DOUBLE: {
      auto* vector = new std::vector<double>();
      if (reserved_size > 0)
        vector->reserve(reserved_size);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_LONGLONG: {
      auto* vector = new std::vector<long long>();
      if (reserved_size > 0)
        vector->reserve(reserved_size);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    case MYSQL_TYPE_NULL: {
      auto* vector = new std::vector<char*>();
      if (reserved_size > 0)
        vector->reserve(reserved_size);
      _vector_buffer = vector->data();
      _vector = vector;
    } break;
    default:
      assert(1 == 0);
  }
}

/**
 * @brief Return the current number of rows in the column.
 *
 * @return A positive integer.
 */
uint32_t mysql_column::array_size() const {
  return _row_count;
}

/**
 * @brief Set the null value into the column at the row specified (we start at
 * index 0). This method can only be called when the content type is
 * MYSQL_TYPE_BOOL. The greatest row value is the current size of the column,
 * this allows to increment its size by 1.
 *
 * @param row The row concerned by the insertion.
 */
void mysql_column::set_null_bool(size_t row) {
  set_null_tiny(row);
}

/**
 * @brief Set the value into the column at the row specified (we start at
 * index 0). This method can only be called when the content type is
 * MYSQL_TYPE_TINY. True is changed into 1 whereas False is changed into 0.
 * The greatest row value is the current size of the column, this allows to
 * increment its size by 1.
 *
 * @param row The row concerned by the insertion.
 * @param value The content to set into the column.
 */
void mysql_column::set_value_bool(size_t row, bool value) {
  set_value_tiny(row, value ? 1 : 0);
}

void mysql_column::_push_value_bool(bool value) {
  _push_value_tiny(value ? 1 : 0);
}

void mysql_column::_push_null_bool() {
  _push_null_tiny();
}

#define SET_VALUE(ftype, vtype)                                              \
  void mysql_column::set_null_##ftype(size_t row) {                          \
    std::vector<vtype>* vector = static_cast<std::vector<vtype>*>(_vector);  \
    if (vector->size() <= row) {                                             \
      assert(vector->size() == row);                                         \
      _push_null_##ftype();                                                  \
    } else {                                                                 \
      (*vector)[row] = {};                                                   \
      _indicator[row] = STMT_INDICATOR_NULL;                                 \
    }                                                                        \
  }                                                                          \
                                                                             \
  void mysql_column::set_value_##ftype(size_t row, vtype value) {            \
    std::vector<vtype>* vector = static_cast<std::vector<vtype>*>(_vector);  \
    if (vector->size() <= row) {                                             \
      assert(vector->size() == row);                                         \
      _push_value_##ftype(value);                                            \
    } else                                                                   \
      (*vector)[row] = value;                                                \
  }                                                                          \
  void mysql_column::_push_value_##ftype(vtype val) {                        \
    std::vector<vtype>* vector = static_cast<std::vector<vtype>*>(_vector);  \
    assert(_indicator.size() == _row_count && _error.size() == _row_count && \
           _length.size() == _row_count && vector->size() == _row_count);    \
    _indicator.push_back(std::isnan(val) || std::isinf(val)                  \
                             ? STMT_INDICATOR_NULL                           \
                             : STMT_INDICATOR_NONE);                         \
    _error.push_back(false);                                                 \
    _length.push_back(0);                                                    \
    vector->push_back(val);                                                  \
    _vector_buffer = vector->data();                                         \
    ++_row_count;                                                            \
  }                                                                          \
  void mysql_column::_push_null_##ftype() {                                  \
    std::vector<vtype>* vector = static_cast<std::vector<vtype>*>(_vector);  \
    assert(_indicator.size() == _row_count && _error.size() == _row_count && \
           _length.size() == _row_count && vector->size() == _row_count);    \
    _indicator.push_back(STMT_INDICATOR_NULL);                               \
    _error.push_back(false);                                                 \
    _length.push_back(0);                                                    \
    vector->push_back({});                                                   \
    _vector_buffer = vector->data();                                         \
    ++_row_count;                                                            \
  }

#define SET_VALUE_CHECK(ftype, vtype)                                        \
  void mysql_column::set_null_##ftype(size_t row) {                          \
    std::vector<vtype>* vector = static_cast<std::vector<vtype>*>(_vector);  \
    if (vector->size() <= row) {                                             \
      assert(vector->size() == row);                                         \
      _push_null_##ftype();                                                  \
    } else {                                                                 \
      (*vector)[row] = {};                                                   \
      _indicator[row] = STMT_INDICATOR_NULL;                                 \
    }                                                                        \
  }                                                                          \
                                                                             \
  void mysql_column::set_value_##ftype(size_t row, vtype value) {            \
    std::vector<vtype>* vector = static_cast<std::vector<vtype>*>(_vector);  \
    if (vector->size() <= row) {                                             \
      assert(vector->size() == row);                                         \
      _push_value_##ftype(value);                                            \
    } else                                                                   \
      (*vector)[row] = value;                                                \
    _indicator[row] = (std::isnan(value) || std::isinf(value))               \
                          ? STMT_INDICATOR_NULL                              \
                          : STMT_INDICATOR_NONE;                             \
  }                                                                          \
  void mysql_column::_push_value_##ftype(vtype val) {                        \
    std::vector<vtype>* vector = static_cast<std::vector<vtype>*>(_vector);  \
    assert(_indicator.size() == _row_count && _error.size() == _row_count && \
           _length.size() == _row_count && vector->size() == _row_count);    \
    _indicator.push_back(std::isnan(val) || std::isinf(val)                  \
                             ? STMT_INDICATOR_NULL                           \
                             : STMT_INDICATOR_NONE);                         \
    _error.push_back(false);                                                 \
    _length.push_back(0);                                                    \
    vector->push_back(val);                                                  \
    _vector_buffer = vector->data();                                         \
    ++_row_count;                                                            \
  }                                                                          \
  void mysql_column::_push_null_##ftype() {                                  \
    std::vector<vtype>* vector = static_cast<std::vector<vtype>*>(_vector);  \
    assert(_indicator.size() == _row_count && _error.size() == _row_count && \
           _length.size() == _row_count && vector->size() == _row_count);    \
    _indicator.push_back(STMT_INDICATOR_NULL);                               \
    _error.push_back(false);                                                 \
    _length.push_back(0);                                                    \
    vector->push_back({});                                                   \
    _vector_buffer = vector->data();                                         \
    ++_row_count;                                                            \
  }

SET_VALUE(i32, int32_t)
SET_VALUE(i64, int64_t)
SET_VALUE(u32, uint32_t)
SET_VALUE(u64, uint64_t)
SET_VALUE(tiny, char)
SET_VALUE_CHECK(f32, float)
SET_VALUE_CHECK(f64, double)
