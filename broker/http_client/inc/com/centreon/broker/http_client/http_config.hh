/*
** Copyright 2022 Centreon
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

#ifndef CCB_HTTP_CLIENT_CONFIG_HH__
#define CCB_HTTP_CLIENT_CONFIG_HH__

namespace com::centreon::broker {

namespace http_client {
/**
 * @brief this class is a bean that contains all config parameters
 *
 */
class http_config {
  // destination address
  asio::ip::tcp::endpoint _endpoint;
  std::string _server_name;
  bool _crypted;
  duration _connect_timeout;
  duration _send_timeout;
  duration _receive_timeout;
  unsigned _second_tcp_keep_alive_interval;
  duration _max_retry_interval;
  unsigned _max_send_retry;
  duration _default_http_keepalive_duration;
  unsigned _max_connections;
  asio::ssl::context_base::method _ssl_method;
  // path of certificate file
  std::string _certificate_path;

 public:
  using pointer = std::shared_ptr<http_config>;

  http_config(const asio::ip::tcp::endpoint& endpoint,
              const std::string& server_name,
              bool crypted = false,
              duration connect_timeout = std::chrono::seconds(10),
              duration send_timeout = std::chrono::seconds(30),
              duration receive_timeout = std::chrono::seconds(30),
              unsigned second_tcp_keep_alive_interval = 30,
              duration max_retry_interval = std::chrono::seconds(10),
              unsigned max_send_retry = 5,
              duration default_http_keepalive_duration = std::chrono::hours(1),
              unsigned max_connections = 10,
              asio::ssl::context_base::method ssl_method =
                  asio::ssl::context_base::tlsv13_client,
              const std::string& certificate_path = "")
      : _endpoint(endpoint),
        _server_name(server_name),
        _crypted(crypted),
        _connect_timeout(connect_timeout),
        _send_timeout(send_timeout),
        _receive_timeout(receive_timeout),
        _second_tcp_keep_alive_interval(second_tcp_keep_alive_interval),
        _max_retry_interval(max_retry_interval),
        _max_send_retry(max_send_retry),
        _default_http_keepalive_duration(default_http_keepalive_duration),
        _max_connections(max_connections),
        _ssl_method(ssl_method),
        _certificate_path(certificate_path) {}

  http_config()
      : _crypted(false),
        _second_tcp_keep_alive_interval(30),
        _max_send_retry(0),
        _max_connections(0) {}

  const asio::ip::tcp::endpoint& get_endpoint() const { return _endpoint; }
  const std::string& get_server_name() const { return _server_name; }
  bool is_crypted() const { return _crypted; }
  const duration& get_connect_timeout() const { return _connect_timeout; }
  const duration& get_send_timeout() const { return _send_timeout; }
  const duration& get_receive_timeout() const { return _receive_timeout; }
  unsigned get_second_tcp_keep_alive_interval() const {
    return _second_tcp_keep_alive_interval;
  }
  const duration& get_max_retry_interval() const { return _max_retry_interval; }
  unsigned get_max_send_retry() const { return _max_send_retry; }
  const duration& get_default_http_keepalive_duration() const {
    return _default_http_keepalive_duration;
  }
  unsigned get_max_connections() const { return _max_connections; }
  asio::ssl::context_base::method get_ssl_method() const { return _ssl_method; }
  const std::string& get_certificate_path() const { return _certificate_path; }
};

};  // namespace http_client

}

namespace fmt {
template <>
struct formatter<com::centreon::broker::http_client::http_config> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const com::centreon::broker::http_client::http_config& conf,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    std::ostringstream s;
    s << conf.get_endpoint();
    return format_to(ctx.out(), "endpoint:{} crypted:{}", s.str(),
                     conf.is_crypted());
  }
};

template <>
struct formatter<asio::ip::tcp::endpoint> : ostream_formatter {};

}  // namespace fmt

#endif  // CCB_HTTP_CLIENT_CONFIG_HH__
