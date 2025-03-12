/**
 * Copyright 2018-2023 Centreon
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

#include "com/centreon/broker/sql/mysql_stmt.hh"

#include <cfloat>
#include <cmath>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/common/utf8.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

mysql_stmt::mysql_stmt(const std::shared_ptr<spdlog::logger>& logger)
    : mysql_stmt_base(false, logger) {}

/**
 * @brief Constructor of a mysql_stmt from a SQL query template. This template
 * can be named or not, i.e. respectively like
 * UPDATE foo SET a=:a_value or UPDATE foo SET a=?
 *
 * @param query The query template
 * @param named a boolean telling if the query is named or not (with ?).
 */
mysql_stmt::mysql_stmt(std::string const& query,
                       bool named,
                       const std::shared_ptr<spdlog::logger>& logger)
    : mysql_stmt_base(query, false, named, logger) {}

/**
 * @brief Constructor of a mysql_stmt from a not named query template and a
 * correspondance table making the relation between column names and their
 * indices.
 *
 * @param query
 * @param bind_mapping
 */
mysql_stmt::mysql_stmt(const std::string& query,
                       const std::shared_ptr<spdlog::logger>& logger,
                       const mysql_bind_mapping& bind_mapping)
    : mysql_stmt_base(query, false, logger, bind_mapping) {}

/**
 * @brief Create a bind compatible with this mysql_stmt. It is then possible
 * to work with it and once finished apply it to the statement for the
 * execution.
 *
 * @return An unique pointer to a mysql_bind.
 */
std::unique_ptr<mysql_bind> mysql_stmt::create_bind() {
  _logger->trace("new mysql bind of stmt {}", get_id());
  auto retval = std::make_unique<mysql_bind>(get_param_count(), _logger);
  return retval;
}

/**
 * @brief Apply a mysql_bind to the statement.
 *
 * @param bind the bind to move into the mysql_stmt.
 */
void mysql_stmt::set_bind(std::unique_ptr<mysql_bind>&& bind) {
  _bind = std::move(bind);
}

/**
 *  Move constructor
 */
mysql_stmt::mysql_stmt(mysql_stmt&& other)
    : mysql_stmt_base(std::move(other)), _bind(std::move(other._bind)) {}

/**
 * @brief Move copy
 *
 * @param other the statement to move.
 *
 * @return a reference to the self statement.
 */
mysql_stmt& mysql_stmt::operator=(mysql_stmt&& other) {
  if (this != &other) {
    mysql_stmt_base::operator=(std::move(other));
    _bind = std::move(other._bind);
  }
  return *this;
}

/**
 * @brief Return an unique pointer to the bind contained inside the statement.
 * Since the bind is in an unique pointer, it is removed from the statement when
 * returned.
 *
 * @return A std::unique_ptr<mysql_bind>
 */
std::unique_ptr<mysql_bind> mysql_stmt::get_bind() {
  if (_bind)
    _logger->trace("mysql bind of stmt {} returned", get_id());
  return std::move(_bind);
}

