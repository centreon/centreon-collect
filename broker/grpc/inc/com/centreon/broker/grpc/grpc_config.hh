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

#ifndef CCB_GRPC_CONFIG_HH
#define CCB_GRPC_CONFIG_HH

CCB_BEGIN()

namespace grpc {
class grpc_config {
 public:
  enum compression_active { NO = 0, AUTO = 1, YES = 2 };

 private:
  std::string _hostport;
  bool _crypted = false;
  std::string _certificate, _cert_key, _ca_cert, _authorization;
  // grpc_compression_level _compress_level;
  compression_active _compress;

 public:
  using pointer = std::shared_ptr<grpc_config>;

  grpc_config() : _compress(NO) {}
  grpc_config(const std::string& hostp)
      : _hostport(hostp), _crypted(false), _compress(NO) {}
  grpc_config(const std::string& hostp,
              bool crypted,
              const std::string& certificate,
              const std::string& cert_key,
              const std::string& ca_cert,
              const std::string& authorization,
              compression_active compression)
      : _hostport(hostp),
        _crypted(crypted),
        _certificate(certificate),
        _cert_key(cert_key),
        _ca_cert(ca_cert),
        _authorization(authorization),
        _compress(compression) {}

  constexpr const std::string& get_hostport() const { return _hostport; }
  constexpr bool is_crypted() const { return _crypted; }
  constexpr const std::string& get_cert() const { return _certificate; }
  constexpr const std::string& get_key() const { return _cert_key; }
  constexpr const std::string& get_ca() const { return _ca_cert; }
  constexpr const std::string& get_authorization() const {
    return _authorization;
  }
  constexpr compression_active get_compression() const { return _compress; }

  friend class factory;
};
};  // namespace grpc
CCB_END()

#endif  // !CCB_GRPC_CONFIG_HH
