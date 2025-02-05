/**
 * Copyright 2023 Centreon
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

#include "com/centreon/broker/sql/mysql_stmt_base.hh"

#include <cfloat>
#include <cmath>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/misc/string.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using com::centreon::common::log_v2::log_v2;

/**
 * @brief Constructor.
 */
mysql_stmt_base::mysql_stmt_base(bool bulk,
                                 const std::shared_ptr<spdlog::logger>& logger)
    : _bulk(bulk), _logger(logger) {}

/**
 * @brief Constructor of a mysql_stmt_base from a SQL query template. This
 * template can be named or not, i.e. respectively like UPDATE foo SET
 * a=:a_value or UPDATE foo SET a=?
 *
 * @param query The query template
 * @param named a boolean telling if the query is named or not (with ?).
 * @param bulk a boolean telling if the stmt is a bulk prepared statement or
 * not.
 */
mysql_stmt_base::mysql_stmt_base(const std::string& query,
                                 bool named,
                                 bool bulk,
                                 const std::shared_ptr<spdlog::logger>& logger)
    : _bulk(bulk), _logger(logger) {
  mysql_bind_mapping bind_mapping;
  std::hash<std::string> hash_fn;
  if (named) {
    std::string q;
    q.reserve(query.size());
    bool in_string(false);
    char open(0);
    int size(0);
    for (std::string::const_iterator it = query.begin(), end = query.end();
         it != end; ++it) {
      if (in_string) {
        if (*it == '\\') {
          q.push_back(*it);
          ++it;
          /* In case of it == end, the query is badly written. It would be a
           * bug in Broker. */
          assert(it != end);
          q.push_back(*it);
        } else {
          q.push_back(*it);
          if (*it == open)
            in_string = false;
        }
      } else {
        if (*it == ':') {
          std::string::const_iterator itt(it + 1);
          while (itt != end && (isalnum(*itt) || *itt == '_'))
            ++itt;
          std::string key(it, itt);
          mysql_bind_mapping::iterator fkit(bind_mapping.find(key));
          if (fkit != bind_mapping.end()) {
            int value(fkit->second);
            bind_mapping.erase(fkit);
            key.push_back('1');
            bind_mapping.insert(std::make_pair(key, value));
            key[key.size() - 1] = '2';
            bind_mapping.insert(std::make_pair(key, size));
          } else
            bind_mapping.insert(std::make_pair(key, size));

          ++size;
          it = itt - 1;
          q.push_back('?');
        } else {
          if (*it == '\'' || *it == '"') {
            in_string = true;
            open = *it;
          }
          q.push_back(*it);
        }
      }
    }
    _id = hash_fn(q);
    _query = q;
    _bind_mapping = bind_mapping;
    _param_count = bind_mapping.size();
  } else {
    _id = hash_fn(query);
    _query = query;

    // How many '?' in the query, we don't count '?' in strings.
    _param_count = _compute_param_count(query);
  }
}

/**
 * @brief Constructor of a mysql_stmt_base from a not named query template and a
 * correspondance table making the relation between column names and their
 * indices.
 *
 * @param query
 * @param bind_mapping
 * @param bulk is this a bulk prepared statement or not?
 */
mysql_stmt_base::mysql_stmt_base(const std::string& query,
                                 bool bulk,
                                 const std::shared_ptr<spdlog::logger>& logger,
                                 const mysql_bind_mapping& bind_mapping)
    : _bulk(bulk),
      _id(std::hash<std::string>{}(query)),
      _query(query),
      _bind_mapping(bind_mapping),
      _logger(logger) {
  if (bind_mapping.empty())
    _param_count = _compute_param_count(query);
  else
    _param_count = bind_mapping.size();
}

/**
 *  Move constructor
 */
mysql_stmt_base::mysql_stmt_base(mysql_stmt_base&& other)
    : _bulk(other._bulk),
      _id(std::move(other._id)),
      _param_count(std::move(other._param_count)),
      _query(std::move(other._query)),
      _bind_mapping(std::move(other._bind_mapping)),
      _pb_mapping(std::move(other._pb_mapping)),
      _logger{std::move(other._logger)} {}

/**
 * @brief Move copy
 *
 * @param other the statement to move.
 *
 * @return a reference to the self statement.
 */
mysql_stmt_base& mysql_stmt_base::operator=(mysql_stmt_base&& other) {
  if (this != &other) {
    _id = std::move(other._id);
    _param_count = std::move(other._param_count);
    _query = std::move(other._query);
    _bind_mapping = std::move(other._bind_mapping);
    _pb_mapping = std::move(other._pb_mapping);
  }
  return *this;
}

/**
 * @brief Compute the number of parameters in the query template. In other
 * words, it computes the number of '?' characters that must be replaced by
 * parameters in a prepared statement.
 *
 * @param query The query template.
 *
 * @return A size_t integer.
 */
size_t mysql_stmt_base::_compute_param_count(const std::string& query) {
  size_t retval = 0u;
  bool in_string{false}, jocker{false};
  for (std::string::const_iterator it = query.begin(), end = query.end();
       it != end; ++it) {
    if (!in_string) {
      if (*it == '?')
        ++retval;
      else if (*it == '\'' || *it == '"')
        in_string = true;
    } else {
      if (jocker)
        jocker = false;
      else if (*it == '\\')
        jocker = true;
      else if (*it == '\'' || *it == '"')
        in_string = false;
    }
  }
  return retval;
}

