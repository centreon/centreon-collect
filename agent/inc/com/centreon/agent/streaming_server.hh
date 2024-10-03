/**
 * Copyright 2024 Centreon
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

#ifndef CENTREON_AGENT_STREAMING_SERVER_HH
#define CENTREON_AGENT_STREAMING_SERVER_HH

#include "com/centreon/common/grpc/grpc_server.hh"

#include "bireactor.hh"
#include "scheduler.hh"

namespace com::centreon::agent {

class server_reactor;

/**
 * @brief grpc engine to agent server (reverse connection)
 * It accept only one connection at a time
 * If another connection occurs, previous connection is shutdown
 * This object is both grpc server and grpc service
 */
class streaming_server : public common::grpc::grpc_server_base,
                         public std::enable_shared_from_this<streaming_server>,
                         public ReversedAgentService::Service {
  std::shared_ptr<boost::asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  const std::string _supervised_host;

  /** active engine to agent connection*/
  std::shared_ptr<server_reactor> _incoming;

  /**
   * @brief All attributes of this object are protected by this mutex
   *
   */
  mutable std::mutex _protect;

  void _start();

 public:
  streaming_server(const std::shared_ptr<boost::asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const std::shared_ptr<common::grpc::grpc_config>& conf,
                   const std::string& supervised_host);

  ~streaming_server();

  static std::shared_ptr<streaming_server> load(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::shared_ptr<common::grpc::grpc_config>& conf,
      const std::string& supervised_host);

  ::grpc::ServerBidiReactor<MessageToAgent, MessageFromAgent>* Import(
      ::grpc::CallbackServerContext* context);

  void shutdown();
};

}  // namespace com::centreon::agent

#endif
