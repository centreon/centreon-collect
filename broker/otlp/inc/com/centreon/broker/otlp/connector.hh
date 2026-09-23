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

#ifndef CCB_OTLP_CONNECTOR_HH
#define CCB_OTLP_CONNECTOR_HH

#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/otlp/otlp_config.hh"

namespace com::centreon::broker::otlp {

/**
 * @brief Endpoint creating OTLP output streams.
 *
 * The muxer filters passed to io::endpoint are what subscribe this module to
 * service and host status events.
 */
class connector : public io::endpoint {
  const otlp_config::pointer _conf;

 public:
  explicit connector(const otlp_config::pointer& conf);
  ~connector() noexcept override = default;
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;

  std::shared_ptr<io::stream> open() override;
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_CONNECTOR_HH
