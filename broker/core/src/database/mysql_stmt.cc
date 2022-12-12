/*
** Copyright 2018-2022 Centreon
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

#include "com/centreon/broker/database/mysql_stmt.hh"

#include <cfloat>
#include <cmath>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

mysql_stmt::mysql_stmt() : _id(0), _param_count(0) {}

mysql_stmt::mysql_stmt(std::string const& query, bool named) {
  mysql_bind_mapping bind_mapping;
  std::hash<std::string> hash_fn;
  if (named) {
    std::string q;
    q.reserve(query.size());
    bool in_string(false);
    char open(0);
    int size(0);
    for (std::string::const_iterator it(query.begin()), end(query.end());
         it != end; ++it) {
      if (in_string) {
        if (*it == '\\') {
          q.push_back(*it);
          it++;
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
            bind_mapping.insert(std::make_pair(std::string(it, itt), size));

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

mysql_stmt::mysql_stmt(std::string const& query,
                       mysql_bind_mapping const& bind_mapping)
    : _id(std::hash<std::string>{}(query)),
      _query(query),
      _bind_mapping(bind_mapping) {
  if (bind_mapping.empty())
    _param_count = _compute_param_count(query);
  else
    _param_count = bind_mapping.size();
}

/**
 *  Move constructor
 */
mysql_stmt::mysql_stmt(mysql_stmt&& other)
    : _id(other._id),
      _param_count(other._param_count),
      _query(other._query),
      _bind(std::move(other._bind)),
      _bind_mapping(other._bind_mapping) {}

/**
 * @brief Move copy
 *
 * @param other the statement to move.
 *
 * @return a reference to the self statement.
 */
