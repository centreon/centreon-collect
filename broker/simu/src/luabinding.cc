/**
 * Copyright 2018 Centreon
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

#include "com/centreon/broker/simu/luabinding.hh"
#include <cassert>
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/lua/broker_log.hh"
#include "com/centreon/broker/lua/broker_utils.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::simu;

/**
 *  Constructor.
 *
 *  @param[in] lua_script the json parameters file
 *  @param[in] conf_params A hash table with user parameters
 */
luabinding::luabinding(std::string const& lua_script,
                       std::map<std::string, misc::variant> const& conf_params,
                       const std::shared_ptr<spdlog::logger>& logger)
    : _logger(logger), _lua_script(lua_script), _total(0) {
  size_t pos(lua_script.find_last_of('/'));
  std::string path(lua_script.substr(0, pos));
  _L = _load_interpreter();
  _update_lua_path(path);

  _logger->debug("simu: initializing the Lua state machine");

  _load_script();
  _init_script(conf_params);
}

/**
 *  Destructor
 */
luabinding::~luabinding() {
  if (_L)
    lua_close(_L);
}

/**
 *  This function updates the Lua variables package.path and package.cpath so
 *  that a script in that directory can be called without difficulty.
 *
 * @param path A directory to add to the Lua paths
 */
void luabinding::_update_lua_path(std::string const& path) {
  /* Working on path: lua scripts */
  lua_getglobal(_L, "package");
  lua_getfield(_L, -1, "path");
  std::string current_path(lua_tostring(_L, -1));
  current_path.append(";");
  current_path.append(path);
  current_path.append("/?.lua");
  lua_pop(_L, 1);
  lua_pushstring(_L, current_path.c_str());
  lua_setfield(_L, -2, "path");

  /* Working on cpath: so libraries */
  lua_getfield(_L, -1, "cpath");
  current_path = lua_tostring(_L, -1);
  current_path.append(";");
  current_path.append(path);
  current_path.append("/lib/?.so");
  lua_pop(_L, 1);
  lua_pushstring(_L, current_path.c_str());
  lua_setfield(_L, -2, "cpath");
  lua_pop(_L, 1);
}

/**
 *  Reads the Lua script, checks its syntax and checks if
 *   - init()
 *   - read()
 *  functions exist in the Lua script. The two first ones are
 *  mandatory whereas the third one is optional.
 */
void luabinding::_load_script() {
  // script loading
  if (luaL_loadfile(_L, _lua_script.c_str()) != 0) {
    char const* error_msg(lua_tostring(_L, -1));
    throw msg_fmt("simu: '{}' could not be loaded: {}", _lua_script, error_msg);
  }

  // Script compilation
  if (lua_pcall(_L, 0, 0, 0) != 0) {
    throw msg_fmt("simu: '{}' could not be compiled", _lua_script);
  }

  // Checking for init() availability: this function is mandatory
  lua_getglobal(_L, "init");
  if (!lua_isfunction(_L, lua_gettop(_L)))
    throw msg_fmt("simu: '{}' init() global function is missing", _lua_script);
  lua_pop(_L, 1);

  // Checking for read() availability: this function is mandatory
  lua_getglobal(_L, "read");
  if (!lua_isfunction(_L, lua_gettop(_L)))
    throw msg_fmt("simu: '{}' read() global function is missing", _lua_script);
  lua_pop(_L, 1);
}

/**
 *  Executes the init() function given in the Lua script
 *  with the content of conf_params as parameter.
 *
 *  @param conf_params A hashtable of data providing various
 *  informations needed by the script to work.
 *
 */
