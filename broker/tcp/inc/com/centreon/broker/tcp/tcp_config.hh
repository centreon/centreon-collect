/**
 * Copyright 2022-2023 Centreon
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

#ifndef CCB_TCP_CONFIG_HH
#define CCB_TCP_CONFIG_HH

namespace com::centreon::broker {

namespace tcp {
class tcp_config {
  const std::string _host;
  uint16_t _port;
  int32_t _read_timeout;
  int _second_keepalive_interval;
  int _keepalive_count;

 public:
  using pointer = std::shared_ptr<tcp_config>;

  tcp_config(const std::string& host,
             uint16_t port,
             int32_t read_timeout = -1,
             int second_keepalive_interval = 30,
             int keepalive_count = 2)
      : _host(host),
        _port(port),
        _read_timeout(read_timeout),
        _second_keepalive_interval(second_keepalive_interval),
        _keepalive_count(keepalive_count) {}

  const std::string& get_host() const { return _host; }
  uint16_t get_port() const { return _port; }
  int32_t get_read_timeout() const { return _read_timeout; }
  int get_second_keepalive_interval() const {
    return _second_keepalive_interval;
  }
  int get_keepalive_count() const { return _keepalive_count; }
};

}  // namespace tcp

}  // namespace com::centreon::broker

#endif
