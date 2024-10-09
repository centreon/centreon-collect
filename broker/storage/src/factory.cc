/**
 * Copyright 2011-2015,2017-2024 Centreon
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

#include "com/centreon/broker/storage/factory.hh"

#include <absl/strings/match.h>

#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/storage/connector.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::storage;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Find a parameter in configuration.
 *
 *  @param[in] cfg Configuration object.
 *  @param[in] key Property to get.
 *
 *  @return Property value.
 */
static std::string const& find_param(config::endpoint const& cfg,
                                     std::string const& key) {
  auto it = cfg.params.find(key);
  if (cfg.params.end() == it)
    throw msg_fmt("storage: no '{}' defined for endpoint '{}'", key, cfg.name);
  return it->second;
}

/**
 *  Check if a configuration match the storage layer.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  @return true if the configuration matches the storage layer.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  if (ext)
    *ext = io::extension("STORAGE", false, false);
  bool is_storage{absl::EqualsIgnoreCase(cfg.type, "storage")};
  return is_storage;
}

/**
 *  Build a storage endpoint from a configuration.
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
  // Find RRD length.
  uint32_t rrd_length;
  if (!absl::SimpleAtoi(find_param(cfg, "length"), &rrd_length)) {
    rrd_length = 15552000;
    log_v2::instance()
        .get(log_v2::CORE)
        ->error(
            "storage: the length field should contain a string containing a "
            "number. We use the default value in replacement 15552000.");
  }

  // Find interval length if set.
  uint32_t interval_length = 0;
  {
    auto it = cfg.params.find("interval");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtoi(it->second, &interval_length)) {
        interval_length = 60;
        log_v2::instance()
            .get(log_v2::CORE)
            ->error(
                "storage: the interval field should contain a string "
                "containing a number. We use the default value in replacement "
                "60.");
      }
    }
    if (!interval_length)
      interval_length = 60;
  }

  // Find storage DB parameters.
  database_config dbcfg(cfg, global_params);

  // Store or not in data_bin.
  bool store_in_data_bin{true};
  {
    auto it = cfg.params.find("store_in_data_bin");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtob(it->second, &store_in_data_bin)) {
        log_v2::instance()
            .get(log_v2::CORE)
            ->error(
                "factory: cannot parse the 'store_in_data_bin' boolean: the "
                "content is '{}'",
                it->second);
        store_in_data_bin = true;
      }
    }
  }

  // Connector.
  std::unique_ptr<storage::connector> c(new storage::connector);
  c->connect_to(dbcfg, rrd_length, interval_length, store_in_data_bin);
  is_acceptor = false;
  return c.release();
}