mysql_stmt& mysql_stmt::operator=(mysql_stmt&& other) {
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
 * @brief Copy operator
 *
 * @param other the statement to copy.
 *
 * @return a reference to the self statement.
 */
mysql_stmt& mysql_stmt::operator=(mysql_stmt const& other) {
  if (this != &other) {
    _id = other._id;
    _param_count = other._param_count;
    _query = other._query;
    _bind_mapping = other._bind_mapping;
    _pb_mapping = other._pb_mapping;
  }
  return *this;
}

int mysql_stmt::_compute_param_count(std::string const& query) {
  int retval(0);
  bool in_string(false), jocker(false);
  for (std::string::const_iterator it(query.begin()), end(query.end());
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

bool mysql_stmt::prepared() const {
  return _id != 0;
}

int mysql_stmt::get_id() const {
  return _id;
}

std::unique_ptr<database::mysql_bind> mysql_stmt::get_bind() {
  return std::move(_bind);
}

void mysql_stmt::operator<<(io::data const& d) {
  // Get event info.
  const io::event_info* info = io::events::instance().get_event_info(d.type());
  if (info) {
    if (info->get_mapping()) {
      for (const mapping::entry* current_entry(info->get_mapping());
           !current_entry->is_null(); ++current_entry) {
        char const* entry_name = current_entry->get_name_v2();
        if (entry_name && entry_name[0]) {
          std::string field{fmt::format(":{}", entry_name)};
          switch (current_entry->get_type()) {
            case mapping::source::BOOL:
              bind_value_as_bool(field, current_entry->get_bool(d));
              break;
            case mapping::source::DOUBLE:
              bind_value_as_f64(field, current_entry->get_double(d));
              break;
            case mapping::source::INT: {
              int v = current_entry->get_int(d);
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero:
                  if (v == 0)
                    bind_value_as_null(field);
                  else
                    bind_value_as_i32(field, v);
                  break;
                case mapping::entry::invalid_on_minus_one:
                  if (v == -1)
                    bind_value_as_null(field);
                  else
                    bind_value_as_i32(field, v);
                  break;
                case mapping::entry::invalid_on_negative:
                  if (v < 0)
                    bind_value_as_null(field);
                  else
                    bind_value_as_i32(field, v);
                  break;
                default:
                  bind_value_as_i32(field, v);
              }
            } break;
            case mapping::source::SHORT: {
              int v = current_entry->get_short(d);
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero:
                  if (v == 0)
                    bind_value_as_null(field);
                  else
                    bind_value_as_i32(field, v);
                  break;
                case mapping::entry::invalid_on_minus_one:
                  if (v == -1)
                    bind_value_as_null(field);
                  else
                    bind_value_as_i32(field, v);
                  break;
                case mapping::entry::invalid_on_negative:
                  if (v < 0)
                    bind_value_as_null(field);
                  else
                    bind_value_as_i32(field, v);
                  break;
                default:
                  bind_value_as_i32(field, v);
              }
            } break;
            case mapping::source::STRING: {
              size_t max_len = 0;
              const std::string& v(current_entry->get_string(d, &max_len));
              fmt::string_view sv;
              if (max_len > 0 && v.size() > max_len) {
                log_v2::sql()->trace(
                    "column '{}' should admit a longer string, it is cut to "
                    "{} "
                    "characters to be stored anyway.",
                    current_entry->get_name_v2(), max_len);
                max_len = misc::string::adjust_size_utf8(v, max_len);
                sv = fmt::string_view(v.data(), max_len);
              } else
                sv = fmt::string_view(v);
              if ((current_entry->get_attribute() &
                   mapping::entry::invalid_on_zero) &&
                  sv.size() == 0)
                bind_value_as_null(field);
              else
                bind_value_as_str(field, sv);
            } break;
            case mapping::source::TIME: {
              time_t v(current_entry->get_time(d));
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero:
                  if (v == 0)
                    bind_value_as_null(field);
                  else
                    bind_value_as_u32(field, v);
                  break;
                case mapping::entry::invalid_on_minus_one:
                  if (v == -1)
                    bind_value_as_null(field);
                  else
                    bind_value_as_u32(field, v);
                  break;
                case mapping::entry::invalid_on_negative:
                  if (v < 0)
                    bind_value_as_null(field);
                  else
                    bind_value_as_i32(field, v);
                  break;
                default:
                  bind_value_as_u32(field, v);
              }
            } break;
            case mapping::source::UINT: {
              uint32_t v(current_entry->get_uint(d));
              switch (v) {
                case 0:
                  if (current_entry->get_attribute() &
                      mapping::entry::invalid_on_zero)
                    bind_value_as_null(field);
                  else
                    bind_value_as_u32(field, v);
                  break;
                case static_cast<uint32_t>(-1):
                  if (current_entry->get_attribute() &
                      mapping::entry::invalid_on_minus_one)
                    bind_value_as_null(field);
                  else
                    bind_value_as_u32(field, v);
                  break;
                default:
                  bind_value_as_u32(field, v);
              }
            } break;
            default:  // Error in one of the mappings.
              throw msg_fmt(
                  "invalid mapping for object "
                  "of type '{}': {} is not a know type ID",
                  info->get_name(), current_entry->get_type());
          };
        }
      }
    } else {
      /* Here is the protobuf case: no mapping */
      const google::protobuf::Message* p =
          static_cast<const io::protobuf_base*>(&d)->msg();
      const google::protobuf::Descriptor* desc = p->GetDescriptor();
      const google::protobuf::Reflection* refl = p->GetReflection();

      for (uint32_t i = 0; i < _pb_mapping.size(); i++) {
        auto& pr = _pb_mapping[i];
        if (std::get<0>(pr).empty())
          continue;
        auto f = desc->field(i);
        std::string field{fmt::format(":{}", std::get<0>(pr))};
        switch (f->type()) {
          case google::protobuf::FieldDescriptor::TYPE_BOOL:
            bind_value_as_bool(field, refl->GetBool(*p, f));
            break;
          case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
            bind_value_as_f64(field, refl->GetDouble(*p, f));
            break;
          case google::protobuf::FieldDescriptor::TYPE_INT32: {
            int32_t v = refl->GetInt32(*p, f);
            switch (v) {
              case 0:
                if (std::get<2>(pr) & mapping::entry::invalid_on_zero)
                  bind_value_as_null(field);
                else
                  bind_value_as_i32(field, v);
                break;
              case -1:
                if (std::get<2>(pr) & mapping::entry::invalid_on_minus_one)
                  bind_value_as_null(field);
                else
                  bind_value_as_i32(field, v);
                break;
              default:
                bind_value_as_i32(field, v);
            }
          } break;
          case google::protobuf::FieldDescriptor::TYPE_UINT32: {
            uint32_t v{refl->GetUInt32(*p, f)};
            switch (v) {
              case 0:
                if (std::get<2>(pr) & mapping::entry::invalid_on_zero)
                  bind_value_as_null(field);
                else
                  bind_value_as_u32(field, v);
                break;
              case static_cast<uint32_t>(-1):
                if (std::get<2>(pr) & mapping::entry::invalid_on_minus_one)
                  bind_value_as_null(field);
                else
                  bind_value_as_u32(field, v);
                break;
              default:
                bind_value_as_u32(field, v);
            }
          } break;
          case google::protobuf::FieldDescriptor::TYPE_INT64: {
            int64_t v{refl->GetInt64(*p, f)};
            switch (v) {
              case 0:
                if (std::get<2>(pr) & mapping::entry::invalid_on_zero)
                  bind_value_as_null(field);
                else
                  bind_value_as_i64(field, v);
                break;
              case -1:
                if (std::get<2>(pr) & mapping::entry::invalid_on_minus_one)
                  bind_value_as_null(field);
                else
                  bind_value_as_i64(field, v);
                break;
              default:
                bind_value_as_i64(field, v);
            }
          } break;
          case google::protobuf::FieldDescriptor::TYPE_UINT64: {
            uint64_t v{refl->GetUInt64(*p, f)};
            switch (v) {
              case 0:
                if (std::get<2>(pr) & mapping::entry::invalid_on_zero)
                  bind_value_as_null(field);
                else
                  bind_value_as_u64(field, v);
                break;
              case static_cast<uint64_t>(-1):
                if (std::get<2>(pr) & mapping::entry::invalid_on_minus_one)
                  bind_value_as_null(field);
                else
                  bind_value_as_u64(field, v);
                break;
              default:
                bind_value_as_u64(field, v);
            }
          } break;
          case google::protobuf::FieldDescriptor::TYPE_ENUM:
            bind_value_as_i32(field, refl->GetEnumValue(*p, f));
            break;
          case google::protobuf::FieldDescriptor::TYPE_STRING: {
            size_t max_len = std::get<1>(pr);
            std::string v(refl->GetString(*p, f));
            fmt::string_view sv;
            if (max_len > 0 && v.size() > max_len) {
              log_v2::sql()->trace(
                  "column '{}' should admit a longer string, it is cut to {} "
                  "characters to be stored anyway.",
                  field, max_len);
              max_len = misc::string::adjust_size_utf8(v, max_len);
              sv = fmt::string_view(v.data(), max_len);
            } else
              sv = fmt::string_view(v);
            if ((std::get<2>(pr) & io::protobuf_base::invalid_on_zero) &&
                sv.size() == 0)
              bind_value_as_null(field);
            else
              bind_value_as_str(field, sv);
          } break;
          default:
            throw msg_fmt(
                "invalid mapping for object "
                "of type '{}': {} is not a know type ID",
                info->get_name(), f->type());
        }
      }
    }
  } else
    throw msg_fmt(
        "cannot bind object of type {}"
        " to database query: mapping does not exist",
        d.type());
}

