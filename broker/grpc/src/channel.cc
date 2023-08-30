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

#include "grpc_stream.pb.h"

#include "com/centreon/broker/exceptions/connection_closed.hh"
#include "com/centreon/broker/grpc/channel.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/misc/trash.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker::grpc;
using namespace com::centreon::exceptions;
using log_v3 = com::centreon::common::log_v3::log_v3;

/**
 * @brief this memory leak is mandatory
 * in case of shutdown, channel object also owned by grpc layers
 * musn't be deleted
 *
 */
using channel_trash_type = com::centreon::broker::misc::trash<channel>;

static channel_trash_type* _trash(new channel_trash_type);

namespace com::centreon::broker {
namespace stream {
std::ostream& operator<<(std::ostream& st,
                         const centreon_stream::CentreonEvent& to_dump) {
  if (to_dump.IsInitialized()) {
    if (to_dump.has_buffer()) {
      st << "buff: "
         << com::centreon::broker::misc::string::debug_buf(
                to_dump.buffer().data(), to_dump.buffer().length(), 20);
    } else {
      std::string dump{to_dump.ShortDebugString()};
      if (dump.size() > 200) {
        dump.resize(200);
        st << fmt::format(" content:'{}...'", dump);
      } else
        st << " content:'" << dump << '\'';
    }
  }
  return st;
}
}  // namespace stream
namespace grpc {
std::ostream& operator<<(std::ostream& st,
                         const detail_centreon_event& to_dump) {
  if (to_dump.to_dump.IsInitialized()) {
    if (to_dump.to_dump.has_buffer()) {
      st << "buff: "
         << com::centreon::broker::misc::string::debug_buf(
                to_dump.to_dump.buffer().data(),
                to_dump.to_dump.buffer().length(), 100);
    } else {
      st << " content:'" << to_dump.to_dump.ShortDebugString() << '\'';
    }
  }
  return st;
}
}  // namespace grpc
}  // namespace com::centreon::broker

channel::channel(const std::string& class_name,
                 const grpc_config::pointer& conf,
                 const uint32_t logger_id)
    : _class_name(class_name),
      _read_pending(false),
      _write_pending(false),
      _error(false),
      _thrown(false),
      _conf(conf),
      _logger_id{logger_id},
      _logger{log_v3::instance().get(_logger_id)} {
  SPDLOG_LOGGER_TRACE(_logger, "channel::channel this={:p}",
                      static_cast<void*>(this));
}

channel::~channel() {
  SPDLOG_LOGGER_TRACE(_logger, "channel::~channel this={:p}",
                      static_cast<void*>(this));
}

void channel::start() {
  start_read(true);
}

constexpr unsigned second_delay_before_delete = 60u;

void channel::to_trash() {
  this->shutdown();
  _thrown = true;
  SPDLOG_LOGGER_DEBUG(_logger, "to_trash this={:p}", static_cast<void*>(this));
  _trash->to_trash(shared_from_this(),
                   time(nullptr) + second_delay_before_delete);
}

int channel::stop() {
  int ret = flush();
  to_trash();
  return ret;
}

/***************************************************************
 *    read section
 ***************************************************************/
std::pair<event_ptr, bool> channel::read(
    const system_clock::time_point& deadline) {
  event_ptr read;
  unique_lock l(_protect);
  if (!_read_queue.empty()) {
    read = _read_queue.front();
    _read_queue.pop_front();
    return std::make_pair(read, true);
  }
  if (is_down()) {
    throw(exceptions::connection_closed("{} connection is down",
                                        __PRETTY_FUNCTION__));
  }
  _read_cond.wait_until(l, deadline, [this]() { return !_read_queue.empty(); });
  if (!_read_queue.empty()) {
    read = _read_queue.front();
    _read_queue.pop_front();
    return std::make_pair(read, true);
  }
  return std::make_pair(read, false);
}

void channel::start_read(bool first_read) {
  _logger = log_v3::instance().get(_logger_id);
  event_ptr to_read;
  {
    lock_guard l(_protect);
    if (_read_pending) {
      return;
    }
    to_read = _read_current = std::make_shared<grpc_event_type>();

    _read_pending = true;
    if (first_read)
      SPDLOG_LOGGER_DEBUG(_logger, "Start call and read");
    else
      SPDLOG_LOGGER_TRACE(_logger, "Start read");
  }
  if (to_read) {
    start_read(to_read, first_read);
  }
}

void channel::on_read_done(bool ok) {
  if (ok) {
    {
      lock_guard l(_protect);
      SPDLOG_LOGGER_DEBUG(_logger, "receive: {}", *_read_current);

      _read_queue.push_back(_read_current);
      _read_cond.notify_one();
      _read_pending = false;
    }
    start_read(false);
  } else {
    _logger->error("{}::{} ", _class_name, __FUNCTION__);
    lock_guard l(_protect);
    _error = true;
  }
}

/***************************************************************
 *    write section
 ***************************************************************/
int channel::write(const event_with_data::pointer& to_send) {
  if (is_down()) {
    throw(exceptions::connection_closed("{} connection is down",
                                        __PRETTY_FUNCTION__));
  }
  {
    lock_guard l(_protect);
    _write_queue.push_back(to_send);
  }
  start_write();
  return 0;
}

void channel::start_write() {
  event_with_data::pointer write_current;
  {
    lock_guard l(_protect);
    if (_write_pending) {
      return;
    }
    if (_write_queue.empty()) {
      return;
    }
    _write_pending = true;
    write_current = _write_current = _write_queue.front();
  }
  if (write_current->bbdo_event)
    SPDLOG_LOGGER_TRACE(log_v2::grpc(), "write: {}",
                        *write_current->bbdo_event);
  else
    SPDLOG_LOGGER_TRACE(log_v2::grpc(), "write: {}", write_current->grpc_event);

  start_write(write_current);
}

void channel::on_write_done(bool ok) {
  if (ok) {
    bool data_to_write = false;
    {
      lock_guard l(_protect);
      _write_pending = false;
      if (_write_current->bbdo_event)
        SPDLOG_LOGGER_TRACE(_logger, "write done: {}",
                            *_write_current->bbdo_event);
      else
        SPDLOG_LOGGER_TRACE(_logger, "write done: {}",
                            _write_current->grpc_event);

      _write_queue.pop_front();
      data_to_write = !_write_queue.empty();
    }
    _write_cond.notify_all();
    if (data_to_write) {
      start_write();
    }
  } else {
    lock_guard l(_protect);
    if (_write_current->bbdo_event)
      SPDLOG_LOGGER_ERROR(_logger, "write failed: {}",
                          *_write_current->bbdo_event);
    else
      SPDLOG_LOGGER_ERROR(_logger, "write failed: {}",
                          _write_current->grpc_event);
    _error = true;
  }
}

int channel::flush() {
  return 0;
}

/**
 * @brief wait for all events sent on the wire
 *
 * @param ms_timeout
 * @return true if all events are sent
 * @return false if timeout expires
 */
bool channel::wait_for_all_events_written(unsigned ms_timeout) {
  unique_lock l(_protect);
  _logger->trace("wait_for_all_events_written _write_queue.size()={}",
                 _write_queue.size());
  return _write_cond.wait_for(l, std::chrono::milliseconds(ms_timeout),
                              [this]() { return _write_queue.empty(); });
}