/**
 * @brief Operator useful to fill a database table row from a neb object. It
 * works well when all the content of the object has an equivalent column in
 * the table.
 *
 * @param d The object to save to the database.
 */
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
              bind_value_as_bool_k(field, current_entry->get_bool(d));
              break;
            case mapping::source::DOUBLE:
              bind_value_as_f64_k(field, current_entry->get_double(d));
              break;
            case mapping::source::INT: {
              int32_t v = current_entry->get_int(d);
              uint32_t attr = current_entry->get_attribute();

              if (((attr & mapping::entry::invalid_on_zero) && v == 0) ||
                  ((attr & mapping::entry::invalid_on_negative) && v < 0) ||
                  ((attr & mapping::entry::invalid_on_minus_one) && v == -1))
                bind_null_i32_k(field);
              else
                bind_value_as_i32_k(field, v);
            } break;
            case mapping::source::SHORT: {
              int32_t v = current_entry->get_short(d);
              uint32_t attr = current_entry->get_attribute();

              if (((attr & mapping::entry::invalid_on_zero) && v == 0) ||
                  ((attr & mapping::entry::invalid_on_negative) && v < 0) ||
                  ((attr & mapping::entry::invalid_on_minus_one) && v == -1))
                bind_null_i32_k(field);
              else
                bind_value_as_i32_k(field, v);
            } break;
            case mapping::source::STRING: {
              size_t max_len = 0;
              const std::string& v(current_entry->get_string(d, &max_len));
              fmt::string_view sv;
              if (max_len > 0 && v.size() > max_len) {
                _logger->trace(
                    "column '{}' should admit a longer string, it is cut to {} "
                    "characters to be stored anyway.",
                    current_entry->get_name_v2(), max_len);
                max_len = common::adjust_size_utf8(v, max_len);
                sv = fmt::string_view(v.data(), max_len);
              } else
                sv = fmt::string_view(v);
              uint32_t attr = current_entry->get_attribute();

              if ((attr & mapping::entry::invalid_on_zero) && sv.size() == 0)
                bind_null_str_k(field);
              else
                bind_value_as_str_k(field, sv);
            } break;
            case mapping::source::TIME: {
              time_t v = current_entry->get_time(d);
              uint32_t attr = current_entry->get_attribute();

              if (((attr & mapping::entry::invalid_on_zero) && v == 0) ||
                  ((attr & mapping::entry::invalid_on_negative) && v < 0) ||
                  ((attr & mapping::entry::invalid_on_minus_one) && v == -1))
                bind_null_u32_k(field);
              else
                bind_value_as_u32_k(field, v);
            } break;
            case mapping::source::UINT: {
              uint32_t v = current_entry->get_uint(d);
              uint32_t attr = current_entry->get_attribute();

              if (((attr & mapping::entry::invalid_on_zero) && v == 0) ||
                  ((attr & mapping::entry::invalid_on_minus_one) &&
                   v == static_cast<uint32_t>(-1)))
                bind_null_u32_k(field);
              else
                bind_value_as_u32_k(field, v);
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

      for (uint32_t i = 0; i < get_pb_mapping().size(); i++) {
        auto& pr = get_pb_mapping()[i];
        if (std::get<0>(pr).empty())
          continue;
        auto f = desc->field(i);
        std::string field{fmt::format(":{}", std::get<0>(pr))};
        switch (f->type()) {
          case google::protobuf::FieldDescriptor::TYPE_BOOL:
            bind_value_as_bool_k(field, refl->GetBool(*p, f));
            break;
          case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
            bind_value_as_f64_k(field, refl->GetDouble(*p, f));
            break;
          case google::protobuf::FieldDescriptor::TYPE_INT32: {
            int32_t v = refl->GetInt32(*p, f);
            uint32_t attr = std::get<2>(pr);

            if (((attr & io::protobuf_base::invalid_on_zero) && v == 0) ||
                ((attr & mapping::entry::invalid_on_negative) && v < 0) ||
                ((attr & mapping::entry::invalid_on_minus_one) && v == -1))
              bind_null_i32_k(field);
            else
              bind_value_as_i32_k(field, v);
          } break;
          case google::protobuf::FieldDescriptor::TYPE_UINT32: {
            uint32_t v = refl->GetUInt32(*p, f);
            uint32_t attr = std::get<2>(pr);

            if (((attr & io::protobuf_base::invalid_on_zero) && v == 0) ||
                ((attr & mapping::entry::invalid_on_minus_one) &&
                 v == static_cast<uint32_t>(-1)))
              bind_null_u32_k(field);
            else
              bind_value_as_u32_k(field, v);
          } break;
          case google::protobuf::FieldDescriptor::TYPE_INT64: {
            int64_t v = refl->GetInt64(*p, f);
            uint32_t attr = std::get<2>(pr);

            if (((attr & io::protobuf_base::invalid_on_zero) && v == 0) ||
                ((attr & mapping::entry::invalid_on_negative) && v < 0) ||
                ((attr & mapping::entry::invalid_on_minus_one) && v == -1))
              bind_null_i64_k(field);
            else
              bind_value_as_i64_k(field, v);
          } break;
          case google::protobuf::FieldDescriptor::TYPE_UINT64: {
            uint64_t v = refl->GetUInt64(*p, f);
            uint32_t attr = std::get<2>(pr);

            if (((attr & io::protobuf_base::invalid_on_zero) && v == 0) ||
                ((attr & mapping::entry::invalid_on_minus_one) &&
                 v == static_cast<uint64_t>(-1)))
              bind_null_u64_k(field);
            else
              bind_value_as_u64_k(field, v);
          } break;
          case google::protobuf::FieldDescriptor::TYPE_ENUM:
            bind_value_as_i32_k(field, refl->GetEnumValue(*p, f));
            break;
          case google::protobuf::FieldDescriptor::TYPE_STRING: {
            size_t max_len = std::get<1>(pr);
            std::string v(refl->GetString(*p, f));
            fmt::string_view sv;
            if (max_len > 0 && v.size() > max_len) {
              _logger->trace(
                  "column '{}' should admit a longer string, it is cut to {} "
                  "characters to be stored anyway.",
                  field, max_len);
              max_len = common::adjust_size_utf8(v, max_len);
              sv = fmt::string_view(v.data(), max_len);
            } else
              sv = fmt::string_view(v);
            uint32_t attr = std::get<2>(pr);
            if (attr & io::protobuf_base::invalid_on_zero && sv.size() == 0)
              bind_null_str_k(field);
            else
              bind_value_as_str_k(field, sv);
          } break;
          default:
            throw msg_fmt(
                "invalid mapping for object of type '{}': {} is not a know "
                "type ID",
                info->get_name(), static_cast<uint32_t>(f->type()));
        }
      }
    }
  } else
    throw msg_fmt(
        "cannot bind object of type {}"
        " to database query: mapping does not exist",
        d.type());
}

