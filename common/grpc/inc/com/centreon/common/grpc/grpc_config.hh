/*
** Copyright 2025 Centreon
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

#ifndef COMMON_GRPC_CONFIG_HH
#define COMMON_GRPC_CONFIG_HH

#include <grpc/compression.h>

namespace com::centreon::common::grpc {

/**
 * @brief calculate grpc compression mask (used both by client and server)
 *
 * @return constexpr uint32_t
 */
constexpr uint32_t calc_accept_all_compression_mask() {
  uint32_t ret = 0;
  for (size_t algo_ind = 0; algo_ind < GRPC_COMPRESS_ALGORITHMS_COUNT;
       algo_ind++) {
    ret += (1u << algo_ind);
  }
  return ret;
}

/**
 * @brief configuration bean used by client and server
 *
 *
 */
class grpc_config {
  /**
   * @brief client case: where to connect
   * server case: address/port to listen
   *
   */
  std::string _hostport;
  bool _crypted = false;
  std::string _certificate, _cert_key, _ca_cert;
  std::string _ca_name;
  bool _compress;
  int _second_keepalive_interval;

  /**
   * @brief (client side) if this parameter is > 0 this is the longest delay in
   * second between two failed connection. if is the
   * GRPC_ARG_MAX_RECONNECT_BACKOFF_MS parameter
   *
   */
  unsigned _second_max_reconnect_backoff;

  /**
   * @brief maximum message length
   * by default grpc message is limited to 4MB
   */
  unsigned _max_message_length;

  std::string _token;
  std::shared_ptr<absl::flat_hash_set<std::string>> _trusted_tokens;

 public:
  using pointer = std::shared_ptr<grpc_config>;

  grpc_config()
      : _compress(false),
        _second_keepalive_interval(30),
        _second_max_reconnect_backoff(0),
        _max_message_length(0) {}

  grpc_config(const std::string& hostp)
      : _hostport(hostp),
        _compress(false),
        _second_keepalive_interval(30),
        _second_max_reconnect_backoff(0),
        _max_message_length(0) {}

  grpc_config(const std::string& hostp, bool crypted)
      : _hostport(hostp),
        _crypted(crypted),
        _compress(false),
        _second_keepalive_interval(30),
        _second_max_reconnect_backoff(0),
        _max_message_length(0) {}

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
        _second_keepalive_interval(second_keepalive_interval),
        _second_max_reconnect_backoff(0),
        _max_message_length(0) {}

  grpc_config(const std::string& hostp,
              bool crypted,
              const std::string& certificate,
              const std::string& cert_key,
              const std::string& ca_cert,
              const std::string& ca_name,
              bool compression,
              int second_keepalive_interval,
              unsigned second_max_reconnect_backoff,
              unsigned max_message_length,
              const std::string& token)
      : _hostport(hostp),
        _crypted(crypted),
        _certificate(certificate),
        _cert_key(cert_key),
        _ca_cert(ca_cert),
        _ca_name(ca_name),
        _compress(compression),
        _second_keepalive_interval(second_keepalive_interval),
        _second_max_reconnect_backoff(second_max_reconnect_backoff),
        _max_message_length(max_message_length),
        _token{token} {}

  grpc_config(
      const std::string& hostp,
      bool crypted,
      const std::string& certificate,
      const std::string& cert_key,
      const std::string& ca_cert,
      const std::string& ca_name,
      bool compression,
      int second_keepalive_interval,
      unsigned second_max_reconnect_backoff,
      unsigned max_message_length,
      const std::shared_ptr<absl::flat_hash_set<std::string>>& trusted_tokens)
      : _hostport(hostp),
        _crypted(crypted),
        _certificate(certificate),
        _cert_key(cert_key),
        _ca_cert(ca_cert),
        _ca_name(ca_name),
        _compress(compression),
        _second_keepalive_interval(second_keepalive_interval),
        _second_max_reconnect_backoff(second_max_reconnect_backoff),
        _max_message_length(max_message_length),
        _trusted_tokens{trusted_tokens} {}

  const std::string& get_hostport() const { return _hostport; }
  bool is_crypted() const { return _crypted; }
  const std::string& get_cert() const { return _certificate; }
  const std::string& get_key() const { return _cert_key; }
  const std::string& get_ca() const { return _ca_cert; }
  const std::string& get_ca_name() const { return _ca_name; }
  bool is_compressed() const { return _compress; }
  int get_second_keepalive_interval() const {
    return _second_keepalive_interval;
  }

  unsigned get_second_max_reconnect_backoff() const {
    return _second_max_reconnect_backoff;
  }

  unsigned get_max_message_length() const { return _max_message_length; }

  const std::string& get_token() const { return _token; }
  const std::shared_ptr<absl::flat_hash_set<std::string>>&
  get_trusted_tokens() {
    return _trusted_tokens;
  }

  bool operator==(const grpc_config& right) const {
    if (_hostport != right._hostport || _crypted != right._crypted ||
        _certificate != right._certificate || _cert_key != right._cert_key ||
        _ca_cert != right._ca_cert || _ca_name != right._ca_name ||
        _compress != right._compress ||
        _second_keepalive_interval != right._second_keepalive_interval ||
        _second_max_reconnect_backoff != right._second_max_reconnect_backoff ||
        _token != right._token) {
      return false;
    }

    // Compare trusted_tokens
    if (_trusted_tokens != nullptr && right._trusted_tokens != nullptr)
      return *_trusted_tokens == *right._trusted_tokens;
    if (_trusted_tokens == nullptr && right._trusted_tokens == nullptr)
      return true;
    return false;
  }

  /**
   * @brief identical to std:string::compare
   *
   * @param right
   * @return int -1, 0 if equal or 1
   */
  int compare(const grpc_config& right) const {
    int ret = _hostport.compare(right._hostport);
    if (ret)
      return ret;
    ret = _crypted - right._crypted;
    if (ret)
      return ret;
    ret = _certificate.compare(right._certificate);
    if (ret)
      return ret;
    ret = _cert_key.compare(right._cert_key);
    if (ret)
      return ret;
    ret = _ca_cert.compare(right._ca_cert);
    if (ret)
      return ret;
    ret = _ca_name.compare(right._ca_name);
    if (ret)
      return ret;
    ret = _compress - right._compress;
    if (ret)
      return ret;
    ret = _second_keepalive_interval - right._second_keepalive_interval;
    if (ret)
      return ret;
    ret = _token.compare(right._token);
    if (ret)
      return ret;

    if (_trusted_tokens != nullptr && right._trusted_tokens != nullptr)
      if (*_trusted_tokens != *right._trusted_tokens) {
        if (_trusted_tokens->size() < right._trusted_tokens->size())
          return -1;
        else
          return 1;
      }

    if ((_trusted_tokens == nullptr&& right._trusted_tokens != nullptr) {
          return -1;  // or any non-zero value
        }(_trusted_tokens != nullptr && right._trusted_tokens == nullptr)) {
      return 1;  // or any non-zero value
    }

    return 0;
  }
};
}  // namespace com::centreon::common::grpc

#endif
