/**
 * Copyright 2018-2024 Centreon
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

#include <spdlog/fmt/ostr.h>

#include <cassert>

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/lua/broker_cache.hh"
#include "com/centreon/broker/lua/broker_event.hh"
#include "com/centreon/broker/lua/broker_log.hh"
#include "com/centreon/broker/lua/broker_socket.hh"
#include "com/centreon/broker/lua/broker_utils.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::lua;
using namespace com::centreon::exceptions;

#ifdef LUA51
static int l_pairs(lua_State* L) {
  if (!luaL_getmetafield(L, 1, "__next"))
    lua_getglobal(L, "next");
  lua_pushvalue(L, 1);
  lua_pushnil(L);
  return 3;
}
#endif

#define RETURN_AND_POP(val)    \
  lua_pop(_L, lua_gettop(_L)); \
  return val

/**
 *  Constructor.
 *
 *  @param[in] lua_script the json parameters file
 *  @param[in] conf_params A hash table with user parameters
 *  @param[in] cache the persistent cache.
 */
luabinding::luabinding(std::string const& lua_script,
                       std::map<std::string, misc::variant> const& conf_params,
                       macro_cache& cache)
    : _L{nullptr},
      _filter{false},
      _flush{false},
      _cache(cache),
      _total{0},
      _broker_api_version{1} {
  size_t pos(lua_script.find_last_of('/'));
  std::string path(lua_script.substr(0, pos));
  _L = _load_interpreter();
  _update_lua_path(path);

  SPDLOG_LOGGER_DEBUG(log_v2::lua(),
                      "lua: initializing the Lua virtual machine");

  try {
    _load_script(lua_script);
    _init_script(conf_params);
  } catch (std::exception const& e) {
    lua_close(_L);
    throw;
  }
}

/**
 * @brief Destructor of luabinding.
 */
luabinding::~luabinding() noexcept {
  stop();
}

int32_t luabinding::stop() {
  int32_t retval = 0;
  if (_L) {
    retval = flush();
    lua_close(_L);
    broker_event::lua_close(_L);
    _L = nullptr;
  }
  return retval;
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
  lua_getfield(_L, 1, "path");
  std::string current_path(
      fmt::format("{};{}/?.lua", lua_tostring(_L, 2), path));
  lua_pop(_L, 1);
  lua_pushlstring(_L, current_path.c_str(), current_path.size());
  lua_setfield(_L, 1, "path");

  /* Working on cpath: so libraries */
  lua_getfield(_L, 1, "cpath");
  current_path = fmt::format("{};{}/lib/?.so", lua_tostring(_L, 2), path);
  lua_pop(_L, 1);
  lua_pushlstring(_L, current_path.c_str(), current_path.size());
  lua_setfield(_L, 1, "cpath");
  lua_pop(_L, 1);
}

/**
 *  Returns true if a filter was configured in the Lua script.
 */
bool luabinding::has_filter() const noexcept {
  return _filter;
}

/**
 *  Returns true if a flush was configured in the Lua script.
 */
bool luabinding::has_flush() const noexcept {
  return _flush;
}

/**
 *  Reads the Lua script, checks its syntax and checks if
 *   - init()
 *   - write()
 *   - filter()
 *   - flush()
 *  functions exist in the Lua script. The two first ones are
 *  mandatory whereas the third one is optional.
 *
 *  It is also here that the broker_api_version variable is checked.
 *
 *  @param lua_script the file name of the lua script.
 */
