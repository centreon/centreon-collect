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

#ifndef CCB_MYSQL_BIND_BASE_HH
#define CCB_MYSQL_BIND_BASE_HH

#include <mysql.h>
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

// Forward declarations
class mysql;

namespace database {
class mysql_bind_base {
  // A vector telling if bindings are already typed or not.
  std::vector<bool> _typed;
  bool _empty = true;

 protected:
  std::vector<MYSQL_BIND> _bind;

  bool _prepared(size_t range) const;
  void _set_typed(uint32_t range);

 public:
  /**
   * @brief Default constructor
   */
  mysql_bind_base() = default;
  mysql_bind_base(int size);
  /**
   * @brief Destructor
   */
  virtual ~mysql_bind_base() noexcept = default;
  MYSQL_BIND* get_bind();
  bool empty() const;
  void set_empty();

  virtual void set_value_as_f64(size_t range, double value) = 0;
  virtual void set_value_as_i32(size_t range, int32_t value) = 0;
  virtual void set_value_as_u32(size_t range, uint32_t value) = 0;
  virtual void set_null_i32(size_t range) = 0;
  virtual void set_value_as_i64(size_t range, int64_t value) = 0;
  virtual void set_value_as_u64(size_t range, uint64_t value) = 0;
  virtual void set_null_u64(size_t range) = 0;
  virtual void set_value_as_bool(size_t range, bool value) = 0;
  virtual void set_value_as_str(size_t range,
                                const fmt::string_view& value) = 0;
  virtual void set_value_as_tiny(size_t range, char value) = 0;
  virtual void set_null_tiny(size_t range) = 0;
};

}  // namespace database

CCB_END()

#endif  // CCB_MYSQL_BIND_BASE_HH
