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

#include "com/centreon/broker/database/mysql_bulk_stmt.hh"

#include <cfloat>
#include <cmath>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/misc/string.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;

/**
 * @brief Default constructor.
 */
mysql_bulk_stmt::mysql_bulk_stmt() : mysql_stmt_base(true), _hist_size(10) {}

/**
 * @brief Constructor of a mysql_bulk_stmt from a SQL query template. This template
 * can be named or not, i.e. respectively like
 * UPDATE foo SET a=:a_value or UPDATE foo SET a=?
 *
 * @param query The query template
 * @param named a boolean telling if the query is named or not (with ?).
 */
mysql_bulk_stmt::mysql_bulk_stmt(std::string const& query, bool named) : mysql_stmt_base(query, named), _hist_size(10) {}

/**
 * @brief Constructor of a mysql_bulk_stmt from a not named query template and a
 * correspondance table making the relation between column names and their
 * indices.
 *
 * @param query
 * @param bind_mapping
 */
mysql_bulk_stmt::mysql_bulk_stmt(const std::string& query,
                       const mysql_bind_mapping& bind_mapping)
    : mysql_stmt_base(query, true, bind_mapping), _hist_size(10) {}

/**
 * @brief Create a bind compatible with this mysql_bulk_stmt. It is then possible
 * to work with it and once finished apply it to the statement for the
 * execution.
 *
 * @return An unique pointer to a mysql_bulk_bind.
 */
std::unique_ptr<mysql_bulk_bind> mysql_bulk_stmt::create_bind() {
  if (!_hist_size.empty()) {
    int avg = 0;
    for (int v : _hist_size) {
      avg += v;
    }
    _reserved_size = avg / _hist_size.size() + 1;
  }
  log_v2::sql()->trace("new mysql bind of stmt {} reserved with {} rows", get_id(),
                       _reserved_size);
  auto retval = std::make_unique<mysql_bulk_bind>(get_param_count(), 0, _reserved_size);
  return retval;
}

/**
 * @brief Apply a mysql_bulk_bind to the statement.
 *
 * @param bind the bind to move into the mysql_bulk_stmt.
 */
void mysql_bulk_stmt::set_bind(std::unique_ptr<mysql_bulk_bind>&& bind) {
  _bind = std::move(bind);
}

/**
 *  Move constructor
 */
mysql_bulk_stmt::mysql_bulk_stmt(mysql_bulk_stmt&& other)
    : mysql_stmt_base(std::move(other)),
      _reserved_size(std::move(other._reserved_size)),
      _bind(std::move(other._bind)) {}

/**
 * @brief Move copy
 *
 * @param other the statement to move.
 *
 * @return a reference to the self statement.
 */
mysql_bulk_stmt& mysql_bulk_stmt::operator=(mysql_bulk_stmt&& other) {
  if (this != &other) {
    mysql_stmt_base::operator=(std::move(other));
    _reserved_size = std::move(other._reserved_size);
    _bind = std::move(other._bind);
  }
  return *this;
}

/**

/**
 * @brief Return an unique pointer to the bind contained inside the statement.
 * Since the bind is in an unique pointer, it is removed from the statement when
 * returned.
 *
 * @return A std::unique_ptr<mysql_bulk_bind>
 */
std::unique_ptr<mysql_bulk_bind> mysql_bulk_stmt::get_bind() {
  if (_bind) {
    log_v2::sql()->trace("mysql bind of stmt {} returned with {} rows",
                         get_id(), _bind->rows_count());
    _hist_size.push_back(_bind->rows_count());
  }
  return std::move(_bind);
}

/**
 * @brief Operator useful to fill a database table row from a neb object. It
 * works well when all the content of the object has an equivalent column in
 * the table.
 *
 * @param d The object to save to the database.
 */
