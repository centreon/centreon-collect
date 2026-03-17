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
#include "bbdo/neb.pb.h"
#include "com/centreon/broker/bam/internal.hh"

#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/neb/internal.hh"

#include "com/centreon/broker/cache/protobuf.hh"
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
  int ba_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    auto dim_ba_event = cache_instance->get_dimension_ba_event(ba_id);
    if (!dim_ba_event) {
      lua_pushnil(L);
    } else {
      lua_createtable(L, 0, 7);
      lua_pushinteger(L, ba_id);
      lua_setfield(L, -2, "ba_id");

      lua_pushstring(L, dim_ba_event->ba_name().c_str());
      lua_setfield(L, -2, "ba_name");

      lua_pushstring(L, dim_ba_event->ba_description().c_str());
      lua_setfield(L, -2, "ba_description");
    }
  }
  return 1;
}

static int l_broker_cache_get_ba_v2(lua_State* L) {
  int ba_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    auto dim_ba_event = cache_instance->get_dimension_ba_event(ba_id);
    if (!dim_ba_event) {
      lua_pushnil(L);
    } else {
      std::shared_ptr<bam::pb_dimension_ba_event> pb_dim_ba_event =
          std::make_shared<bam::pb_dimension_ba_event>(
              dim_ba_event->to_protobuf());
      broker_event::create(L, pb_dim_ba_event);
    }
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
  int bv_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    auto dim_bv_event = cache_instance->get_dimension_bv_event(bv_id);
    if (!dim_bv_event) {
      lua_pushnil(L);
    } else {
      lua_createtable(L, 0, 3);
      lua_pushinteger(L, dim_bv_event->bv_id());
      lua_setfield(L, -2, "bv_id");

      lua_pushstring(L, dim_bv_event->bv_name().c_str());
      lua_setfield(L, -2, "bv_name");

      lua_pushstring(L, dim_bv_event->bv_description().c_str());
      lua_setfield(L, -2, "bv_description");
    }
  }
  return 1;
}

static int l_broker_cache_get_bv_v2(lua_State* L) {
  int bv_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    auto dim_bv_event = cache_instance->get_dimension_bv_event(bv_id);
    if (!dim_bv_event) {
      lua_pushnil(L);
    } else {
      std::shared_ptr<bam::pb_dimension_bv_event> pb_dim_bv_event =
          std::make_shared<bam::pb_dimension_bv_event>(
              dim_bv_event->to_protobuf());
      broker_event::create(L, pb_dim_bv_event);
    }
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
  uint32_t ba_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    int i = 1;
    lua_newtable(L);
    cache::global_cache::lock l(cache_instance);
    cache_instance->enumerate_bvs(ba_id, [&](uint64_t bv_id) {
      lua_pushinteger(L, bv_id);
      lua_rawseti(L, -2, i);
      ++i;
    });
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
  int id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    const cache::host_group* hg = cache_instance->get_host_group(id);
    if (hg) {
      lua_pushstring(L, hg->name().c_str());
    } else {
      lua_pushnil(L);
    }
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
  int id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    const cache::host_group* hg = cache_instance->get_host_group(id);
    if (hg) {
      lua_pushstring(L, hg->alias().c_str());
    } else {
      lua_pushnil(L);
    }
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
  int id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    const cache::host* hst = cache_instance->get_host(id, l);
    if (hst) {
      lua_pushstring(L, hst->name().c_str());
    } else {
      lua_pushnil(L);
    }
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
  uint32_t host_id(luaL_checkinteger(L, 2));
  uint32_t svc_id(luaL_checkinteger(L, 3));
  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    const cache::service* serv =
        cache_instance->get_service(host_id, svc_id, l);
    if (serv) {
      broker_event::create_as_table(
          L, neb::pb_service(std::move(serv->to_protobuf())));
    } else {
      lua_pushnil(L);
    }
  }
  return 1;
}

static int l_broker_cache_get_service_v2(lua_State* L) {
  uint32_t host_id(luaL_checkinteger(L, 2));
  uint32_t svc_id(luaL_checkinteger(L, 3));
  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    const cache::service* serv =
        cache_instance->get_service(host_id, svc_id, l);
    if (serv) {
      std::shared_ptr<neb::pb_service> pb_serv =
          std::make_shared<neb::pb_service>(std::move(serv->to_protobuf()));
      broker_event::create(L, pb_serv);
    } else {
      lua_pushnil(L);
    }
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
  uint32_t host_id(luaL_checkinteger(L, 2));
  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    const cache::host* hst = cache_instance->get_host(host_id, l);
    if (hst) {
      neb::pb_host pb_hst(std::move(hst->to_protobuf()));
      broker_event::create_as_table(
          L, neb::pb_host(std::move(hst->to_protobuf())));
    } else {
      lua_pushnil(L);
    }
  }
  return 1;
}

