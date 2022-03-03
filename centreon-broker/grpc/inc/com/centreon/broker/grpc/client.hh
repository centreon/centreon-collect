/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_GRPC_CLIENT_HH
#define CCB_GPRC_CLIENT_HH

#include "channel.hh"
#include "grpc_stream.grpc.pb.h"

CCB_BEGIN()

namespace grpc {
/**
 * @brief definition of client interface
 * client is asynchronous
 *
 */
class client : public channel,
               public ::grpc::ClientBidiReactor<
                   ::com::centreon::broker::stream::centreon_event,
                   ::com::centreon::broker::stream::centreon_event> {
  channel_ptr _channel;
  std::unique_ptr<com::centreon::broker::stream::centreon_bbdo::Stub> _stub;
  std::unique_ptr<::grpc::ClientContext> _context;

  bool _read_pending;
  bool _read_started;
  bool _write_pending;
  bool _error;
  event_ptr _read_current, _write_current;

 protected:
  client& operator=(const client&) = delete;
  client(const client&) = delete;

  client(const std::string& hostport);

  void start_read();
  void start_write();

 public:
  using pointer = std::shared_ptr<client>;

  pointer shared_from_this() {
    return std::static_pointer_cast<client>(channel::shared_from_this());
  }

  static pointer create(const std::string& hostport);

  ~client();

  bool is_down() const override { return _error || _thrown; }

  std::pair<event_ptr, bool> read(time_t deadline) override;
  std::pair<event_ptr, bool> read(
      const system_clock::time_point& deadline) override;
  std::pair<event_ptr, bool> read(
      const system_clock::duration& timeout) override;

  int write(const event_ptr&) override;
  int flush() override;
  int stop() override;

  void OnReadDone(bool ok) override;
  void OnWriteDone(bool ok) override;
};

}  // namespace grpc

CCB_END()

#endif  // !CCB_GRPC_STREAM_HH
