/**
 * Copyright 2011-2024 Centreon
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

#include "com/centreon/broker/influxdb/factory.hh"
#include <absl/strings/match.h>
#include <nlohmann/json.hpp>

#include "com/centreon/broker/influxdb/connector.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::influxdb;
using namespace nlohmann;
using namespace com::centreon::exceptions;

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
    throw msg_fmt("influxdb: no '{}' defined for endpoint '{}'", key, cfg.name);
  return it->second;
}

/**
 *  Check if a configuration match the storage layer.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  @return True if the configuration matches the storage layer.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  bool is_ifdb{absl::EqualsIgnoreCase(cfg.type, "influxdb")};
  if (ext)
    *ext = io::extension("INFLUXDB", false, false);
  if (is_ifdb) {
    cfg.params["cache"] = "yes";
    cfg.cache_enabled = true;
  }
  return is_ifdb;
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
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache) const {
  std::string user(find_param(cfg, "db_user"));
  std::string passwd(find_param(cfg, "db_password"));
  std::string addr(find_param(cfg, "db_host"));
  std::string db(find_param(cfg, "db_name"));

  unsigned short port(0);
  {
    std::stringstream ss;
    auto it = cfg.params.find("db_port");
    if (it == cfg.params.end())
      port = 8086;
    else {
      ss << it->second;
      ss >> port;
      if (!ss.eof())
        throw msg_fmt(
            "influxdb: couldn't parse port '{}' defined for endpoint '{}'",
            ss.str(), cfg.name);
    }
  }

  uint32_t queries_per_transaction;
  {
    auto it = cfg.params.find("queries_per_transaction");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtoi(it->second, &queries_per_transaction)) {
        throw msg_fmt(
            "influxdb: couldn't parse queries_per_transaction '{}' defined for "
            "endpoint '{}'",
            it->second, cfg.name);
      }
    } else
      queries_per_transaction = 1000;
  }

  auto chk_str = [](json const& js) -> std::string {
    if (!js.is_string() || js.get<std::string>().empty()) {
      throw msg_fmt(
          "influxdb: couldn't get the configuration of a metric column name");
    }
    return js.get<std::string>();
  };
  auto chk_bool = [](std::string const& boolean) -> bool {
    if (boolean == "yes" || boolean == "true")
      return true;
    return false;
  };

  // Get status query.
  std::string status_timeseries{find_param(cfg, "status_timeseries")};
  std::vector<column> status_column_list;
  json const& status_columns = cfg.cfg["status_column"];
  if (status_columns.is_object())
    status_column_list.push_back(column(
        chk_str(status_columns["name"]), chk_str(status_columns["value"]),
        chk_bool(chk_str(status_columns["is_tag"])),
        column::parse_type(chk_str(status_columns["type"]))));
  else if (status_columns.is_array())
    for (json const& object : status_columns)
      status_column_list.push_back(
          column(chk_str(object["name"]), chk_str(object["value"]),
                 chk_bool(chk_str(object["is_tag"])),
                 column::parse_type(chk_str(object["type"]))));

  // Get metric query.*/
  std::string metric_timeseries(find_param(cfg, "metrics_timeseries"));
  std::vector<column> metric_column_list;
  json const& metric_columns = cfg.cfg["metrics_column"];
  if (metric_columns.is_object())
    metric_column_list.push_back(column(
        chk_str(metric_columns["name"]), chk_str(metric_columns["value"]),
        chk_bool(chk_str(metric_columns["is_tag"])),
        column::parse_type(chk_str(metric_columns["type"]))));
  else if (metric_columns.is_array())
    for (json const& object : metric_columns)
      metric_column_list.push_back(
          column(chk_str(object["name"]), chk_str(object["value"]),
                 chk_bool(chk_str(object["is_tag"])),
                 column::parse_type(chk_str(object["type"]))));

  // Connector.
  std::unique_ptr<influxdb::connector> c(new influxdb::connector);
  c->connect_to(user, passwd, addr, port, db, queries_per_transaction,
                status_timeseries, status_column_list, metric_timeseries,
                metric_column_list, cache);
  is_acceptor = false;
  return c.release();
}
