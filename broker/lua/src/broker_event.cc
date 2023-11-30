/**
* Copyright 2020-2021 Centreon
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

#include "com/centreon/broker/lua/broker_event.hh"
#include <google/protobuf/message.h>

#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::lua;
using namespace com::centreon::exceptions;

static void _write_item(lua_State* L,
                        const google::protobuf::Message* p,
                        const google::protobuf::FieldDescriptor* f);

/**
 *  The Lua broker_event constructor
 *
 *  @param L The Lua interpreter
 *  @param e An event to wrap
 *
 */
void broker_event::create(lua_State* L, std::shared_ptr<io::data> e) {
  void* userdata = lua_newuserdata(L, sizeof(std::shared_ptr<io::data>));
  if (!userdata)
    throw msg_fmt("Unable to build a lua broker_event");

  new (userdata) std::shared_ptr<io::data>(e);

  luaL_getmetatable(L, "broker_event");
  lua_setmetatable(L, -2);
}

static void _message_to_table(lua_State* L,
                              const google::protobuf::Message* p) {
  const google::protobuf::Descriptor* desc = p->GetDescriptor();
  const google::protobuf::Reflection* refl = p->GetReflection();
  for (int i = 0; i < desc->field_count(); i++) {
    const google::protobuf::FieldDescriptor* f = desc->field(i);

    auto oof = f->containing_oneof();
    if (oof) {
      if (!refl->GetOneofFieldDescriptor(*p, oof)) {
        continue;
      }
    }

    _write_item(L, p, f);
    lua_rawset(L, -3);
  }
}

