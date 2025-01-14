/**
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

#include "com/centreon/broker/exceptions/connection_closed.hh"
#include "com/centreon/broker/grpc/grpc_bridge.hh"
#include "com/centreon/common/hex_dump.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker::grpc;
using namespace com::centreon::exceptions;
using log_v2 = com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker {
namespace stream {

/**
 * @brief << operator for CentreonEvent
 * used by fmt with ostream_formatter
 *
 * @param st
 * @param to_dump
 * @return * std::ostream&
 */
std::ostream& operator<<(std::ostream& st,
                         const centreon_stream::CentreonEvent& to_dump) {
  if (to_dump.IsInitialized()) {
    if (to_dump.has_buffer()) {
      st << "buff: "
         << com::centreon::common::debug_buf(to_dump.buffer().data(),
                                             to_dump.buffer().length(), 20);
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
/**
 * @brief << operator for detail_centreon_event with more details than
 * CentreonEvent used by fmt with ostream_formatter
 *
 * @param st
 * @param to_dump
 * @return * std::ostream&
 */
std::ostream& operator<<(std::ostream& st,
                         const detail_centreon_event& to_dump) {
  if (to_dump.to_dump.IsInitialized()) {
    if (to_dump.to_dump.has_buffer()) {
      st << "buff: "
         << com::centreon::common::debug_buf(to_dump.to_dump.buffer().data(),
                                             to_dump.to_dump.buffer().length(),
                                             100);
    } else {
      st << " content:'" << to_dump.to_dump.ShortDebugString() << '\'';
    }
  }
  return st;
}
}  // namespace grpc
}  // namespace com::centreon::broker

/**
 * @brief this header is used to identify poller
 *
 */
const std::string com::centreon::broker::grpc::authorization_header(
    "authorization");

/**
 * @brief when BiReactor::OnDone is called by grpc layers, we should delete
 * this. But this object is even used by feeder or failover.
 * So it's stored in this container and just removed from this container when
 * OnDone is called
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
std::set<std::shared_ptr<stream<bireactor_class>>>*
    stream<bireactor_class>::_instances =
        new std::set<std::shared_ptr<stream<bireactor_class>>>;

template <class bireactor_class>
std::mutex stream<bireactor_class>::_instances_m;

/**
 * @brief Construct a new stream<bireactor class>::stream object
 *
 * @tparam bireactor_class
 * @param conf
 * @param class_name used by logs to identify server or client case
 */
template <class bireactor_class>
stream<bireactor_class>::stream(const grpc_config::pointer& conf,
                                const std::string_view& class_name)
    : io::stream("GRPC"),
      _conf(conf),
      _class_name(class_name),
      _logger{log_v2::instance().get(log_v2::GRPC)} {
  SPDLOG_LOGGER_DEBUG(_logger, "create {} this={:p}", _class_name,
                      static_cast<const void*>(this));
}

/**
 * @brief Destroy the stream<bireactor class>::stream object
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
stream<bireactor_class>::~stream() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete {} this={:p}", _class_name,
                      static_cast<const void*>(this));
}

/**
 * @brief after creation object is stored in _instances static container
 *
 * @tparam bireactor_class
 * @param strm
 */
template <class bireactor_class>
void stream<bireactor_class>::register_stream(
    const std::shared_ptr<stream<bireactor_class>>& strm) {
  std::lock_guard l(_instances_m);
  _instances->insert(strm);
}

/**
 * @brief call StartRead if still alive and no pending reading
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void stream<bireactor_class>::start_read() {
  std::lock_guard l(_protect);
  if (!_alive) {
    return;
  }
  event_ptr to_read;
  {
    std::lock_guard l(_read_m);
    if (_read_current) {
      return;
    }
    to_read = _read_current = std::make_shared<grpc_event_type>();
  }
  SPDLOG_LOGGER_TRACE(_logger, "{:p} {} Start read",
                      static_cast<const void*>(this), _class_name);
  bireactor_class::StartRead(to_read.get());
}

/**
 * @brief completion read handler
 * if ok, event is pushed in queue that will be read by stream::read
 *
 * @tparam bireactor_class
 * @param ok
 */
template <class bireactor_class>
void stream<bireactor_class>::OnReadDone(bool ok) {
  if (ok) {
    {
      std::unique_lock l(_read_m);
      SPDLOG_LOGGER_TRACE(_logger, "{:p} {} receive: {}",
                          static_cast<const void*>(this), _class_name,
                          *_read_current);
      _read_queue.push(_read_current);
      _read_current.reset();
    }
    _read_cond.notify_one();
    start_read();
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} {} fail read from stream",
                        static_cast<void*>(this), _class_name);
    stop();
  }
}

