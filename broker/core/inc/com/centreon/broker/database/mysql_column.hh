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

#ifndef CCB_DATABASE_MYSQL_COLUMN_HH
#define CCB_DATABASE_MYSQL_COLUMN_HH

#include <fmt/format.h>
#include <mysql.h>
#include <cmath>
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace database {
class mysql_column {
  int _type;
  int _row_count;
  int32_t _current_row = 0;
  // This is pointer to a std::vector<>, but we cannot specify its items types
  // in advance.
  void* _vector;
  // This pointer is just a pointer to _vector.data().
  void* _vector_buffer;

  std::vector<my_bool> _is_null;
  std::vector<my_bool> _error;
  std::vector<unsigned long> _length;

  void _free_vector();
  void _resize_column(int32_t s);

 public:
  mysql_column(int type = MYSQL_TYPE_LONG, int row_count = 1, int length = 0);
  mysql_column(mysql_column&& other);
  mysql_column& operator=(mysql_column&& other);
  ~mysql_column();
  int get_type() const;
  void* get_buffer();
  void set_type(int type);

  template <typename T>
  void set_value(int32_t row, T value) {
    T* vector(static_cast<T*>(_vector));
    vector[row] = value;
  }

  void set_value(int32_t row, const fmt::string_view& str);
  my_bool* is_null_buffer();
  bool is_null() const;
  my_bool* error_buffer();
  unsigned long* length_buffer();
};

template <>
inline void mysql_column::set_value<double>(int32_t row, double val) {
  std::vector<double>* vector = static_cast<std::vector<double>*>(_vector);
  _is_null[row] = (std::isnan(val) || std::isinf(val));
  (*vector)[row] = val;
}

template <>
inline void mysql_column::set_value<float>(int32_t row, float val) {
  std::vector<float>* vector = static_cast<std::vector<float>*>(_vector);
  _is_null[row] = (std::isnan(val) || std::isinf(val));
  (*vector)[row] = val;
}

}  // namespace database

CCB_END()

#endif  // CCB_DATABASE_MYSQL_COLUMN_HH