static void _write_item(lua_State* L,
                        const google::protobuf::Message* p,
                        const google::protobuf::FieldDescriptor* f) {
  const google::protobuf::Reflection* refl = p->GetReflection();
  if (f) {
    const std::string& entry_name = f->name();
    lua_pushlstring(L, entry_name.c_str(), entry_name.size());
    if (f->is_repeated()) {
      size_t s = refl->FieldSize(*p, f);
      lua_newtable(L);
      switch (f->type()) {
        case google::protobuf::FieldDescriptor::TYPE_BOOL:
          for (size_t i = 0; i < s; i++) {
            lua_pushboolean(L, refl->GetRepeatedBool(*p, f, i));
            lua_rawseti(L, -2, i + 1);
          }
          break;
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
          for (size_t i = 0; i < s; i++) {
            lua_pushnumber(L, refl->GetRepeatedDouble(*p, f, i));
            lua_rawseti(L, -2, i + 1);
          }
          break;
        case google::protobuf::FieldDescriptor::TYPE_INT32:
          for (size_t i = 0; i < s; i++) {
            lua_pushinteger(L, refl->GetRepeatedInt32(*p, f, i));
            lua_rawseti(L, -2, i + 1);
          }
          break;
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
          for (size_t i = 0; i < s; i++) {
            lua_pushinteger(L, refl->GetRepeatedUInt32(*p, f, i));
            lua_rawseti(L, -2, i + 1);
          }
          break;
        case google::protobuf::FieldDescriptor::TYPE_INT64:
          for (size_t i = 0; i < s; i++) {
            lua_pushinteger(L, refl->GetRepeatedInt64(*p, f, i));
            lua_rawseti(L, -2, i + 1);
          }
          break;
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
          for (size_t i = 0; i < s; i++) {
            lua_pushinteger(L, refl->GetRepeatedUInt64(*p, f, i));
            lua_rawseti(L, -2, i + 1);
          }
          break;
        case google::protobuf::FieldDescriptor::TYPE_ENUM:
          for (size_t i = 0; i < s; i++) {
            lua_pushinteger(L, refl->GetRepeatedEnumValue(*p, f, i));
            lua_rawseti(L, -2, i + 1);
          }
          break;
        case google::protobuf::FieldDescriptor::TYPE_STRING: {
          for (size_t i = 0; i < s; i++) {
            const std::string& s = refl->GetRepeatedString(*p, f, i);
            lua_pushlstring(L, s.c_str(), s.size());
            lua_rawseti(L, -2, i + 1);
          }
        } break;
        case google::protobuf::FieldDescriptor::TYPE_MESSAGE: {
          for (size_t i = 0; i < s; i++) {
            lua_newtable(L);
            _message_to_table(L, &refl->GetRepeatedMessage(*p, f, i));
            lua_rawseti(L, -2, i + 1);
          }
        } break;
        default:
          lua_pushlstring(L, "not_implemented", 15);
          break;
      }
    } else {
      switch (f->type()) {
        case google::protobuf::FieldDescriptor::TYPE_BOOL:
          lua_pushboolean(L, refl->GetBool(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
          lua_pushnumber(L, refl->GetDouble(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_INT32:
          lua_pushinteger(L, refl->GetInt32(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
          lua_pushinteger(L, refl->GetUInt32(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_INT64:
          lua_pushinteger(L, refl->GetInt64(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
          lua_pushinteger(L, refl->GetUInt64(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_ENUM:
          lua_pushinteger(L, refl->GetEnumValue(*p, f));
          break;
        case google::protobuf::FieldDescriptor::TYPE_STRING: {
          const std::string& s = refl->GetString(*p, f);
          lua_pushlstring(L, s.c_str(), s.size());
        } break;
        case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
          lua_newtable(L);
          _message_to_table(L, &refl->GetMessage(*p, f));
          break;
        default:
          lua_pushlstring(L, "not_implemented", 15);
          break;
      }
    }
  }
}

/**
 *  Given an event d, this method converts it to a Lua table.
 *  The result is stored on the Lua interpreter stack. This internal function
 *  is essentially needed by the broker api version 1. We do not transpose
 *  events to table in api v2.
 *
 *  @param L The Lua interpreter.
 *  @param d The event to convert.
 */
void broker_event::create_as_table(lua_State* L, const io::data& d) {
  uint32_t type(d.type());
  uint16_t cat(category_of_type(type));
  uint16_t elem(element_of_type(type));
  lua_newtable(L);
  lua_pushstring(L, "_type");
  lua_pushinteger(L, type);
  lua_rawset(L, -3);

  lua_pushstring(L, "category");
  lua_pushinteger(L, cat);
  lua_rawset(L, -3);

  lua_pushstring(L, "element");
  lua_pushinteger(L, elem);
  lua_rawset(L, -3);
  const io::event_info* info(io::events::instance().get_event_info(d.type()));
  if (info) {
    if (info->get_mapping()) {
      for (const mapping::entry* current_entry(info->get_mapping());
           !current_entry->is_null(); ++current_entry) {
        const char* entry_name(current_entry->get_name_v2());
        if (entry_name && entry_name[0]) {
          lua_pushstring(L, entry_name);
          switch (current_entry->get_type()) {
            case mapping::source::BOOL:
              lua_pushboolean(L, current_entry->get_bool(d));
              break;
            case mapping::source::DOUBLE:
              lua_pushnumber(L, current_entry->get_double(d));
              break;
            case mapping::source::INT:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  int val(current_entry->get_int(d));
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  int val(current_entry->get_int(d));
                  if (val == -1)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_int(d));
              }
              break;
            case mapping::source::SHORT:
              lua_pushinteger(L, current_entry->get_short(d));
              break;
            case mapping::source::STRING:
              if (current_entry->get_attribute() ==
                  mapping::entry::invalid_on_zero) {
                std::string val{current_entry->get_string(d)};
                if (val.empty())
                  lua_pushnil(L);
                else
                  lua_pushstring(L, val.c_str());
              } else
                lua_pushstring(L, current_entry->get_string(d).c_str());
              break;
            case mapping::source::TIME:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  time_t val = current_entry->get_time(d);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  time_t val = current_entry->get_time(d);
                  if (val == -1)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_time(d));
              }
              break;
            case mapping::source::UINT:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  uint32_t val = current_entry->get_uint(d);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  uint32_t val = current_entry->get_uint(d);
                  if (val == static_cast<uint32_t>(-1))
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_uint(d));
              }
              break;
            case mapping::source::ULONG:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  uint64_t val = current_entry->get_ulong(d);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  uint64_t val = current_entry->get_ulong(d);
                  if (val == static_cast<uint64_t>(-1))
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_ulong(d));
              }
              break;

            default:  // Error in one of the mappings.
              throw msg_fmt(
                  "invalid mapping for object "
                  "of type '{}': {}"
                  " is not a known type ID",
                  info->get_name(), current_entry->get_type());
          }
          lua_rawset(L, -3);
        }
      }
    } else {
      /* Here is the protobuf case: no mapping */
      const google::protobuf::Message* p =
          static_cast<const io::protobuf_base*>(&d)->msg();
      _message_to_table(L, p);
    }
  } else
    throw msg_fmt(
        "cannot bind object of type {}"
        " to lua table: mapping does not exist",
        d.type());
}

/**
 *  The Lua broker_event destructor
 *
 *  @param L The Lua interpreter
 *
 *  @return 0
 */
static int l_broker_event_destructor(lua_State* L) {
  void* ptr = luaL_checkudata(L, 1, "broker_event");

  if (ptr) {
    auto event = static_cast<std::shared_ptr<io::data>*>(ptr);
    event->reset();
  }
  return 0;
}

/**
 * @brief The pairs() Lua function needed for iteration on Broker event content.
 *
 * @param L The Lua interpreter. We need to access its stack where our element
 * is.
 *
 * @return 3 (The number of element on the stack to return: the __next internal
 * function, the broker event and nil).
 */
static int l_broker_event_pairs(lua_State* L) {
  std::shared_ptr<io::data> e{*static_cast<std::shared_ptr<io::data>*>(
      luaL_checkudata(L, 1, "broker_event"))};
  luaL_getmetafield(L, 1, "__next");
  lua_insert(L, 1);
  lua_pushnil(L);
  return 3;
}

/**
 * @brief The next() internal function needed for iteration on Broker event
 * content.
 *
 * @param L The Lua interpreter. We need to access its stack.
 *
 * @return Usually 2 (the number of elements when the function exits. Elements
 * are a key and its value.) or 1 when the iteration is over (the returned
 * element is nil).
 */
static int l_broker_event_next(lua_State* L) {
  std::shared_ptr<io::data> e{*static_cast<std::shared_ptr<io::data>*>(
      luaL_checkudata(L, 1, "broker_event"))};
  size_t keyl;
  const char* key = lua_tolstring(L, 2, &keyl);

  if (key == nullptr) {
    lua_pushstring(L, "_type");
    lua_pushinteger(L, e->type());
    return 2;
  } else if (strcmp(key, "_type") == 0) {
    lua_pushstring(L, "category");
    lua_pushinteger(L, static_cast<uint32_t>(e->type()) >> 16);
    return 2;
  } else if (strcmp(key, "category") == 0) {
    lua_pushstring(L, "element");
    lua_pushinteger(L, e->type() & 0xffff);
    return 2;
  }

  io::event_info const* info = io::events::instance().get_event_info(e->type());
  if (info) {
    if (info->get_mapping()) {
      bool found = false;
      for (const mapping::entry* current_entry = info->get_mapping();
           !current_entry->is_null(); ++current_entry) {
        char const* entry_name(current_entry->get_name_v2());
        if (!entry_name || *entry_name == 0)
          continue;
        if (strcmp(key, "element") == 0)
          found = true;
        else if (!found && strcmp(entry_name, key) == 0) {
          found = true;
          continue;
        }
        if (found) {
          lua_pushstring(L, entry_name);
          switch (current_entry->get_type()) {
            case mapping::source::BOOL:
              lua_pushboolean(L, current_entry->get_bool(*e));
              break;
            case mapping::source::DOUBLE:
              lua_pushnumber(L, current_entry->get_double(*e));
              break;
            case mapping::source::INT:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  int val(current_entry->get_int(*e));
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  int val(current_entry->get_int(*e));
                  if (val == -1)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_int(*e));
              }
              break;
            case mapping::source::SHORT:
              lua_pushinteger(L, current_entry->get_short(*e));
              break;
            case mapping::source::STRING:
              if (current_entry->get_attribute() ==
                  mapping::entry::invalid_on_zero) {
                std::string val{current_entry->get_string(*e)};
                if (val.empty())
                  lua_pushnil(L);
                else
                  lua_pushstring(L, val.c_str());
              } else
                lua_pushstring(L, current_entry->get_string(*e).c_str());
              break;
            case mapping::source::TIME:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  time_t val = current_entry->get_time(*e);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  time_t val = current_entry->get_time(*e);
                  if (val == -1)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_time(*e));
              }
              break;
            case mapping::source::UINT:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  uint32_t val = current_entry->get_uint(*e);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  uint32_t val = current_entry->get_uint(*e);
                  if (val == static_cast<uint32_t>(-1))
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_uint(*e));
              }
              break;
            case mapping::source::ULONG:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  uint64_t val = current_entry->get_ulong(*e);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  uint64_t val = current_entry->get_ulong(*e);
                  if (val == static_cast<uint64_t>(-1))
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_ulong(*e));
              }
              break;

            default:  // Error in one of the mappings.
              lua_pushnil(L);
          }
          return 2;
        }
      }
    } else {
      /* Here is the protobuf case: no mapping */
      const google::protobuf::Message* p =
          static_cast<const io::protobuf_base*>(e.get())->msg();
      const google::protobuf::Descriptor* desc = p->GetDescriptor();
      const google::protobuf::Reflection* refl = p->GetReflection();
      const google::protobuf::FieldDescriptor* f;
      if (strcmp(key, "element") == 0)
        f = desc->field(0);
      else {
        f = desc->FindFieldByName(key);
        int idx = f->index();
        if (idx + 1 < desc->field_count())
          f = desc->field(f->index() + 1);
        else
          f = nullptr;
      }
      if (f) {
        auto oof = f->containing_oneof();
        if (oof) {
          if (!refl->GetOneofFieldDescriptor(*p, oof)) {
            return 0;
          }
        }
        _write_item(L, p, f);
        return 2;
      }
    }
    lua_pushnil(L);
    return 1;
  } else
    throw msg_fmt(
        "unable to parse object of type {}"
        " ; it does not look like a BBDO event",
        e->type());
}

