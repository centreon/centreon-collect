/*
** Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_TELEGRAF_CONF_SERVER_HH
#define CCE_MOD_OTL_TELEGRAF_CONF_SERVER_HH

namespace com::centreon::engine::modules::opentelemetry::telegraf {

namespace http = com::centreon::common::http;

/**
 * @brief telegraf configuration bean given by json config file
 *
 */
class conf_server_config {
  asio::ip::tcp::endpoint _listen_endpoint;
  bool _crypted;
  unsigned _second_keep_alive_interval;
  std::string _public_cert;
  std::string _private_key;
  unsigned _check_interval;

 public:
  using pointer = std::shared_ptr<conf_server_config>;

  conf_server_config(const rapidjson::Value& json_config_v,
                     asio::io_context& io_context);

  const asio::ip::tcp::endpoint& get_listen_endpoint() const {
    return _listen_endpoint;
  }
  bool is_crypted() const { return _crypted; }
  unsigned get_second_keep_alive_interval() const {
    return _second_keep_alive_interval;
  }

  unsigned get_check_interval() const { return _check_interval; }

  const std::string& get_public_cert() const { return _public_cert; }
  const std::string& get_private_key() const { return _private_key; }

  bool operator==(const conf_server_config& right) const;
};

/**
 * @brief http(s) session used by telegraf to get his configuration
 *
 * @tparam connection_class http_connection or https_connection
 */
template <class connection_class = http::http_connection>
class conf_session : public connection_class {
  conf_server_config::pointer _telegraf_conf;

  void wait_for_request();

  void on_receive_request(const std::shared_ptr<http::request_type>& request);

  void answer_to_request(const std::shared_ptr<http::request_type>& request,
                         const std::string& host);

  bool _otel_connector_to_stream(const std::string& cmd_name,
                                 const std::string& cmd_line,
                                 const std::string& host,
                                 const std::string& service,
                                 std::string& to_append);

 public:
  using my_type = conf_session<connection_class>;
  using pointer = std::shared_ptr<my_type>;

  conf_session(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const http::http_config::pointer& conf,
               const http::ssl_ctx_initializer& ssl_initializer,
               const conf_server_config::pointer& telegraf_conf)
      : connection_class(io_context, logger, conf, ssl_initializer),
        _telegraf_conf(telegraf_conf) {}

  pointer shared_from_this() {
    return std::static_pointer_cast<my_type>(
        connection_class::shared_from_this());
  }

  void on_accept() override;
};

}  // namespace com::centreon::engine::modules::opentelemetry::telegraf

#endif
