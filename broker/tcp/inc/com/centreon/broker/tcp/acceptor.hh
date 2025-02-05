/**
 * Copyright 2011-2013 Centreon
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

#ifndef CCB_TCP_ACCEPTOR_HH
#define CCB_TCP_ACCEPTOR_HH

#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/tcp/tcp_config.hh"

namespace com::centreon::broker::tcp {

/**
 *  @class acceptor acceptor.hh "com/centreon/broker/tcp/acceptor.hh"
 *  @brief TCP acceptor.
 *
 *  Accept TCP connections.
 */
class acceptor : public io::endpoint {
  tcp_config::pointer _conf;

  absl::flat_hash_set<std::string> _children;
  std::mutex _childrenm;
  std::shared_ptr<asio::ip::tcp::acceptor> _acceptor;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  acceptor(const tcp_config::pointer& conf);
  ~acceptor() noexcept;

  acceptor(const acceptor&) = delete;
  acceptor& operator=(const acceptor&) = delete;

  void add_child(std::string const& child);
  std::shared_ptr<io::stream> open() override;
  void remove_child(std::string const& child);
  void stats(nlohmann::json& tree) override;
  bool is_ready() const override;
};

}  // namespace com::centreon::broker::tcp

#endif  // !CCB_TCP_ACCEPTOR_HH
