/**
 * Copyright 2018-2021 Centreon
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

#include "com/centreon/broker/lua/broker_utils.hh"

#include <fmt/format.h>
#include <sys/stat.h>
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "com/centreon/broker/config/applier/state.hh"
#include "common/crypto/base64.hh"

#include <openssl/evp.h>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/sql/table_max_size.hh"
#include "com/centreon/common/hex_dump.hh"
#include "com/centreon/common/perfdata.hh"
#include "com/centreon/common/utf8.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::lua;
using namespace com::centreon::exceptions;
using namespace com::centreon::common::crypto;
using namespace nlohmann;
using com::centreon::common::log_v2::log_v2;

static void broker_json_encode(lua_State* L, std::ostringstream& oss);
static void broker_json_decode(lua_State* L, const json& it);

/**
 *  The json_encode function for Lua tables
 *
 *  @param L The Lua interpreter
 *  @param oss The output stream
 */
static void broker_json_encode_table(lua_State* L, std::ostringstream& oss) {
  bool array(false);
  /* We must parse the table from the first key */
  lua_pushnil(L); /* this tells lua_next to start from the first key */
  if (lua_next(L, -2)) {
#if LUA53
    if (lua_isinteger(L, -2)) {
#else
    if (lua_isnumber(L, -2)) {
#endif
      int index(lua_tointeger(L, -2));
      if (index == 1) {
        array = true;
        oss << '[';
        broker_json_encode(L, oss);
        lua_pop(L, 1);
        while (lua_next(L, -2)) {
#if LUA53
          if (lua_isinteger(L, -2)) {
#else
          if (lua_isnumber(L, -2)) {
#endif
            oss << ',';
            broker_json_encode(L, oss);
          }
          lua_pop(L, 1);
        }
        oss << ']';
      }
    }
  } else {
    /* There are no key, the table is empty */
    oss << "[]";
    return;
  }

  if (!array) {
    oss << "{\"" << lua_tostring(L, -2) << "\":";
    broker_json_encode(L, oss);
    lua_pop(L, 1);
    while (lua_next(L, -2)) {
      oss << ",\"" << lua_tostring(L, -2) << "\":";
      broker_json_encode(L, oss);
      lua_pop(L, 1);
    }
    oss << '}';
  }
}

static void escape_str(const char* content, std::ostringstream& oss) {
  /* If the string contains '"', we must escape it */
  size_t pos(strcspn(content, "\\\"\t\r\n"));
  if (content[pos] != 0) {
    std::string str(content);
    char replacement[3] = "\\\\";
    do {
      switch (str[pos]) {
        case '\\':
          replacement[1] = '\\';
          break;
        case '"':
          replacement[1] = '"';
          break;
        case '\t':
          replacement[1] = 't';
          break;
        case '\r':
          replacement[1] = 'r';
          break;
        case '\n':
          replacement[1] = 'n';
          break;
      }
      str.replace(pos, 1, replacement);
      pos += 2;
    } while ((pos = str.find_first_of("\\\"\t\r\n", pos)) != std::string::npos);
    oss << str;
  } else
    oss << content;
}

static void _message_to_json(std::ostringstream& oss,
                             const google::protobuf::Message* p) {
  std::string tmpl;
  const google::protobuf::Descriptor* desc = p->GetDescriptor();
  const google::protobuf::Reflection* refl = p->GetReflection();
  for (int i = 0; i < desc->field_count(); i++) {
    auto f = desc->field(i);
    auto oof = f->containing_oneof();
    if (oof) {
      if (!refl->GetOneofFieldDescriptor(*p, oof)) {
        continue;
      }
    }
    const std::string& entry_name = f->name();
    if (f->is_repeated()) {
      size_t s = refl->FieldSize(*p, f);
      if (i > 0)
        oss << ", ";
      switch (f->type()) {
        case google::protobuf::FieldDescriptor::TYPE_BOOL:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ",";
            oss << refl->GetRepeatedBool(*p, f, j);
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ",";
            oss << refl->GetRepeatedDouble(*p, f, j);
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_INT32:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ",";
            oss << refl->GetRepeatedInt32(*p, f, j);
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ",";
            oss << refl->GetRepeatedUInt32(*p, f, j);
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
        case google::protobuf::FieldDescriptor::TYPE_INT64:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ",";
            oss << refl->GetRepeatedInt64(*p, f, j);
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ",";
            oss << refl->GetRepeatedUInt64(*p, f, j);
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_ENUM:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ",";
            oss << refl->GetRepeatedEnumValue(*p, f, j);
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_STRING:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ",";
            oss << "\"" << refl->GetRepeatedStringReference(*p, f, j, &tmpl)
                << "\"";
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t i = 0; i < s; i++) {
            if (i > 0)
              oss << ", ";
            oss << '{';
            _message_to_json(oss, &refl->GetRepeatedMessage(*p, f, i));
            oss << '}';
          }
          oss << ']';
          break;
        case google::protobuf::FieldDescriptor::TYPE_BYTES:
          oss << fmt::format("\"{}\":[", entry_name);
          for (size_t j = 0; j < s; j++) {
            if (j > 0)
              oss << ',';
            tmpl = refl->GetRepeatedStringReference(*p, f, j, &tmpl);
            oss << '"' << com::centreon::common::hex_dump(tmpl, 0) << '"';
          }
          oss << ']';
          break;
        default:  // Error, a type not handled
          throw msg_fmt(
              "protobuf {} type ID is not handled in the broker json converter",
              static_cast<uint32_t>(f->type()));
      }
    } else {
      if (i > 0)
        oss << ", ";
      switch (f->type()) {
        case google::protobuf::FieldDescriptor::TYPE_BOOL:
          oss << fmt::format("\"{}\":{}", entry_name, refl->GetBool(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
          oss << fmt::format("\"{}\":{}", entry_name, refl->GetDouble(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_INT32:
          oss << fmt::format("\"{}\":{}", entry_name, refl->GetInt32(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
          oss << fmt::format("\"{}\":{}", entry_name, refl->GetUInt32(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
        case google::protobuf::FieldDescriptor::TYPE_INT64:
          oss << fmt::format("\"{}\":{}", entry_name, refl->GetInt64(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
          oss << fmt::format("\"{}\":{}", entry_name, refl->GetUInt64(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_ENUM:
          oss << fmt::format("\"{}\":{}", entry_name,
                             refl->GetEnumValue(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_STRING:
          oss << fmt::format("\"{}\":\"{}\"", entry_name,
                             refl->GetStringReference(*p, f, &tmpl));
          break;
        case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
          oss << fmt::format("\"{}\":{{", entry_name);
          _message_to_json(oss, &refl->GetMessage(*p, f));
          oss << '}';
          break;
        case google::protobuf::FieldDescriptor::TYPE_BYTES:
          tmpl = refl->GetStringReference(*p, f, &tmpl);
          oss << fmt::format(R"("{}":"{}")", entry_name,
                             com::centreon::common::hex_dump(tmpl, 0));
          break;
        default:  // Error, a type not handled
          throw msg_fmt(
              "protobuf {} type ID is not handled in the broker json converter",
              static_cast<uint32_t>(f->type()));
      }
    }
  }
}

static void broker_json_encode_broker_event(std::shared_ptr<io::data> e,
                                            std::ostringstream& oss) {
  io::event_info const* info = io::events::instance().get_event_info(e->type());
  if (info) {
    oss << fmt::format("{{\"_type\":{}, \"category\":{}, \"element\":{}",
                       e->type(), static_cast<uint32_t>(e->type()) >> 16,
                       static_cast<uint32_t>(e->type()) & 0xffff);
    if (info->get_mapping()) {
      for (const mapping::entry* current_entry = info->get_mapping();
           !current_entry->is_null(); ++current_entry) {
        char const* entry_name(current_entry->get_name_v2());
        if (entry_name && *entry_name) {
          switch (current_entry->get_type()) {
            case mapping::source::BOOL:
              oss << fmt::format(", \"{}\":{}", entry_name,
                                 current_entry->get_bool(*e));
              break;
            case mapping::source::DOUBLE:
              oss << fmt::format(", \"{}\":{}", entry_name,
                                 current_entry->get_double(*e));
              break;
            case mapping::source::INT:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  int val(current_entry->get_int(*e));
                  if (val != 0)
                    oss << fmt::format(", \"{}\":{}", entry_name, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  int val(current_entry->get_int(*e));
                  if (val != -1)
                    oss << fmt::format(", \"{}\":{}", entry_name, val);
                } break;
                default:
                  oss << fmt::format(", \"{}\":{}", entry_name,
                                     current_entry->get_int(*e));
              }
              break;
            case mapping::source::SHORT:
              oss << fmt::format(", \"{}\":{}", entry_name,
                                 current_entry->get_short(*e));
              break;
            case mapping::source::STRING:
              if (current_entry->get_attribute() ==
                  mapping::entry::invalid_on_zero) {
                std::string val{current_entry->get_string(*e)};
                if (!val.empty()) {
                  oss << fmt::format(", \"{}\":\"", entry_name);
                  escape_str(val.c_str(), oss);
                  oss << '"';
                }
              } else {
                oss << fmt::format(", \"{}\":\"", entry_name);
                escape_str(current_entry->get_string(*e).c_str(), oss);
                oss << '"';
              }
              break;
            case mapping::source::TIME:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  time_t val = current_entry->get_time(*e);
                  if (val != 0)
                    oss << fmt::format(", \"{}\":\"{}\"", entry_name, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  time_t val = current_entry->get_time(*e);
                  if (val != -1)
                    oss << fmt::format(", \"{}\":\"{}\"", entry_name, val);
                } break;
                default:
                  oss << fmt::format(", \"{}\":\"{}\"", entry_name,
                                     current_entry->get_time(*e));
              }
              break;
            case mapping::source::UINT:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  uint32_t val = current_entry->get_uint(*e);
                  if (val != 0)
                    oss << fmt::format(", \"{}\":{}", entry_name, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  uint32_t val = current_entry->get_uint(*e);
                  if (val != static_cast<uint32_t>(-1))
                    oss << fmt::format(", \"{}\":{}", entry_name, val);
                } break;
                default:
                  oss << fmt::format(", \"{}\":{}", entry_name,
                                     current_entry->get_uint(*e));
              }
              break;
            case mapping::source::ULONG:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  uint64_t val = current_entry->get_ulong(*e);
                  if (val != 0)
                    oss << fmt::format(", \"{}\":{}", entry_name, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  uint64_t val = current_entry->get_ulong(*e);
                  if (val != static_cast<uint64_t>(-1))
                    oss << fmt::format(", \"{}\":{}", entry_name, val);
                } break;
                default:
                  oss << fmt::format(", \"{}\":{}", entry_name,
                                     current_entry->get_ulong(*e));
              }
              break;

            default:  // Error in one of the mappings.
              throw msg_fmt(
                  "invalid mapping for object "
                  "of type '{}': {}"
                  " is not a known type ID",
                  info->get_name(), current_entry->get_type());
          }
        }
      }
    } else {
      oss << ", ";
      /* Here is the protobuf case: no mapping */
      const google::protobuf::Message* p =
          static_cast<const io::protobuf_base*>(e.get())->msg();
      _message_to_json(oss, p);
    }
  } else
    throw msg_fmt(
        "cannot bind object of type {}"
        " to database query: mapping does not exist",
        e->type());
  oss << '}';
}

/**
 *  The json_encode function for Lua objects others than tables
 *
 *  @param L The Lua interpreter
 *  @param oss The output stream
 */
static void broker_json_encode(lua_State* L, std::ostringstream& oss) {
  switch (lua_type(L, -1)) {
    case LUA_TNUMBER:
      oss << lua_tostring(L, -1);
      break;
    case LUA_TSTRING: {
      /* If the string contains '"', we must escape it */
      const char* content = lua_tostring(L, -1);
      oss << '"';
      escape_str(content, oss);
      oss << '"';
    } break;
    case LUA_TBOOLEAN:
      oss << (lua_toboolean(L, -1) ? "true" : "false");
      break;
    case LUA_TTABLE:
      broker_json_encode_table(L, oss);
      break;
    case LUA_TUSERDATA: {
      void* ptr = luaL_checkudata(L, 1, "broker_event");
      if (ptr) {
        auto event = static_cast<std::shared_ptr<io::data>*>(ptr);
        broker_json_encode_broker_event(*event, oss);
      }
    } break;
    default:
      luaL_error(L, "json_encode: type not implemented");
  }
}

/**
 *  The Lua json_encode function (the real one)
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_json_encode(lua_State* L) noexcept {
  try {
    std::ostringstream oss;
    broker_json_encode(L, oss);
    std::string s{oss.str()};
    lua_pushlstring(L, s.c_str(), s.size());
    return 1;
  } catch (const std::exception& e) {
    auto logger = log_v2::instance().get(log_v2::LUA);
    logger->error("lua: json_encode encountered an error: {}", e.what());
  }
  return 0;
}

/**
 *  The Lua json_decode function for arrays.
 *
 *  @param L The Lua interpreter
 *  @param it The current json_iterator
 */
static void broker_json_decode_array(lua_State* L, const json& it) {
  size_t size{it.size()};
  lua_createtable(L, size, 0);
  auto cit = it.begin();
  for (uint32_t i = 1; i <= size; ++i, ++cit) {
    broker_json_decode(L, *cit);
    lua_rawseti(L, -2, i);
  }
}

/**
 *  The Lua json_decode function for objects.
 *
 *  @param L The Lua interpreter
 *  @param it The current json_iterator
 */
static void broker_json_decode_object(lua_State* L, const json& it) {
  size_t size{it.size()};
  lua_createtable(L, 0, size);
  for (auto cit = it.begin(); cit != it.end(); ++cit) {
    lua_pushlstring(L, cit.key().c_str(), cit.key().size());
    broker_json_decode(L, cit.value());
    lua_settable(L, -3);
  }
}

/**
 *  The Lua json_decode function for anything else than array and object.
 *
 *  @param L The Lua interpreter
 *  @param it The current json_iterator
 */
static void broker_json_decode(lua_State* L, const json& it) {
  switch (it.type()) {
    case json::value_t::string: {
      std::string str(it.get<std::string>());
      size_t pos(str.find_first_of("\\"));
      while (pos != std::string::npos) {
        switch (str[pos + 1]) {
          case '\\':
            str.replace(pos, 2, "\\");
            break;
          case '"':
            str.replace(pos, 2, "\"");
            break;
          case 't':
            str.replace(pos, 2, "\t");
            break;
          case 'r':
            str.replace(pos, 2, "\r");
            break;
          case 'n':
            str.replace(pos, 2, "\n");
            break;
        }
        ++pos;
        pos = str.find_first_of("\\", pos);
      }
      lua_pushlstring(L, str.c_str(), str.size());
    } break;
    case json::value_t::number_unsigned:
      lua_pushinteger(L, it.get<uint32_t>());
      break;
    case json::value_t::number_integer:
      lua_pushinteger(L, it.get<int32_t>());
      break;
    case json::value_t::number_float:
      lua_pushnumber(L, it.get<double>());
      break;
    case json::value_t::boolean:
      lua_pushboolean(L, it.get<bool>() ? 1 : 0);
      break;
    case json::value_t::array:
      broker_json_decode_array(L, it);
      break;
    case json::value_t::object:
      broker_json_decode_object(L, it);
      break;
    case json::value_t::null:
      lua_pushnil(L);
      break;
    default:
      luaL_error(L, "Unrecognized type in json content");
  }
}

/**
 *  The Lua json_decode function (the real one)
 *
 *  @param L The Lua interpreter
 */
static int l_broker_json_decode(lua_State* L) {
  char const* content(luaL_checkstring(L, -1));

  json it;
  try {
    it = json::parse(content);
  } catch (const json::parse_error& e) {
    std::string err{e.what()};
    lua_pushnil(L);
    lua_pushlstring(L, err.c_str(), err.size());
    return 2;
  }
  broker_json_decode(L, it);
  return 1;
}

/**
 * @brief This function is useful for debug purposes. It shows the stack
 * of the Lua state machine.
 *
 * @param L The Lua state machine
 */
static auto l_stacktrace = [](lua_State* L) -> void {
  int n = lua_gettop(L);  // number of arguments
  for (int i = 1; i <= n; i++) {
    int t = lua_type(L, i);
    switch (t) {
      case LUA_TSTRING:
        printf("%d: '%s'\n", i, lua_tostring(L, i));
        break;
      case LUA_TBOOLEAN:
        printf("%d: %s\n", i, lua_toboolean(L, i) ? "true" : "false");
        break;
      case LUA_TNUMBER:
        printf("%d: %g\n", i, lua_tonumber(L, i));
        break;
      default:
        printf("%d: %s\n", i, lua_typename(L, t));
    }
  }
};

/**
 *  The Lua parse_perfdata function
 *
 * @param L The Lua interpreter
 *
 * @return 1
 */
static int l_broker_parse_perfdata(lua_State* L) {
  char const* perf_data(lua_tostring(L, 1));
  int full(lua_toboolean(L, 2));
  auto logger = log_v2::instance().get(log_v2::LUA);
  std::list<com::centreon::common::perfdata> pds{
      com::centreon::common::perfdata::parse_perfdata(0, 0, perf_data, logger)};
  lua_createtable(L, 0, pds.size());
  for (auto& pd : pds) {
    pd.resize_name(com::centreon::common::adjust_size_utf8(
        pd.name(), get_centreon_storage_metrics_col_size(
                       centreon_storage_metrics_metric_name)));
    pd.resize_unit(com::centreon::common::adjust_size_utf8(
        pd.unit(), get_centreon_storage_metrics_col_size(
                       centreon_storage_metrics_unit_name)));

    lua_pushlstring(L, pd.name().c_str(), pd.name().size());
    if (full) {
      std::string_view name{pd.name()};
      std::string_view metric;
      std::string_view fullinstance;
      std::list<std::string_view> subinstance;
      lua_createtable(L, 0, 15);
      int find_sharp = name.find("#");
      int find_tilde = name.find("~");
      if (find_sharp == -1) {
        metric = pd.name();
      } else {
        metric = name.substr(find_sharp + 1);
        fullinstance = name.substr(0, find_sharp);
        subinstance = absl::StrSplit(fullinstance, '~');
      }
      std::list<std::string_view> metric_fields{absl::StrSplit(metric, '.')};

      lua_pushnumber(L, pd.value());
      lua_setfield(L, -2, "value");
      lua_pushlstring(L, pd.unit().c_str(), pd.unit().size());
      lua_setfield(L, -2, "uom");
      lua_pushlstring(L, metric.data(), metric.length());
      lua_setfield(L, -2, "metric_name");
      if (find_sharp < 1) {
        lua_pushlstring(L, "", sizeof("") - 1);
        lua_setfield(L, -2, "instance");
      } else if (find_tilde == -1) {
        lua_pushlstring(L, name.data(), name.substr(0, find_sharp).length());
        lua_setfield(L, -2, "instance");
      } else {
        lua_pushlstring(L, name.data(), name.substr(0, find_tilde).length());
        lua_setfield(L, -2, "instance");
      }
      int find_pts = name.find_last_of(".");
      lua_pushlstring(L, name.substr(find_pts + 1, name.size()).data(),
                      name.substr(find_pts + 1, name.size()).length());
      lua_setfield(L, -2, "metric_unit");
      lua_pushlstring(L, "metric_fields", sizeof("metric_fields") - 1);
      lua_createtable(L, 0, 1);
      int i = 0;
      for (auto const& field : metric_fields) {
        ++i;
        lua_pushlstring(L, field.data(), field.length());
        lua_rawseti(L, -2, i);
      }
      lua_settable(L, -3);
      lua_pushlstring(L, "subinstance", sizeof("subinstance") - 1);
      lua_createtable(L, 0, 1);
      i = 0;
      for (std::list<std::string_view>::const_iterator
               it(std::next(subinstance.begin())),
           end(subinstance.end());
           it != end; ++it) {
        ++i;
        lua_pushlstring(L, it->data(), it->length());
        lua_rawseti(L, -2, i);
      }
      lua_settable(L, -3);
      lua_pushnumber(L, pd.min());
      lua_setfield(L, -2, "min");
      lua_pushnumber(L, pd.max());
      lua_setfield(L, -2, "max");
      lua_pushnumber(L, pd.warning());
      lua_setfield(L, -2, "warning_high");
      lua_pushnumber(L, pd.warning_low());
      lua_setfield(L, -2, "warning_low");
      lua_pushboolean(L, pd.warning_mode());
      lua_setfield(L, -2, "warning_mode");
      lua_pushnumber(L, pd.critical());
      lua_setfield(L, -2, "critical_high");
      lua_pushnumber(L, pd.critical_low());
      lua_setfield(L, -2, "critical_low");
      lua_pushboolean(L, pd.critical_mode());
      lua_setfield(L, -2, "critical_mode");
      lua_settable(L, -3);
    } else {
      lua_pushnumber(L, pd.value());
      lua_settable(L, -3);
    }
  }
  return 1;
}

/**
 *  The Lua url_encode function
 *
 * @param L The Lua interpreter
 *
 * @return 1
 */
static int l_broker_url_encode(lua_State* L) {
  size_t len;
  char const* str = lua_tolstring(L, -1, &len);

  std::string retval;
  retval.reserve(len * 1.5);
  for (char const* cc = str; *cc; ++cc) {
    char c = *cc;
    // Keep alphanumeric and other accepted characters intact
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      retval.push_back(c);
      continue;
    }

    // Any other characters are percent-encoded
    retval.append(fmt::format("\%{:02X}", int((unsigned char)c)));
  }

  lua_pushstring(L, retval.c_str());
  return 1;
}

/**
 *  The Lua base64_encode function
 *
 * @param L The Lua interpreter
 *
 * @return 1
 */
static int l_broker_base64_encode(lua_State* L) {
  size_t len;
  char const* str = lua_tolstring(L, -1, &len);
  std::string_view sv(str, len);

  std::string retval = com::centreon::common::crypto::base64_encode(sv);

  lua_pushlstring(L, retval.c_str(), retval.size());
  return 1;
}

/**
 *  The Lua base64_decode function
 *
 * @param L The Lua interpreter
 *
 * @return 1
 */
static int l_broker_base64_decode(lua_State* L) {
  size_t len;
  char const* str = lua_tolstring(L, -1, &len);
  std::string_view sv(str, len);

  std::string retval = base64_decode(sv);

  lua_pushlstring(L, retval.c_str(), retval.size());
  return 1;
}

/**
 * @brief The Lua stat function that is just a binding to the C stat().
 * The Lua function will return the asked object or nil followed by an
 * error message.
 *
 * @param L The Lua interpreter
 *
 * @return 1 if the call is ok, 2 otherwise.
 */
static int l_broker_stat(lua_State* L) {
  char const* filename = lua_tostring(L, -1);

  struct stat s;
  if (stat(filename, &s) == -1) {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    return 2;
  } else {
    lua_createtable(L, 0, 7);
    lua_pushinteger(L, s.st_uid);
    lua_setfield(L, -2, "uid");
    lua_pushinteger(L, s.st_gid);
    lua_setfield(L, -2, "gid");
    lua_pushinteger(L, s.st_size);
    lua_setfield(L, -2, "size");
    lua_pushinteger(L, s.st_atime);
    lua_setfield(L, -2, "atime");
    lua_pushinteger(L, s.st_mtime);
    lua_setfield(L, -2, "mtime");
    lua_pushinteger(L, s.st_ctime);
    lua_setfield(L, -2, "ctime");
    return 1;
  }
}

static void md5_message(const unsigned char* message,
                        size_t message_len,
                        unsigned char** digest,
                        unsigned int* digest_len) {
  EVP_MD_CTX* mdctx;
  auto logger = log_v2::instance().get(log_v2::LUA);
  if ((mdctx = EVP_MD_CTX_new()) == nullptr) {
    logger->error("lua: fail to call MD5 (EVP_MD_CTX_new call)");
  }
  if (1 != EVP_DigestInit_ex(mdctx, EVP_md5(), nullptr)) {
    logger->error("lua: fail to call MD5 (EVP_DigestInit_ex call)");
  }
  if (1 != EVP_DigestUpdate(mdctx, message, message_len)) {
    logger->error("lua: fail to call MD5 (EVP_DigestUpdate call)");
  }
  if ((*digest = (unsigned char*)OPENSSL_malloc(EVP_MD_size(EVP_md5()))) ==
      nullptr) {
    logger->error("lua: fail to call MD5 (OPENSSL_malloc call)");
  }
  if (1 != EVP_DigestFinal_ex(mdctx, *digest, digest_len)) {
    logger->error("lua: fail to call MD5 (EVP_DigestFinal_ex call)");
  }
  EVP_MD_CTX_free(mdctx);
}

static int l_broker_md5(lua_State* L) {
  auto digit = [](unsigned char d) -> char {
    if (d < 10)
      return '0' + d;
    else
      return 'a' + (d - 10);
  };
  size_t len;
  const unsigned char* str =
      reinterpret_cast<const unsigned char*>(lua_tolstring(L, -1, &len));
  unsigned char* md5;
  uint32_t md5_len;
  md5_message(str, len, &md5, &md5_len);
  char result[2 * md5_len + 1];
  char* tmp = result;
  for (uint32_t i = 0; i < md5_len; i++) {
    *tmp = digit(md5[i] >> 4);
    ++tmp;
    *tmp = digit(md5[i] & 0xf);
    ++tmp;
  }
  *tmp = 0;
  lua_pushstring(L, result);
  OPENSSL_free(md5);
  return 1;
}

/**
 * @brief The Lua bbdo_version function (the real one). In the Lua, it returns
 * a string with the bbdo version configured.
 *
 * @param L The Lua interpreter
 *
 * @return 1
 */
static int l_broker_bbdo_version(lua_State* L) {
  auto bbdo = config::applier::state::instance().get_bbdo_version();
  std::string ret{
      fmt::format("{}.{}.{}", bbdo.major_v, bbdo.minor_v, bbdo.patch)};
  lua_pushlstring(L, ret.c_str(), ret.size());
  return 1;
}

/**
 *  Load the Lua interpreter with the standard libraries
 *  and the broker lua sdk.
 *
 *  @return The Lua interpreter as a lua_State*
 */
void broker_utils::broker_utils_reg(lua_State* L) {
  luaL_Reg s_broker_regs[] = {{"json_encode", l_broker_json_encode},
                              {"json_decode", l_broker_json_decode},
                              {"parse_perfdata", l_broker_parse_perfdata},
                              {"url_encode", l_broker_url_encode},
                              {"base64_encode", l_broker_base64_encode},
                              {"base64_decode", l_broker_base64_decode},
                              {"stat", l_broker_stat},
                              {"md5", l_broker_md5},
                              {"bbdo_version", l_broker_bbdo_version},
                              {nullptr, nullptr}};

#ifdef LUA51
  luaL_register(L, "broker", s_broker_regs);
  lua_pop(L, 1);
#else
  luaL_newlibtable(L, s_broker_regs);
  luaL_setfuncs(L, s_broker_regs, 0);
  lua_setglobal(L, "broker");
#endif
}
