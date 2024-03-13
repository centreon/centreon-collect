/*
** Copyright 2020 Centreon
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

#ifndef CCB_LUA_BROKER_EVENT_HH
#define CCB_LUA_BROKER_EVENT_HH

#include "com/centreon/broker/io/events.hh"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace com::centreon::broker {

namespace lua {
/**
 *  @class broker_event broker_event.hh
 * "com/centreon/broker/lua/broker_event.hh"
 *  @brief Class providing BBDO event to the lua interpreter
 *
 *  This class provides a binding to Lua to access BBDO events.
 *
 *  In lua API v1, each broker event was converted into Lua tables. This
 *  procedure worked well but was expensive for cbd, because each call to
 *  write needed a conversion to table of its parameter.
 *
 *  Now, when the broker_api_version global variable is set to 2, events are
 *  just encapsulated into Lua userdata. We tried to paste the table behavior
 *  to this object. So almost every script working on version 1 should also
 *  work with version 2.
 *
 */
class broker_event {
  struct gc_info {
    gc_info() : _broker_event_cpt(0), _last_full_gc(time(nullptr)) {}

    unsigned _broker_event_cpt;
    time_t _last_full_gc;
  };

  static std::map<const lua_State*, gc_info> _gc_info;
  static std::mutex _gc_info_m;

  static int l_broker_event_destructor(lua_State* L);

 public:
  static void broker_event_reg(lua_State* L);
  static void create(lua_State* L, std::shared_ptr<io::data> e);
  static void create_as_table(lua_State* L, const io::data& e);
  static void lua_close(const lua_State* L);
};
}  // namespace lua

}  // namespace com::centreon::broker

#endif  // !CCB_LUA_BROKER_EVENT_HH
