/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/broker/grpc/factory.hh"

#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/grpc/acceptor.hh"
#include "com/centreon/broker/grpc/connector.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using namespace com::centreon::exceptions;

/**
 *  Check if a configuration supports this protocol.
 *
 *  @param[in] cfg  Object configuration.
 *
 *  @return True if the configuration has this protocol.
 */
bool factory::has_endpoint(com::centreon::broker::config::endpoint& cfg,
                           io::extension* ext) {
  if (ext)
    *ext = io::extension("GRPC", false, false);
  return cfg.type == "grpc";
}

static std::string read_file(const std::string& path) {
  std::ifstream file(path);
  std::stringstream ss;
  ss << file.rdbuf();
  file.close();
  return ss.str();
}

/**
 *  Create a new endpoint from a configuration.
 *
 *  @param[in]  cfg         Endpoint configuration.
 *  @param[out] is_acceptor Set to true if the endpoint is an acceptor.
 *  @param[in]  cache       Unused.
 *
 *  @return Endpoint matching configuration.
 */
io::endpoint* factory::new_endpoint(
    com::centreon::broker::config::endpoint& cfg,
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache) const {
  (void)cache;

  std::map<std::string, std::string>::const_iterator it;

  // Find host (if exists).
  std::string host;
  it = cfg.params.find("host");
  if (it != cfg.params.end())
    host = it->second;
  if (!host.empty() &&
      (std::isspace(host[0]) || std::isspace(host[host.size() - 1]))) {
    log_v2::grpc()->error(
        "GRPC: 'host' must be a string matching a host, not beginning or "
        "ending with spaces for endpoint {}, it contains '{}'",
        cfg.name, host);
    throw msg_fmt(
        "GRPC: invalid host value '{}' defined for endpoint '{}"
        "', it must not begin or end with spaces.",
        host, cfg.name);
  }

  // Find port (must exist).
  uint16_t port;
  it = cfg.params.find("port");
  if (it == cfg.params.end()) {
    log_v2::grpc()->error("GRPC: no 'port' defined for endpoint '{}'",
                          cfg.name);
    throw msg_fmt("GRPC: no 'port' defined for endpoint '{}'", cfg.name);
  }
  try {
    port = static_cast<uint16_t>(std::stol(it->second));
  } catch (const std::exception& e) {
    log_v2::grpc()->error(
        "GRPC: 'port' must be an integer and not '{}' for endpoint '{}'",
        it->second, cfg.name);
    throw msg_fmt("GRPC: invalid port value '{}' defined for endpoint '{}'",
                  it->second, cfg.name);
  }

  bool crypted = false;
  it = cfg.params.find("encryption");
  if (it != cfg.params.end()) {
    crypted = !strcasecmp(it->second.c_str(), "yes");
  }

  // public certificate
  std::string certificate;
  it = cfg.params.find("public_cert");
  if (it != cfg.params.end()) {
    try {
      certificate = read_file(it->second);
    } catch (const std::exception& e) {
      log_v2::grpc()->error("{} failed to open cert file from:{} {}",
                            __PRETTY_FUNCTION__, it->second.c_str(), e.what());
    }
  }

  // private key
  std::string certificate_key;
  it = cfg.params.find("private_key");
  if (it != cfg.params.end()) {
    try {
      certificate_key = read_file(it->second);
    } catch (const std::exception& e) {
      log_v2::grpc()->error("{} failed to open key file from:{} {}",
                            __PRETTY_FUNCTION__, it->second.c_str(), e.what());
    }
  }

  // CA certificate.
  std::string certificate_authority;
  it = cfg.params.find("ca_certificate");
  if (it != cfg.params.end()) {
    try {
      certificate_authority = read_file(it->second);
    } catch (const std::exception& e) {
      log_v2::grpc()->error("{} failed to open authority file from:{} {}",
                            __PRETTY_FUNCTION__, it->second.c_str(), e.what());
    }
  }

  std::string authorization;
  it = cfg.params.find("authorization");
  if (it != cfg.params.end()) {
    authorization = it->second;
  }

  grpc_compression_level compression_level(GRPC_COMPRESS_LEVEL_NONE);
  it = cfg.params.find("compression");
  if (it != cfg.params.end() && !strcasecmp(it->second.c_str(), "yes")) {
    compression_level = GRPC_COMPRESS_LEVEL_HIGH;
  }

  it = cfg.params.find("compression_level");
  if (it != cfg.params.end()) {
    unsigned val;
    if (!absl::SimpleAtoi(it->second, &val) || val > GRPC_COMPRESS_LEVEL_HIGH) {
      log_v2::grpc()->error(
          "{} compression_level must be a positive integer less than or equal "
          "to {} => use GRPC_COMPRESS_LEVEL_HIGH",
          __PRETTY_FUNCTION__, GRPC_COMPRESS_LEVEL_HIGH);
    } else {
      compression_level = static_cast<grpc_compression_level>(val);
    }
  }

  grpc_config::pointer conf(std::make_shared<grpc_config>(
      "", crypted, certificate, certificate_key, certificate_authority,
      authorization, compression_level));

  // Acceptor.
  std::unique_ptr<io::endpoint> endp;
  if (host.empty()) {
    is_acceptor = true;
    conf->_hostport = "0.0.0.0:" + std::to_string(port);
    std::unique_ptr<grpc::acceptor> a(new grpc::acceptor(conf));
    endp.reset(a.release());
  }
  // Connector.
  else {
    is_acceptor = false;
    conf->_hostport = host + ':' + std::to_string(port);
    std::unique_ptr<grpc::connector> c(new grpc::connector(conf));
    endp.reset(c.release());
  }

  return endp.release();
}