void mysql_stmt::bind_value_as_i32(int range, int value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_i32(range, value);
}

void mysql_stmt::bind_value_as_i32(std::string const& name, int value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_i32(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_i32(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_i32(it->second, value);
      else
        log_v2::sql()->error(
            "mysql: cannot bind object with name '{}' to i32 value {} in "
            "statement {}",
            name, value, get_id());
    }
  }
}

void mysql_stmt::bind_value_as_u32(int range, uint32_t value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_u32(range, value);
}

void mysql_stmt::bind_value_as_u32(std::string const& name, uint32_t value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_u32(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_u32(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_u32(it->second, value);
      else
        log_v2::sql()->error(
            "mysql: cannot bind object with name '{}' to u32 value {} in "
            "statement {}",
            name, value, get_id());
    }
  }
}

/**
 *  Bind the value to the variable at index range.
 *
 * @param range The index in the statement.
 * @param value The value to bind. It can be Inf or NaN.
 */
void mysql_stmt::bind_value_as_i64(int range, int64_t value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_i64(range, value);
}

/**
 *  Bind the value to the variable name.
 *
 * @param name The column name in the statement that should receive the value.
 * @param value The value to bind. It can be Inf or NaN.
 */
void mysql_stmt::bind_value_as_i64(std::string const& name, int64_t value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_i64(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_i64(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_i64(it->second, value);
      else
        log_v2::sql()->error(
            "mysql: cannot bind object with name '{}' to i64 value {} in "
            "statement {}",
            name, value, get_id());
    }
  }
}

/**
 *  Bind the value to the variable at index range.
 *
 * @param range The index in the statement.
 * @param value The value to bind. It can be Inf or NaN.
 */
