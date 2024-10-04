/**
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

#include <absl/strings/match.h>

#include "grpc_stream.grpc.pb.h"

#include "com/centreon/broker/grpc/factory.hh"

#include "com/centreon/broker/grpc/acceptor.hh"
#include "com/centreon/broker/grpc/connector.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using namespace com::centreon::exceptions;
using com::centreon::common::log_v2::log_v2;

/**
 *  Check if a configuration supports this protocol.
 *  Possible endpoints are:
 *   * grpc
 *   * bbdo_server with 'grpc' transport_protocol
 *   * bbdo_client with 'grpc' transport_protocol
 *
 *  @param[in] cfg  Object configuration.
 *
 *  @return True if the configuration has this protocol.
 */
bool factory::has_endpoint(com::centreon::broker::config::endpoint& cfg,
                           io::extension* ext) {
  if (ext)
    *ext = io::extension("GRPC", false, false);
  /* Legacy case: we create a grpc endpoint */
  if (cfg.type == "grpc")
    return true;

  /* New case: we create a bbdo_server or a bbdo_client with transport protocol
   * set to 'grpc' */
  if ((cfg.type == "bbdo_server" || cfg.type == "bbdo_client") &&
      absl::EqualsIgnoreCase(cfg.params["transport_protocol"], "grpc"))
    return true;

  return false;
}

