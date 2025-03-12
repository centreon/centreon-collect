/**
 * Copyright 2011-2013, 2020-2021 Centreon
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

#ifndef CCB_TCP_CONNECTOR_HH
#define CCB_TCP_CONNECTOR_HH

#include "com/centreon/broker/io/limit_endpoint.hh"
#include "com/centreon/broker/tcp/tcp_config.hh"

namespace com::centreon::broker {

namespace tcp {
/**
 *  @class connector connector.hh "com/centreon/broker/tcp/connector.hh"
 *  @brief TCP connector.
 *
 *  Connect to some remote TCP host.
 */
class connector : public io::limit_endpoint {
  tcp_config::pointer _conf;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  connector(const tcp_config::pointer& conf);
  ~connector() noexcept = default;

  connector& operator=(const connector&) = delete;
  connector(const connector&) = delete;

  std::shared_ptr<io::stream> open() override;

  std::shared_ptr<io::stream> create_stream() override;
};
}  // namespace tcp

}  // namespace com::centreon::broker

#endif  // !CCB_TCP_CONNECTOR_HH
