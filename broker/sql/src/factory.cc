/**
 * Copyright 2011-2013,2015, 2024 Centreon
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

#include "com/centreon/broker/sql/factory.hh"

#include <absl/strings/match.h>

#include "com/centreon/broker/sql/connector.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::sql;
using com::centreon::common::log_v2::log_v2;

/**
 *  Check if an endpoint match a configuration.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  @return True if the endpoint match the configuration.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  if (ext)
    *ext = io::extension("SQL", false, false);
  bool is_sql{absl::EqualsIgnoreCase(cfg.type, "sql")};
  return is_sql;
}

/**
 *  Create an endpoint.
 *
 *  @param[in]  cfg         Endpoint configuration.
 *  @param[out] is_acceptor Will be set to false.
 *  @param[in]  cache       Unused.
 *
 *  @return New endpoint.
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params,
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache [[maybe_unused]]) const {
  // Database configuration.
  database_config dbcfg(cfg, global_params);

  // Cleanup check interval.
  uint32_t cleanup_check_interval = 0;
  {
    auto it = cfg.params.find("cleanup_check_interval");
    if (it != cfg.params.end() &&
        !absl::SimpleAtoi(it->second, &cleanup_check_interval)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "sql: the 'cleanup_check_interval' value must be a positive "
              "integer. Otherwise, 0 is used for its value.");
      cleanup_check_interval = 0u;
    }
  }

  bool enable_cmd_cache = false;
  {
    auto it = cfg.params.find("enable_command_cache");
    if (it != cfg.params.end() &&
        !absl::SimpleAtob(it->second, &enable_cmd_cache)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "sql: the 'enable_command_cache' value must be a boolean. "
              "Otherwise 'false' is used for its value.");
      enable_cmd_cache = false;
    }
  }

  // Loop timeout
  // By default, 30 seconds
  int32_t loop_timeout = cfg.read_timeout;
  if (loop_timeout < 0)
    loop_timeout = 30;

  // Instance timeout
  // By default, 5 minutes.
  uint32_t instance_timeout(5 * 60);
  {
    auto it = cfg.params.find("instance_timeout");
    if (it != cfg.params.end() &&
        !absl::SimpleAtoi(it->second, &instance_timeout)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "sql: the 'instance_timeout' value must be a positive integer. "
              "Otherwise, 300 is used for its value.");
      instance_timeout = 300;
    }
  }

  // Use state events ?
  bool wse = false;
  {
    auto it = cfg.params.find("with_state_events");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtob(it->second, &wse)) {
        log_v2::instance()
            .get(log_v2::CORE)
            ->error(
                "factory: cannot parse the 'with_state_events' boolean: the "
                "content is '{}'",
                it->second);
        wse = false;
      }
    }
  }

  // Connector.
  auto c{std::make_unique<sql::connector>()};
  c->connect_to(dbcfg, cleanup_check_interval, loop_timeout, instance_timeout,
                wse, enable_cmd_cache);
  is_acceptor = false;
  return c.release();
}
