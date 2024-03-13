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

template <class connection_class>
class conf_session : public connection_class {
 public:
  void on_accept() override;
};

}  // namespace com::centreon::engine::modules::opentelemetry::telegraf

#endif