static int l_broker_cache_get_host_v2(lua_State* L) {
  uint32_t host_id(luaL_checkinteger(L, 2));
  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    const cache::host* hst = cache_instance->get_host(host_id, l);
    if (hst) {
      std::shared_ptr<neb::pb_host> pb_hst =
          std::make_shared<neb::pb_host>(std::move(hst->to_protobuf()));
      broker_event::create(L, pb_hst);
    } else {
      lua_pushnil(L);
    }
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
  int index_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    const cache::host_serv_pair* hst_serv =
        cache_instance->get_host_serv_id(index_id);
    if (hst_serv) {
      lua_createtable(L, 0, 3);

      lua_pushinteger(L, index_id);
      lua_setfield(L, -2, "index_id");

      lua_pushinteger(L, hst_serv->first);
      lua_setfield(L, -2, "host_id");

      lua_pushinteger(L, hst_serv->second);
      lua_setfield(L, -2, "service_id");
    } else {
      lua_pushnil(L);
    }
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
  int instance_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    const cache::instance* inst = cache_instance->get_instance(instance_id);
    if (inst) {
      lua_pushstring(L, inst->name().c_str());
    } else {
      lua_pushnil(L);
    }
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
  int metric_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    uint64_t index_id = cache_instance->get_index_id_from_metric_id(metric_id);
    if (index_id > 0) {
      lua_createtable(L, 0, 2);

      lua_pushinteger(L, metric_id);
      lua_setfield(L, -2, "metric_id");

      lua_pushinteger(L, index_id);
      lua_setfield(L, -2, "index_id");
    } else {
      lua_pushnil(L);
    }
  }
  return 1;
}

static int l_broker_cache_get_metric_mapping_v2(lua_State* L) {
  int metric_id(luaL_checkinteger(L, 2));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    uint64_t index_id = cache_instance->get_index_id_from_metric_id(metric_id);
    if (index_id > 0) {
      std::shared_ptr<storage::pb_metric_mapping> to_push =
          std::make_shared<storage::pb_metric_mapping>();
      to_push->mut_obj().set_metric_id(metric_id);
      to_push->mut_obj().set_index_id(index_id);
      broker_event::create(L, to_push);
    } else {
      lua_pushnil(L);
    }
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
  int host_id(luaL_checkinteger(L, 2));
  int service_id(luaL_checkinteger(L, 3));
  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    const cache::service* srv =
        cache_instance->get_service(host_id, service_id, l);
    if (srv) {
      lua_pushstring(L, srv->description().c_str());
    } else {
      lua_pushnil(L);
    }
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
  int id(luaL_checkinteger(L, 2));
  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    const cache::service_group* sg = cache_instance->get_service_group(id);
    if (sg) {
      lua_pushstring(L, sg->name().c_str());
    } else {
      lua_pushnil(L);
    }
  }
  return 1;
}

/**
 *  The get_servicegroups() method available in the Lua interpreter
 *  It returns an array of objects, each one containing group_id and
 * group_name.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_servicegroups(lua_State* L) {
  uint64_t host_id(luaL_checkinteger(L, 2));
  uint64_t service_id(luaL_checkinteger(L, 3));

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    lua_newtable(L);
    int i = 1;
    cache::global_cache::lock l(cache_instance);
    cache_instance->enumerate_service_group(
        host_id, service_id,
        [&](uint64_t group_id, const cache::string& group_name) {
          lua_createtable(L, 0, 2);
          lua_pushinteger(L, group_id);
          lua_setfield(L, -2, "group_id");

          lua_pushstring(L, group_name.c_str());
          lua_setfield(L, -2, "group_name");
          lua_rawseti(L, -2, i);
          ++i;
        });
  }
  return 1;
}

/**
 *  The get_hostgroups() method available in the Lua interpreter
 *  It returns an array of host groups from a host id.
 *
 *  @param L The Lua interpreter (host_id)
 *
 *  @return 1
 */