void luabinding::_load_script(const std::string& lua_script) {
  // script loading
  if (luaL_loadfile(_L, lua_script.c_str()) != 0) {
    char const* error_msg(lua_tostring(_L, -1));
    throw msg_fmt("lua: '{}' could not be loaded: {}", lua_script, error_msg);
  }

  // Script compilation
  if (lua_pcall(_L, 0, 0, 0) != 0) {
    throw msg_fmt("lua: '{}' could not be compiled", lua_script);
  }

  // Checking for init() availability: this function is mandatory
  lua_getglobal(_L, "init");
  if (!lua_isfunction(_L, lua_gettop(_L)))
    throw msg_fmt("lua: '{}' init() global function is missing", lua_script);
  lua_pop(_L, 1);

  // Checking for write() availability: this function is mandatory
  lua_getglobal(_L, "write");
  if (!lua_isfunction(_L, lua_gettop(_L)))
    throw msg_fmt("lua: '{}' write() global function is missing", lua_script);
  lua_pop(_L, 1);

  // Checking for filter() availability: this function is optional
  lua_getglobal(_L, "filter");
  if (!lua_isfunction(_L, lua_gettop(_L))) {
    SPDLOG_LOGGER_DEBUG(
        log_v2::lua(),
        "lua: filter() global function is missing, the write() function will "
        "be called for each event");
    _filter = false;
  } else {
    _filter = true;
    /* Just a call with cat = 1 and elem = 2 of filter to check its return.
     * It is not sufficent but checks almost all cases. */
    lua_pushinteger(_L, 1);
    lua_pushinteger(_L, 2);
    if (lua_pcall(_L, 2, 1, 0) != 0) {
      const char* ret = lua_tostring(_L, -1);
      if (ret)
        log_v2::lua()->error(
            "lua: The filter() function doesn't work correctly: {}", ret);
      else
        log_v2::lua()->error(
            "lua: The filter() function doesn't work correctly");
      _filter = false;
    } else {
      if (!lua_isboolean(_L, -1)) {
        log_v2::lua()->error(
            "lua: The filter() function should return a boolean.");
        _filter = false;
      }
    }
  }
  lua_pop(_L, lua_gettop(_L));

  // Checking for flush() availability: this function is optional
  lua_getglobal(_L, "flush");
  if (!lua_isfunction(_L, lua_gettop(_L))) {
    SPDLOG_LOGGER_DEBUG(log_v2::lua(),
                        "lua: flush() global function is not defined");
    _flush = false;
  } else
    _flush = true;
  lua_pop(_L, 1);

  /* Checking the version api */
  lua_getglobal(_L, "broker_api_version");
#if LUA53
  if (lua_isinteger(_L, 1))
#else
  if (lua_isnumber(_L, 1))
#endif
    _broker_api_version = lua_tointeger(_L, 1);
  else if (lua_isnumber(_L, 1))
    _broker_api_version = static_cast<uint32_t>(lua_tonumber(_L, 1));
  else if (lua_isstring(_L, 1)) {
    char* end;
    _broker_api_version = strtol(lua_tostring(_L, 1), &end, 10);
  }

  if (_broker_api_version != 1 && _broker_api_version != 2) {
    SPDLOG_LOGGER_ERROR(
        log_v2::lua(),
        "broker_api_version represents the Lua broker api to use, it must be "
        "one of (1, 2) and not '{}'. Setting it to 1",
        _broker_api_version);
    _broker_api_version = 1;
  }
  lua_pop(_L, 1);

#ifdef LUA51
  if (_broker_api_version == 2) {
    lua_getglobal(_L, "pairs");
    lua_setglobal(_L, "__pairs");
    lua_pushcfunction(_L, l_pairs);
    lua_setglobal(_L, "pairs");
  }
#endif

  SPDLOG_LOGGER_INFO(log_v2::lua(), "Lua broker_api_version set to {}",
                     _broker_api_version);

  // Registers the broker_log object
  broker_log::broker_log_reg(_L);

  // Registers the broker event object
  broker_event::broker_event_reg(_L);

  // Registers the broker socket object
  broker_socket::broker_socket_reg(_L);

  // Registers the broker utils
  broker_utils::broker_utils_reg(_L);

  // Registers the broker cache
  broker_cache::broker_cache_reg(_L, _cache, _broker_api_version);
  assert(lua_gettop(_L) == 0);
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
  for (std::map<std::string, misc::variant>::const_iterator
           it(conf_params.begin()),
       end(conf_params.end());
       it != end; ++it) {
    switch (it->second.user_type()) {
      case misc::variant::type_int:
      case misc::variant::type_uint:
        lua_pushstring(_L, it->first.c_str());
        lua_pushinteger(_L, it->second.as_int());
        lua_rawset(_L, -3);
        break;
      case misc::variant::type_long:
      case misc::variant::type_ulong:
        lua_pushstring(_L, it->first.c_str());
        lua_pushinteger(_L, it->second.as_long());
        lua_rawset(_L, -3);
        break;
      case misc::variant::type_double:
        lua_pushstring(_L, it->first.c_str());
        lua_pushnumber(_L, it->second.as_double());
        lua_rawset(_L, -3);
        break;
      case misc::variant::type_string:
        lua_pushstring(_L, it->first.c_str());
        lua_pushstring(_L, it->second.as_string().c_str());
        lua_rawset(_L, -3);
        break;
      default:
        /* Should not arrive */
        assert(1 == 0);
    }
  }
  if (lua_pcall(_L, 1, 0, 0) != 0) {
    const char* ret = lua_tostring(_L, -1);
    if (ret)
      throw msg_fmt("lua: error running function `init' {}", ret);
    else
      throw msg_fmt("lua: unknown error running function `init'");
  }
}

