/**
 * Copyright 2018-2024 Centreon
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
#ifndef CCB_MYSQL_RESULT_HH
#define CCB_MYSQL_RESULT_HH

#include "com/centreon/broker/sql/mysql_bind_result.hh"

namespace com::centreon::broker {

class mysql_connection;

namespace database {
/**
 *  @class mysql mysql.hh "com/centreon/broker/storage/mysql.hh"
 *  @brief Class managing the mysql connection
 *
 *  Here is a binding to the C MYSQL_RES. This is useful because of the
 *  delete functionality that must call the mysql_free_result() function.
 */
class mysql_result {
  mysql_connection* _parent;
  std::shared_ptr<MYSQL_RES> _result;

  // The row contains the result for a simple query (no statement).
  MYSQL_ROW _row;

  // the bind and the statement_id are filled in both or empty both.
  std::unique_ptr<database::mysql_bind_result> _bind;
  int _statement_id;

 public:
  mysql_result(mysql_connection* parent, int statement = 0);
  mysql_result(mysql_connection* parent, MYSQL_RES* res);
  mysql_result(mysql_result&& other);
  ~mysql_result() noexcept = default;
  mysql_result& operator=(mysql_result&& other);
  mysql_result& operator=(const mysql_result&) = delete;
  bool value_as_bool(int idx);
  float value_as_f32(int idx);
  double value_as_f64(int idx);
  int32_t value_as_i32(int idx);
  char value_as_tiny(int idx);
  std::string value_as_str(int idx);
  uint32_t value_as_u32(int idx);
  int64_t value_as_i64(int idx);
  uint64_t value_as_u64(int idx);
  bool value_is_null(int idx);
  bool empty() const;
  int get_rows_count() const;
  void set(MYSQL_RES* res);
  MYSQL_RES* get();
  void set_bind(std::unique_ptr<database::mysql_bind_result>&& bind);
  std::unique_ptr<database::mysql_bind_result>& get_bind();
  void set_row(MYSQL_ROW row);
  int get_statement_id() const;
  mysql_connection* get_connection();
  int get_num_fields() const;
  char const* get_field_name(int idx) const;
};
}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_RESULT_HH
