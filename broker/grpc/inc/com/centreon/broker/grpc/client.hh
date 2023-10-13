/**
 * Copyright 2022 Centreon
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

#ifndef CCB_GRPC_CLIENT_HH
#define CCB_GRPC_CLIENT_HH

#include "channel.hh"
#include "grpc_stream.grpc.pb.h"

namespace com::centreon::broker {

namespace grpc {
/**
 * @brief definition of client interface
 * client is asynchronous
 *
 */
class client : public channel,
               public ::grpc::ClientBidiReactor<
                   ::com::centreon::broker::stream::CentreonEvent,
                   ::com::centreon::broker::stream::CentreonEvent> {
  channel_ptr _channel;
  std::unique_ptr<com::centreon::broker::stream::centreon_bbdo::Stub> _stub;
  std::unique_ptr<::grpc::ClientContext> _context;
  bool _hold_to_remove;
  // recursive_mutex is mandatory as we don't know if grpc layers can do a
  // direct call from StartWrite to OnWriteDone
  std::recursive_mutex _hold_mutex;

 protected:
  client& operator=(const client&) = delete;
  client(const client&) = delete;

  client(const grpc_config::pointer& conf);

  void start_read(event_ptr& to_read, bool first_read) override;
  void start_write(const event_with_data::pointer& to_send) override;

  void remove_hold();

 public:
  using pointer = std::shared_ptr<client>;

  pointer shared_from_this() {
    return std::static_pointer_cast<client>(channel::shared_from_this());
  }

  static pointer create(const grpc_config::pointer& conf);

  ~client();

  void shutdown() override;

  void OnReadDone(bool ok) override;
  void OnWriteDone(bool ok) override;
};

}  // namespace grpc

}  // namespace com::centreon::broker

#endif  // !CCB_GRPC_STREAM_HH
