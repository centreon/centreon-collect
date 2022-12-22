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

CCB_BEGIN()

namespace http_client {
class http_config {
  asio::ip::tcp::endpoint _endpoint;
  bool _crypted;
  duration _connect_timeout;
  duration _send_timeout;
  duration _receive_timeout;
  unsigned _second_tcp_keep_alive_interval;
  duration _max_retry_interval;
  unsigned _max_send_retry;
  duration _default_http_keepalive_duration;

 public:
  using pointer = std::shared_ptr<http_config>;

  http_config(const asio::ip::tcp::endpoint& endpoint,
              bool crypted = false,
              duration connect_timeout = std::chrono::seconds(10),
              duration send_timeout = std::chrono::seconds(30),
              duration receive_timeout = std::chrono::seconds(30),
              unsigned second_tcp_keep_alive_interval = 30,
              duration max_retry_interval = std::chrono::seconds(10),
              unsigned max_send_retry = 5,
              duration default_http_keepalive_duration = std::chrono::hours(1))
      : _endpoint(endpoint),
        _crypted(crypted),
        _connect_timeout(connect_timeout),
        _send_timeout(send_timeout),
        _receive_timeout(receive_timeout),
        _second_tcp_keep_alive_interval(second_tcp_keep_alive_interval),
        _max_retry_interval(max_retry_interval),
        _max_send_retry(max_send_retry),
        _default_http_keepalive_duration(default_http_keepalive_duration) {}

  const asio::ip::tcp::endpoint& get_endpoint() const { return _endpoint; }
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
};

};  // namespace http_client

CCB_END()

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

}  // namespace fmt

#endif  // CCB_HTTP_CLIENT_CONFIG_HH__
