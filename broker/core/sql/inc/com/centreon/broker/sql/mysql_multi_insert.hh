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
 *    inline void fill_row(std::string & query) const override {
 *      query.append(fmt::format("'{}',{}", data->name, data->value))
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
  virtual void fill_row(std::string& query) const = 0;
};

/**
 * @brief this class emulates a bulk insert by create as many multi insert
 * queries as necessary (INSERT INTO toto (col1, col2,..) VALUES (col0row0,
 * col1row0...) (col0row1, col1row1,...) )
 *
 */
class mysql_multi_insert {
  const std::string _query;
  const std::string _on_duplicate_key_part;

  std::list<row_filler::pointer> _rows;

 public:
  mysql_multi_insert(const std::string& query,
                     const std::string& on_duplicate_key_part)
      : _query(query), _on_duplicate_key_part(on_duplicate_key_part) {}

  mysql_multi_insert(const mysql_multi_insert& src) = delete;

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

  /**
   * @brief clear _rows
   * needed for reuse this class after push_stmt
   *
   */
  void clear_rows() { _rows.clear(); }

  unsigned push_stmt(mysql& pool, int thread_id = -1) const;

  size_t rows_count() const { return _rows.size(); }
};

/**
 * @brief the goal of this class is to simplify request declaration when you
 * have to deal both with bulk queries and multi insert
 * It has two constructor one who will use bulk queries only available on
 * mariadb and multi insert Then You have to call execute to execute query(ies)
 * It's the base class of bulk_or_multi_bbdo_event
 *
 */
class bulk_or_multi {
 protected:
  // mariadb attributes
  std::unique_ptr<mysql_bulk_stmt> _bulk_stmt;
  std::unique_ptr<mysql_bulk_bind> _bulk_bind;
  unsigned _bulk_row;

  // multi insert attribute used when bulk queries aren't available
  std::unique_ptr<mysql_multi_insert> _mult_insert;

 public:
  bulk_or_multi(mysql& connexion,
                const std::string& request,
                unsigned bulk_row);

  bulk_or_multi(const std::string& query,
                const std::string& on_duplicate_key_part);

  virtual ~bulk_or_multi() = default;

  void execute(mysql& connexion);

  virtual void add_event(const std::shared_ptr<io::data>& event);
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
 * record you need to call add_event method When you have finished call execute
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
   */
  bulk_or_multi_bbdo_event(mysql& connexion,
                           const std::string& request,
                           unsigned bulk_row,
                           binder_lambda& binder)
      : bulk_or_multi(connexion, request, bulk_row), _binder(binder) {}

  /**
   * @brief in case of bulk, this method binds event to stmt bind by using
   * binder functor in cas of multi insert, it saves event in a queue, the event
   * will be bound to stmt when requests will be executed
   *
   * @param event
   */
  void add_event(const std::shared_ptr<io::data>& event) override {
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
   */
  bulk_or_multi_bbdo_event(const std::string& query,
                           const std::string& on_duplicate_key_part,
                           binder_lambda& binder)
      : bulk_or_multi(query, on_duplicate_key_part), _binder(binder) {}

  /**
   * @brief in case of bulk, this method binds event to stmt bind by using
   * binder functor in cas of multi insert, it saves event in a queue, the event
   * will be bound to stmt when requests will be executed
   *
   * @param event
   */
  void add_event(const std::shared_ptr<io::data>& event) override {
    // this local class is used at the query creation
    class final_row_filler : public database::row_filler {
      std::shared_ptr<io::data> _event;
      binder_lambda& _binder;

     public:
      final_row_filler(const std::shared_ptr<io::data>& event,
                       binder_lambda& binder)
          : _event(event), _binder(binder) {}

      void fill_row(std::string& query) const override {
        _binder(_event, query);
      }
    };

    _mult_insert->push(std::make_unique<final_row_filler>(event, _binder));
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
 * @return std::unique_ptr<bulk_or_multi>
 */
template <typename binder_lambda>
std::unique_ptr<bulk_or_multi> create_bulk_or_multi_bbdo_event(
    mysql& connexion,
    const std::string& request,
    unsigned bulk_row,
    binder_lambda& binder) {
  return std::unique_ptr<bulk_or_multi>(
      new bulk_or_multi_bbdo_event<binder_lambda, true>(connexion, request,
                                                        bulk_row, binder));
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
 * @return std::unique_ptr<bulk_or_multi>
 */
template <typename binder_lambda>
std::unique_ptr<bulk_or_multi> create_bulk_or_multi_bbdo_event(
    const std::string& query,
    const std::string& on_duplicate_key_part,
    binder_lambda& binder) {
  return std::unique_ptr<bulk_or_multi>(
      new bulk_or_multi_bbdo_event<binder_lambda, false>(
          query, on_duplicate_key_part, binder));
}

}  // namespace database

CCB_END()

#endif  // CCB_MYSQL_MULTI_INSERT_HH
