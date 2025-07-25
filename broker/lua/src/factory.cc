/**
 * Copyright 2017-2024 Centreon
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

#include "com/centreon/broker/lua/factory.hh"
#include <absl/strings/match.h>
#include <nlohmann/json.hpp>
#include "com/centreon/broker/lua/connector.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/crypto/aes256.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::lua;
using namespace nlohmann;

extern std::shared_ptr<com::centreon::common::crypto::aes256>
    credentials_decrypt;

/**
 *  Find a parameter in configuration.
 *
 *  @param[in] cfg Configuration object.
 *  @param[in] key Property to get.
 *
 *  @return Property value.
 */
static std::string find_param(config::endpoint const& cfg,
                              std::string const& key) {
  auto it = cfg.params.find(key);
  if (cfg.params.end() == it)
    throw msg_fmt("lua: no '{}' defined for endpoint '{}'", key, cfg.name);
  return it->second;
}

/**
 *  Check if an endpoint match a configuration.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  @return true if the endpoint match the configuration.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  if (ext)
    *ext = io::extension("LUA", false, false);
  bool is_lua{absl::EqualsIgnoreCase(cfg.type, "lua")};
  if (is_lua) {
    cfg.params["cache"] = "yes";
    cfg.cache_enabled = true;
  }
  return is_lua;
}

/**
 *  Create an endpoint.
 *
 *  @param[in]  cfg         Endpoint configuration.
 *  @param[out] is_acceptor Will be set to false.
 *  @param[in]  cache       The persistent cache.
 *
 *  @return New endpoint.
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache) const {
  std::map<std::string, misc::variant> conf_map;
  std::string err;

  std::string filename(find_param(cfg, "path"));
  json const& js{cfg.cfg["lua_parameter"]};

  if (!err.empty())
    throw msg_fmt("lua: couldn't read a configuration json");

  auto decrypt_param = [](const std::string_view& value_name,
                          const std::string_view& raw_val) -> std::string {
    if (raw_val.substr(0, 9) == "encrypt::") {
      if (!credentials_decrypt) {
        throw msg_fmt(
            "lua: unable to decrypt {}, no encryption key file available",
            value_name);
      }
      try {
        return credentials_decrypt->decrypt(raw_val.substr(9));
      } catch (const std::exception& e) {
        throw msg_fmt("lua: unable to decrypt {}: {}", value_name, e.what());
      }
    }
    return std::string(raw_val);
  };

  if (js.is_object()) {
    json const& name{js.at("name")};
    json const& type{js.at("type")};
    json const& value{js.at("value")};

    std::string_view value_name = name.get<std::string_view>();
    if (value_name.empty())
      throw msg_fmt(
          "lua: couldn't read a configuration field because"
          " its name is empty");
    std::string t((type.get<std::string>().empty()) ? "string"
                                                    : type.get<std::string>());
    if (t == "string") {
      conf_map.insert({std::string(value_name),
                       misc::variant(value.get<std::string_view>())});
    } else if (t == "password")
      conf_map.insert({std::string(value_name),
                       misc::variant(decrypt_param(
                           value_name, value.get<std::string_view>()))});
    else if (t == "number") {
      bool ko = false;
      std::string const& v(value.get<std::string>());
      int32_t val;
      if (!absl::SimpleAtoi(v, &val))
        ko = true;
      else
        conf_map.insert({std::string(value_name), misc::variant(val)});

      // Second attempt using floating point numbers
      if (ko) {
        double val;
        if (absl::SimpleAtod(v, &val)) {
          conf_map.insert({std::string(value_name), misc::variant(val)});
          ko = false;
        } else
          ko = true;
      }
      if (ko)
        throw msg_fmt("lua: unable to read '{}' content ({}) as a number",
                      std::string(value_name), value.get<std::string>());
    }
  } else if (js.is_array()) {
    for (json const& obj : js) {
      json const& name{obj.at("name")};
      json const& type{obj.at("type")};
      json const& value{obj.at("value")};

      std::string_view value_name = name.get<std::string_view>();
      if (value_name.empty())
        throw msg_fmt(
            "lua: couldn't read a configuration field because"
            " its name is empty");
      std::string t((type.get<std::string>().empty())
                        ? "string"
                        : type.get<std::string>());
      if (t == "string") {
        conf_map.insert({std::string(value_name),
                         misc::variant(value.get<std::string_view>())});
      } else if (t == "password") {
        conf_map.insert({std::string(value_name),
                         misc::variant(decrypt_param(
                             value_name, value.get<std::string_view>()))});
      } else if (t == "number") {
        int32_t val;
        if (absl::SimpleAtoi(value.get<std::string>(), &val))
          conf_map.insert({std::string(value_name), misc::variant(val)});
        else
          throw msg_fmt("lua: unable to read '{}' content ({}) as a number",
                        std::string(value_name), value.get<std::string>());
      }
    }
  }
  // Connector.
  auto c{std::make_unique<lua::connector>()};
  c->connect_to(filename, conf_map, cache);
  is_acceptor = false;
  return c.release();
}
