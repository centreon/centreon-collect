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

#include "com/centreon/broker/lua/broker_cache.hh"

#include "com/centreon/broker/lua/broker_event.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::lua;

/**
 *  broker_cache destructor
 *
 *  @param L The Lua interpreter
 *
 *  @return an integer, here always 0.
 */
static int l_broker_cache_destructor(lua_State* L) {
  (void)L;
  return 0;
}

/**
 *  The get_ba() method available in the Lua interpreter.
 *  It returns a table containing the ba data.
 *
 * @param L The Lua interpreter
 *
 * @return 1
 */
static int l_broker_cache_get_ba_v1(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int ba_id(luaL_checkinteger(L, 2));

  try {
    const DimensionBaEvent& ba(cache->get_dimension_ba_event(ba_id)->obj());
    lua_createtable(L, 0, 7);
    lua_pushinteger(L, ba.ba_id());
    lua_setfield(L, -2, "ba_id");

    lua_pushstring(L, ba.ba_name().c_str());
    lua_setfield(L, -2, "ba_name");

    lua_pushstring(L, ba.ba_description().c_str());
    lua_setfield(L, -2, "ba_description");
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

static int l_broker_cache_get_ba_v2(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int ba_id(luaL_checkinteger(L, 2));

  try {
    broker_event::create(L, cache->get_dimension_ba_event(ba_id));
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

static inline void lua_pushstring(lua_State* state, const std::string& str) {
  lua_pushlstring(state, str.c_str(), str.length());
}

/**
 *  The get_bv() method available in the Lua interpreter.
 *  It returns a table containing the bv data.
 *
 * @param L The Lua interpreter
 *
 * @return 1
 */
static int l_broker_cache_get_bv_v1(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int bv_id(luaL_checkinteger(L, 2));

  try {
    const bam::pb_dimension_bv_event& bv(*cache->get_dimension_bv_event(bv_id));
    lua_createtable(L, 0, 3);
    lua_pushinteger(L, bv.obj().bv_id());
    lua_setfield(L, -2, "bv_id");

    lua_pushstring(L, bv.obj().bv_name());
    lua_setfield(L, -2, "bv_name");

    lua_pushstring(L, bv.obj().bv_description());
    lua_setfield(L, -2, "bv_description");
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

static int l_broker_cache_get_bv_v2(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int bv_id(luaL_checkinteger(L, 2));

  try {
    broker_event::create(L, cache->get_dimension_bv_event(bv_id));
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_bvs() method available in the Lua interpreter
 *  It returns an array of bv ids.
 *
 * @param L The Lua interpreter
 *
 * @return 1
 */
static int l_broker_cache_get_bvs(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  uint32_t ba_id(luaL_checkinteger(L, 2));

  auto const& relations(cache->get_dimension_ba_bv_relation_events());
  auto it = relations.find(ba_id);

  lua_newtable(L);

  int i = 1;
  while (it != relations.end() && it->first == ba_id) {
    lua_pushinteger(L, it->second->obj().bv_id());
    lua_rawseti(L, -2, i);
    ++i;
    ++it;
  }
  return 1;
}

/**
 *  The get_hostgroup_name() method available in the Lua interpreter
 *  It returns a string.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_hostgroup_name(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int id(luaL_checkinteger(L, 2));

  try {
    std::string const& hg{cache->get_host_group_name(id)};
    lua_pushstring(L, hg.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_hostgroup_alias() method available in the Lua interpreter
 *  It returns a string.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_hostgroup_alias(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int id(luaL_checkinteger(L, 2));

  try {
    std::string const& hg{cache->get_host_group_alias(id)};
    lua_pushstring(L, hg.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_hostname() method available in the Lua interpreter
 *  It returns a string.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_hostname(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int id(luaL_checkinteger(L, 2));

  try {
    std::string const& hst{cache->get_host_name(id)};
    lua_pushstring(L, hst.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_service() method available in the Lua interpreter
 *  It returns a broker_event of type 'service'.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_service_v1(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  uint32_t host_id(luaL_checkinteger(L, 2));
  uint32_t svc_id(luaL_checkinteger(L, 3));

  try {
    broker_event::create_as_table(L, *cache->get_service(host_id, svc_id));
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

static int l_broker_cache_get_service_v2(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  uint32_t host_id(luaL_checkinteger(L, 2));
  uint32_t svc_id(luaL_checkinteger(L, 3));

  try {
    broker_event::create(L, cache->get_service(host_id, svc_id));
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_host() method available in the Lua interpreter
 *  It returns a table containing various attributes of the host.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_host_v1(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int id(luaL_checkinteger(L, 2));

  try {
    broker_event::create_as_table(L, *cache->get_host(id));
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

static int l_broker_cache_get_host_v2(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int id(luaL_checkinteger(L, 2));

  try {
    broker_event::create(L, cache->get_host(id));
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_index_mapping() method available in the Lua interpreter.
 *  It returns a table with three keys: index_id, host_id and service_id.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_index_mapping(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int index_id(luaL_checkinteger(L, 2));

  try {
    const storage::pb_index_mapping& mapping{
        cache->get_index_mapping(index_id)};
    const auto& m_obj = mapping.obj();
    lua_createtable(L, 0, 3);

    lua_pushinteger(L, m_obj.index_id());
    lua_setfield(L, -2, "index_id");

    lua_pushinteger(L, m_obj.host_id());
    lua_setfield(L, -2, "host_id");

    lua_pushinteger(L, m_obj.service_id());
    lua_setfield(L, -2, "service_id");
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_instance_name() method available in the Lua interpreter.
 *  It returns a string.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_instance_name(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int instance_id(luaL_checkinteger(L, 2));

  try {
    std::string const& instance{cache->get_instance(instance_id)};
    lua_pushstring(L, instance.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_metric_mapping() method available in the Lua interpreter.
 *  It returns a table with two keys: metric_id, index_id.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_metric_mapping_v1(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int metric_id(luaL_checkinteger(L, 2));

  try {
    const storage::pb_metric_mapping& mapping(
        *cache->get_metric_mapping(metric_id));
    const auto& m_obj = mapping.obj();
    lua_createtable(L, 0, 2);

    lua_pushinteger(L, m_obj.metric_id());
    lua_setfield(L, -2, "metric_id");

    lua_pushinteger(L, m_obj.index_id());
    lua_setfield(L, -2, "index_id");
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

static int l_broker_cache_get_metric_mapping_v2(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int metric_id(luaL_checkinteger(L, 2));

  try {
    const std::shared_ptr<storage::pb_metric_mapping>& mm{
        cache->get_metric_mapping(metric_id)};
    broker_event::create(L, mm);
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_service_description() method available in the Lua interpreter.
 *  It returns a string.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_service_description(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int host_id(luaL_checkinteger(L, 2));
  int service_id(luaL_checkinteger(L, 3));

  try {
    std::string const& svc{cache->get_service_description(host_id, service_id)};
    lua_pushstring(L, svc.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_servicegroup_name() method available in the Lua interpreter
 *  It returns a string.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_servicegroup_name(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int id(luaL_checkinteger(L, 2));

  try {
    std::string const& sg{cache->get_service_group_name(id)};
    lua_pushstring(L, sg.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_servicegroups() method available in the Lua interpreter
 *  It returns an array of objects, each one containing group_id and group_name.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_servicegroups(lua_State* L) {
  macro_cache const* cache{
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache"))};
  uint64_t host_id(luaL_checkinteger(L, 2));
  uint64_t service_id(luaL_checkinteger(L, 3));

  auto const& members = cache->get_service_group_members();

  auto first(members.lower_bound(std::make_tuple(host_id, service_id, 0)));
  auto second(members.upper_bound(std::make_tuple(host_id, service_id + 1, 0)));

  lua_newtable(L);

  if (first != members.end()) {
    int i{1};
    for (auto it(first), end(second); it != end; ++it) {
      lua_createtable(L, 0, 2);
      const ServiceGroupMember& sgm =
          std::static_pointer_cast<neb::pb_service_group_member>(it->second)
              ->obj();

      lua_pushinteger(L, sgm.servicegroup_id());
      lua_setfield(L, -2, "group_id");

      lua_pushstring(L, sgm.name().c_str());
      lua_setfield(L, -2, "group_name");
      lua_rawseti(L, -2, i);
      ++i;
    }
  }
  return 1;
}

/**
 *  The get_hostgroups() method available in the Lua interpreter
 *  It returns an array of host groups from a host id.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_hostgroups(lua_State* L) {
  macro_cache const* cache{
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache"))};
  uint64_t id{static_cast<uint64_t>(luaL_checkinteger(L, 2))};

  auto const& members = cache->get_host_group_members();

  auto const first = members.lower_bound({id, 0});
  auto const second = members.upper_bound({id + 1, 0});

  lua_newtable(L);
  if (first != members.end()) {
    int i = 1;
    for (auto it(first); it != second; ++it) {
      lua_createtable(L, 0, 2);
      std::shared_ptr<io::data> evt = it->second;
      const HostGroupMember& hgm =
          std::static_pointer_cast<neb::pb_host_group_member>(it->second)
              ->obj();
      lua_pushinteger(L, hgm.hostgroup_id());
      lua_setfield(L, -2, "group_id");

      lua_pushstring(L, hgm.name().c_str());
      lua_setfield(L, -2, "group_name");
      lua_rawseti(L, -2, i);
      ++i;
    }
  }
  return 1;
}

/**
 *  The get_action_url() method available in the Lua interpreter
 *  This function works on hosts or services.
 *  For a host, it needs a host_id as parameter and returns a string with the
 *  action url.
 *  For a service, it needs a host_id and a service_id as parameter and
 *  returns a string with the action url.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_action_url(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  try {
    std::string const& action_url(cache->get_action_url(host_id, service_id));
    lua_pushstring(L, action_url.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_notes() method available in the Lua interpreter
 *  This function works on hosts or services.
 *  It needs a host_id as parameter for a host and an additional service_id for
 *  a service. It returns a string with the notes.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_notes(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  try {
    std::string const& notes(cache->get_notes(host_id, service_id));
    lua_pushstring(L, notes.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_notes_url() method available in the Lua interpreter
 *  This function works on hosts or services.
 *  It needs a host_id as parameter for a host and an additional service_id for
 *  a service. It returns a string with the notes url.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_notes_url(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  try {
    std::string const& notes_url(cache->get_notes_url(host_id, service_id));
    lua_pushstring(L, notes_url.c_str());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  The get_severity() method available in the Lua interpreter
 *  This function works on hosts or services.
 *  It needs a host_id as parameter for a host and an additional service_id for
 *  a service. It returns a string with the severity value or nil if not found.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int32_t l_broker_cache_get_severity(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  try {
    int32_t severity = cache->get_severity(host_id, service_id);
    lua_pushinteger(L, severity);
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

static int32_t l_broker_cache_get_check_command(lua_State* L) {
  macro_cache const* cache(
      *static_cast<macro_cache**>(luaL_checkudata(L, 1, "lua_broker_cache")));
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  try {
    std::string_view check_command =
        cache->get_check_command(host_id, service_id);
    lua_pushlstring(L, check_command.data(), check_command.size());
  } catch (std::exception const& e) {
    (void)e;
    lua_pushnil(L);
  }
  return 1;
}

/**
 *  Load the Lua interpreter with the standard libraries
 *  and the broker lua sdk.
 *
 *  @param L The Lua interpreter
 *  @param cache The cache to share with the Lua interpreter
 *
 *  @return The Lua interpreter as a lua_State*
 */
void broker_cache::broker_cache_reg(lua_State* L,
                                    macro_cache const& cache,
                                    uint32_t api_version) {
  macro_cache const** udata(static_cast<macro_cache const**>(
      lua_newuserdata(L, sizeof(macro_cache*))));
  *udata = &cache;

  luaL_Reg s_broker_cache_regs[] = {
      {"__gc", l_broker_cache_destructor},
      {"get_ba", l_broker_cache_get_ba_v1},
      {"get_bv", l_broker_cache_get_bv_v1},
      {"get_bvs", l_broker_cache_get_bvs},
      {"get_hostgroup_name", l_broker_cache_get_hostgroup_name},
      {"get_hostgroups", l_broker_cache_get_hostgroups},
      {"get_hostname", l_broker_cache_get_hostname},
      {"get_host", l_broker_cache_get_host_v1},
      {"get_service", l_broker_cache_get_service_v1},
      {"get_index_mapping", l_broker_cache_get_index_mapping},
      {"get_instance_name", l_broker_cache_get_instance_name},
      {"get_metric_mapping", l_broker_cache_get_metric_mapping_v1},
      {"get_service_description", l_broker_cache_get_service_description},
      {"get_servicegroup_name", l_broker_cache_get_servicegroup_name},
      {"get_servicegroups", l_broker_cache_get_servicegroups},
      {"get_notes_url", l_broker_cache_get_notes_url},
      {"get_notes", l_broker_cache_get_notes},
      {"get_action_url", l_broker_cache_get_action_url},
      {"get_severity", l_broker_cache_get_severity},
      {"get_check_command", l_broker_cache_get_check_command},
      {"get_hostgroup_alias", l_broker_cache_get_hostgroup_alias},
      {nullptr, nullptr}};

  if (api_version == 2) {
    s_broker_cache_regs[1].func = l_broker_cache_get_ba_v2;
    s_broker_cache_regs[2].func = l_broker_cache_get_bv_v2;
    s_broker_cache_regs[7].func = l_broker_cache_get_host_v2;
    s_broker_cache_regs[8].func = l_broker_cache_get_service_v2;
    s_broker_cache_regs[11].func = l_broker_cache_get_metric_mapping_v2;
  }

  // Create a metatable. It is not exposed to Lua.
  // The "lua_broker" label is used by Lua internally to identify things.
  luaL_newmetatable(L, "lua_broker_cache");

  // Register the C functions into the metatable we just created.
#ifdef LUA51
  luaL_register(L, NULL, s_broker_cache_regs);
#else
  luaL_setfuncs(L, s_broker_cache_regs, 0);
#endif

  // The Lua stack at this point looks like:
  // 1  =>  userdata                  => -2
  // 2  =>  metatable "lua_broker"    => -1
  lua_pushvalue(L, -1);

  // The Lua stack at this point looks like:
  // 1  =>  userdata                  => -3
  // 2  =>  metatable "lua_broker"    => -2
  // 3  =>  metatable "lua_broker"    => -1

  // Set the __index field of the metatable to point to itself
  lua_setfield(L, -1, "__index");

  // The Lua stack at this point looks like:
  // 1  =>  userdata                  => -2
  // 2  =>  metatable "lua_broker"    => -1

  // Now, we use setmetatable to set it to our userdata
  lua_setmetatable(L, -2);

  // And now, we use setglobal to store userdata as the variable "broker".
  lua_setglobal(L, "broker_cache");
}
