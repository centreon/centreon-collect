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

#ifndef CCB_GRPC_CHANNEL_HH
#define CCB_GRPC_CHANNEL_HH

#include "grpc_config.hh"

namespace com {
namespace centreon {
namespace broker {
namespace grpc {
struct detail_centreon_event;
std::ostream& operator<<(std::ostream&, const detail_centreon_event&);
}  // namespace grpc
namespace stream {
std::ostream& operator<<(std::ostream&, const centreon_event&);
}  // namespace stream
}  // namespace broker
}  // namespace centreon
}  // namespace com

namespace centreon_grpc = com::centreon::broker::grpc;
namespace centreon_stream = com::centreon::broker::stream;
using grpc_event_type = centreon_stream::centreon_event;
using event_ptr = std::shared_ptr<grpc_event_type>;

CCB_BEGIN()

namespace grpc {

using channel_ptr = std::shared_ptr<::grpc::Channel>;
using uint64_vector = std::vector<uint64_t>;

struct detail_centreon_event {
  detail_centreon_event(const centreon_stream::centreon_event& todump)
      : to_dump(todump) {}
  const centreon_stream::centreon_event& to_dump;
};

const std::string authorization_header("authorization");

/**
 * @brief base class of grpc communication final class server or client
 *
 */
class channel : public std::enable_shared_from_this<channel> {
 private:
  channel& operator=(const channel&) = delete;
  channel(const channel&) = delete;

 protected:
  using event_queue = std::list<event_ptr>;

  const std::string _class_name;

  /// read section
  event_queue _read_queue;
  bool _read_pending;
  event_ptr _read_current;

  // write section
  event_queue _write_queue;
  event_ptr _write_current;
  bool _write_pending;

  bool _error;
  bool _thrown;

  grpc_config::pointer _conf;

  mutable std::mutex _protect;
  mutable std::condition_variable _read_cond, _write_cond;

  channel(const std::string& class_name, const grpc_config::pointer& conf);

  void start_read(bool first_read);
  void start_write();

  virtual void start_write(const event_ptr&) = 0;
  virtual void start_read(event_ptr&, bool first_read) = 0;

  void on_write_done(bool ok);
  void on_read_done(bool ok);

 public:
  using pointer = std::shared_ptr<channel>;

  void start();

  virtual ~channel();

  void to_trash();
  bool is_down() const { return _error || _thrown; };
  bool is_alive() const { return !_error && !_thrown; }

  std::pair<event_ptr, bool> read(time_t deadline) {
    return read(system_clock::from_time_t(deadline));
  }
  std::pair<event_ptr, bool> read(const system_clock::duration& deadline) {
    return read(system_clock::now() + deadline);
  }
  std::pair<event_ptr, bool> read(const system_clock::time_point& deadline);

  int write(const event_ptr&);
  int flush();
  virtual int stop();

  bool wait_for_all_events_written(unsigned ms_timeout);
};
}  // namespace grpc

CCB_END()

#endif  // !CCB_GRPC_CHANNEL_HH
