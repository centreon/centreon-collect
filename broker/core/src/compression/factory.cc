/**
 * Copyright 2011-2013, 2022 Centreon
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

#include "com/centreon/broker/compression/factory.hh"

#include <absl/strings/match.h>

#include "com/centreon/broker/compression/opener.hh"
#include "com/centreon/broker/compression/stream.hh"
#include "com/centreon/broker/config/parser.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::compression;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Check if an endpoint configuration match the compression layer.
 *
 *  @param[in] cfg  Configuration object.
 *  @param[out] flag Returns no, maybe or yes, corresponding to the no, auto,
 *                   yes configured in the configuration file.
 *
 *  @return False everytime because the compression layer must not be set at
 *  the broker configuration. This avoids the compression while the negotiation
 *  is running. We will be able to add this endpoint later, following the flag
 *  value.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  if (ext) {
    if (direct_grpc_serialized(cfg)) {
      return false;
    }
    if (cfg.type == "bbdo_server" || cfg.type == "bbdo_client") {
      auto it = cfg.params.find("transport_protocol");
      if (it != cfg.params.end()) {
        if (absl::EqualsIgnoreCase(cfg.params["transport_protocol"], "grpc")) {
          *ext = io::extension("COMPRESSION", false, false);
          return false;
        }
      }

      it = cfg.params.find("compression");
      bool has_compression;
      if (it == cfg.params.end())
        has_compression = false;
      else if (!absl::SimpleAtob(it->second, &has_compression)) {
        log_v2::instance()
            .get(log_v2::CORE)
            ->error(
                "TLS: the field 'compression' in endpoint '{}' should be a "
                "boolean",
                cfg.name);
        has_compression = false;
      }

      if (cfg.get_io_type() == config::endpoint::output) {
        if (!has_compression)
          *ext = io::extension("COMPRESSION", false, false);
        else
          *ext = io::extension("COMPRESSION", false, true);
      } else
        *ext = io::extension("COMPRESSION", true, false);
    } else {
      /* legacy case */
      auto it = cfg.params.find("compression");
      bool has_compression;
      if (it == cfg.params.end())
        has_compression = false;
      else if (!absl::SimpleAtob(it->second, &has_compression)) {
        if (absl::EqualsIgnoreCase(it->second, "auto"))
          has_compression = true;
        else {
          log_v2::instance()
              .get(log_v2::CORE)
              ->error(
                  "TLS: the field 'compression' in endpoint '{}' should be a "
                  "boolean",
                  cfg.name);
          has_compression = false;
        }
      }

      if (!has_compression)
        *ext = io::extension("COMPRESSION", false, false);
      else if (absl::EqualsIgnoreCase(it->second, "auto"))
        *ext = io::extension("COMPRESSION", true, false);
      else
        *ext = io::extension("COMPRESSION", false, true);
    }
  }
  return false;
}

/**
 *  Create an endpoint matching the configuration object.
 *
 *  @param[in]  cfg         Configuration object.
 *  @param[out] is_acceptor Unused.
 *  @param[in]  cache       cache
 *
 *  @return New endpoint object.
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor [[maybe_unused]],
    std::shared_ptr<persistent_cache> cache [[maybe_unused]]) const {
  // Get compression level.
  int level{-1};
  auto it = cfg.params.find("compression_level");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &level)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "compression: the 'compression_level' should be an integer and "
              "not "
              "'{}'",
              it->second);
      level = -1;
    }
  }

  // Get buffer size.
  uint32_t size = 0;
  it = cfg.params.find("compression_buffer");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &size)) {
      log_v2::instance()
          .get(log_v2::CORE)
          ->error(
              "compression: compression_buffer is the size of the compression "
              "buffer represented by an integer and not '{}'",
              it->second);
      size = 0;
    }
  }

  // Create compression object.
  auto openr{std::make_unique<compression::opener>(level, size)};
  return openr.release();
}

/**
 *  Create a new compression stream.
 *
 *  @param[in] to          Lower-layer stream.
 *  @param[in] is_acceptor Unused.
 *  @param[in] proto_name  Unused.
 *
 *  @return New compression stream.
 */
std::shared_ptr<io::stream> factory::new_stream(
    std::shared_ptr<io::stream> to,
    bool is_acceptor,
    const std::unordered_map<std::string, std::string>& options) {
  (void)is_acceptor;
  (void)options;
  std::shared_ptr<io::stream> s{std::make_shared<stream>()};
  s->set_substream(to);
  return s;
}
