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

#ifndef CENTREON_AGENT_STREAMING_CLIENT_HH
#define CENTREON_AGENT_STREAMING_CLIENT_HH

#include "com/centreon/common/grpc/grpc_client.hh"

#include "bireactor.hh"
#include "scheduler.hh"

namespace com::centreon::agent {

class streaming_client;

class client_reactor
    : public bireactor<
          ::grpc::ClientBidiReactor<MessageFromAgent, MessageToAgent>> {
  std::weak_ptr<streaming_client> _parent;
  ::grpc::ClientContext _context;

 public:
  client_reactor(const std::shared_ptr<boost::asio::io_context>& io_context,
                 const std::shared_ptr<spdlog::logger>& logger,
                 const std::shared_ptr<streaming_client>& parent,
                 const std::string& peer);

  std::shared_ptr<client_reactor> shared_from_this() {
    return std::static_pointer_cast<client_reactor>(
        bireactor<::grpc::ClientBidiReactor<MessageFromAgent, MessageToAgent>>::
            shared_from_this());
  }

  ::grpc::ClientContext& get_context() { return _context; }

  void on_incomming_request(
      const std::shared_ptr<MessageToAgent>& request) override;

  void on_error() override;

  void shutdown() override;
};

/**
 * @brief this object not only manages connection to engine, but also embed
 * check scheduler
 *
 */
class streaming_client : public common::grpc::grpc_client_base,
                         public std::enable_shared_from_this<streaming_client> {
  std::shared_ptr<boost::asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;
  std::string _supervised_host;

  std::unique_ptr<AgentService::Stub> _stub;

  std::shared_ptr<client_reactor> _reactor;
  std::shared_ptr<scheduler> _sched;

  /**
   * @brief All attributes of this object are protected by this mutex
   *
   */
  std::mutex _protect;

  void _create_reactor();

  void _start();

  void _send(const std::shared_ptr<MessageFromAgent>& request);

 public:
  streaming_client(const std::shared_ptr<boost::asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const std::shared_ptr<common::grpc::grpc_config>& conf,
                   const std::string& supervised_host);

  static std::shared_ptr<streaming_client> load(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      const std::shared_ptr<common::grpc::grpc_config>& conf,
      const std::string& supervised_host);

  void on_incomming_request(const std::shared_ptr<client_reactor>& caller,
                            const std::shared_ptr<MessageToAgent>& request);
  void on_error(const std::shared_ptr<client_reactor>& caller);

  void shutdown();

  // use only for tests
  engine_to_agent_request_ptr get_last_message_to_agent() const {
    return _sched->get_last_message_to_agent();
  }
};

}  // namespace com::centreon::agent

#endif