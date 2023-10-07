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
std::ostream& operator<<(std::ostream&, const CentreonEvent&);
}  // namespace stream
}  // namespace broker
}  // namespace centreon
}  // namespace com

namespace centreon_grpc = com::centreon::broker::grpc;
namespace centreon_stream = com::centreon::broker::stream;
using grpc_event_type = centreon_stream::CentreonEvent;
using event_ptr = std::shared_ptr<grpc_event_type>;

namespace com::centreon::broker {

namespace grpc {

using channel_ptr = std::shared_ptr<::grpc::Channel>;
using uint64_vector = std::vector<uint64_t>;

struct detail_centreon_event {
  detail_centreon_event(const centreon_stream::CentreonEvent& todump)
      : to_dump(todump) {}
  const centreon_stream::CentreonEvent& to_dump;
};

const std::string authorization_header("authorization");

constexpr uint32_t calc_accept_all_compression_mask() {
  uint32_t ret = 0;
  for (size_t algo_ind = 0; algo_ind < GRPC_COMPRESS_ALGORITHMS_COUNT;
       algo_ind++) {
    ret += (1u << algo_ind);
  }
  return ret;
}

/**
 * @brief Abstract base class of grpc communication final class server or client
 *
 */
class channel : public std::enable_shared_from_this<channel> {
 public:
  /**
   * @brief we pass our protobuf objects to grpc_event without copy
   * so we must avoid that grpc_event delete message of protobuf object
   * This the goal of this struct.
   * At destruction, it releases protobuf object from grpc_event.
   * Destruction of protobuf object is the job of shared_ptr<io::protobuf>
   */
  struct event_with_data {
    using pointer = std::shared_ptr<event_with_data>;
    grpc_event_type grpc_event;
    std::shared_ptr<io::data> bbdo_event;
    typedef google::protobuf::Message* (grpc_event_type::*releaser_type)();
    releaser_type releaser;

    event_with_data() : releaser(nullptr) {}

    event_with_data(const std::shared_ptr<io::data>& bbdo_evt,
                    releaser_type relser)
        : bbdo_event(bbdo_evt), releaser(relser) {}

    event_with_data(const event_with_data&) = delete;
    event_with_data& operator=(const event_with_data&) = delete;

    ~event_with_data() {
      if (releaser) {
        (grpc_event.*releaser)();
      }
    }
  };

 private:
  channel& operator=(const channel&) = delete;
  channel(const channel&) = delete;

 protected:
  using read_queue = std::deque<event_ptr>;
  using write_queue = std::deque<event_with_data::pointer>;

  const std::string _class_name;

  /// read section
  read_queue _read_queue;
  bool _read_pending;
  event_ptr _read_current;

  // write section
  write_queue _write_queue;
  event_with_data::pointer _write_current;
  bool _write_pending;

  bool _error;
  bool _thrown;

  grpc_config::pointer _conf;

  /* Logger */
  const uint32_t _logger_id;
  std::shared_ptr<spdlog::logger> _logger;

  mutable std::mutex _protect;
  mutable std::condition_variable _read_cond, _write_cond;

  channel(const std::string& class_name,
          const grpc_config::pointer& conf,
          const uint32_t logger_id);

  void start_read(bool first_read);
  void start_write();

  virtual void start_write(const event_with_data::pointer&) = 0;
  virtual void start_read(event_ptr&, bool first_read) = 0;

  void on_write_done(bool ok);
  void on_read_done(bool ok);

 public:
  using pointer = std::shared_ptr<channel>;

  void start();

  virtual ~channel();

  void to_trash();
  virtual void shutdown() = 0;
  bool is_down() const { return _error || _thrown; };
  bool is_alive() const { return !_error && !_thrown; }
  grpc_config::pointer get_conf() const { return _conf; }

  std::pair<event_ptr, bool> read(time_t deadline) {
    return read(system_clock::from_time_t(deadline));
  }
  std::pair<event_ptr, bool> read(const system_clock::duration& deadline) {
    return read(system_clock::now() + deadline);
  }
  std::pair<event_ptr, bool> read(const system_clock::time_point& deadline);

  int write(const event_with_data::pointer&);
  int flush();
  virtual int stop();

  bool wait_for_all_events_written(unsigned ms_timeout);
};
}  // namespace grpc

}  // namespace com::centreon::broker

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<centreon_stream::CentreonEvent> : ostream_formatter {};

template <>
struct formatter<centreon_grpc::detail_centreon_event> : ostream_formatter {};

}  // namespace fmt

#endif  // !CCB_GRPC_CHANNEL_HH
