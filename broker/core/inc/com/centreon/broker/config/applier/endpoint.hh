/**
 * Copyright 2011-2012,2015-2024 Centreon
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

#ifndef CCB_CONFIG_APPLIER_ENDPOINT_HH
#define CCB_CONFIG_APPLIER_ENDPOINT_HH

#include "com/centreon/broker/multiplexing/muxer_filter.hh"

namespace com::centreon::broker {

// Forward declarations.
namespace io {
class endpoint;
}
namespace multiplexing {
class muxer;
}
namespace processing {
class failover;
class endpoint;
}  // namespace processing

namespace config {
// Forward declaration.
class endpoint;

namespace applier {
/**
 *  @class endpoint endpoint.hh "com/centreon/broker/config/applier/endpoint.hh"
 *  @brief Apply the configuration of endpoints.
 *
 *  Apply the configuration of the configured endpoints.
 */
class endpoint {
  std::map<config::endpoint, processing::endpoint*> _endpoints;
  std::timed_mutex _endpointsm;
  std::atomic_bool _discarding;

  std::shared_ptr<spdlog::logger> _logger;

  endpoint();
  ~endpoint();
  void _discard();
  processing::failover* _create_failover(
      config::endpoint& cfg,
      const std::map<std::string, std::string>& global_params,
      std::shared_ptr<multiplexing::muxer> mux,
      std::shared_ptr<io::endpoint> endp,
      std::list<config::endpoint>& l);
  std::shared_ptr<io::endpoint> _create_endpoint(
      config::endpoint& cfg,
      const std::map<std::string, std::string>& global_params,
      bool& is_acceptor);
  void _diff_endpoints(
      std::map<config::endpoint, processing::endpoint*> const& current,
      std::list<config::endpoint> const& new_endpoints,
      std::list<config::endpoint>& to_create,
      std::list<config::endpoint>& to_delete);

 public:
  typedef std::map<config::endpoint, processing::endpoint*>::iterator iterator;

  endpoint& operator=(const endpoint&) = delete;
  endpoint(const endpoint&) = delete;
  void apply(std::list<config::endpoint> const& endpoints,
             const std::map<std::string, std::string>& global_params);
  iterator endpoints_begin();
  iterator endpoints_end();
  std::timed_mutex& endpoints_mutex();
  static endpoint& instance();
  static void load();
  static void unload();
  static bool loaded();

  static multiplexing::muxer_filter parse_filters(
      const std::set<std::string>& str_filters,
      const multiplexing::muxer_filter& forbidden_filter);
};
}  // namespace applier
}  // namespace config

}  // namespace com::centreon::broker

#endif  // !CCB_CONFIG_APPLIER_ENDPOINT_HH
