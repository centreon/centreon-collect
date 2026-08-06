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

#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/otlp/connector.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;
using com::centreon::exceptions::msg_fmt;
using log_v2 = com::centreon::common::log_v2::log_v2;

namespace {

std::string get_string(const config::endpoint& cfg,
                       const std::string& key,
                       const std::string& def) {
  auto it = cfg.params.find(key);
  return it == cfg.params.end() ? def : it->second;
}

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

bool get_bool(const config::endpoint& cfg,
              const std::string& key,
              bool def) {
  auto it = cfg.params.find(key);
  if (it == cfg.params.end())
    return def;
  bool out = def;
  if (!absl::SimpleAtob(it->second, &out))
    throw msg_fmt("otlp: '{}' must be a boolean for endpoint '{}'", key,
                  cfg.name);
  return out;
}

}  // namespace

bool factory::has_endpoint(const config::endpoint& cfg,
                           io::extension* ext) const {
  if (ext)
    *ext = io::extension("OTLP", false, false);
  return absl::EqualsIgnoreCase(cfg.type, "otlp");
}

otlp_config::pointer factory::parse_config(const config::endpoint& cfg) {
  auto conf = std::make_shared<otlp_config>();

  const std::string endpoint = get_string(cfg, "endpoint", "");
  if (endpoint.empty())
    throw msg_fmt("otlp: no 'endpoint' defined for endpoint '{}'", cfg.name);

  const bool encryption = get_bool(cfg, "encryption", false);
  conf->grpc = std::make_shared<com::centreon::common::grpc::grpc_config>(
      endpoint, encryption, get_string(cfg, "certificate", ""),
      get_string(cfg, "private_key", ""),
      get_string(cfg, "ca_certificate", ""), get_string(cfg, "ca_name", ""),
      get_bool(cfg, "compression", false),
      static_cast<int>(get_uint(cfg, "keepalive_interval", 30)));

  conf->max_datapoints_per_batch =
      get_uint(cfg, "max_datapoints_per_batch", 5000);
  conf->max_send_interval = get_uint(cfg, "max_send_interval", 10);
  conf->max_inflight_requests = get_uint(cfg, "max_inflight_requests", 4);
  conf->export_timeout = get_uint(cfg, "export_timeout", 30);

  /* On by default: the study calls for thresholds, status and bounds to be
   * exported unless explicitly disabled. */
  conf->send_thresholds = get_bool(cfg, "send_thresholds", true);
  conf->send_status = get_bool(cfg, "send_status", true);
  conf->send_min_max = get_bool(cfg, "send_min_max", true);

  if (conf->max_datapoints_per_batch == 0)
    throw msg_fmt("otlp: 'max_datapoints_per_batch' must be > 0 for '{}'",
                  cfg.name);
  if (conf->max_inflight_requests == 0)
    throw msg_fmt("otlp: 'max_inflight_requests' must be > 0 for '{}'",
                  cfg.name);

  return conf;
}

io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache [[maybe_unused]]) const {
  is_acceptor = false;

  auto conf = parse_config(cfg);

  /* host.name resolution needs the persistent cache. Guarded because the
   * applier state is not loaded under unit tests. */
  if (config::applier::state::loaded())
    cache::global_cache::load(
        com::centreon::common::pool::io_context_ptr(),
        config::applier::state::instance().cache_dir() + ".cache.global");

  SPDLOG_LOGGER_INFO(log_v2::instance().get(log_v2::OTL),
                     "otlp: endpoint '{}' exporting to {}", cfg.name,
                     conf->grpc->get_hostport());

  return new connector(conf);
}