static int l_broker_cache_get_hostgroups(lua_State* L) {
  uint64_t host_id{static_cast<uint64_t>(luaL_checkinteger(L, 2))};

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    lua_newtable(L);
    int i = 1;
    cache::global_cache::lock l(cache_instance);
    cache_instance->enumerate_host_group(
        host_id, [&](uint64_t group_id, const cache::string& group_name) {
          lua_createtable(L, 0, 2);
          lua_pushinteger(L, group_id);
          lua_setfield(L, -2, "group_id");

          lua_pushstring(L, group_name.c_str());
          lua_setfield(L, -2, "group_name");
          lua_rawseti(L, -2, i);
          ++i;
        });
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
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    if (service_id > 0) {
      auto serv = cache_instance->get_service(host_id, service_id, l);
      if (serv) {
        lua_pushstring(L, serv->action_url().c_str());
      } else {
        lua_pushnil(L);
      }
    } else {
      auto hst = cache_instance->get_host(host_id, l);
      if (hst) {
        lua_pushstring(L, hst->action_url().c_str());
      } else {
        lua_pushnil(L);
      }
    }
  }
  return 1;
}

/**
 *  The get_notes() method available in the Lua interpreter
 *  This function works on hosts or services.
 *  It needs a host_id as parameter for a host and an additional service_id
 * for a service. It returns a string with the notes.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_notes(lua_State* L) {
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    if (service_id > 0) {
      auto serv = cache_instance->get_service(host_id, service_id, l);
      if (serv) {
        lua_pushstring(L, serv->notes().c_str());
      } else {
        lua_pushnil(L);
      }
    } else {
      auto hst = cache_instance->get_host(host_id, l);
      if (hst) {
        lua_pushstring(L, hst->notes().c_str());
      } else {
        lua_pushnil(L);
      }
    }
  }
  return 1;
}

/**
 *  The get_notes_url() method available in the Lua interpreter
 *  This function works on hosts or services.
 *  It needs a host_id as parameter for a host and an additional service_id
 * for a service. It returns a string with the notes url.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int l_broker_cache_get_notes_url(lua_State* L) {
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    if (service_id > 0) {
      auto serv = cache_instance->get_service(host_id, service_id, l);
      if (serv) {
        lua_pushstring(L, serv->notes_url().c_str());
      } else {
        lua_pushnil(L);
      }
    } else {
      auto hst = cache_instance->get_host(host_id, l);
      if (hst) {
        lua_pushstring(L, hst->notes_url().c_str());
      } else {
        lua_pushnil(L);
      }
    }
  }
  return 1;
}

/**
 *  The get_severity() method available in the Lua interpreter
 *  This function works on hosts or services.
 *  It needs a host_id as parameter for a host and an additional service_id
 * for a service. It returns a string with the severity value or nil if not
 * found.
 *
 *  @param L The Lua interpreter
 *
 *  @return 1
 */
static int32_t l_broker_cache_get_severity(lua_State* L) {
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);

  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::lock l(cache_instance);
    std::optional<int32_t> severity =
        cache_instance->get_severity(host_id, service_id);
    if (severity) {
      lua_pushinteger(L, *severity);
    } else {
      lua_pushnil(L);
    }
  }
  return 1;
}

static int32_t l_broker_cache_get_check_command(lua_State* L) {
  int host_id = luaL_checkinteger(L, 2);
  int service_id = 0;
  if (lua_gettop(L) >= 3)
    service_id = luaL_checkinteger(L, 3);
  auto cache_instance = cache::global_cache::instance_ptr();
  if (!cache_instance) {
    lua_pushnil(L);
  } else {
    cache::global_cache::upgrade_lock l(cache_instance);
    if (service_id > 0) {
      auto serv = cache_instance->get_service(host_id, service_id, l);
      if (serv) {
        lua_pushstring(L, serv->check_command().c_str());
      } else {
        lua_pushnil(L);
      }
    } else {
      auto hst = cache_instance->get_host(host_id, l);
      if (hst) {
        lua_pushstring(L, hst->check_command().c_str());
      } else {
        lua_pushnil(L);
      }
    }
  }
  return 1;
}

/**
 *  Load the Lua interpreter with the standard libraries
 *  and the broker lua sdk.
 *
 *  @param L The Lua interpreter
 *
 *  @return The Lua interpreter as a lua_State*
 */
void broker_cache::broker_cache_reg(lua_State* L, uint32_t api_version) {
  // only for backward compatibility in order to use broker_cache:get_... syntax
  struct dummy {};
  static dummy _dummy;
  dummy const** udata(
      static_cast<dummy const**>(lua_newuserdata(L, sizeof(dummy*))));
  *udata = &_dummy;

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
