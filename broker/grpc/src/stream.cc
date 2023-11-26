/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "grpc_stream.grpc.pb.h"

#include "com/centreon/broker/grpc/stream.hh"

#include "com/centreon/broker/grpc/client.hh"
#include "com/centreon/broker/grpc/server.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker::grpc;
using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using log_v2 = com::centreon::common::log_v2::log_v2;

namespace fmt {

// mandatory to log
template <>
struct formatter<io::raw> : ostream_formatter {};

}  // namespace fmt

com::centreon::broker::grpc::stream::stream(const grpc_config::pointer& conf)
    : io::stream("GRPC"),
      _accept(false),
      _logger{log_v2::instance().get(log_v2::GRPC)} {
  log_v2::instance()
      .get(log_v2::FUNCTIONS)
      ->trace("grpc::stream constructor {}", static_cast<void*>(this));
  _channel = client::create(conf);
}

com::centreon::broker::grpc::stream::stream(
    const std::shared_ptr<accepted_service>& accepted)
    : io::stream("GRPC"),
      _accept(true),
      _channel(accepted),
      _logger{log_v2::instance().get(log_v2::GRPC)} {
  log_v2::instance()
      .get(log_v2::FUNCTIONS)
      ->trace("grpc::stream constructor {}", static_cast<void*>(this));
}

com::centreon::broker::grpc::stream::~stream() noexcept {
  log_v2::instance()
      .get(log_v2::FUNCTIONS)
      ->trace("grpc::stream destructor {}", static_cast<void*>(this));
  if (_channel)
    _channel->to_trash();
}

#define READ_IMPL                                                             \
  _logger = log_v2::instance().get(log_v2::GRPC);                             \
  std::pair<event_ptr, bool> read_res = _channel->read(duration_or_deadline); \
  if (read_res.second) {                                                      \
    const grpc_event_type& to_convert = *read_res.first;                      \
    if (to_convert.has_buffer()) {                                            \
      d = std::make_shared<io::raw>();                                        \
      std::static_pointer_cast<io::raw>(d)->_buffer.assign(                   \
          to_convert.buffer().begin(), to_convert.buffer().end());            \
      SPDLOG_LOGGER_TRACE(_logger, "receive:{}",                              \
                          *std::static_pointer_cast<io::raw>(d));             \
    } else {                                                                  \
      return false;                                                           \
    }                                                                         \
  } else {                                                                    \
    if (_channel->is_down()) {                                                \
      d.reset(new io::raw);                                                   \
      throw msg_fmt("Connection lost");                                       \
    }                                                                         \
  }                                                                           \
  return read_res.second;

bool com::centreon::broker::grpc::stream::read(std::shared_ptr<io::data>& d,
                                               time_t duration_or_deadline) {
  READ_IMPL
}

bool com::centreon::broker::grpc::stream::read(
    std::shared_ptr<io::data>& d,
    const system_clock::time_point& duration_or_deadline) {
  READ_IMPL
}

bool com::centreon::broker::grpc::stream::read(
    std::shared_ptr<io::data>& d,
    const system_clock::duration& duration_or_deadline){READ_IMPL}

int32_t com::centreon::broker::grpc::stream::write(
    std::shared_ptr<io::data> const& d) {
  _logger = log_v2::instance().get(log_v2::GRPC);
  if (_channel->is_down())
    throw msg_fmt("Connection lost");

  event_ptr to_send(std::make_shared<grpc_event_type>());

  std::shared_ptr<io::raw> raw_src = std::static_pointer_cast<io::raw>(d);
  to_send->mutable_buffer()->assign(raw_src->_buffer.begin(),
                                    raw_src->_buffer.end());
  return _channel->write(to_send);
}

int32_t com::centreon::broker::grpc::stream::flush() {
  return _channel->flush();
}

int32_t com::centreon::broker::grpc::stream::stop() {
  log_v2::instance()
      .get(log_v2::FUNCTIONS)
      ->trace("grpc::stream stop {}", static_cast<void*>(this));
  return _channel->stop();
}

bool com::centreon::broker::grpc::stream::is_down() const {
  return _channel->is_down();
}

/**
 * @brief wait for connection write queue empty
 *
 * @param ms_timeout
 * @return true queue is empty
 * @return false timeout expired
 */
bool com::centreon::broker::grpc::stream::wait_for_all_events_written(
    unsigned ms_timeout) {
  log_v2::instance()
      .get(log_v2::CORE)
      ->info("grpc::stream::wait_for_all_events_written");
  if (_channel->is_down()) {
    return true;
  }

  return _channel->wait_for_all_events_written(ms_timeout);
}
