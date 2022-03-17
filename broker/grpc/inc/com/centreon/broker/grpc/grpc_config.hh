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
  std::string _hostport;
  bool _crypted;
  std::string _certificate, _cert_key, _ca_cert, _authorization;

 public:
  using pointer = std::shared_ptr<grpc_config>;

  grpc_config() : _crypted(false) {}
  grpc_config(const std::string& hostp) : _hostport(hostp), _crypted(false) {}
  grpc_config(const std::string& hostp,
              bool crypted,
              const std::string& certificate,
              const std::string& cert_key,
              const std::string& ca_cert,
              const std::string& authorization)
      : _hostport(hostp),
        _crypted(crypted),
        _certificate(certificate),
        _cert_key(cert_key),
        _ca_cert(ca_cert),
        _authorization(authorization) {}

  const std::string& get_hostport() const { return _hostport; }
  bool is_crypted() const { return _crypted; }
  const std::string& get_cert() const { return _certificate; }
  const std::string& get_key() const { return _cert_key; }
  const std::string& get_ca() const { return _ca_cert; }
  const std::string& get_authorization() const { return _authorization; }

  friend class factory;
};
};  // namespace grpc
CCB_END()

#endif  // !CCB_GRPC_CONFIG_HH