/**
 * @brief peek an event from read_queue,
 * if queue is empty it waits for incoming event
 *
 * @tparam bireactor_class
 * @param d out event
 * @param deadline max timepoint to wait
 * @return true d point to an event
 * @return false
 * @throw msg_fmt if read_queue is empty and not alive (shutdown has been called
 * or an error has ocurred)
 */
template <class bireactor_class>
bool stream<bireactor_class>::read(std::shared_ptr<io::data>& d,
                                   time_t deadline) {
  auto extract_event = [this, &d]() -> bool {
    event_ptr first = _read_queue.front();
    _read_queue.pop();
    const grpc_event_type& to_convert = *first;
    if (first->has_buffer()) {
      d = std::make_shared<io::raw>();
      std::static_pointer_cast<io::raw>(d)->_buffer.assign(
          to_convert.buffer().begin(), to_convert.buffer().end());
      SPDLOG_LOGGER_TRACE(_logger, "{:p} {} read:{}", static_cast<void*>(this),
                          _class_name, *std::static_pointer_cast<io::raw>(d));
      return true;
    } else {
      d = protobuf_to_event(first);
      if (d) {
        SPDLOG_LOGGER_TRACE(_logger, "{:p} {} read:{}",
                            static_cast<void*>(this), _class_name, *d);
      }
      return d ? true : false;
    }
  };

  std::unique_lock l(_read_m);
  if (!_read_queue.empty()) {
    return extract_event();
  }
  if (!_alive) {
    d.reset();
    throw(exceptions::connection_closed("{} connection is down",
                                        __PRETTY_FUNCTION__));
  }
  _read_cond.wait_until(l, std::chrono::system_clock::from_time_t(deadline),
                        [this]() { return !_read_queue.empty(); });
  if (!_read_queue.empty())
    return extract_event();
  else
    return false;
}

/**
 * @brief peeks an event from write queue and pushes it on the wire
 * does nothing if a write is already pending
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void stream<bireactor_class>::start_write() {
  std::lock_guard l(_protect);
  if (!_alive) {
    return;
  }
  event_with_data::pointer to_send;
  {
    std::unique_lock l(_write_m);
    if (_write_pending || _write_queue.empty()) {
      return;
    }
    to_send = _write_queue.front();
    _write_pending = true;
  }

  if (to_send->bbdo_event)
    SPDLOG_LOGGER_TRACE(_logger, "{:p} {} write: {}", static_cast<void*>(this),
                        _class_name, *to_send->bbdo_event);
  else
    SPDLOG_LOGGER_TRACE(_logger, "{:p} {} write: {}", static_cast<void*>(this),
                        _class_name, to_send->grpc_event);

  bireactor_class::StartWrite(&to_send->grpc_event);
}

/**
 * @brief write completion handler
 * if ok first element of write queue is popped and next event is pushed on the
 * wire
 *
 * @tparam bireactor_class
 * @param ok
 */
template <class bireactor_class>
void stream<bireactor_class>::OnWriteDone(bool ok) {
  if (ok) {
    {
      std::unique_lock l(_write_m);
      event_with_data::pointer written = _write_queue.front();
      if (written->bbdo_event)
        SPDLOG_LOGGER_TRACE(_logger, "{:p} {} write done: {}",
                            static_cast<void*>(this), _class_name,
                            *written->bbdo_event);
      else
        SPDLOG_LOGGER_TRACE(_logger, "{:p} {} write done: {}",
                            static_cast<void*>(this), _class_name,
                            written->grpc_event);

      _write_queue.pop();
      _write_pending = false;
    };
    _write_cond.notify_one();
    start_write();
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} {} fail write to stream",
                        static_cast<void*>(this), _class_name);
    stop();
  }
}

/**
 * @brief push an event on write queue and start write
 *
 * @tparam bireactor_class
 * @param d
 * @return int32_t
 * @throw msg_fmt is object is not alive (after shutdown or an error)
 */
