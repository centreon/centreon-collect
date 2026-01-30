/**
 * Copyright 2026 Centreon
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

#ifndef CCB_EVENT_SCRIPT_STREAM_HH
#define CCB_EVENT_SCRIPT_STREAM_HH

#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/common/process/process.hh"

namespace com::centreon::broker::event_script {

/**
 * @brief this struct is used to create unordered containers of protobuf
 * events Caution, it must not be used with ordered containers (,) operator
 * only test equality
 *
 */
struct io_data_compare {
  using is_transparent = void;
  bool operator()(const std::shared_ptr<io::protobuf_base>& left,
                  const std::shared_ptr<io::protobuf_base>& right) const;
  size_t operator()(const std::shared_ptr<io::protobuf_base>& to_hash) const;
};

class stream : public io::stream, public std::enable_shared_from_this<stream> {
  std::chrono::system_clock::duration _managed_event_ttl;
  std::chrono::system_clock::duration _timeout;

  struct event_with_time {
    std::shared_ptr<io::protobuf_base> evt;
    std::chrono::system_clock::time_point inserted;
  };

  using event_cont = boost::multi_index::multi_index_container<
      event_with_time,
      boost::multi_index::indexed_by<
          boost::multi_index::hashed_unique<
              BOOST_MULTI_INDEX_MEMBER(event_with_time,
                                       std::shared_ptr<io::protobuf_base>,
                                       evt),
              io_data_compare,
              io_data_compare>,
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              event_with_time,
              std::chrono::system_clock::time_point,
              inserted)>>>;

  event_cont _events;

  std::shared_ptr<spdlog::logger> _logger;

  std::atomic_uint _to_ack;
  bool _writing ABSL_GUARDED_BY(_write_queue_m);

  std::queue<std::shared_ptr<io::protobuf_base>> _write_queue
      ABSL_GUARDED_BY(_write_queue_m);
  absl::Mutex _write_queue_m;

  com::centreon::common::process_args::pointer _script_cmdline;

  void _write(const std::shared_ptr<io::protobuf_base>& event);
  void _write_completion(const common::process<true>& proc,
                         int exit_code,
                         common::e_exit_status exit_status,
                         const std::string& std_err);

 public:
  stream(const std::string_view& script_path,
         const std::chrono::system_clock::duration& managed_event_ttl,
         const std::chrono::system_clock::duration& timeout);

  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  int write(const std::shared_ptr<io::data>& d) override;
  int32_t stop() override;
};

}  // namespace com::centreon::broker::event_script

#endif