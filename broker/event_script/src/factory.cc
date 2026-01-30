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

#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/str_join.h>

#include "com/centreon/broker/event_script/connector.hh"
#include "com/centreon/broker/event_script/factory.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::event_script;
using namespace nlohmann;
using namespace com::centreon::exceptions;

/**
 *  Check if a configuration match the event_script layer.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  @return True if the configuration matches the event_script layer.
 */
bool factory::has_endpoint(const config::endpoint& cfg,
                           io::extension* ext) const {
  if (ext)
    *ext = io::extension("EVENT_SCRIPT", false, false);
  return absl::EqualsIgnoreCase(cfg.type, "event_script");
}

/**
 *  Build a storage endpoint from a configuration.
 *
 *  @param[in]  cfg         Endpoint configuration.
 *  @param[out] is_acceptor Will be set to false.
 *  @param[in]  cache       Unused.
 *
 *  @return Endpoint matching the given configuration.
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache) const {
  auto it = cfg.params.find("script_path");
  if (it == cfg.params.end()) {
    throw msg_fmt("No script_path parameter");
  }

  auto script_path = absl::StripAsciiWhitespace(it->second);

  unsigned managed_event_ttl(3600);
  it = cfg.params.find("managed_event_ttl");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &managed_event_ttl)) {
      throw msg_fmt(
          "event_script: couldn't parse managed_event_ttl '{}' defined "
          "for "
          "endpoint '{}'",
          it->second, cfg.name);
    }
  }

  unsigned timeout = 15;
  it = cfg.params.find("timeout");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtoi(it->second, &timeout)) {
      throw msg_fmt(
          "event_script: couldn't parse minute_managed_event_ttl '{}' defined "
          "for "
          "endpoint '{}'",
          it->second, cfg.name);
    }
  }

  if (cfg.write_filters.size() != 1) {
    throw msg_fmt("event_script allows only one mandatory read filter: {}",
                  absl::StrJoin(cfg.write_filters, " "));
  }

  const io::events::events_container& filter(
      io::events::instance().get_matching_events(*cfg.write_filters.begin()));

  if (filter.size() != 1) {
    throw msg_fmt("event_script allows only one mandatory read filter: {}",
                  *cfg.write_filters.begin());
  }
  // Connector.
  std::unique_ptr<event_script::connector> c(new event_script::connector);
  c->connect_to(script_path, std::chrono::seconds(managed_event_ttl),
                std::chrono::seconds(timeout));
  is_acceptor = false;
  return c.release();
}
