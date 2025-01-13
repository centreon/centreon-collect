/**
 * Copyright 2013,2015,2017, 2021-2023 Centreon
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

#ifndef CCB_BBDO_CONNECTOR_HH
#define CCB_BBDO_CONNECTOR_HH

#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/io/extension.hh"

namespace com::centreon::broker::bbdo {

/**
 *  @class connector connector.hh "broker/core/bbdo/connector.hh"
 *  @brief BBDO connector.
 *
 *  Initiate direct BBDO protocol connections.
 */
class connector : public io::endpoint {
  bool _is_input;
  bool _coarse;
  bool _negotiate;
  time_t _timeout;
  uint32_t _ack_limit;
  std::list<std::shared_ptr<io::extension>> _extensions;
  const bool _grpc_serialized;

  std::shared_ptr<io::stream> _open(std::shared_ptr<io::stream> stream);

 public:
  connector(bool negotiate, time_t timeout, bool connector_is_input,
            bool coarse = false, uint32_t ack_limit = 1000,
            std::list<std::shared_ptr<io::extension>>&& extensions = {},
            bool grpc_serialized = false);
  ~connector() noexcept = default;
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;
  std::shared_ptr<io::stream> open() override;
};
}  // namespace com::centreon::broker::bbdo

#endif  // !CCB_BBDO_CONNECTOR_HH
