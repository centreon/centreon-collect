/**
 * Copyright 2026 Centreon
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

#include "com/centreon/broker/otlp/factory.hh"

#include "broker/core/cache/broker_cache.hh"
#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/otlp/connector.hh"
#include "com/centreon/common/file_system.hh"
#include "com/centreon/broker/persistent_cache.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;
using com::centreon::exceptions::msg_fmt;
using log_v2 = com::centreon::common::log_v2::log_v2;

namespace {

/**
 * @brief String parameter of an endpoint.
 *
 * @param cfg endpoint configuration
 * @param key parameter name
 * @param def value when the parameter is absent
 * @return the parameter value, or def
 */
std::string get_string(const config::endpoint& cfg,
                       const std::string& key,
                       const std::string& def) {
  auto it = cfg.params.find(key);
  return it == cfg.params.end() ? def : it->second;
}

/**
 * @brief Unsigned integer parameter of an endpoint.
 *
 * @param cfg endpoint configuration
 * @param key parameter name
 * @param def value when the parameter is absent
 * @return the parameter value, or def
 * @throw msg_fmt if the value is not a number
 */
uint32_t get_uint(const config::endpoint& cfg,
                  const std::string& key,
                  uint32_t def) {
  auto it = cfg.params.find(key);
  if (it == cfg.params.end())
    return def;
  uint32_t out = 0;
  if (!absl::SimpleAtoi(it->second, &out))
    throw msg_fmt("otlp: '{}' must be numeric for endpoint '{}'", key,
                  cfg.name);
  return out;
}

/**
 * @brief Boolean parameter of an endpoint.
 *
 * @param cfg endpoint configuration
 * @param key parameter name
 * @param def value when the parameter is absent
 * @return the parameter value, or def
 * @throw msg_fmt if the value is not a boolean
 */
bool get_bool(const config::endpoint& cfg, const std::string& key, bool def) {
  auto it = cfg.params.find(key);
  if (it == cfg.params.end())
    return def;
  bool out = def;
  if (!absl::SimpleAtob(it->second, &out))
    throw msg_fmt("otlp: '{}' must be a boolean for endpoint '{}'", key,
                  cfg.name);
  return out;
}

/**
 * @brief Read the PEM file named by a TLS parameter of an endpoint.
 *
 * @param cfg endpoint configuration
 * @param key parameter name, its value is a file path
 * @return the file content, or an empty string if the parameter is absent or
 * empty
 * @throw msg_fmt if the file cannot be read
 */
std::string read_tls_file(const config::endpoint& cfg, const std::string& key) {
  const std::string path = get_string(cfg, key, "");
  if (path.empty())
    return {};
  try {
    return com::centreon::common::read_file_content(path);
  } catch (const std::exception& e) {
    throw msg_fmt("otlp: cannot read '{}' for endpoint '{}': {}", key, cfg.name,
                  e.what());
  }
}

}  // namespace

/**
 * @brief Tell whether an endpoint configuration is an OTLP output.
 *
 * @param cfg endpoint configuration
 * @param ext if not null, set to the OTLP extension
 * @return true if the type is "otlp" (case-insensitive)
 */
bool factory::has_endpoint(const config::endpoint& cfg,
                           io::extension* ext) const {
  if (ext)
    *ext = io::extension("OTLP", false, false);
  return absl::EqualsIgnoreCase(cfg.type, "otlp");
}

/**
 * @brief Build the configuration of an OTLP endpoint from its parameters and
 * their default values.
 *
 * @param cfg endpoint configuration
 * @return the parsed configuration
 * @throw msg_fmt if endpoint is missing, a value is malformed, or
 * max_datapoints_per_batch or max_inflight_requests is 0
 */
otlp_config::pointer factory::parse_config(const config::endpoint& cfg) {
  auto conf = std::make_shared<otlp_config>();

  const std::string endpoint = get_string(cfg, "endpoint", "");
  if (endpoint.empty())
    throw msg_fmt("otlp: no 'endpoint' defined for endpoint '{}'", cfg.name);

  const bool encryption = get_bool(cfg, "encryption", false);
  // The TLS parameters are file paths, as in the gRPC module; they are only
  // read when encryption is on.
  std::string certificate, private_key, ca_certificate;
  if (encryption) {
    certificate = read_tls_file(cfg, "certificate");
    private_key = read_tls_file(cfg, "private_key");
    ca_certificate = read_tls_file(cfg, "ca_certificate");
  }
  conf->grpc = std::make_shared<com::centreon::common::grpc::grpc_config>(
      endpoint, encryption, certificate, private_key, ca_certificate,
      get_string(cfg, "ca_name", ""), get_bool(cfg, "compression", false),
      static_cast<int>(get_uint(cfg, "keepalive_interval", 30)));

  conf->max_datapoints_per_batch =
      get_uint(cfg, "max_datapoints_per_batch", 5000);
  conf->max_send_interval = get_uint(cfg, "max_send_interval", 10);
  conf->max_inflight_requests = get_uint(cfg, "max_inflight_requests", 4);
  conf->export_timeout = get_uint(cfg, "export_timeout", 30);

  conf->mapping_file = get_string(cfg, "mapping_file", "");

  // On default
  conf->send_thresholds = get_bool(cfg, "send_thresholds", true);
  conf->send_status = get_bool(cfg, "send_status", true);
  conf->send_min_max = get_bool(cfg, "send_min_max", true);

  conf->host_ip_exclude_link_local =
      get_bool(cfg, "host_ip_exclude_link_local", false);

  if (conf->max_datapoints_per_batch == 0)
    throw msg_fmt("otlp: 'max_datapoints_per_batch' must be > 0 for '{}'",
                  cfg.name);
  if (conf->max_inflight_requests == 0)
    throw msg_fmt("otlp: 'max_inflight_requests' must be > 0 for '{}'",
                  cfg.name);

  return conf;
}

/**
 * @brief Enable the persistent cache of the endpoint, whatever the "cache"
 * setting: the connector keeps the CMA host information in it.
 *
 * @param cfg endpoint configuration
 */
void factory::set_default_values(config::endpoint& cfg) const {
  cfg.cache_enabled = true;
}

/**
 * @brief Create the connector of an OTLP output.
 *
 * @param cfg endpoint configuration
 * @param global_params unused
 * @param is_acceptor set to false: an OTLP output is always a connector
 * @return the new connector. If the endpoint has its cache enabled, the
 * connector gets a persistent cache that keeps the CMA host information
 * across restarts.
 * @throw msg_fmt if the configuration is invalid or the mapping file can't be
 * loaded
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const absl::btree_map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor) const {
  is_acceptor = false;

  auto conf = parse_config(cfg);

  /* The endpoints do not receive a persistent cache anymore, the OTLP output
   * creates its own one, as config::applier::endpoint did before. Guarded
   * because the applier state is not loaded under unit tests. */
  std::shared_ptr<persistent_cache> cache;
  if (cfg.cache_enabled && config::applier::state::loaded())
    cache = std::make_shared<persistent_cache>(
        fmt::format("{}.cache.{}",
                    config::applier::state::instance().cache_dir(), cfg.name),
        log_v2::instance().get(log_v2::OTL));

  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::OTL),
                     "otlp: endpoint '{}' exporting to {}", cfg.name,
                     conf->grpc->get_hostport());

  return new connector(conf, std::move(cache));
}
