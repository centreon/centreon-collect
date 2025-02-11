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

#ifndef CENTREON_AGENT_BIREACTOR_HH
#define CENTREON_AGENT_BIREACTOR_HH

#include "agent.grpc.pb.h"

namespace com::centreon::agent {

template <class bireactor_class>
class bireactor
    : public bireactor_class,
      public std::enable_shared_from_this<bireactor<bireactor_class>> {
 private:
  /**
   * @brief we store reactor instances in this container until OnDone is called
   * by grpc layers. We allocate this container and never free this because
   * threads terminate in unknown order.
   */
  static std::set<std::shared_ptr<bireactor>>* _instances;
  static std::mutex _instances_m;

  bool _write_pending;
  std::deque<std::shared_ptr<MessageFromAgent>> _write_queue;
  std::shared_ptr<MessageToAgent> _read_current;

  const std::string_view _class_name;

  const std::string _peer;

 protected:
  std::shared_ptr<boost::asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  bool _alive;
  /**
   * @brief All attributes of this object are protected by this mutex
   *
   */
  mutable std::mutex _protect;

 public:
  bireactor(const std::shared_ptr<boost::asio::io_context>& io_context,
            const std::shared_ptr<spdlog::logger>& logger,
            const std::string_view& class_name,
            const std::string& peer);

  virtual ~bireactor();

  static void register_stream(const std::shared_ptr<bireactor>& strm);

  void start_read();

  void start_write();
  void write(const std::shared_ptr<MessageFromAgent>& request);

  // bireactor part
  void OnReadDone(bool ok) override;

  virtual void on_incomming_request(
      const std::shared_ptr<MessageToAgent>& request) = 0;

  virtual void on_error() = 0;

  void OnWriteDone(bool ok) override;

  // server version
  void OnDone();
  // client version
  void OnDone(const ::grpc::Status& /*s*/);

  virtual void shutdown();
};

}  // namespace com::centreon::agent

#endif
