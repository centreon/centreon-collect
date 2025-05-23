/**
 * Copyright 2013,2015,2017-2023 Centreon
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

#ifndef CCB_BBDO_ACCEPTOR_HH
#define CCB_BBDO_ACCEPTOR_HH

#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/io/extension.hh"

namespace com::centreon::broker {

// Forward declaration.
namespace processing {
class thread;
}

namespace bbdo {
// Forward declaration.
class stream;

/**
 *  @class acceptor acceptor.hh "broker/core/bbdo/acceptor.hh"
 *  @brief BBDO acceptor.
 *
 *  Accept incoming BBDO connections.
 */
class acceptor : public io::endpoint {
  bool _coarse;
  std::string _name;
  bool _negotiate;
  const bool _is_output;
  time_t _timeout;
  uint32_t _ack_limit;
  std::list<std::shared_ptr<io::extension>> _extensions;
  const bool _grpc_serialized;

 public:
  acceptor(std::string name, bool negotiate, time_t timeout,
           bool one_peer_retention_mode = false, bool coarse = false,
           uint32_t ack_limit = 1000,
           std::list<std::shared_ptr<io::extension>>&& extensions = {},
           bool grpc_serialized = false);
  ~acceptor() noexcept;
  acceptor(const acceptor&) = delete;
  acceptor& operator=(const acceptor&) = delete;
  std::shared_ptr<io::stream> open() override;
  void stats(nlohmann::json& tree) override;
  bool is_output() const { return _is_output; }

 private:
  uint32_t _negotiate_features(std::shared_ptr<io::stream> stream,
                               std::shared_ptr<bbdo::stream> my_bbdo);
  void _open(std::shared_ptr<io::stream> stream);
};
}  // namespace bbdo

}  // namespace com::centreon::broker

#endif  // !CCB_BBDO_ACCEPTOR_HH
