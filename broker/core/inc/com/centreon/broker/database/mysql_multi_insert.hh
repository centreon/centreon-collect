/*
** Copyright 2018-2023 Centreon
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

#ifndef CCB_MYSQL_MULTI_INSERT_HH
#define CCB_MYSQL_MULTI_INSERT_HH

#include "com/centreon/broker/database/mysql_stmt.hh"

CCB_BEGIN()

namespace database {

/**
 * @brief this class used as a data provider is the data provider of
 * multi_insert_class.
 * It embeds two things: data and code to bind data to the
 * statement example of implementation:
 * @code {.c++}
 *   struct row {
 *    using pointer = std::shared_ptr<row>;
 *    std::string name;
 *    double value;
 *  };
 *
 *  struct row_filler : public database::row_filler {
 *    row::pointer data;
 *    row_filler(const row::pointer& dt) : data(dt) {}
 *
 *    inline void fill_row(unsigned stmt_first_column,
 *                           mysql_stmt& to_bind) const override {
 *      to_bind.bind_value_as_str(stmt_first_column++, data->name);
 *      to_bind.bind_value_as_f64(stmt_first_column++, data->value);
 *    }
 *  };
 * @endcode
 *
 *
 */
class row_filler {
 public:
  using pointer = std::unique_ptr<row_filler>;
  virtual ~row_filler() {}
  virtual void fill_row(unsigned stmt_first_column,
                        mysql_stmt& to_bind) const = 0;
};

class mysql_multi_insert {
  const std::string _query;
  const unsigned _nb_column;
  const std::string _on_duplicate_key_part;

  std::list<row_filler::pointer> _rows;

 public:
  mysql_multi_insert(const std::string& query,
                     unsigned nb_column,
                     const std::string& on_duplicate_key_part)
      : _query(query),
        _nb_column(nb_column),
        _on_duplicate_key_part(on_duplicate_key_part) {}

  mysql_multi_insert(const mysql_multi_insert&) = delete;
  mysql_multi_insert& operator=(const mysql_multi_insert&) = delete;

  /**
   * @brief push data into the request
   * data will be bound to query during push_request call
   *
   * @param data data that will be bound
   */
  inline void push(row_filler::pointer&& data) {
    _rows.emplace_back(std::move(data));
  }

  unsigned push_stmt(mysql& pool, int thread_id = -1) const;
};

}  // namespace database

CCB_END()

#endif  // CCB_MYSQL_MULTI_INSERT_HH
