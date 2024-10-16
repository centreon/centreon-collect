/**
 * Copyright 2009-2013,2015-2023 Centreon
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

#ifndef CCB_CONFIG_ENDPOINT_HH
#define CCB_CONFIG_ENDPOINT_HH


#include "com/centreon/broker/multiplexing/muxer.hh"

namespace com::centreon::broker::config {
/**
 *  @class endpoint endpoint.hh "com/centreon/broker/config/endpoint.hh"
 *  @brief Hold configuration of an endpoint.
 *
 *  An endpoint is an external source or destination for events.
 *  This can either be an XML stream, a database, a file, ...
 *  This class holds the configuration of an endpoint.
 */
class endpoint {
 public:
  enum io_type {
    input,   // the endpoint is an input (marked as is in the config)
    output,  // the endpoint is an output (marked as is in the config)
  };

 private:
  const io_type _type;

 public:
  time_t buffering_timeout;
  std::list<std::string> failovers;
  std::string name;
  std::map<std::string, std::string> params;
  std::set<std::string> read_filters;
  time_t read_timeout;
  uint32_t retry_interval;
  std::string type;
  std::set<std::string> write_filters;
  bool cache_enabled;
  nlohmann::json cfg;

  endpoint() = delete;
  endpoint(io_type way);
  endpoint(const endpoint& other);
  ~endpoint() noexcept = default;
  endpoint& operator=(const endpoint& other);
  bool operator==(const endpoint& other) const;
  bool operator!=(const endpoint& other) const;
  bool operator<(const endpoint& other) const;

  io_type get_io_type() const { return _type; }
};
}  // namespace com::centreon::broker::config

#endif  // !CCB_CONFIG_ENDPOINT_HH
