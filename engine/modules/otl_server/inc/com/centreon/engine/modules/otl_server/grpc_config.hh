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

#ifndef CCE_MOD_OTL_SERVER_GRPC_CONFIG_HH
#define CCE_MOD_OTL_SERVER_GRPC_CONFIG_HH

namespace com::centreon::engine::modules::otl_server {

class grpc_config {
 private:
  std::string _hostport;
  bool _crypted = false;
  std::string _certificate, _cert_key, _ca_cert;
  std::string _ca_name;
  bool _compress;
  int _second_keepalive_interval;

  static void read_file(const rapidjson::Value& json_config,
                        const std::string_view& key,
                        std::string& file_content);

 public:
  using pointer = std::shared_ptr<grpc_config>;

  grpc_config() : _compress(false), _second_keepalive_interval(30) {}
  grpc_config(const std::string& hostp, bool crypted)
      : _hostport(hostp),
        _crypted(crypted),
        _compress(false),
        _second_keepalive_interval(30) {}
  grpc_config(const std::string& hostp,
              bool crypted,
              const std::string& certificate,
              const std::string& cert_key,
              const std::string& ca_cert,
              const std::string& ca_name,
              bool compression,
              int second_keepalive_interval)
      : _hostport(hostp),
        _crypted(crypted),
        _certificate(certificate),
        _cert_key(cert_key),
        _ca_cert(ca_cert),
        _ca_name(ca_name),
        _compress(compression),
        _second_keepalive_interval(second_keepalive_interval) {}

  grpc_config(const rapidjson::Value& json_config);

  constexpr const std::string& get_hostport() const { return _hostport; }
  constexpr bool is_crypted() const { return _crypted; }
  constexpr const std::string& get_cert() const { return _certificate; }
  constexpr const std::string& get_key() const { return _cert_key; }
  constexpr const std::string& get_ca() const { return _ca_cert; }
  const std::string& get_ca_name() const { return _ca_name; }
  constexpr bool is_compressed() const { return _compress; }

  int get_second_keepalive_interval() const {
    return _second_keepalive_interval;
  }
};
}  // namespace com::centreon::engine::modules::otl_server

#endif  // !CCE_MOD_OTL_SERVER_GRPC_CONFIG_HH
