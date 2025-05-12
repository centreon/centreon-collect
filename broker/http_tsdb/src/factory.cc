/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
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

/**
 * @brief extract an int param from conf file
 * @throw msg_fmt if value is not a integer
 *
 * @tparam int_type uint32_t, double, unsigned....
 * @param cfg
 * @param param_name
 * @param value out parsed value
 */
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

static const absl::flat_hash_map<std::string, asio::ssl::context_base::method>
    _conf_to_ssl_method = {
        {"sslv2", asio::ssl::context_base::method::sslv2_client},
        {"sslv3", asio::ssl::context_base::method::sslv3_client},
        {"tlsv1", asio::ssl::context_base::method::tlsv1_client},
        {"sslv23", asio::ssl::context_base::method::sslv23_client},
        {"tlsv11", asio::ssl::context_base::method::tlsv11_client},
        {"tlsv12", asio::ssl::context_base::method::tlsv12_client},
        {"tlsv13", asio::ssl::context_base::method::tlsv13_client},
        {"tls", asio::ssl::context_base::method::tls_client}};

/**
 * @brief this method parse conf and fill these attributes in conf bean:
 *  - "http_target" -> http_tsdb_config._http_target /write by default
 *  - "db_user" -> http_tsdb_config._user mandatory
 *  - "db_password" -> http_tsdb_config._pwd mandatory
 *  - "db_host" "db_port" -> http_config._endpoint mandatory
 *  - "encryption" -> http_config->_crypted
 *  - "queries_per_transaction" -> http_tsdb_config._max_queries_per_transaction
 *  - "max_send_interval" -> http_tsdb_config._max_send_interval
 *  - "connect_timeout" -> http_config._connect_timeout
 *  - "send_timeout" -> http_config._send_timeout
 *  - "receive_timeout" -> http_config._receive_timeout
 *  - "second_tcp_keep_alive_interval" ->
 *    http_config._second_tcp_keep_alive_interval
 *  - "default_http_keepalive_duration" -> _default_http_keepalive_duration
 *    used in case we receive a keepalive without timeout
 *  - "max_connections" -> http_config._max_connections
 *    the number of connection to the server ie the number of simultaneous
 * requests
 *  - "ssl_method" -> http_config._ssl_method
 *    allowed values are:
 *      - sslv2
 *      - sslv3
 *      - tlsv1
 *      - sslv23
 *      - tlsv11
 *      - tlsv12
 *      - tlsv13
 *      - tls
 *      .
 *  - "certificate_path" -> http_config._certificate_path
 *  .
 * @throw if db_user or db_password or db_host aren't found in cfg
 * @param cfg
 * @param conf out bean filled
 */
void factory::create_conf(const config::endpoint& cfg,
                          http_tsdb_config& conf) const {
  std::string user(find_param(cfg, "db_user"));
  std::string passwd(find_param(cfg, "db_password"));
  std::string addr(find_param(cfg, "db_host"));

  std::string target = "/write";
  std::map<std::string, std::string>::const_iterator it{
      cfg.params.find("http_target")};
  if (it != cfg.params.end()) {
    target = it->second;
  }

  bool encryption = false;
  it = cfg.params.find("encryption");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtob(it->second, &encryption)) {
      throw msg_fmt(
          "couldn't parse encryption '{}' defined for "
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

  //  time out
  duration connect_timeout = std::chrono::seconds(10);
  extract_duration("connect_timeout", connect_timeout);
  duration send_timeout = std::chrono::seconds(10);
  extract_duration("send_timeout", send_timeout);
  duration receive_timeout = std::chrono::seconds(30);
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
  // Get status query.
  std::vector<column> status_column_list;
  if (cfg.cfg.find("status_column") != cfg.cfg.end()) {
    status_column_list = get_columns(cfg.cfg["status_column"]);
  }

  // Get metric query.*/
  std::vector<column> metric_column_list;
  if (cfg.cfg.find("metrics_column") != cfg.cfg.end()) {
    metric_column_list = get_columns(cfg.cfg["metrics_column"]);
  }

  asio::ip::tcp::resolver resolver{*_io_context};
  boost::system::error_code err;
  auto endpoints = resolver.resolve(addr, std::to_string(port), err);
  if (err || endpoints.empty()) {
    throw msg_fmt("can't resolve {}:{} for {} : {}", addr, port, cfg.name,
                  err.message());
  }
  auto res_ep = endpoints.begin()->endpoint();

  asio::ssl::context_base::method ssl_method =
      asio::ssl::context_base::tlsv13_client;
  it = cfg.params.find("ssl_method");
  if (it != cfg.params.end()) {
    auto method_search = _conf_to_ssl_method.find(it->second);
    if (method_search != _conf_to_ssl_method.end()) {
      ssl_method = method_search->second;
    } else {
      throw msg_fmt("unknown value for ssl_method: {}", it->second);
    }
  }

  std::string certificate_path;
  it = cfg.params.find("certificate_path");
  if (it != cfg.params.end()) {
    certificate_path = it->second;
  }

  common::http::http_config http_cfg(
      res_ep, addr, encryption, connect_timeout, send_timeout, receive_timeout,
      second_tcp_keep_alive_interval, std::chrono::seconds(1), 0,
      default_http_keepalive_duration, max_connections, ssl_method,
      certificate_path);

  conf =
      http_tsdb_config(http_cfg, target, user, passwd, queries_per_transaction,
                       status_column_list, metric_column_list);
}

std::vector<column> factory::get_columns(const json& cfg) {
  std::vector<column> ret;

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

  if (cfg.is_object())
    ret.push_back(column(chk_str(cfg["name"]), chk_str(cfg["value"]),
                         chk_bool(chk_str(cfg["is_tag"])),
                         column::parse_type(chk_str(cfg["type"]))));
  else if (cfg.is_array())
    for (json const& object : cfg) {
      ret.push_back(column(chk_str(object["name"]), chk_str(object["value"]),
                           chk_bool(chk_str(object["is_tag"])),
                           column::parse_type(chk_str(object["type"]))));
    }
  return ret;
}
