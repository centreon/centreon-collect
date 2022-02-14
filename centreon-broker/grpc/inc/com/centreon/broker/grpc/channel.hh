/*
** Copyright 2011-2013,2015,2017 Centreon
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

#ifndef CCB_GRPC_CHANNEL_HH
#define CCB_GRPC_CHANNEL_HH

#include "grpc_grpc.hh"

namespace com {
namespace centreon {
namespace broker {
namespace grpc {
class detail_centreon_event;
std::ostream& operator<<(std::ostream&, const detail_centreon_event&);
}  // namespace grpc
namespace stream {
class centreon_event;
class centreon_raw_data;
std::ostream& operator<<(std::ostream&, const centreon_event&);
}  // namespace stream
}  // namespace broker
}  // namespace centreon
}  // namespace com

namespace centreon_stream = com::centreon::broker::stream;
namespace centreon_grpc = com::centreon::broker::grpc;

CCB_BEGIN()

namespace grpc {

using grpc_event = centreon_stream::centreon_event;
using grpc_raw_data = centreon_stream::centreon_raw_data;
using event_ptr = std::shared_ptr<grpc_event>;
using channel_ptr = std::shared_ptr<::grpc::Channel>;

struct detail_centreon_event {
  detail_centreon_event(const centreon_stream::centreon_event& todump)
      : to_dump(todump) {}
  const centreon_stream::centreon_event& to_dump;
};

/**
 * @brief base class of grpc communication final class server or client
 *
 */
class channel : public std::enable_shared_from_this<channel> {
  channel& operator=(const channel&) = delete;
  channel(const channel&) = delete;

 protected:
  std::string _hostport;

  using event_queue = std::list<event_ptr>;

  event_queue _write_queue, _read_queue;

  bool _thrown;

  int _nb_written;

  mutable std::mutex _protect;
  mutable std::condition_variable _read_cond;

  channel(const std::string& hostport)
      : _hostport(hostport), _thrown(false), _nb_written(0) {}

 public:
  using pointer = std::shared_ptr<channel>;

  virtual ~channel() = default;

  void to_trash();
  virtual bool is_down() const = 0;

  virtual std::pair<event_ptr, bool> read(time_t deadline) = 0;
  virtual std::pair<event_ptr, bool> read(
      const system_clock::time_point& deadline) = 0;
  virtual std::pair<event_ptr, bool> read(
      const system_clock::duration& deadline) = 0;

  virtual int write(const event_ptr&) = 0;
  virtual int flush() = 0;
  virtual int stop() = 0;
};
}  // namespace grpc

CCB_END()

#endif  // !CCB_GRPC_CHANNEL_HH