/**
 * @brief Return True if the prepared statement is prepared.
 *
 * @return True on success, False otherwise.
 */
bool mysql_stmt_base::prepared() const {
  return _id != 0;
}

/**
 * @brief Accessor to the id of the prepared statement.
 *
 * @return A uint32_t integer (0 if not prepared).
 */
uint32_t mysql_stmt_base::get_id() const {
  return _id;
}

/**
 * @brief Accessor to the query stored in the prepared statement.
 *
 * @return A reference to the query.
 */
const std::string& mysql_stmt_base::get_query() const {
  return _query;
}

/**
 * @brief Return the number of '?' characters that have to be replaced in the
 * prepared statement.
 *
 * @return an integer.
 */
size_t mysql_stmt_base::get_param_count() const {
  return _param_count;
}

/**
 * @brief Set the mapping between fields of a protobuf bbdo object to the
 * prepared statement.
 * For each element in the mapping, we have 3 elements:
 * * the name of the field
 * * the max length in case of string field, 0 otherwise.
 * * some attributes see the class com::centreon::broker::mapping::entry for
 *   more details.
 *
 * @param mapping A vector with the mapping.
 */
void mysql_stmt_base::set_pb_mapping(
    std::vector<std::tuple<std::string, uint32_t, uint16_t>>&& mapping) {
  _pb_mapping = std::move(mapping);
}

#define BIND_VALUE(ftype, vtype)                                               \
  void mysql_stmt_base::bind_value_as_##ftype##_k(const std::string& name,     \
                                                  vtype value) {               \
    mysql_bind_mapping::iterator it(_bind_mapping.find(name));                 \
    if (it != _bind_mapping.end()) {                                           \
      bind_value_as_##ftype(it->second, value);                                \
    } else {                                                                   \
      std::string key(name);                                                   \
      key.append("1");                                                         \
      it = _bind_mapping.find(key);                                            \
      if (it != _bind_mapping.end()) {                                         \
        bind_value_as_##ftype(it->second, value);                              \
        key[key.size() - 1] = '2';                                             \
        it = _bind_mapping.find(key);                                          \
        if (it != _bind_mapping.end())                                         \
          bind_value_as_##ftype(it->second, value);                            \
        else                                                                   \
          _logger->error("mysql: cannot bind object with name '{}' to " #ftype \
                         " value {} in "                                       \
                         "statement {}",                                       \
                         name, value, get_id());                               \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  void mysql_stmt_base::bind_null_##ftype##_k(const std::string& name) {       \
    mysql_bind_mapping::iterator it(_bind_mapping.find(name));                 \
    if (it != _bind_mapping.end()) {                                           \
      bind_null_##ftype(it->second);                                           \
    } else {                                                                   \
      std::string key(name);                                                   \
      key.append("1");                                                         \
      it = _bind_mapping.find(key);                                            \
      if (it != _bind_mapping.end()) {                                         \
        bind_null_##ftype(it->second);                                         \
        key[key.size() - 1] = '2';                                             \
        it = _bind_mapping.find(key);                                          \
        if (it != _bind_mapping.end())                                         \
          bind_null_##ftype(it->second);                                       \
        else                                                                   \
          _logger->error("mysql: cannot bind object with name '{}' to " #ftype \
                         " null value in "                                     \
                         "statement {}",                                       \
                         name, get_id());                                      \
      }                                                                        \
    }                                                                          \
  }

BIND_VALUE(i32, int32_t)
BIND_VALUE(u32, uint32_t)
BIND_VALUE(i64, int64_t)
BIND_VALUE(u64, uint64_t)
BIND_VALUE(f32, float)
BIND_VALUE(f64, double)
BIND_VALUE(tiny, char)
BIND_VALUE(bool, bool)
BIND_VALUE(str, const fmt::string_view&)

#undef BIND_VALUE

const std::vector<std::tuple<std::string, uint32_t, uint16_t>>&
mysql_stmt_base::get_pb_mapping() const {
  return _pb_mapping;
}

void mysql_stmt_base::bind_value_as_i64_ext(size_t range,
                                            int64_t value,
                                            uint32_t invalid_on) {
  if (invalid_on & mapping::entry::invalid_on_zero) {
    if (value == 0) {
      bind_null_i64(range);
      return;
    }
  }
  if (invalid_on & mapping::entry::invalid_on_minus_one) {
    if (value == -1) {
      bind_null_i64(range);
      return;
    }
  }
  if (invalid_on & mapping::entry::invalid_on_negative) {
    if (value < 0) {
      bind_null_i64(range);
      return;
    }
  }
  bind_value_as_i64(range, value);
}

void mysql_stmt_base::bind_value_as_u64_ext(size_t range,
                                            uint64_t value,
                                            uint32_t invalid_on) {
  if (value == 0 && (invalid_on & mapping::entry::invalid_on_zero))
    bind_null_u64(range);
  else
    bind_value_as_u64(range, value);
}
