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

#ifndef CCB_GRPC_STREAM_HH__
#define CCB_GRPC_STREAM_HH__

#include <boost/asio/io_context.hpp>
#include "com/centreon/broker/io/raw.hh"
#include "grpc_config.hh"

namespace centreon_stream = com::centreon::broker::stream;

namespace com::centreon::broker {
namespace stream {
std::ostream& operator<<(std::ostream&, const CentreonEvent&);
}

namespace grpc {

extern const std::string authorization_header;

struct detail_centreon_event;
std::ostream& operator<<(std::ostream&, const detail_centreon_event&);

using grpc_event_type = centreon_stream::CentreonEvent;
using event_ptr = std::shared_ptr<grpc_event_type>;

struct detail_centreon_event {
  detail_centreon_event(const centreon_stream::CentreonEvent& todump)
      : to_dump(todump) {}
  const centreon_stream::CentreonEvent& to_dump;
};

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

template <class bireactor_class>
class stream : public io::stream,
               public bireactor_class,
               public std::enable_shared_from_this<stream<bireactor_class>> {
  /**
   * @brief we store reactor instances in this container until OnDone is called
   * by grpc layers. We allocate this container and never free this because
   * threads terminate in unknown order.
   */
  static std::set<std::shared_ptr<stream>>* _instances;
  static std::mutex _instances_m;

  using read_queue = std::queue<event_ptr>;
  using write_queue = std::queue<event_with_data::pointer>;

  read_queue _read_queue;
  write_queue _write_queue;

  std::atomic_bool _alive = true;

  event_ptr _read_current;
  std::condition_variable _read_cond;
  std::mutex _read_m;

  std::atomic_bool _write_pending = false;
  std::condition_variable _write_cond;
  std::mutex _write_m;

  grpc_config::pointer _conf;
  const std::string_view _class_name;

  std::shared_ptr<asio::io_context> _io_context;

  std::mutex _protect;

  void start_write();

 protected:
  stream(const grpc_config::pointer& conf,
         const std::string_view& class_name,
         const std::shared_ptr<asio::io_context> io_context,
         const std::shared_ptr<spdlog::logger>& logger);

  // called only by public stop
  virtual void shutdown();

  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

 public:
  virtual ~stream();

  static void register_stream(
      const std::shared_ptr<stream<bireactor_class>>& strm);

  void start_read();

  // bireactor part
  void OnReadDone(bool ok) override;

  void OnWriteDone(bool ok) override;

  // server version
  void OnDone();
  // client version
  void OnDone(const ::grpc::Status& /*s*/);

  // io::stream part
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  int32_t write(std::shared_ptr<io::data> const& d) override;

  int32_t flush() override;
  int32_t stop() override;

  bool wait_for_all_events_written(unsigned ms_timeout) override;
};

}  // namespace grpc

}  // namespace com::centreon::broker

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<centreon_stream::CentreonEvent> : ostream_formatter {};

template <>
struct formatter<com::centreon::broker::grpc::detail_centreon_event>
    : ostream_formatter {};

template <>
struct formatter<com::centreon::broker::io::raw> : ostream_formatter {};

}  // namespace fmt

#endif  // !CCB_GRPC_STREAM_HH
