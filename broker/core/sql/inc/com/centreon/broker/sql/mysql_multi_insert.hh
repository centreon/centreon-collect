/**
 * Copyright 2023-2024 Centreon
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

#ifndef CCB_MYSQL_MULTI_INSERT_HH
#define CCB_MYSQL_MULTI_INSERT_HH

#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/broker/sql/mysql_bulk_bind.hh"
#include "com/centreon/broker/sql/mysql_bulk_stmt.hh"
#include "com/centreon/broker/sql/mysql_stmt.hh"

namespace com::centreon::broker {

namespace database {

/**
 * @brief this class emulates a bulk insert by create as many multi insert
 * queries as necessary (INSERT INTO toto (col1, col2,..) VALUES (col0row0,
 * col1row0...) (col0row1, col1row1,...) )
 *
 */
class mysql_multi_insert {
  // The base query, something like "INSERT INTO toto (col1, col2, ...) VALUES"
  const std::string _query;

  // The second part of the query, like "ON DUPLICATE KEY UPDATE...". This part
  // may be empty.
  const std::string _on_duplicate_key_part;
  const unsigned _max_query_begin_length;

  // The list of queries. The first one is filled and once its length arrives
  // to _max_query_begin_length, a new one is appended.
  std::list<std::string> _queries;

 public:
  mysql_multi_insert(const std::string& query,
                     const std::string& on_duplicate_key_part);

  mysql_multi_insert(const mysql_multi_insert& src) = delete;

  mysql_multi_insert& operator=(const mysql_multi_insert&) = delete;

  /**
   * @brief push data into the request
   * data will be bound to query during push_request call
   *
   * @param data data that will be bound it must be like that "data1,
   * 'data_string2', data3,..." without ()
   */
  void push(const std::string& data);

  /**
   * @brief clear _rows
   * needed for reuse this class after push_queries
   *
   */
  void clear_queries() { _queries.clear(); }

  unsigned execute_queries(mysql& pool,
                           my_error::code ec = my_error::empty,
                           int thread_id = -1);
};

/**
 * @brief the goal of this class is to simplify request declaration when you
 * have to deal both with bulk queries and multi insert
 * It has two constructor one who will use bulk queries only available on
 * mariadb and multi insert Then You have to call execute to execute query(ies)
 * It's the base class of bulk_or_multi_bbdo_event
 *
 * to fill rows:
 * - bulk_case: you have to call add_bulk_row with a functor as
 *    void(mysql_bulk_bind&)
 *    this functor must call bulk_event_binder::next_row() at the end
 * - multi insert: you have either to pass string data as (5,'erzerez',...)
 *   to add_multi_row
 *   or pass a std::string() functor to add_multi_row
 *
 * bulk example:
 * @code {.c++}
 *    bulk_or_multi toto;
 *    auto bulk_binder = [](database::mysql_bulk_bind & b) {
 *      b.set_value_as_str(0,'rterteztfd');
 *    };
 *    toto.add_bulk_row(bulk_binder);
 * @endcode
 *
 * multi example:
 * @code {.c++}
 *    bulk_or_multi toto;
 *    auto multi_binder = []() {
 *       return "('dfsvertvrt',5,4)";
 *    };
 *    toto.add_multi_row(multi_binder);
 *    // OR
 *    toto.add_multi_row("('dfsvertvrt',5,4)");
 * @endcode
 *
 */
class bulk_or_multi {
 protected:
  // mariadb attributes
  std::unique_ptr<mysql_bulk_stmt> _bulk_stmt;
  std::unique_ptr<mysql_bulk_bind> _bulk_bind;
  unsigned _bulk_row;
  unsigned _row_count;
  std::chrono::system_clock::time_point _first_row_add_time;

  std::chrono::system_clock::duration _execute_delay_ready;
  unsigned _row_count_ready;

  // multi insert attribute used when bulk queries aren't available
  std::unique_ptr<mysql_multi_insert> _mult_insert;

  mutable std::mutex _protect;

 public:
  bulk_or_multi(mysql& connexion,
                const std::string& request,
                unsigned bulk_row,
                const std::shared_ptr<spdlog::logger>& logger,
                const std::chrono::system_clock::duration execute_delay_ready =
                    std::chrono::seconds(10),
                unsigned row_count_ready = 100000);

  bulk_or_multi(const std::string& query,
                const std::string& on_duplicate_key_part,
                const std::chrono::system_clock::duration execute_delay_ready =
                    std::chrono::seconds(10),
                unsigned row_count_ready = 100000);

  void execute(mysql& connexion,
               my_error::code ec = my_error::empty,
               int thread_id = -1);

  void on_add_row();

  bool ready();

  unsigned row_count() const { return _row_count; }
  std::chrono::seconds get_oldest_waiting_event_delay() const;

  /**
   * @brief Tell if this bulk_or_multi is based on a bulk prepared statement or
   * not.
   *
   * @return a boolean.
   */
  bool is_bulk() const { return _bulk_stmt.get(); }

  /**
   * @brief Add new data to the query. This method *must* be used only when
   * this bulk_or_multi class is based on a bulk prepared statement.
   *
   * @tparam binder_functor This is a void function that takes an argument
   * database::mysql_bulk_bind&. It is used to fill the bind given in parameter.
   * @param filler A function of type binder_functor.
   */
  template <typename binder_functor>
  void add_bulk_row(const binder_functor& filler) {
    std::lock_guard<std::mutex> l(_protect);
    filler(*_bulk_bind);
    on_add_row();
  }

  /**
   * @brief Add new data to the query. This method *must* be used only when
   * bulk_or_multi does not support bulk prepared statement.
   *
   * @tparam query_filler_functor This is a function with no argument returning
   * a string of the form "(XX,'YY',ZZ,...)" so that the result can be added
   * to the query.
   * @param filler A function of type query_fill_functor.
   */
  template <typename query_filler_functor>
  void add_multi_row(const query_filler_functor& filler) {
    std::lock_guard<std::mutex> l(_protect);
    _mult_insert->push(filler());
    on_add_row();
  }

  /**
   * @brief Add new data to the query. This method *must* be used only when
   * bulk_or_multi does not support bulk prepared statement.
   *
   * @param query_data A string to concatenate to the query. The string should
   * be of the form "(XX,'YY',ZZ,...)"
   */
  void add_multi_row(const std::string& query_data) {
    std::lock_guard<std::mutex> l(_protect);
    _mult_insert->push(query_data);
    on_add_row();
  }

  void lock() { _protect.lock(); }
  void unlock() { _protect.unlock(); };
};

}  // namespace database

}  // namespace com::centreon::broker

#endif  // CCB_MYSQL_MULTI_INSERT_HH