/**
 * @brief This function is the implementation of indexation on a broker event.
 * It is useful to be able to get for example the name of a host, we enter
 * something like host.name
 *
 * @param L The Lua interpreter to access its stack.
 *
 * @return 1 (the number of element returned by this function which is just
 * a value).
 */
static int l_broker_event_index(lua_State* L) {
  std::shared_ptr<io::data> e{*static_cast<std::shared_ptr<io::data>*>(
      luaL_checkudata(L, 1, "broker_event"))};
  const char* key = luaL_checkstring(L, 2);

  if (strcmp(key, "_type") == 0) {
    lua_pushinteger(L, e->type());
    return 1;
  } else if (strcmp(key, "category") == 0) {
    lua_pushinteger(L, (static_cast<uint32_t>(e->type()) >> 16));
    return 1;
  } else if (strcmp(key, "element") == 0) {
    lua_pushinteger(L, e->type() & 0xffff);
    return 1;
  }

  io::event_info const* info = io::events::instance().get_event_info(e->type());
  if (info) {
    if (info->get_mapping()) {
      for (const mapping::entry* current_entry = info->get_mapping();
           !current_entry->is_null(); ++current_entry) {
        char const* entry_name(current_entry->get_name_v2());
        if (entry_name && strcmp(entry_name, key) == 0) {
          switch (current_entry->get_type()) {
            case mapping::source::BOOL:
              lua_pushboolean(L, current_entry->get_bool(*e));
              break;
            case mapping::source::DOUBLE:
              lua_pushnumber(L, current_entry->get_double(*e));
              break;
            case mapping::source::INT:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  int val(current_entry->get_int(*e));
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  int val(current_entry->get_int(*e));
                  if (val == -1)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_int(*e));
              }
              break;
            case mapping::source::SHORT:
              lua_pushinteger(L, current_entry->get_short(*e));
              break;
            case mapping::source::STRING:
              if (current_entry->get_attribute() ==
                  mapping::entry::invalid_on_zero) {
                std::string val{current_entry->get_string(*e)};
                if (val.empty())
                  lua_pushnil(L);
                else
                  lua_pushstring(L, val.c_str());
              } else
                lua_pushstring(L, current_entry->get_string(*e).c_str());
              break;
            case mapping::source::TIME:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  time_t val = current_entry->get_time(*e);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  time_t val = current_entry->get_time(*e);
                  if (val == -1)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_time(*e));
              }
              break;
            case mapping::source::UINT:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  uint32_t val = current_entry->get_uint(*e);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  uint32_t val = current_entry->get_uint(*e);
                  if (val == static_cast<uint32_t>(-1))
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_uint(*e));
              }
              break;
            case mapping::source::ULONG:
              switch (current_entry->get_attribute()) {
                case mapping::entry::invalid_on_zero: {
                  uint64_t val = current_entry->get_ulong(*e);
                  if (val == 0)
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                case mapping::entry::invalid_on_minus_one: {
                  uint64_t val = current_entry->get_ulong(*e);
                  if (val == static_cast<uint64_t>(-1))
                    lua_pushnil(L);
                  else
                    lua_pushinteger(L, val);
                } break;
                default:
                  lua_pushinteger(L, current_entry->get_ulong(*e));
              }
              break;

            default:  // Error in one of the mappings.
              throw msg_fmt(
                  "invalid mapping for object of type '{}': {} is not a known "
                  "type ID",
                  info->get_name(), current_entry->get_type());
          }
          return 1;
        }
      }
    } else {
      /* Here is the protobuf case: no mapping */
      const google::protobuf::Message* p =
          static_cast<const io::protobuf_base*>(e.get())->msg();
      const google::protobuf::Descriptor* desc = p->GetDescriptor();
      const google::protobuf::Reflection* refl = p->GetReflection();
      auto f = desc->FindFieldByName(key);
      if (f) {
        auto oof = f->containing_oneof();
        if (oof) {
          if (!refl->GetOneofFieldDescriptor(*p, oof))
            return 0;
        }

        _write_item(L, p, f);
        return 1;
      }
    }
  } else
    throw msg_fmt(
        "cannot bind object of type {} "
        " to lua userdata: mapping does not exist",
        e->type());
  return 0;
}

/**
 *  Register the broker_event element in the Lua interpreter.
 *
 *  @param The Lua interpreter as a lua_State*
 */
void broker_event::broker_event_reg(lua_State* L) {
  luaL_Reg s_broker_event_regs[] = {{"__gc", l_broker_event_destructor},
                                    {"__index", l_broker_event_index},
                                    {"__next", l_broker_event_next},
                                    {"__pairs", l_broker_event_pairs},
                                    {nullptr, nullptr}};

  const char* name = "broker_event";
  luaL_newmetatable(L, name);

  // Register the C functions into the metatable we just created.
#ifdef LUA51
  luaL_register(L, NULL, s_broker_event_regs);
#else
  luaL_setfuncs(L, s_broker_event_regs, 0);
#endif

  lua_setglobal(L, name);
}