// void mysql_bulk_stmt::operator<<(io::data const& d) {
//   // Get event info.
//   const io::event_info* info =
//   io::events::instance().get_event_info(d.type()); if (info) {
//     if (info->get_mapping()) {
//       for (const mapping::entry* current_entry(info->get_mapping());
//            !current_entry->is_null(); ++current_entry) {
//         char const* entry_name = current_entry->get_name_v2();
//         if (entry_name && entry_name[0]) {
//           std::string field{fmt::format(":{}", entry_name)};
//           switch (current_entry->get_type()) {
//             case mapping::source::BOOL:
//               bind_value_as_bool(field, current_entry->get_bool(d));
//               break;
//             case mapping::source::DOUBLE:
//               bind_value_as_f64(field, current_entry->get_double(d));
//               break;
//             case mapping::source::INT: {
//               int v(current_entry->get_int(d));
//               switch (current_entry->get_attribute()) {
//                 case mapping::entry::invalid_on_zero:
//                   if (v == 0)
//                     bind_null_i32(field);
//                   else
//                     bind_value_as_i32(field, v);
//                   break;
//                 case mapping::entry::invalid_on_minus_one:
//                   if (v == -1)
//                     bind_null_i32(field);
//                   else
//                     bind_value_as_i32(field, v);
//                   break;
//                 case mapping::entry::invalid_on_negative:
//                   if (v < 0)
//                     bind_null_i32(field);
//                   else
//                     bind_value_as_i32(field, v);
//                   break;
//                 default:
//                   bind_value_as_i32(field, v);
//               }
//             } break;
//             case mapping::source::SHORT: {
//               int v = current_entry->get_short(d);
//               switch (current_entry->get_attribute()) {
//                 case mapping::entry::invalid_on_zero:
//                   if (v == 0)
//                     bind_null_i32(field);
//                   else
//                     bind_value_as_i32(field, v);
//                   break;
//                 case mapping::entry::invalid_on_minus_one:
//                   if (v == -1)
//                     bind_null_i32(field);
//                   else
//                     bind_value_as_i32(field, v);
//                   break;
//                 case mapping::entry::invalid_on_negative:
//                   if (v < 0)
//                     bind_null_i32(field);
//                   else
//                     bind_value_as_i32(field, v);
//                   break;
//                 default:
//                   bind_value_as_i32(field, v);
//               }
//             } break;
//             case mapping::source::STRING: {
//               size_t max_len = 0;
//               const std::string& v(current_entry->get_string(d, &max_len));
//               fmt::string_view sv;
//               if (max_len > 0 && v.size() > max_len) {
//                 log_v2::sql()->trace(
//                     "column '{}' should admit a longer string, it is cut to
//                     {} " "characters to be stored anyway.",
//                     current_entry->get_name_v2(), max_len);
//                 max_len = misc::string::adjust_size_utf8(v, max_len);
//                 sv = fmt::string_view(v.data(), max_len);
//               } else
//                 sv = fmt::string_view(v);
//               if (current_entry->get_attribute() ==
//                   mapping::entry::invalid_on_zero) {
//                 if (sv.size() == 0)
//                   bind_null_str(field);
//                 else
//                   bind_value_as_str(field, sv);
//               } else
//                 bind_value_as_str(field, sv);
//             } break;
//             case mapping::source::TIME: {
//               time_t v(current_entry->get_time(d));
//               switch (current_entry->get_attribute()) {
//                 case mapping::entry::invalid_on_zero:
//                   if (v == 0)
//                     bind_null_u32(field);
//                   else
//                     bind_value_as_u32(field, v);
//                   break;
//                 case mapping::entry::invalid_on_minus_one:
//                   if (v == -1)
//                     bind_null_u32(field);
//                   else
//                     bind_value_as_u32(field, v);
//                   break;
//                 case mapping::entry::invalid_on_negative:
//                   if (v < 0)
//                     bind_null_u32(field);
//                   else
//                     bind_value_as_i32(field, v);
//                   break;
//                 default:
//                   bind_value_as_u32(field, v);
//               }
//             } break;
//             case mapping::source::UINT: {
//               uint32_t v(current_entry->get_uint(d));
//               switch (current_entry->get_attribute()) {
//                 case mapping::entry::invalid_on_zero:
//                   bind_value_as_u32(field, v);
//                   break;
//                 case mapping::entry::invalid_on_minus_one:
//                   if (v == (uint32_t)-1)
//                     bind_null_u32(field);
//                   else
//                     bind_value_as_u32(field, v);
//                   break;
//                 default:
//                   bind_value_as_u32(field, v);
//               }
//             } break;
//             default:  // Error in one of the mappings.
//               throw msg_fmt(
//                   "invalid mapping for object "
//                   "of type '{}': {} is not a know type ID",
//                   info->get_name(), current_entry->get_type());
//           };
//         }
//       }
//     } else {
//       /* Here is the protobuf case: no mapping */
//       const google::protobuf::Message* p =
//           static_cast<const io::protobuf_base*>(&d)->msg();
//       const google::protobuf::Descriptor* desc = p->GetDescriptor();
//       const google::protobuf::Reflection* refl = p->GetReflection();
//
//       for (uint32_t i = 0; i < _pb_mapping.size(); i++) {
//         auto& pr = _pb_mapping[i];
//         if (std::get<0>(pr).empty())
//           continue;
//         auto f = desc->field(i);
//         std::string field{fmt::format(":{}", std::get<0>(pr))};
//         switch (f->type()) {
//           case google::protobuf::FieldDescriptor::TYPE_BOOL:
//             bind_value_as_bool(field, refl->GetBool(*p, f));
//             break;
//           case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
//             bind_value_as_f64(field, refl->GetDouble(*p, f));
//             break;
//           case google::protobuf::FieldDescriptor::TYPE_INT32: {
//             int32_t v{refl->GetInt32(*p, f)};
//             switch (std::get<2>(pr)) {
//               case io::protobuf_base::invalid_on_zero:
//                 if (v == 0)
//                   bind_null_i32(field);
//                 else
//                   bind_value_as_i32(field, v);
//                 break;
//               case io::protobuf_base::invalid_on_minus_one:
//                 if (v == -1)
//                   bind_null_i32(field);
//                 else
//                   bind_value_as_i32(field, v);
//                 break;
//               default:
//                 bind_value_as_i32(field, v);
//             }
//           } break;
//           case google::protobuf::FieldDescriptor::TYPE_UINT32: {
//             uint32_t v{refl->GetUInt32(*p, f)};
//             switch (std::get<2>(pr)) {
//               case io::protobuf_base::invalid_on_zero:
//                 if (v == 0)
//                   bind_null_u32(field);
//                 else
//                   bind_value_as_u32(field, v);
//                 break;
//               case io::protobuf_base::invalid_on_minus_one:
//                 if (v == (uint32_t)-1)
//                   bind_null_u32(field);
//                 else
//                   bind_value_as_u32(field, v);
//                 break;
//               default:
//                 bind_value_as_u32(field, v);
//             }
//           } break;
//           case google::protobuf::FieldDescriptor::TYPE_INT64: {
//             int64_t v{refl->GetInt64(*p, f)};
//             switch (std::get<2>(pr)) {
//               case io::protobuf_base::invalid_on_zero:
//                 if (v == 0)
//                   bind_null_i64(field);
//                 else
//                   bind_value_as_i64(field, v);
//                 break;
//               case io::protobuf_base::invalid_on_minus_one:
//                 if (v == -1)
//                   bind_null_i64(field);
//                 else
//                   bind_value_as_i64(field, v);
//                 break;
//               default:
//                 bind_value_as_i64(field, v);
//             }
//           } break;
//           case google::protobuf::FieldDescriptor::TYPE_UINT64: {
//             uint64_t v{refl->GetUInt64(*p, f)};
//             switch (std::get<2>(pr)) {
//               case io::protobuf_base::invalid_on_zero:
//                 if (v == 0)
//                   bind_null_u64(field);
//                 else
//                   bind_value_as_u64(field, v);
//                 break;
//               case io::protobuf_base::invalid_on_minus_one:
//                 if (v == (uint64_t)-1)
//                   bind_null_u64(field);
//                 else
//                   bind_value_as_u64(field, v);
//                 break;
//               default:
//                 bind_value_as_u64(field, v);
//             }
//           } break;
//           case google::protobuf::FieldDescriptor::TYPE_ENUM:
//             bind_value_as_i32(field, refl->GetEnumValue(*p, f));
//             break;
//           case google::protobuf::FieldDescriptor::TYPE_STRING: {
//             size_t max_len = std::get<1>(pr);
//             std::string v(refl->GetString(*p, f));
//             fmt::string_view sv;
//             if (max_len > 0 && v.size() > max_len) {
//               log_v2::sql()->trace(
//                   "column '{}' should admit a longer string, it is cut to {}
//              max_len = misc::string::adjust_size_utf8(v, max_len);
//              sv = fmt::string_view(v.data(), max_len);
//            } else
//              sv = fmt::string_view(v);
//            if (std::get<2>(pr) == io::protobuf_base::invalid_on_zero) {
//              if (sv.size() == 0)
//                bind_null_str(field);
//              else
//                bind_value_as_str(field, sv);
//            } else
//              bind_value_as_str(field, sv);
//          } break;
//          default:
//            throw msg_fmt(
//                "invalid mapping for object "
//                "of type '{}': {} is not a know type ID",
//                info->get_name(), f->type());
//        }
//      }
//    }
//  } else
//    throw msg_fmt(
//        "cannot bind object of type {}"
//        " to database query: mapping does not exist",
//        d.type());
//}

#define BIND_VALUE(ftype, vtype)                                      \
  void mysql_bulk_stmt::bind_value_as_##ftype(size_t range, vtype value) { \
    if (!_bind)                                                       \
      _bind = std::make_unique<database::mysql_bulk_bind>(get_param_count(), 0, \
                                                     _reserved_size); \
    _bind->set_value_as_##ftype(range, value);                        \
  }                                                                   \
                                                                      \
  void mysql_bulk_stmt::bind_null_##ftype(size_t range) {                  \
    if (!_bind)                                                       \
      _bind = std::make_unique<database::mysql_bulk_bind>(get_param_count(), 0, \
                                                     _reserved_size); \
    _bind->set_null_##ftype(range);                                   \
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
