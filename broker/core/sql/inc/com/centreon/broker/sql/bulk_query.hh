/**
 * Copyright 2025 Centreon
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

#ifndef CCB_SQL_BULK_QUERY_HH
#define CCB_SQL_BULK_QUERY_HH
#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/broker/sql/mysql_bulk_bind.hh"
#include "com/centreon/broker/sql/mysql_bulk_stmt.hh"

using namespace com::centreon::broker::database;

namespace com::centreon::broker::sql {

template <bool IsMariadb>
class bulk_query {
  mysql& _mysql;
  std::unique_ptr<database::mysql_bulk_stmt> _stmt;
  std::unique_ptr<database::mysql_bulk_bind> _current_bind;

 public:
  bulk_query(mysql& mysql,
             const std::string_view& query,
             const std::string_view& on_duplicate,
             size_t nb_variables)
      : _mysql{mysql} {
    std::string tmp("(");
    tmp.reserve(3 * nb_variables + 3);
    for (int i = 0; i < nb_variables; ++i)
      tmp.append("{},");
    tmp[tmp.size() - 1] = ')';
    std::string q(fmt::format("{} {} {}", query, tmp, on_duplicate));
    _stmt = std::make_unique<database::mysql_bulk_stmt>(q);
    _mysql.prepare_statement(*_stmt);
    _current_bind = _stmt->create_bind();
  }

  inline void next_row() { _current_bind->next_row(); }

  inline void set_value_as_i32(size_t index, int32_t value) {
    _current_bind->set_value_as_i32(index, value);
  }

  inline void set_value_as_str(size_t index, const std::string& value) {
    _current_bind->set_value_as_str(index, value);
  }

  inline void set_value_as_bool(size_t index, bool value) {
    _current_bind->set_value_as_bool(index, value);
  }

  void run() {
    _stmt->set_bind(std::move(_current_bind));
    _mysql.run_statement(*_stmt);
  }
};

}  // namespace com::centreon::broker::sql

#endif /* !CCB_SQL_BULK_QUERY_HH */
