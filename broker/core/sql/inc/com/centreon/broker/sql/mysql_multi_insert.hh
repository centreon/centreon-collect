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

#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/broker/sql/mysql_bulk_bind.hh"
#include "com/centreon/broker/sql/mysql_bulk_stmt.hh"
#include "com/centreon/broker/sql/mysql_stmt.hh"

CCB_BEGIN()

namespace database {

/**
 * @brief this class emulates a bulk insert by create as many multi insert
 * queries as necessary (INSERT INTO toto (col1, col2,..) VALUES (col0row0,
 * col1row0...) (col0row1, col1row1,...) )
 *
 */
class mysql_multi_insert {
  const std::string _query;
  const std::string _on_duplicate_key_part;
  unsigned _max_data_length;

  std::list<std::string> _queries_data;

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
  void clear_queries() { _queries_data.clear(); }

  unsigned execute_queries(mysql& pool,
                           my_error::code ec = my_error::empty,
                           int thread_id = -1) const;
};

/**
 * @brief the goal of this class is to simplify request declaration when you
 * have to deal both with bulk queries and multi insert
 * It has two constructor one who will use bulk queries only available on
 * mariadb and multi insert Then You have to call execute to execute query(ies)
 * It's the base class of bulk_or_multi_bbdo_event
 *
 * This class can be used in two ways:
 * - provide two lambda for use with create_bulk_or_multi_bbdo_event
 * - direct fill _bulk_bind  (don't  forget to call _bulk_bind->next_row()) or
 * _mult_insert
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
                const std::chrono::system_clock::duration execute_delay_ready,
                unsigned row_count_ready);

  bulk_or_multi(const std::string& query,
                const std::string& on_duplicate_key_part,
                const std::chrono::system_clock::duration execute_delay_ready,
                unsigned row_count_ready);

  virtual ~bulk_or_multi() = default;

  void execute(mysql& connexion);

  void on_add_event();

  bool ready();

  virtual void add_event(const std::shared_ptr<io::data>& event){};

  unsigned row_count() const { return _row_count; }
  std::chrono::seconds get_oldest_waiting_event_delay() const;

  bool is_bulk() const { return _bulk_bind.get(); }

  /**
   * @brief accessor to direct fill bulk_bind
   *
   * @return mysql_bulk_bind&
   */
  mysql_bulk_bind& bulk_bind() { return *_bulk_bind; }

  /**
   * @brief accessor to direct fill multi_insert
   *
   * @return mysql_multi_insert&
   */
  mysql_multi_insert& multi_insert() { return *_mult_insert; }

  void lock() { _protect.lock(); }
  void unlock() { _protect.unlock(); };
};

/**
 * @brief this class can do a multi or bulk insert in a single interface
 * binder_lambda must be a functor with the signature
 * void (const std::shared_ptr<io::data>& event, database::mysql_bind_base*
 * binder) in case of bulk usage
 * void (const std::shared_ptr<io::data>& event, std::string &query) in case of
 * multi insert usage
 *  To use it, you must use one of the two constructor (bulk
 * or multiinsert) with the functor in the last argument. Then for each event to
 * record you need to call add_event method
 * When you have finished call execute
 *
 * @code {.c++}
 * static auto event_binder = [](const std::shared_ptr<io::data>& event,
 *                             database::mysql_bulk_bind* binder) {
 *   binder->set_value_as_str(0, fmt::format("toto{}", event_binder_index));
 *   binder->set_value_as_f64(1, 12.34 + event_binder_index);
 * };
 * static auto mysql_event_binder = [](const std::shared_ptr<io::data>& event,
 *                             std::string & query) {
 *   query.append(fmt::format("'toto{}',{}",
 * event_binder_index, 12.34+event_binder_index));
 * };
 *
 *
 * if (_mysql.support_bulk_statement()) {
 *    inserter = database::create_bulk_or_multi_bbdo_event(
 *     *mysql, "INSERT INTO ut_test (name, value) VALUES (?,?)",
 *     10, event_binder);
 * }
 * else {
 *    inserter = database::create_bulk_or_multi_bbdo_event(
 *     "INSERT INTO ut_test (name, value) VALUES", "",
 *     mysql_event_binder);
 * }
 * inserter->add_event(evt);
 * inserter->execute(*_mysql);
 * @endcode
 *
 *
 * @tparam binder_lambda bulk mariadb binder
 * @tparam bulk  boolean true if use of bulk statement
 */
template <typename binder_lambda, bool bulk>
class bulk_or_multi_bbdo_event;

// bulk specialisation
template <typename binder_lambda>
class bulk_or_multi_bbdo_event<binder_lambda, true> : public bulk_or_multi {
 protected:
  binder_lambda& _binder;

 public:
  /**
   * @brief bulk constructor to use when bulk queries are available
   *
   * @param connexion
   * @param request query
   * @param bulk_row number of row in one bulk request
   * @param binder this void(const std::shared_ptr<io::data>& event,
   * database::mysql_bulk_bind* binder) find bulk_bind with the event
   * @param execute_delay_ready when first event add is older than
   * execute_delay_ready, ready() return true
   * @param row_count_ready when row_count_ready have been added to the query,
   * ready() return true
   */
  bulk_or_multi_bbdo_event(
      mysql& connexion,
      const std::string& request,
      unsigned bulk_row,
      binder_lambda& binder,
      const std::chrono::system_clock::duration execute_delay_ready,
      unsigned row_count_ready)
      : bulk_or_multi(connexion,
                      request,
                      bulk_row,
                      execute_delay_ready,
                      row_count_ready),
        _binder(binder) {}