template <class bireactor_class>
int32_t stream<bireactor_class>::write(std::shared_ptr<io::data> const& d) {
  if (!_alive)
    throw(exceptions::connection_closed("{} connection is down",
                                        __PRETTY_FUNCTION__));
  event_with_data::pointer to_send;

  if (_conf->get_grpc_serialized() &&
      std::dynamic_pointer_cast<io::protobuf_base>(d)) {  // no bbdo serialize
    to_send = create_event_with_data(d);
  } else {
    to_send = std::make_shared<event_with_data>();
    std::shared_ptr<io::raw> raw_src = std::static_pointer_cast<io::raw>(d);
    to_send->grpc_event.mutable_buffer()->assign(raw_src->_buffer.begin(),
                                                 raw_src->_buffer.end());
  }
  {
    std::lock_guard l(_write_m);
    _write_queue.push(to_send);
  }
  start_write();
  return 0;
}

/**
 * @brief called when reactor is
 * if override, it must the last method called by parent class
 * @tparam bireactor_class
 */
template <class bireactor_class>
void stream<bireactor_class>::OnDone() {
  stop();
  /**grpc has a bug, sometimes if we delete this class in this handler as it is
   * described in examples, it also deletes used channel and does a pthread_join
   * of the current thread which go to a EDEADLOCK error and call grpc::Crash.
   * So we uses asio thread to do the job
   */
  common::pool::io_context().post(
      [me = std::enable_shared_from_this<
           stream<bireactor_class>>::shared_from_this(),
       logger = _logger]() {
        std::lock_guard l(_instances_m);
        SPDLOG_LOGGER_DEBUG(logger, "{:p} server::OnDone()",
                            static_cast<void*>(me.get()));
        _instances->erase(
            std::static_pointer_cast<stream<bireactor_class>>(me));
      });
}

/**
 * @brief client OnDone bireactor override
 * Called after an error or shutdown
 *
 * @tparam bireactor_class
 * @param status
 */
template <class bireactor_class>
void stream<bireactor_class>::OnDone(const ::grpc::Status& status) {
  stop();
  /**grpc has a bug, sometimes if we delete this class in this handler as it is
   * described in examples, it also deletes used channel and does a
   * pthread_join of the current thread which go to a EDEADLOCK error and call
   * grpc::Crash. So we uses asio thread to do the job
   */
  common::pool::io_context().post(
      [me = std::enable_shared_from_this<
           stream<bireactor_class>>::shared_from_this(),
       status, logger = _logger]() {
        std::lock_guard l(_instances_m);
        SPDLOG_LOGGER_DEBUG(logger, "{:p} client::OnDone({}) {}",
                            static_cast<void*>(me.get()),
                            status.error_message(), status.error_details());
        _instances->erase(
            std::static_pointer_cast<stream<bireactor_class>>(me));
      });
}

/**
 * @brief just log
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void stream<bireactor_class>::shutdown() {
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} {}::shutdown", static_cast<void*>(this),
                      _class_name);
}

/**
 * @brief does nothing
 *
 * @tparam bireactor_class
 * @return int32_t
 */
template <class bireactor_class>
int32_t stream<bireactor_class>::flush() {
  return 0;
}

/**
 * @brief shutdown if not yet done
 *
 * @tparam bireactor_class
 * @return int32_t
 */
template <class bireactor_class>
int32_t stream<bireactor_class>::stop() {
  std::lock_guard l(_protect);
  if (_alive) {
    SPDLOG_LOGGER_DEBUG(_logger, "{:p} {}::stop", static_cast<void*>(this),
                        _class_name);
    _alive = false;
    this->shutdown();
  }
  return 0;
}

/**
 * @brief wait for all events sent on the wire
 *
 * @param ms_timeout
 * @return true if all events are sent
 * @return false if timeout expires
 */
template <class bireactor_class>
bool stream<bireactor_class>::wait_for_all_events_written(unsigned ms_timeout) {
  std::unique_lock l(_write_m);
  return _write_cond.wait_for(l, std::chrono::milliseconds(ms_timeout),
                              [this]() { return _write_queue.empty(); });
}

namespace com::centreon::broker::grpc {

template class stream<
    ::grpc::ClientBidiReactor<::com::centreon::broker::stream::CentreonEvent,
                              ::com::centreon::broker::stream::CentreonEvent>>;

template class stream<
    ::grpc::ServerBidiReactor<::com::centreon::broker::stream::CentreonEvent,
                              ::com::centreon::broker::stream::CentreonEvent>>;

}  // namespace com::centreon::broker::grpc
