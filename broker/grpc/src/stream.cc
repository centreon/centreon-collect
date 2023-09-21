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
#include "com/centreon/broker/grpc/grpc_bridge.hh"
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
      _conf(conf),
      _logger_id{log_v2::instance().create_logger_or_get_id("grpc")},
      _logger{log_v2::instance().get(_logger_id)} {
  _channel = client::create(conf);
}

com::centreon::broker::grpc::stream::stream(
    const std::shared_ptr<accepted_service>& accepted)
    : io::stream("GRPC"),
      _accept(true),
      _channel(accepted),
      _conf(_channel->get_conf()),
      _logger_id{log_v2::instance().create_logger_or_get_id("grpc")},
      _logger{log_v2::instance().get(_logger_id)} {}

com::centreon::broker::grpc::stream::~stream() noexcept {
  if (_channel)
    _channel->to_trash();
}

#define READ_IMPL                                                             \
  _logger = log_v2::instance().get(_logger_id);                               \
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
      d = protobuf_to_event(read_res.first);                                  \
      return d ? true : false;                                                \
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
  _logger = log_v2::instance().get(_logger_id);
  if (_channel->is_down())
    throw msg_fmt("Connection lost");

  channel::event_with_data::pointer to_send;

  if (_conf->get_grpc_serialized() &&
      std::dynamic_pointer_cast<io::protobuf_base>(d)) {  // no bbdo serialize
    to_send = create_event_with_data(d);
  } else {
    to_send = std::make_shared<channel::event_with_data>();
    std::shared_ptr<io::raw> raw_src = std::static_pointer_cast<io::raw>(d);
    to_send->grpc_event.mutable_buffer()->assign(raw_src->_buffer.begin(),
                                                 raw_src->_buffer.end());
  }
  return _channel->write(to_send);
}

int32_t com::centreon::broker::grpc::stream::flush() {
  return _channel->flush();
}

int32_t com::centreon::broker::grpc::stream::stop() {
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
  log_v2::instance().get(0)->info("grpc::stream::wait_for_all_events_written");
  if (_channel->is_down()) {
    return true;
  }

  return _channel->wait_for_all_events_written(ms_timeout);
}