void luabinding::_init_script(
    std::map<std::string, misc::variant> const& conf_params) {
  lua_getglobal(_L, "init");
  lua_newtable(_L);
  for (const auto& [name, val] : conf_params) {
    std::visit(
        [this, &name](auto&& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, int32_t> ||
                        std::is_same_v<T, uint32_t>) {
            lua_pushstring(_L, name.c_str());
            lua_pushinteger(_L, static_cast<lua_Integer>(v));
            lua_rawset(_L, -3);
          } else if constexpr (std::is_same_v<T, int64_t> ||
                               std::is_same_v<T, uint64_t>) {
            lua_pushstring(_L, name.c_str());
            lua_pushinteger(_L, static_cast<lua_Integer>(v));
            lua_rawset(_L, -3);
          } else if constexpr (std::is_same_v<T, double>) {
            lua_pushstring(_L, name.c_str());
            lua_pushnumber(_L, v);
            lua_rawset(_L, -3);
          } else if constexpr (std::is_same_v<T, std::string>) {
            lua_pushstring(_L, name.c_str());
            lua_pushstring(_L, v.c_str());
            lua_rawset(_L, -3);
          }
          // bool and std::monostate are not pushed to Lua
        },
        val);
  }
  if (lua_pcall(_L, 1, 0, 0) != 0)
    throw msg_fmt("simu: error running function `init' {} ",
                  lua_tostring(_L, -1));
}

/**
 *  The read method called by the stream::read method.
 *
 *  @param d The event to read.
 *
 *  @return The number of events written.
 */
bool luabinding::read(std::shared_ptr<io::data>& data) {
  bool retval(false);
  _logger->trace("simu: luabinding::read call");

  // Total to acknowledge incremented
  ++_total;

  // Let's get the function to call
  lua_getglobal(_L, "read");

  if (lua_pcall(_L, 0, 1, 0) != 0)
    throw msg_fmt("simu: error running function `read' {} ",
                  lua_tostring(_L, -1));

  if (lua_istable(_L, -1))
    retval = _parse_event(data);
  else if (lua_isnil(_L, -1))
    lua_pop(_L, -1);
  else {
    throw msg_fmt(
        "simu: `read' must return a table or a nil value ({})"
        " type return)",
        lua_type(_L, -1));
  }

  return retval;
}

/**
 *  Given an event d, this method converts it to a Lua table.
 *  The result is stored on the Lua interpreter stack.
 *
 *  @param d The event to convert.
 *  @return true if an object is correctly filled, false otherwise.
 */
