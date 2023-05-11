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

#include "com/centreon/broker/database/mysql_bulk_bind.hh"
#include "com/centreon/broker/database/mysql_bulk_stmt.hh"
#include "com/centreon/broker/database/mysql_stmt.hh"
#include "com/centreon/broker/mysql.hh"

CCB_BEGIN()

namespace database {

/**
 * @brief this class only provide a similar interface to bulk_bind
 * it's used by row_filler class described above
 *
 */
class stmt_binder : public mysql_bind_base {
  unsigned _stmt_first_column;
  mysql_stmt& _to_bind;

 public:
  stmt_binder(mysql_stmt& to_bind) : _stmt_first_column(0), _to_bind(to_bind) {}

  void inc_stmt_first_column(unsigned nb_columns) {
    _stmt_first_column += nb_columns;
  }

  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_DOUBLE.
   *
   * @param range A non negative integer.
   * @param value The double value to set.
   */
  void set_value_as_f64(size_t range, double value) override {
    _to_bind.bind_value_as_f64(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   * @param value The integer value to set.
   */
  void set_value_as_i32(size_t range, int32_t value) override {
    _to_bind.bind_value_as_i32(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONG.
   *
   * @param range A non negative integer.
   * @param value The unsigned integer value to set.
   */
  void set_value_as_u32(size_t range, uint32_t value) override {
    _to_bind.bind_value_as_u32(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG. The value must not
   * match the invalid_on bitfield, otherwise the value is set to NULL.
   *
   * @param range A non negative integer.
   * @param value The unsigned long integer value to set.
   */
  void set_value_as_u64(size_t range, uint64_t value) override {
    _to_bind.bind_value_as_u64(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   * @param value The long integer value to set.
   */
  void set_value_as_i64(size_t range, int64_t value) override {
    _to_bind.bind_value_as_i64(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of NULL at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_LONGLONG.
   *
   * @param range A non negative integer.
   */
  void set_null_u64(size_t range) override { _to_bind.bind_null_u64(range); }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_TINY.
   *
   * @param range A non negative integer.
   * @param value The boolean value to set.
   */
  void set_value_as_bool(size_t range, bool value) override {
    _to_bind.bind_value_as_bool(range + _stmt_first_column, value);
  }
  /**
   * @brief Setter of the value at the column at index range and at the current
   * row. The type of the column must be MYSQL_TYPE_STRING.
   *
   * @param range A non negative integer.
   * @param value The string to set.
   */
  void set_value_as_str(size_t range, const fmt::string_view& value) override {
    _to_bind.bind_value_as_str(range + _stmt_first_column, value);
  }
  /**
   * @brief Set the given value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   * @param value The value to set.
   */
  void set_value_as_tiny(size_t range, char value) override {
    _to_bind.bind_value_as_tiny(range + _stmt_first_column, value);
  }
  /**
   * @brief Set the NULL value at the column in the prepared statement at index
   * range in the current row of the column. The type of the column must be
   * MYSQL_TYPE_TINY.
   *
   * @param range Index of the column(from 0).
   */
  void set_null_tiny(size_t range) override {
    _to_bind.bind_null_tiny(range + _stmt_first_column);
  }
};

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
 *    inline void fill_row(stmt_binder& to_bind) const override {
 *      to_bind.set_value_as_str(0, data->name);
 *      to_bind.set_value_as_f64(1, data->value);
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
  virtual void fill_row(stmt_binder& to_bind) const = 0;
};

/**
 * @brief this class emulates a bulk insert by create as many multi insert
 * statements as necessary (INSERT INTO toto (col1, col2,..) VALUES (col0row0,
 * col1row0...) (col0row1, col1row1,...) )
 *
 */
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

  mysql_multi_insert(const mysql_multi_insert& src)
      : _query(src._query),
        _nb_column(src._nb_column),
        _on_duplicate_key_part(src._on_duplicate_key_part) {}

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

  size_t rows_count() const { return _rows.size(); }
};

/**
 * @brief the goal of this class is to simplify request declaration when you
 * have to deal both with bulk queries and multi insert
 *
 */
class bulk_or_multi {
 protected:
  std::unique_ptr<mysql_bulk_stmt> _bulk_stmt;
  std::unique_ptr<mysql_bulk_bind> _bulk_bind;
  unsigned _bulk_row;

  std::unique_ptr<mysql_multi_insert> _mult_insert;

 public:
  bulk_or_multi(mysql& connexion,
                const std::string& request,
                unsigned bulk_row);

  bulk_or_multi(const std::string& query,
                unsigned nb_column,
                const std::string& on_duplicate_key_part);

  virtual ~bulk_or_multi() = default;

  void execute(mysql& connexion);

  virtual void add_event(const std::shared_ptr<io::data>& event);
};

/**
 * @brief this class can do a multi or bulk insert in a single interface
 * binder_lambda must be a functor with teh signature
 * void (const std::shared_ptr<io::data>& event, database::mysql_bind_base*
 * binder) To use it, you must use one of the two constructor (bulk or
 * multiinsert) with the functor in the las argument.
 * Then for each event to record you need to call add_event method
 *
 * @tparam binder_lambda
 */
template <typename binder_lambda>
class bulk_or_multi_bbdo_event : public bulk_or_multi {
 protected:
  binder_lambda& _binder;

 public:
  /**
   * @brief bulk constructor to use when bulk queries are available
   *
   * @param connexion
   * @param request
   * @param bulk_row
   */
  bulk_or_multi_bbdo_event(mysql& connexion,
                           const std::string& request,
                           unsigned bulk_row,
                           binder_lambda& binder)
      : bulk_or_multi(connexion, request, bulk_row), _binder(binder) {}

  /**
   * @brief multi_insert constructor to use when bulk queries aren't available
   *
   * @param query
   * @param nb_column
   * @param on_duplicate_key_part
   */
  bulk_or_multi_bbdo_event(const std::string& query,
                           unsigned nb_column,
                           const std::string& on_duplicate_key_part,
                           binder_lambda& binder)
      : bulk_or_multi(query, nb_column, on_duplicate_key_part),
        _binder(binder) {}

  /**
   * @brief in case of bulk, this method binds event to stmt bind by using
   * binder functor in cas of multi insert, it saves event in a queue, the event
   * will be bound to stmt when requests will be executed
   *
   * @param event
   */
  void add_event(const std::shared_ptr<io::data>& event) override {
    if (_bulk_bind) {
      _binder(event, _bulk_bind.get());
    } else {
      class final_row_filler : public database::row_filler {
        std::shared_ptr<io::data> _event;
        binder_lambda& _binder;

       public:
        final_row_filler(const std::shared_ptr<io::data>& event,
                         binder_lambda& binder)
            : _event(event), _binder(binder) {}

        void fill_row(stmt_binder& to_bind) const override {
          _binder(_event, &to_bind);
        }
      };

      _mult_insert->push(std::make_unique<final_row_filler>(event, _binder));
    }
  }
};

/**
 * @brief Create a bulk_or_multi_bbdo_event object with lambda type deduction
 * (bulk case)
 *
 * @tparam binder_lambda
 * @param connexion
 * @param request
 * @param bulk_row
 * @param binder
 * @return std::unique_ptr<bulk_or_multi>
 */
template <typename binder_lambda>
std::unique_ptr<bulk_or_multi> create_bulk_or_multi_bbdo_event(
    mysql& connexion,
    const std::string& request,
    unsigned bulk_row,
    binder_lambda& binder) {
  return std::unique_ptr<bulk_or_multi>(
      new bulk_or_multi_bbdo_event<binder_lambda>(connexion, request, bulk_row,
                                                  binder));
}

/**
 * @brief Create a bulk_or_multi_bbdo_event object with lambda type deduction
 * (multi insert case)
 *
 * @tparam binder_lambda
 * @param query
 * @param nb_column
 * @param on_duplicate_key_part
 * @param binder
 * @return std::unique_ptr<bulk_or_multi>
 */
template <typename binder_lambda>
std::unique_ptr<bulk_or_multi> create_bulk_or_multi_bbdo_event(
    const std::string& query,
    unsigned nb_column,
    const std::string& on_duplicate_key_part,
    binder_lambda& binder) {
  return std::unique_ptr<bulk_or_multi>(
      new bulk_or_multi_bbdo_event<binder_lambda>(
          query, nb_column, on_duplicate_key_part, binder));
}

}  // namespace database

CCB_END()

#endif  // CCB_MYSQL_MULTI_INSERT_HH