/**
 *  The write method called by the stream::write method.
 *
 *  @param data The event to write.
 *
 *  @return The number of events written.
 */
int luabinding::write(std::shared_ptr<io::data> const& data) noexcept {
  int retval = 0;
  if (log_v2::lua()->level() == spdlog::level::trace) {
    SPDLOG_LOGGER_TRACE(log_v2::lua(), "lua: luabinding::write call {}", *data);
  } else {
    SPDLOG_LOGGER_DEBUG(log_v2::lua(), "lua: luabinding::write call");
  }

  // Give data to cache.
  _cache.write(data);

  // Process event.
  uint32_t mess_type(data->type());
  uint16_t cat(category_of_type(mess_type));
  uint16_t elem(element_of_type(mess_type));

  bool execute_write = true;

  // Total to acknowledge incremented
  ++_total;

  if (has_filter()) {
    // Let's get the function to call
    lua_getglobal(_L, "filter");
    lua_pushinteger(_L, cat);
    lua_pushinteger(_L, elem);

    if (lua_pcall(_L, 2, 1, 0) != 0) {
      const char* ret = lua_tostring(_L, -1);
      if (ret)
        SPDLOG_LOGGER_ERROR(log_v2::lua(),
                            "lua: error while running function `filter()': {}",
                            ret);
      else
        SPDLOG_LOGGER_ERROR(
            log_v2::lua(),
            "lua: unknown error while running function `filter()'");
      RETURN_AND_POP(0);
    }

    if (!lua_isboolean(_L, -1)) {
      SPDLOG_LOGGER_ERROR(log_v2::lua(), "lua: `filter' must return a boolean");
      RETURN_AND_POP(0);
    }

    execute_write = lua_toboolean(_L, -1);
    SPDLOG_LOGGER_DEBUG(log_v2::lua(), "lua: `filter' returned {}",
                        (execute_write ? "true" : "false"));
    lua_pop(_L, lua_gettop(_L));
  }

  if (!execute_write)
    return 0;

  // Let's get the function to call
  lua_getglobal(_L, "write");

  // We add data as argument
  switch (_broker_api_version) {
    case 1: {
      // Let's build the table from the event as argument to write()

      io::data const& d(*data);
      broker_event::create_as_table(_L, d);
    } break;
    case 2:
      broker_event::create(_L, data);
      break;
  }

  if (lua_pcall(_L, 1, 1, 0) != 0) {
    const char* ret = lua_tostring(_L, -1);
    if (ret)
      SPDLOG_LOGGER_ERROR(log_v2::lua(),
                          "lua: error running function `write' {}", ret);
    else
      SPDLOG_LOGGER_ERROR(log_v2::lua(),
                          "lua: unknown error running function `write'");
    RETURN_AND_POP(0);
  }

  if (!lua_isboolean(_L, -1)) {
    SPDLOG_LOGGER_ERROR(log_v2::lua(), "lua: `write' must return a boolean");
    RETURN_AND_POP(0);
  }
  int acknowledge = lua_toboolean(_L, -1);

  // We have to acknowledge rejected events by the filter. It is only possible
  // when an acknowledgement is sent by the write function.
  if (acknowledge) {
    retval = _total;
    _total = 0;
  }
  RETURN_AND_POP(retval);
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

  return L;
}

int32_t luabinding::flush() noexcept {
  if (!_flush)
    return 0;
  // Let's get the function to call
  lua_getglobal(_L, "flush");
  if (lua_pcall(_L, 0, 1, 0) != 0) {
    const char* ret = lua_tostring(_L, -1);
    if (ret)
      SPDLOG_LOGGER_ERROR(log_v2::lua(),
                          "lua: error running function `flush' {}", ret);
    else
      SPDLOG_LOGGER_ERROR(log_v2::lua(),
                          "lua: unknown error running function `flush'");
    RETURN_AND_POP(0);
  }
  if (!lua_isboolean(_L, -1)) {
    SPDLOG_LOGGER_ERROR(log_v2::lua(), "lua: `flush' must return a boolean");
    RETURN_AND_POP(0);
  }
  bool acknowledge = lua_toboolean(_L, -1);

  int32_t retval = 0;
  if (acknowledge) {
    retval = _total;
    _total = 0;
  }
  RETURN_AND_POP(retval);
}