bool luabinding::_parse_event(std::shared_ptr<io::data>& d) {
  bool retval(true);
  d.reset();
  lua_pushnil(_L);  // push nil, so lua_next removes it from stack and puts (k,
                    // v) on stack
  std::map<std::string, misc::variant> map;
  while (lua_next(_L, -2) != 0) {  // -2, because we have table at -1
    if (lua_isstring(_L, -2)) {    // only store stuff with string keys
      char const* key(lua_tostring(_L, -2));
      if (lua_isboolean(_L, -1))
        map.insert({key, misc::variant(lua_toboolean(_L, -1))});
#if LUA53
      else if (lua_isinteger(_L, -1))
        map.insert(
            {key, misc::variant(static_cast<int64_t>(lua_tointeger(_L, -1)))});
      else if (lua_isnumber(_L, -1))
        map.insert({key, misc::variant(lua_tonumber(_L, -1))});
#else
      else if (lua_isnumber(_L, -1)) {
        const double eps = 0.0000001;
        double valued = lua_tonumber(_L, -1);
        int64_t vi = lua_tointeger(_L, -1);
        double valuei = vi;
        if (valued - valuei < eps && valuei - valued < eps)
          map.insert({key, misc::variant(vi)});
        else
          map.insert({key, misc::variant(valued)});
      }
#endif
      else if (lua_isstring(_L, -1))
        map.insert({key, misc::variant(lua_tostring(_L, -1))});
      else
        throw msg_fmt(
            "simu: item with key {}"
            " is not supported for a broker event",
            key);
    }
    lua_pop(_L, 1);  // remove value, keep key for lua_next
  }

  lua_pop(_L, 1);  // pop table
  // Extract _type as uint64_t from whatever numeric type was stored.
  auto get_as_uint64 = [](const misc::variant& v) -> uint64_t {
    return std::visit(
        [](auto&& val) -> uint64_t {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_arithmetic_v<T> &&
                        !std::is_same_v<T, std::monostate>)
            return static_cast<uint64_t>(val);
          return 0;
        },
        v);
  };
  auto get_as_string = [](const misc::variant& v) -> std::string {
    return std::visit(
        [](auto&& val) -> std::string {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, std::string>)
            return val;
          else if constexpr (std::is_arithmetic_v<T> &&
                             !std::is_same_v<T, std::monostate>)
            return fmt::format("{}", val);
          return "";
        },
        v);
  };

  io::event_info const* info(
      io::events::instance().get_event_info(get_as_uint64(map["_type"])));
  if (info) {
    // Create event
    std::unique_ptr<io::data> t(info->get_operations().constructor());
    if (t) {
      // Browse all mapping to unserialize the object.
      for (mapping::entry const* current_entry(info->get_mapping());
           !current_entry->is_null(); ++current_entry) {
        if (!current_entry->get_name_v2())
          continue;
        auto it = map.find(current_entry->get_name_v2());
        if (it != map.end()) {
          // Skip entries that should not be serialized.
          switch (current_entry->get_type()) {
            case mapping::source::BOOL:
              current_entry->set_bool(
                  *t, std::visit(
                          [](auto&& v) -> bool {
                            using T = std::decay_t<decltype(v)>;
                            if constexpr (std::is_arithmetic_v<T> &&
                                          !std::is_same_v<T, std::monostate>)
                              return static_cast<bool>(v);
                            return false;
                          },
                          it->second));
              break;
            case mapping::source::DOUBLE:
              current_entry->set_double(
                  *t, std::visit(
                          [](auto&& v) -> double {
                            using T = std::decay_t<decltype(v)>;
                            if constexpr (std::is_arithmetic_v<T> &&
                                          !std::is_same_v<T, std::monostate>)
                              return static_cast<double>(v);
                            return 0.0;
                          },
                          it->second));
              break;
            case mapping::source::INT:
              current_entry->set_int(
                  *t, static_cast<int32_t>(std::get<int64_t>(it->second)));
              break;
            case mapping::source::SHORT:
              current_entry->set_short(
                  *t,
                  static_cast<short>(std::get<int64_t>(it->second)));
              break;
            case mapping::source::STRING:
              current_entry->set_string(*t, get_as_string(it->second));
              break;
            case mapping::source::TIME:
              current_entry->set_time(*t, get_as_uint64(it->second));
              break;
            case mapping::source::UINT:
              current_entry->set_uint(
                  *t, static_cast<uint32_t>(get_as_uint64(it->second)));
              break;
            default:
              throw msg_fmt(
                  "simu: invalid mapping for "
                  "object of type '{}"
                  "': {}"
                  " is not a known type ID",
                  info->get_name(), current_entry->get_type());
          }
        }
      }
      d.reset(t.release());
    } else
      throw msg_fmt(
          "simu: cannot create object of ID {}"
          " whereas it has been registered",
          static_cast<int32_t>(get_as_uint64(map["_type"])));
  } else {
    _logger->info(
        "simu: cannot unserialize event of ID {}: event was not registered and "
        "will therefore be ignored",
        static_cast<uint32_t>(get_as_uint64(map["_type"])));
    retval = false;
  }
  return retval;
}

/**
 *  Load the Lua interpreter with classical and custom libraries.
 *
 * @return The Lua interpreter
 */
lua_State* luabinding::_load_interpreter() {
  lua_State* L = luaL_newstate();

  // Read common lua libraries
  luaL_openlibs(L);

  // Registers the broker_log object
  lua::broker_log::broker_log_reg(L);

  // Registers the broker_utils object
  lua::broker_utils::broker_utils_reg(L);

  return L;
}
