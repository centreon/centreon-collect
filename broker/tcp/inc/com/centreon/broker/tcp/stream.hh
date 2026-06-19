/**
 * Copyright 2011-2013,2015,2017 Centreon
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

#ifndef CCB_TCP_STREAM_HH
#define CCB_TCP_STREAM_HH

#include <absl/base/thread_annotations.h>
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/tcp/tcp_config.hh"
#include "com/centreon/broker/tcp/tcp_connection.hh"

namespace com::centreon::broker {

namespace tcp {
// Forward declaration.
class acceptor;

/**
 *  @class stream stream.hh "com/centreon/broker/tcp/stream.hh"
 *  @brief TCP stream.
 *
 *  TCP stream.
 */
class stream : public io::stream {
  static absl::flat_hash_set<const stream*>* _instances
      ABSL_GUARDED_BY(_instances_m);
  static absl::Mutex _instances_m;

  tcp_config::pointer _conf;
  tcp_connection::pointer _connection;
  const io::endpoint* _parent;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  stream(const io::endpoint* parent,
         const tcp_config::pointer& conf,
         const std::shared_ptr<spdlog::logger>& logger);
  stream(const io::endpoint* parent,
         const tcp_connection::pointer& conn,
         const tcp_config::pointer& conf,
         const std::shared_ptr<spdlog::logger>& logger);
  ~stream() noexcept;
  stream& operator=(const stream&) = delete;
  stream(const stream&) = delete;

  const io::endpoint* parent() const { return _parent; }
  std::string peer() const override final;
  std::string raw_peer() const {
    return _connection ? _connection->peer() : "";
  }
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  int32_t flush() override;
  int32_t stop() override;
  int32_t write(std::shared_ptr<io::data> const& d) override;
  bool wait_for_all_events_written(unsigned ms_timeout) override;

  template <class visitor>
  static void visit_all_instances(visitor&& visit);
};

template <class visitor>
void stream::visit_all_instances(visitor&& visit) {
  absl::MutexLock l(&_instances_m);
  for (const auto& inst : *_instances) {
    visit(*inst);
  }
}

}  // namespace tcp

}  // namespace com::centreon::broker

#endif  // !CCB_TCP_STREAM_HH