static std::string read_file(const std::string& path) {
  std::ifstream file(path);
  if (file.is_open()) {
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    return ss.str();
  } else
    throw msg_fmt("Cannot open file '{}': {}", path, strerror(errno));
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
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache [[maybe_unused]]) const {
  if (cfg.type == "bbdo_server" || cfg.type == "bbdo_client")
    return _new_endpoint_bbdo_cs(cfg, is_acceptor);

  // Find host (if exists).
  std::string host;
  auto it = cfg.params.find("host");
  if (it != cfg.params.end())
    host = it->second;
  if (!host.empty() &&
      (std::isspace(host[0]) || std::isspace(host[host.size() - 1]))) {
    log_v2::instance()
        .get(log_v2::CORE)
        ->error(
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
    log_v2::instance()
        .get(log_v2::CORE)
        ->error("GRPC: no 'port' defined for endpoint '{}'", cfg.name);
    throw msg_fmt("GRPC: no 'port' defined for endpoint '{}'", cfg.name);
  }
  {
    uint32_t port32;
    if (!absl::SimpleAtoi(it->second, &port32)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "GRPC: 'port' must be an integer and not '{}' for endpoint '{}'",
              it->second, cfg.name);
      throw msg_fmt("GRPC: invalid port value '{}' defined for endpoint '{}'",
                    it->second, cfg.name);
    }
    if (port32 > 65535)
      throw msg_fmt("GRPC: invalid port value '{}' defined for endpoint '{}'",
                    it->second, cfg.name);
    else
      port = port32;
  }

  bool encrypted = false;
  it = cfg.params.find("encryption");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtob(it->second, &encrypted))
      throw msg_fmt("GRPC: 'encryption' field should be a boolean and not '{}'",
                    it->second);
  }

  // public certificate
  std::string certificate;
  it = cfg.params.find("public_cert");
  if (it != cfg.params.end()) {
    try {
      certificate = read_file(it->second);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::TLS),
                          "Failed to open cert file '{}': {}", it->second,
                          e.what());
      throw msg_fmt("Failed to open cert file '{}': {}", it->second, e.what());
    }
  }

  // private key
  std::string certificate_key;
  it = cfg.params.find("private_key");
  if (it != cfg.params.end()) {
    try {
      certificate_key = read_file(it->second);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::TLS),
                          "Failed to open certificate key file '{}': {}",
                          it->second, e.what());
      throw msg_fmt("Failed to open certificate key file '{}': {}", it->second,
                    e.what());
    }
  }

  // CA certificate.
  std::string certificate_authority;
  it = cfg.params.find("ca_certificate");
  if (it != cfg.params.end()) {
    try {
      certificate_authority = read_file(it->second);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::TLS),
                          "Failed to open authority certificate file '{}': {}",
                          it->second, e.what());
      throw msg_fmt("Failed to open authority certificate file '{}': {}",
                    it->second, e.what());
    }
  }

  std::string authorization;
  it = cfg.params.find("authorization");
  if (it != cfg.params.end()) {
    authorization = it->second;
  }

  std::string ca_name;
  it = cfg.params.find("ca_name");
  if (it == cfg.params.end())
    it = cfg.params.find("tls_hostname");
  if (it != cfg.params.end())
    ca_name = it->second;

  bool compression = false;
  it = cfg.params.find("compression");
  if (it != cfg.params.end() && !strcasecmp(it->second.c_str(), "yes"))
    compression = true;

  // keepalive conf
  int keepalive_interval = 30;
  it = cfg.params.find("keepalive_interval");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &keepalive_interval)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "GRPC: 'keepalive_interval' field should be an integer and not "
              "'{}'",
              it->second);
      throw msg_fmt(
          "GRPC: 'keepalive_interval' field should be an integer and not '{}'",
          it->second);
    }
  }

  std::string hostport;
  if (host.empty()) {
    is_acceptor = true;
    hostport = fmt::format("0.0.0.0:{}", port);
  } else {
    is_acceptor = false;
    hostport = fmt::format("{}:{}", host, port);
  }

  grpc_config::pointer conf(std::make_shared<grpc_config>(
      hostport, encrypted, certificate, certificate_key, certificate_authority,
      authorization, ca_name, compression, keepalive_interval,
      direct_grpc_serialized(cfg)));

  std::unique_ptr<io::endpoint> endp;

  // Acceptor.
  if (is_acceptor) {
    log_v2::instance()
        .get(log_v2::CORE)
        ->debug("GRPC: encryption {} on gRPC server port {}",
                encrypted ? "enabled" : "disabled", port);
    endp = std::make_unique<grpc::acceptor>(conf);
  }
  // Connector.
  else {
    log_v2::instance()
        .get(log_v2::CORE)
        ->debug("GRPC: encryption {} on gRPC client port {}",
                encrypted ? "enabled" : "disabled", port);
    endp = std::make_unique<grpc::connector>(conf);
  }

  return endp.release();
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
io::endpoint* factory::_new_endpoint_bbdo_cs(
    com::centreon::broker::config::endpoint& cfg,
    bool& is_acceptor) const {
  // Find host (if exists).
  std::string host;
  auto it = cfg.params.find("host");
  if (it != cfg.params.end())
    host = it->second;
  if (!host.empty() &&
      (std::isspace(host[0]) || std::isspace(host[host.size() - 1]))) {
    log_v2::instance()
        .get(log_v2::CORE)
        ->error(
            "GRPC: 'host' must be a string matching a host, not beginning or "
            "ending with spaces for endpoint {}, it contains '{}'",
            cfg.name, host);
    throw msg_fmt(
        "GRPC: invalid host value '{}' defined for endpoint '{}"
        "', it must not begin or end with spaces.",
        host, cfg.name);
  } else if (host.empty()) {
    /* host is empty */
    if (cfg.type == "bbdo_server")
      host = "0.0.0.0";
    else
      throw msg_fmt("GRPC: you must specify a host to connect to.");
  }

  // Find port (must exist).
  uint16_t port;
  it = cfg.params.find("port");
  if (it == cfg.params.end()) {
    log_v2::instance()
        .get(log_v2::CORE)
        ->error("GRPC: no 'port' defined for endpoint '{}'", cfg.name);
    throw msg_fmt("GRPC: no 'port' defined for endpoint '{}'", cfg.name);
  }
  {
    uint32_t port32;
    if (!absl::SimpleAtoi(it->second, &port32)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "GRPC: 'port' must be an integer and not '{}' for endpoint '{}'",
              it->second, cfg.name);
      throw msg_fmt("GRPC: invalid port value '{}' defined for endpoint '{}'",
                    it->second, cfg.name);
    }
    if (port32 > 65535)
      throw msg_fmt("GRPC: invalid port value '{}' defined for endpoint '{}'",
                    it->second, cfg.name);
    else
      port = port32;
  }

  // Find authorization token (if exists).
  std::string authorization;
  it = cfg.params.find("authorization");
  if (it != cfg.params.end())
    authorization = it->second;
  log_v2::instance()
      .get(log_v2::CORE)
      ->debug("GRPC: 'authorization' field contains '{}'", authorization);

  // Find ca_name token (if exists).
  std::string ca_name;
  it = cfg.params.find("ca_name");
  if (it == cfg.params.end())
    it = cfg.params.find("tls_hostname");
  if (it != cfg.params.end())
    ca_name = it->second;

  log_v2::instance()
      .get(log_v2::CORE)
      ->debug("GRPC: 'ca_name' field contains '{}'", ca_name);

  bool encryption = false;
  it = cfg.params.find("encryption");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtob(it->second, &encryption)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error("GRPC: 'encryption' field should be a boolean and not '{}'",
                  it->second);
      throw msg_fmt("GRPC: 'encryption' field should be a boolean and not '{}'",
                    it->second);
    }
  }

  // Find private key (if exists).
  std::string private_key;
  it = cfg.params.find("private_key");
  if (it != cfg.params.end()) {
    if (encryption) {
      try {
        private_key = read_file(it->second);
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::TLS),
                            "Failed to open private key file '{}': {}",
                            it->second, e.what());
        throw msg_fmt("Failed to open private key file '{}': {}", it->second,
                      e.what());
      }
    } else
      log_v2::instance()
          .get(log_v2::CORE)
          ->warn("GRPC: 'private_key' ignored since 'encryption' is disabled");
  }

  // Certificate.
  std::string certificate;
  it = cfg.params.find("certificate");
  if (it != cfg.params.end()) {
    if (encryption) {
      try {
        certificate = read_file(it->second);
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::TLS),
                            "Failed to open certificate file '{}': {}",
                            it->second, e.what());
        throw msg_fmt("Failed to open certificate file '{}': {}", it->second,
                      e.what());
      }
    } else
      log_v2::instance()
          .get(log_v2::CORE)
          ->warn("GRPC: 'certificate' ignored since 'encryption' is disabled");
  }

  // CA Certificate.
  std::string ca_certificate;
  it = cfg.params.find("ca_certificate");
  if (it != cfg.params.end()) {
    if (encryption) {
      try {
        ca_certificate = read_file(it->second);
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(
            log_v2::instance().get(log_v2::TLS),
            "Failed to open authority certificate file '{}': {}", it->second,
            e.what());
        throw msg_fmt("Failed to open authority certificate file '{}': {}",
                      it->second, e.what());
      }
    } else
      log_v2::instance()
          .get(log_v2::CORE)
          ->warn(
              "GRPC: 'ca_certificate' ignored since 'encryption' is disabled");
  }

  bool compression = false;
  it = cfg.params.find("compression");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtob(it->second, &compression))
      throw msg_fmt(
          "GRPC: 'compression' field should be a boolean and not '{}'",
          it->second);
  }

  bool enable_retention = false;
  it = cfg.params.find("retention");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtob(it->second, &enable_retention))
      throw msg_fmt("GRPC: 'retention' field should be a boolean and not '{}'",
                    it->second);
  }

  // keepalive conf
  int keepalive_interval = 30;
  it = cfg.params.find("keepalive_interval");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &keepalive_interval)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "GRPC: 'keepalive_interval' field should be an integer and not "
              "'{}'",
              it->second);
      throw msg_fmt(
          "GRPC: 'keepalive_interval' field should be an integer and not '{}'",
          it->second);
    }
  }

  std::string hostport;
  if (cfg.type == "bbdo_server") {
    is_acceptor = true;
    hostport = fmt::format("0.0.0.0:{}", port);
  } else {
    is_acceptor = false;
    hostport = fmt::format("{}:{}", host, port);
  }

  grpc_config::pointer conf(std::make_shared<grpc_config>(
      hostport, encryption, certificate, private_key, ca_certificate,
      authorization, ca_name, compression, keepalive_interval,
      direct_grpc_serialized(cfg)));

  // Acceptor.
  std::unique_ptr<io::endpoint> endp;
  if (is_acceptor) {
    log_v2::instance()
        .get(log_v2::CORE)
        ->debug("GRPC: encryption {} on gRPC server port {}",
                encryption ? "enabled" : "disabled", port);
    endp = std::make_unique<grpc::acceptor>(conf);
  }

  // Connector.
  else {
    log_v2::instance()
        .get(log_v2::CORE)
        ->debug("GRPC: encryption {} on gRPC client port {}",
                encryption ? "enabled" : "disabled", port);
    endp = std::make_unique<grpc::connector>(conf);
  }

  return endp.release();
}
