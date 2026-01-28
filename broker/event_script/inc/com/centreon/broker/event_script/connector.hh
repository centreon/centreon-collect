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

#ifndef CCB_EVENT_SCRIPT_CONNECTOR_HH
#define CCB_EVENT_SCRIPT_CONNECTOR_HH

#include "com/centreon/broker/io/endpoint.hh"

namespace com::centreon::broker::event_script {

/**
 *  @class connector connector.hh "com/centreon/broker/influxdb/connector.hh"
 *  @brief Connect to an influxdb stream.
 */
class connector : public io::endpoint {
  std::string _script_path;
  std::chrono::system_clock::duration _managed_event_ttl;

 public:
  connector();
  ~connector() noexcept = default;
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;
  void connect_to(const std::string_view& script_path,
                  std::chrono::system_clock::duration managed_event_ttl);
  std::shared_ptr<io::stream> open() override;
};

}  // namespace com::centreon::broker::event_script

#endif  // !CCB_INFLUXDB_CONNECTOR_HH