  /**
   * @brief in case of bulk, this method binds event to stmt bind by using
   * binder functor in cas of multi insert, it saves event in a queue, the event
   * will be bound to stmt when requests will be executed
   *
   * @param event
   */
  void add_event(const std::shared_ptr<io::data>& event) override {
    bulk_or_multi::on_add_event();
    _binder(event, _bulk_bind.get());
    _bulk_bind->next_row();
  }
};

// non bulk specialization
template <typename binder_lambda>
class bulk_or_multi_bbdo_event<binder_lambda, false> : public bulk_or_multi {
 protected:
  binder_lambda& _binder;

 public:
  /**
   * @brief multi_insert constructor to use when bulk queries aren't available
   *
   * @param query
   * @param on_duplicate_key_part end of the request as ON DUPLICATE KEY UPDATE
   * current_level=VALUES(current_level)
   * @param binder this void(const std::shared_ptr<io::data>& event, std::string
   * & query) append data to the query, don't provide ( and )
   * @param execute_delay_ready when first event add is older than
   * execute_delay_ready, ready() return true
   * @param row_count_ready when row_count_ready have been added to the query,
   * ready() return true
   */
  bulk_or_multi_bbdo_event(
      const std::string& query,
      const std::string& on_duplicate_key_part,
      binder_lambda& binder,
      const std::chrono::system_clock::duration execute_delay_ready,
      unsigned row_count_ready)

      : bulk_or_multi(query,
                      on_duplicate_key_part,
                      execute_delay_ready,
                      row_count_ready),
        _binder(binder) {}

  /**
   * @brief in case of bulk, this method binds event to stmt bind by using
   * binder functor in cas of multi insert, it saves event in a queue, the event
   * will be bound to stmt when requests will be executed
   *
   * @param event
   */
  void add_event(const std::shared_ptr<io::data>& event) override {
    bulk_or_multi::on_add_event();
    std::string data_to_add;
    _binder(event, data_to_add);
    _mult_insert->push(data_to_add);
  }
};

/**
 * @brief Create a bulk_or_multi_bbdo_event object with lambda type deduction
 * (bulk case)
 *
 * @tparam binder_lambda void(const std::shared_ptr<io::data>& event,
 * database::mysql_bulk_bind* binder) (deduced)
 * @param connexion
 * @param request query
 * @param bulk_row number of row in one bulk request
 * @param binder this void(const std::shared_ptr<io::data>& event,
 * database::mysql_bulk_bind* binder) find bulk_bind with the event
 * @param execute_delay_ready when first event add is older than
 * execute_delay_ready, ready() return true
 * @param row_count_ready when row_count_ready have been added to the query,
 * ready() return true
 * @return std::unique_ptr<bulk_or_multi>
 */
template <typename binder_lambda>
std::unique_ptr<bulk_or_multi> create_bulk_or_multi_bbdo_event(
    mysql& connexion,
    const std::string& request,
    unsigned bulk_row,
    binder_lambda& binder,
    const std::chrono::system_clock::duration execute_delay_ready =
        std::chrono::seconds(10),
    unsigned row_count_ready = 100000) {
  return std::unique_ptr<bulk_or_multi>(
      new bulk_or_multi_bbdo_event<binder_lambda, true>(
          connexion, request, bulk_row, binder, execute_delay_ready,
          row_count_ready));
}

/**
 * @brief Create a bulk_or_multi_bbdo_event object with lambda type deduction
 * (multi insert case)
 *
 * @tparam binder_lambda void(const std::shared_ptr<io::data>& event,
 * std::string & query) (deduced)
 * @param query
 * @param on_duplicate_key_part end of the request as ON DUPLICATE KEY UPDATE
 * current_level=VALUES(current_level)
 * @param binder this void(const std::shared_ptr<io::data>& event, std::string
 * & query) append data to the query, don't provide ( and )
 * @param execute_delay_ready when first event add is older than
 * execute_delay_ready, ready() return true
 * @param row_count_ready when row_count_ready have been added to the query,
 * ready() return true
 * @return std::unique_ptr<bulk_or_multi>
 */
template <typename binder_lambda>
std::unique_ptr<bulk_or_multi> create_bulk_or_multi_bbdo_event(
    const std::string& query,
    const std::string& on_duplicate_key_part,
    binder_lambda& binder,
    const std::chrono::system_clock::duration execute_delay_ready =
        std::chrono::seconds(10),
    unsigned row_count_ready = 100000) {
  return std::unique_ptr<bulk_or_multi>(
      new bulk_or_multi_bbdo_event<binder_lambda, false>(
          query, on_duplicate_key_part, binder, execute_delay_ready,
          row_count_ready));
}

}  // namespace database

CCB_END()

#endif  // CCB_MYSQL_MULTI_INSERT_HH
