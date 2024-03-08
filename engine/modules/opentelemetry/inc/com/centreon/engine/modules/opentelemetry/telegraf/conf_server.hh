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
class conf_server_config {
  std::string _listen_address;
  unsigned short _port;
  bool _crypted;
  unsigned _second_keep_alive_interval;

 public:
  using pointer = std::shared_ptr<conf_server_config>;

  conf_server_config(const rapidjson::Value& json_config_v);

  const std::string get_listen_address() const { return _listen_address; }
  unsigned short get_port() const { return _port; }
  bool is_crypted() const { return _crypted; }
  unsigned get_second_keep_alive_interval() const {
    return _second_keep_alive_interval;
  }

  bool operator==(const conf_server_config& right) const;
};

class http_session : public std::enable_shared_from_this<http_session> {};

class https_session : public std::enable_shared_from_this<https_session> {
};

class http_server : public std::enable_shared_from_this<http_server> {
  conf_server_config::pointer _conf;
  std::shared_ptr<asio::io_context> _io_ctx;

 public:
  http_server(const conf_server_config::pointer& conf,
              const std::shared_ptr<asio::io_context>& ctx);

  std::shared_ptr<http_server> create(
      const conf_server_config::pointer& conf,
      const std::shared_ptr<asio::io_context>& ctx);


  void shutdown();
};

}  // namespace com::centreon::engine::modules::opentelemetry::telegraf

#endif