void mysql_stmt::bind_value_as_f32(size_t range, float value) {
  if (!_bind)
    _bind = std::make_unique<database::mysql_bind>(get_param_count(), _logger);

  if (std::isinf(value) || std::isnan(value))
    _bind->set_null_f32(range);
  else
    _bind->set_value_as_f32(range, value);
}

void mysql_stmt::bind_null_f32(size_t range) {
  if (!_bind)
    _bind = std::make_unique<database::mysql_bind>(get_param_count(), _logger);
  _bind->set_null_f32(range);
}

void mysql_stmt::bind_value_as_f64(size_t range, double value) {
  if (!_bind)
    _bind = std::make_unique<database::mysql_bind>(get_param_count(), _logger);

  if (std::isinf(value) || std::isnan(value))
    _bind->set_null_f64(range);
  else
    _bind->set_value_as_f64(range, value);
}

void mysql_stmt::bind_null_f64(size_t range) {
  if (!_bind)
    _bind = std::make_unique<database::mysql_bind>(get_param_count(), _logger);
  _bind->set_null_f64(range);
}

#define BIND_VALUE(ftype, vtype)                                              \
  void mysql_stmt::bind_value_as_##ftype(size_t range, vtype value) {         \
    if (!_bind)                                                               \
      _bind =                                                                 \
          std::make_unique<database::mysql_bind>(get_param_count(), _logger); \
    _bind->set_value_as_##ftype(range, value);                                \
  }                                                                           \
                                                                              \
  void mysql_stmt::bind_null_##ftype(size_t range) {                          \
    if (!_bind)                                                               \
      _bind =                                                                 \
          std::make_unique<database::mysql_bind>(get_param_count(), _logger); \
    _bind->set_null_##ftype(range);                                           \
  }

BIND_VALUE(i32, int32_t)
BIND_VALUE(u32, uint32_t)
BIND_VALUE(i64, int64_t)
BIND_VALUE(u64, uint64_t)
BIND_VALUE(tiny, char)
BIND_VALUE(bool, bool)
BIND_VALUE(str, const fmt::string_view&)

#undef BIND_VALUE
