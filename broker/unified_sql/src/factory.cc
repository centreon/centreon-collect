/**
 * Copyright 2022-2024 Centreon
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

#include "com/centreon/broker/unified_sql/factory.hh"

#include <absl/strings/match.h>

#include "com/centreon/broker/unified_sql/connector.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::unified_sql;
using com::centreon::common::log_v2::log_v2;

/**
 *  Find a parameter in configuration.
 *
 *  @param[in] cfg Configuration object.
 *  @param[in] key Property to get.
 *
 *  @return Property value.
 */
static const std::string& find_param(const config::endpoint& cfg,
                                     std::string const& key) {
  auto it = cfg.params.find(key);
  if (cfg.params.end() == it)
    throw msg_fmt("unified_sql: no '{}' defined for endpoint '{}'", key,
                  cfg.name);
  return it->second;
}

/**
 *  Check if a configuration match the unified_sql layer.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  @return true if the configuration matches the unified_sql layer.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  if (ext)
    *ext = io::extension("STORAGE", false, false);
  bool is_unified_sql{absl::EqualsIgnoreCase(cfg.type, "unified_sql")};
  return is_unified_sql;
}

/**
 *  Build a unified_sql endpoint from a configuration.
 *
 *  @param[in]  cfg         Endpoint configuration.
 *  @param[out] is_acceptor Will be set to false.
 *  @param[in]  cache       Unused.
 *
 *  @return Endpoint matching the given configuration.
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params,
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache [[maybe_unused]]) const {
  auto logger = log_v2::instance().get(log_v2::SQL);
  // Find RRD length.
  uint32_t rrd_length;
  if (!absl::SimpleAtoi(find_param(cfg, "length"), &rrd_length)) {
    /* This default length represents 180 days. */
    rrd_length = 15552000;
    logger->error(
        "unified_sql: the length field should contain a string containing a "
        "number. We use the default value in replacement 15552000.");
  }

  // Find interval length if set.
  uint32_t interval_length{0};
  {
    auto it = cfg.params.find("interval");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtoi(it->second, &interval_length)) {
        interval_length = 60;
        logger->error(
            "unified_sql: the interval field should contain a string "
            "containing a number. We use the default value in replacement 60.");
      }
    }
    if (!interval_length)
      interval_length = 60;
  }

  // Find unified_sql DB parameters.
  database_config dbcfg(cfg, global_params);

  // Store or not in data_bin.
  bool store_in_data_bin(true);
  {
    auto it = cfg.params.find("store_in_data_bin");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtob(it->second, &store_in_data_bin)) {
        logger->error(
            "factory: cannot parse the 'store_in_data_bin' boolean: the "
            "content is '{}'",
            it->second);
        store_in_data_bin = true;
      }
    }
  }

  // Store or not in resources.
  bool store_in_resources{true};
  {
    auto it = cfg.params.find("store_in_resources");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtob(it->second, &store_in_resources)) {
        logger->error(
            "factory: cannot parse the 'store_in_resources' boolean: the "
            "content is '{}'",
            it->second);
        store_in_resources = true;
      }
    }
  }

  // Store or not in hosts_services.
  bool store_in_hosts_services{true};
  {
    auto it = cfg.params.find("store_in_hosts_services");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtob(it->second, &store_in_hosts_services)) {
        logger->error(
            "factory: cannot parse the 'store_in_hosts_services' boolean: the "
            "content is '{}'",
            it->second);
        store_in_hosts_services = true;
      }
    }
  }

  logger->debug("SQL: store in hosts/services: {}",
                store_in_hosts_services ? "yes" : "no");
  logger->debug("SQL: store in resources: {}",
                store_in_resources ? "yes" : "no");

  // Loop timeout
  // By default, 30 seconds
  int32_t loop_timeout = cfg.read_timeout;
  if (loop_timeout < 0)
    loop_timeout = 30;

  // Instance timeout
  // By default, 5 minutes.
  uint32_t instance_timeout = 5 * 60;
  {
    auto it = cfg.params.find("instance_timeout");
    if (it != cfg.params.end() &&
        !absl::SimpleAtoi(it->second, &instance_timeout)) {
      logger->error(
          "factory: cannot parse the 'instance_timeout' value. It should be an "
          "unsigned integer. 300 is set by default.");
      instance_timeout = 300;
    }
  }

  // Connector.
  auto c = std::make_unique<unified_sql::connector>();
  c->connect_to(dbcfg, rrd_length, interval_length, loop_timeout,
                instance_timeout, store_in_data_bin, store_in_resources,
                store_in_hosts_services);
  is_acceptor = false;
  return c.release();
}