void mysql_stmt::bind_value_as_u64(int range, uint64_t value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_u64(range, value);
}

/**
 *  Bind the value to the variable name.
 *
 * @param name The column name in the statement that should receive the value.
 * @param value The value to bind. It can be Inf or NaN.
 */
void mysql_stmt::bind_value_as_u64(std::string const& name, uint64_t value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_u64(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_u64(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_u64(it->second, value);
      else
        log_v2::sql()->error(
            "mysql: cannot bind object with name '{}' to u64 value {} in "
            "statement {}",
            name, value, get_id());
    }
  }
}

/**
 *  Bind the value to the variable at index range.
 *
 * @param range The index in the statement.
 * @param value The value to bind. It can be Inf or NaN.
 */
void mysql_stmt::bind_value_as_f32(int range, float value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_f32(range, value);
}

void mysql_stmt::bind_value_as_f32(std::string const& name, float value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_f32(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_f32(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_f32(it->second, value);
      else
        log_v2::sql()->error(
            "mysql: cannot bind object with name '{}' to f32 value {} in "
            "statement {}",
            name, value, get_id());
    }
  }
}

/**
 *  Bind the value to the variable at index range.
 *
 * @param range The index in the statement.
 * @param value The value to bind. It can be Inf or NaN.
 */
void mysql_stmt::bind_value_as_f64(int range, double value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_f64(range, value);
}

void mysql_stmt::bind_value_as_f64(std::string const& name, double value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_f64(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_f64(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_f64(it->second, value);
      else
        log_v2::sql()->error(
            "mysql: cannot bind object with name '{}' to f64 value {} in "
            "statement {}",
            name, value, get_id());
    }
  }
}

void mysql_stmt::bind_value_as_tiny(int range, char value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_tiny(range, value);
}

void mysql_stmt::bind_value_as_tiny(std::string const& name, char value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_tiny(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_tiny(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_tiny(it->second, value);
      else
        log_v2::sql()->error(
            "mysql: cannot bind object with name '{}' to tiny value {} in "
            "statement {}",
            name, value, get_id());
    }
  }
}

void mysql_stmt::bind_value_as_bool(int range, bool value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_bool(range, value);
}

void mysql_stmt::bind_value_as_bool(std::string const& name, bool value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_bool(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_bool(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_bool(it->second, value);
      else
        log_v2::sql()->error(
            "mysql: cannot bind object with name '{}' to bool value {} in "
            "statement {}",
            name, value, get_id());
    }
  }
}

void mysql_stmt::bind_value_as_str(int range, const fmt::string_view& value) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_str(range, value);
}

void mysql_stmt::bind_value_as_str(std::string const& name,
                                   const fmt::string_view& value) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_str(it->second, value);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_str(it->second, value);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_str(it->second, value);
    } else
      log_v2::sql()->error(
          "mysql: cannot bind object with name '{}' to string value {} in "
          "statement {}",
          name, value, get_id());
  }
}

void mysql_stmt::bind_value_as_null(int range) {
  if (!_bind) {
    _bind = std::make_unique<database::mysql_bind>(_param_count);
    _bind->set_current_row(_current_row);
  }
  _bind->set_value_as_null(range);
}

void mysql_stmt::bind_value_as_null(std::string const& name) {
  mysql_bind_mapping::iterator it(_bind_mapping.find(name));
  if (it != _bind_mapping.end()) {
    bind_value_as_null(it->second);
  } else {
    std::string key(name);
    key.append("1");
    it = _bind_mapping.find(key);
    if (it != _bind_mapping.end()) {
      bind_value_as_null(it->second);
      key[key.size() - 1] = '2';
      it = _bind_mapping.find(key);
      if (it != _bind_mapping.end())
        bind_value_as_null(it->second);
    } else
      log_v2::sql()->error(
          "mysql: cannot bind object with name '{}' to null in statement {}",
          name, get_id());
  }
}

std::string const& mysql_stmt::get_query() const {
  return _query;
}

int mysql_stmt::get_param_count() const {
  return _param_count;
}

void mysql_stmt::set_pb_mapping(
    std::vector<std::tuple<std::string, uint32_t, uint16_t>>&& mapping) {
  _pb_mapping = std::move(mapping);
}

void mysql_stmt::set_current_row(int row) {
  _current_row = row;
  if (_bind)
    _bind->set_current_row(row);
}
