/*
** Copyright 2011-2017 Centreon
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

#include "com/centreon/broker/http_tsdb/factory.hh"

#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/http_tsdb/column.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_tsdb;
using namespace nlohmann;
using namespace com::centreon::exceptions;

factory::factory(const std::string& name,
                 const std::shared_ptr<asio::io_context>& io_context)
    : _name(name), _io_context(io_context) {}

/**
 *  Find a parameter in configuration.
 *
 *  @param[in] cfg Configuration object.
 *  @param[in] key Property to get.
 *
 *  @return Property value.
 */
std::string factory::find_param(config::endpoint const& cfg,
                                std::string const& key) const {
  std::map<std::string, std::string>::const_iterator it{cfg.params.find(key)};
  if (cfg.params.end() == it)
    throw msg_fmt("{}: no '{}' defined for endpoint '{}'", _name, key,
                  cfg.name);
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
  bool is_ifdb{absl::EqualsIgnoreCase(cfg.type, _name)};
  if (ext)
    *ext = io::extension(boost::algorithm::to_upper_copy(_name), false, false);
  if (is_ifdb) {
    cfg.params["cache"] = "yes";
    cfg.cache_enabled = true;
  }
  return is_ifdb;
}

template <typename int_type>
void extract_int(const config::endpoint& cfg,
                 const std::string& param_name,
                 int_type& value) {
  auto search = cfg.params.find(param_name);
  if (search != cfg.params.end()) {
    if (!absl::SimpleAtoi(search->second, &value)) {
      throw msg_fmt("couldn't parse {} '{}' defined for endpoint '{}'",
                    param_name, search->second, cfg.name);
    }
  }
}

void factory::create_conf(const config::endpoint& cfg,
                          http_tsdb_config& conf) const {
  std::string user(find_param(cfg, "db_user"));
  std::string passwd(find_param(cfg, "db_password"));
  std::string addr(find_param(cfg, "db_host"));
  std::string db(find_param(cfg, "db_name"));

  bool encryption = false;
  std::map<std::string, std::string>::const_iterator it{
      cfg.params.find("encryption")};
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtob(it->second, &encryption)) {
      throw msg_fmt(
          "couldn't parse ecnryption '{}' defined for "
          "endpoint '{}'",
          it->second, cfg.name);
    }
  }

  uint32_t port(encryption ? 443 : 80);
  extract_int(cfg, "db_port", port);

  unsigned queries_per_transaction = 1000;
  extract_int(cfg, "queries_per_transaction", queries_per_transaction);

  auto extract_duration = [&](const std::string& param_name, duration& value) {
    unsigned seconds = 0;
    it = cfg.params.find(param_name);
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtoi(it->second, &seconds)) {
        throw msg_fmt(
            "couldn't parse {} '{}' defined for "
            "endpoint '{}'",
            param_name, it->second, cfg.name);
      }
      value = std::chrono::seconds(seconds);
    }
  };

  duration max_send_interval = std::chrono::seconds(10);
  extract_duration("max_send_interval", max_send_interval);

  //  time out
  duration connect_timeout = std::chrono::seconds(10);
  extract_duration("connect_timeout", connect_timeout);
  duration send_timeout = std::chrono::seconds(10);
  extract_duration("send_timeout", send_timeout);
  duration receive_timeout = std::chrono::seconds(60);
  extract_duration("receive_timeout", receive_timeout);

  unsigned second_tcp_keep_alive_interval = 30;
  extract_int(cfg, "second_tcp_keep_alive_interval",
              second_tcp_keep_alive_interval);

  duration default_http_keepalive_duration = std::chrono::hours(1);
  extract_duration("default_http_keepalive_duration",
                   default_http_keepalive_duration);

  unsigned max_connections = 5;
  extract_int(cfg, "max_connections", max_connections);

  //      columns
  auto chk_str = [](json const& js) -> std::string {
    if (!js.is_string() || js.get<std::string>().empty()) {
      throw msg_fmt("couldn't get the configuration of a metric column name");
    }
    return js.get<std::string>();
  };
  auto chk_bool = [](std::string const& boolean) -> bool {
    if (boolean == "yes" || boolean == "true")
      return true;
    return false;
  };

  // Get status query.
  std::vector<column> status_column_list;
  if (cfg.cfg.find("status_column") != cfg.cfg.end()) {
    json const& status_columns = cfg.cfg["status_column"];
    if (status_columns.is_object())
      status_column_list.push_back(column(
          chk_str(status_columns["name"]), chk_str(status_columns["value"]),
          chk_bool(chk_str(status_columns["is_tag"])),
          column::parse_type(chk_str(status_columns["type"]))));
    else if (status_columns.is_array())
      for (json const& object : status_columns) {
        status_column_list.push_back(
            column(chk_str(object["name"]), chk_str(object["value"]),
                   chk_bool(chk_str(object["is_tag"])),
                   column::parse_type(chk_str(object["type"]))));
      }
  }

  // Get metric query.*/
  std::vector<column> metric_column_list;
  if (cfg.cfg.find("metrics_column") != cfg.cfg.end()) {
    json const& metric_columns = cfg.cfg["metrics_column"];
    if (metric_columns.is_object())
      metric_column_list.push_back(column(
          chk_str(metric_columns["name"]), chk_str(metric_columns["value"]),
          chk_bool(chk_str(metric_columns["is_tag"])),
          column::parse_type(chk_str(metric_columns["type"]))));
    else if (metric_columns.is_array())
      for (json const& object : metric_columns) {
        metric_column_list.push_back(
            column(chk_str(object["name"]), chk_str(object["value"]),
                   chk_bool(chk_str(object["is_tag"])),
                   column::parse_type(chk_str(object["type"]))));
      }
  }

  asio::ip::tcp::resolver resolver{*_io_context};
  asio::ip::tcp::resolver::query query{addr, std::to_string(port)};

  asio::ip::tcp::resolver::iterator res_it{resolver.resolve(query)};
  asio::ip::tcp::resolver::iterator res_end;
  if (res_it == res_end) {
    throw msg_fmt("can't resolve {}:{} for {}", addr, port, cfg.name);
  }
  http_client::http_config http_cfg(
      res_it->endpoint(), encryption, connect_timeout, send_timeout,
      receive_timeout, second_tcp_keep_alive_interval, std::chrono::seconds(1),
      0, default_http_keepalive_duration, max_connections);

  conf = http_tsdb_config(http_cfg, user, passwd, db, queries_per_transaction,
                          max_send_interval, status_column_list,
                          metric_column_list);
}