/**
 * Copyright 2011 - 2019-2024 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/tcp/factory.hh"

#include <absl/strings/match.h>

#include "com/centreon/broker/tcp/acceptor.hh"
#include "com/centreon/broker/tcp/connector.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tcp;
using namespace com::centreon::exceptions;
using com::centreon::common::log_v2::log_v2;

/**
 *  Check if a configuration supports this protocol.
 *  Possible endpoints are:
 *   * tcp
 *   * bbdo_server with 'tcp' transport_protocol
 *   * bbdo_client with 'tcp' transport_protocol
 *
 *  @param[in] cfg  Object configuration.
 *
 *  @return True if the configuration has this protocol.
 */
bool factory::has_endpoint(com::centreon::broker::config::endpoint& cfg,
                           io::extension* ext) {
  if (ext)
    *ext = io::extension("TCP", false, false);
  /* Legacy case: we create a tcp endpoint */
  if (cfg.type == "ip" || cfg.type == "tcp" || cfg.type == "ipv4" ||
      cfg.type == "ipv6")
    return true;

  /* New case: we create a bbdo_server or a bbdo_client with transport protocol
   * set to 'grpc' */
  if ((cfg.type == "bbdo_server" || cfg.type == "bbdo_client") &&
      absl::EqualsIgnoreCase(cfg.params["transport_protocol"], "tcp"))
    return true;

  return false;
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
  auto logger = log_v2::instance().get(log_v2::TCP);

  if (cfg.type == "bbdo_server" || cfg.type == "bbdo_client")
    return _new_endpoint_bbdo_cs(cfg, is_acceptor);

  // Find host (if exists).
  std::string host;
  auto it = cfg.params.find("host");
  if (it != cfg.params.end())
    host = it->second;
  if (!host.empty() &&
      (std::isspace(host[0]) || std::isspace(host[host.size() - 1]))) {
    logger->error(
        "TCP: 'host' must be a string matching a host, not beginning or "
        "ending with spaces for endpoint {}, it contains '{}'",
        cfg.name, host);
    throw msg_fmt(
        "TCP: invalid host value '{}' defined for endpoint '{}"
        "', it must not begin or end with spaces.",
        host, cfg.name);
  }

  // Find port (must exist).
  uint16_t port;
  it = cfg.params.find("port");
  if (it == cfg.params.end()) {
    logger->error("TCP: no 'port' defined for endpoint '{}'", cfg.name);
    throw msg_fmt("TCP: no 'port' defined for endpoint '{}'", cfg.name);
  }
  uint32_t port32;
  if (!absl::SimpleAtoi(it->second, &port32)) {
    logger->error(
        "TCP: 'port' must be an integer and not '{}' for endpoint '{}'",
        it->second, cfg.name);
    throw msg_fmt("TCP: invalid port value '{}' defined for endpoint '{}'",
                  it->second, cfg.name);
  }
  if (port32 > 65535)
    throw msg_fmt("TCP: invalid port value '{}' defined for endpoint '{}'",
                  it->second, cfg.name);
  else
    port = port32;

  int read_timeout(-1);
  it = cfg.params.find("socket_read_timeout");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &read_timeout)) {
      logger->error(
          "TCP: 'socket_read_timeout' must be an integer and not '{}' for "
          "endpoint '{}'",
          it->second, cfg.name);
      throw msg_fmt(
          "TCP: invalid socket read timeout value '{}' defined for endpoint "
          "'{}'",
          it->second, cfg.name);
    }
  }

  // keepalive conf
  int keepalive_count = 2;
  it = cfg.params.find("keepalive_count");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &keepalive_count)) {
      logger->error(
          "TCP: 'keepalive_count' field should be an integer and not '{}'",
          it->second);
      throw msg_fmt(
          "TCP: 'keepalive_count' field should be an integer and not '{}'",
          it->second);
    }
  }

  int keepalive_interval = 30;
  it = cfg.params.find("keepalive_interval");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &keepalive_interval)) {
      logger->error(
          "TCP: 'keepalive_interval' field should be an integer and not '{}'",
          it->second);
      throw msg_fmt(
          "TCP: 'keepalive_interval' field should be an integer and not '{}'",
          it->second);
    }
  }

  tcp_config::pointer conf(std::make_shared<tcp_config>(
      host, port, read_timeout, keepalive_interval, keepalive_count));

  // Acceptor.
  std::unique_ptr<io::endpoint> endp;
  if (host.empty())
    is_acceptor = true;
  // Connector.
  else
    is_acceptor = false;

  if (is_acceptor)
    endp = std::make_unique<tcp::acceptor>(conf);
  else
    endp = std::make_unique<tcp::connector>(conf);
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
  auto logger = log_v2::instance().get(log_v2::TCP);

  // Find host (if exists).
  std::string host;
  auto it = cfg.params.find("host");
  if (it != cfg.params.end())
    host = it->second;
  if (!host.empty() &&
      (std::isspace(host[0]) || std::isspace(host[host.size() - 1]))) {
    logger->error(
        "TCP: 'host' must be a string matching a host, not beginning or "
        "ending with spaces for endpoint {}, it contains '{}'",
        cfg.name, host);
    throw msg_fmt(
        "TCP: invalid host value '{}' defined for endpoint '{}"
        "', it must not begin or end with spaces.",
        host, cfg.name);
  } else if (host.empty()) {
    /* host is empty */
    if (cfg.type == "bbdo_server")
      host = "0.0.0.0";
    else {
      logger->error("TCP: you must specify a host to connect to.");
      throw msg_fmt("TCP: you must specify a host to connect to.");
    }
  }

  // Find port (must exist).
  uint16_t port;
  it = cfg.params.find("port");
  if (it == cfg.params.end()) {
    logger->error("TCP: no 'port' defined for endpoint '{}'", cfg.name);
    throw msg_fmt("TCP: no 'port' defined for endpoint '{}'", cfg.name);
  }
  {
    uint32_t port32;
    if (!absl::SimpleAtoi(it->second, &port32)) {
      logger->error(
          "TCP: 'port' must be an integer and not '{}' for endpoint '{}'",
          it->second, cfg.name);
      throw msg_fmt("TCP: invalid port value '{}' defined for endpoint '{}'",
                    it->second, cfg.name);
    }
    if (port32 > 65535) {
      logger->error("TCP: invalid port value '{}' defined for endpoint '{}'",
                    it->second, cfg.name);
      throw msg_fmt("TCP: invalid port value '{}' defined for endpoint '{}'",
                    it->second, cfg.name);
    } else
      port = port32;
  }

  // Find authorization token (if exists).
  it = cfg.params.find("authorization");
  if (it != cfg.params.end()) {
    logger->error(
        "TCP: 'authorization' token works only with gRPC transport protocol, "
        "you should fix that");
    throw msg_fmt(
        "TCP: 'authorization' token works only with gRPC transport protocol, "
        "you should fix that");
  }

  bool enable_retention = false;
  it = cfg.params.find("retention");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtob(it->second, &enable_retention)) {
      logger->error("TCP: 'retention' field should be a boolean and not '{}'",
                    it->second);
      throw msg_fmt("TCP: 'retention' field should be a boolean and not '{}'",
                    it->second);
    }
  }

  // Acceptor.
  std::unique_ptr<io::endpoint> endp;
  if (cfg.type == "bbdo_server")
    is_acceptor = true;
  else if (cfg.type == "bbdo_client")
    is_acceptor = false;

  int read_timeout = -1;

  // keepalive conf
  int keepalive_count = 2;
  it = cfg.params.find("keepalive_count");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &keepalive_count)) {
      logger->error(
          "TCP: 'keepalive_count' field should be an integer and not '{}'",
          it->second);
      throw msg_fmt(
          "TCP: 'keepalive_count' field should be an integer and not '{}'",
          it->second);
    }
  }

  int keepalive_interval = 30;
  it = cfg.params.find("keepalive_interval");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &keepalive_interval)) {
      logger->error(
          "TCP: 'keepalive_interval' field should be an integer and not '{}'",
          it->second);
      throw msg_fmt(
          "TCP: 'keepalive_interval' field should be an integer and not '{}'",
          it->second);
    }
  }

  tcp_config::pointer conf(std::make_shared<tcp_config>(
      host, port, read_timeout, keepalive_interval, keepalive_count));

  if (is_acceptor)
    endp = std::make_unique<tcp::acceptor>(conf);
  else
    endp = std::make_unique<tcp::connector>(conf);
  return endp.release();
}
