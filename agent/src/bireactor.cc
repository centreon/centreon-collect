/**
 * Copyright 2024 Centreon
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

#include "bireactor.hh"
#include "spdlog/spdlog.h"

using namespace com::centreon::agent;

/**
 * @brief when BiReactor::OnDone is called by grpc layers, we should delete
 * this. But this object is even used by others.
 * So it's stored in this container and just removed from this container when
 * OnDone is called
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
std::set<std::shared_ptr<bireactor<bireactor_class>>>*
    bireactor<bireactor_class>::_instances =
        new std::set<std::shared_ptr<bireactor<bireactor_class>>>;

template <class bireactor_class>
std::mutex bireactor<bireactor_class>::_instances_m;

template <class bireactor_class>
bireactor<bireactor_class>::bireactor(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string_view& class_name,
    const std::string& peer)
    : _write_pending(false),
      _class_name(class_name),
      _peer(peer),
      _io_context(io_context),
      _logger(logger),
      _alive(true) {
  SPDLOG_LOGGER_DEBUG(_logger, "create {} this={:p} peer:{}", _class_name,
                      static_cast<const void*>(this), _peer);
}

template <class bireactor_class>
bireactor<bireactor_class>::~bireactor() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete {} this={:p} peer:{}", _class_name,
                      static_cast<const void*>(this), _peer);
}

template <class bireactor_class>
void bireactor<bireactor_class>::register_stream(
    const std::shared_ptr<bireactor>& strm) {
  std::lock_guard l(_instances_m);
  _instances->insert(strm);
}

template <class bireactor_class>
void bireactor<bireactor_class>::start_read() {
  std::lock_guard l(_protect);
  if (!_alive) {
    return;
  }
  std::shared_ptr<MessageToAgent> to_read;
  if (_read_current) {
    return;
  }
  to_read = _read_current = std::make_shared<MessageToAgent>();
  bireactor_class::StartRead(to_read.get());
}

template <class bireactor_class>
void bireactor<bireactor_class>::OnReadDone(bool ok) {
  if (ok) {
    std::shared_ptr<MessageToAgent> read;
    {
      std::lock_guard l(_protect);
      SPDLOG_LOGGER_TRACE(_logger, "{:p} {} peer {} receive: {}",
                          static_cast<const void*>(this), _class_name, _peer,
                          _read_current->ShortDebugString());
      read = _read_current;
      _read_current.reset();
    }
    start_read();
    if (read->has_config()) {
      on_incomming_request(read);
    }
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} {} peer:{} fail read from stream",
                        static_cast<void*>(this), _class_name, _peer);
    on_error();
    shutdown();
  }
}

template <class bireactor_class>
void bireactor<bireactor_class>::write(
    const std::shared_ptr<MessageFromAgent>& request) {
  {
    std::lock_guard l(_protect);
    if (!_alive) {
      return;
    }
    _write_queue.push_back(request);
  }
  start_write();
}

template <class bireactor_class>
void bireactor<bireactor_class>::start_write() {
  std::shared_ptr<MessageFromAgent> to_send;
  {
    std::lock_guard l(_protect);
    if (!_alive || _write_pending || _write_queue.empty()) {
      return;
    }
    to_send = _write_queue.front();
    _write_pending = true;
  }
  bireactor_class::StartWrite(to_send.get());
}

template <class bireactor_class>
void bireactor<bireactor_class>::OnWriteDone(bool ok) {
  if (ok) {
    {
      std::lock_guard l(_protect);
      _write_pending = false;
      SPDLOG_LOGGER_DEBUG(_logger, "{:p} {} {} bytes sent",
                          static_cast<const void*>(this), _class_name,
                          (*_write_queue.begin())->ByteSizeLong());
      SPDLOG_LOGGER_TRACE(_logger, "{:p} {} {} sent",
                          static_cast<const void*>(this), _class_name,
                          (*_write_queue.begin())->ShortDebugString());
      _write_queue.pop_front();
    }
    start_write();
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} {} peer {} fail write to stream",
                        static_cast<void*>(this), _class_name, _peer);
    on_error();
    shutdown();
  }
}

template <class bireactor_class>
void bireactor<bireactor_class>::OnDone() {
  /**grpc has a bug, sometimes if we delete this class in this handler as it is
   * described in examples, it also deletes used channel and does a pthread_join
   * of the current thread witch go to a EDEADLOCK error and call grpc::Crash.
   * So we uses asio thread to do the job
   */
  _io_context->post([me = std::enable_shared_from_this<
                         bireactor<bireactor_class>>::shared_from_this(),
                     &peer = _peer, logger = _logger]() {
    std::lock_guard l(_instances_m);
    SPDLOG_LOGGER_DEBUG(logger, "{:p} server::OnDone() to {}",
                        static_cast<void*>(me.get()), peer);
    _instances->erase(std::static_pointer_cast<bireactor<bireactor_class>>(me));
  });
}

template <class bireactor_class>
void bireactor<bireactor_class>::OnDone(const ::grpc::Status& status) {
  /**grpc has a bug, sometimes if we delete this class in this handler as it is
   * described in examples, it also deletes used channel and does a
   * pthread_join of the current thread witch go to a EDEADLOCK error and call
   * grpc::Crash. So we uses asio thread to do the job
   */
  _io_context->post([me = std::enable_shared_from_this<
                         bireactor<bireactor_class>>::shared_from_this(),
                     status, &peer = _peer, logger = _logger]() {
    std::lock_guard l(_instances_m);
    if (status.ok()) {
      SPDLOG_LOGGER_DEBUG(logger, "{:p} peer: {} client::OnDone({}) {}",
                          static_cast<void*>(me.get()), peer,
                          status.error_message(), status.error_details());
    } else {
      SPDLOG_LOGGER_ERROR(logger, "{:p} peer:{} client::OnDone({}) {}",
                          static_cast<void*>(me.get()), peer,
                          status.error_message(), status.error_details());
    }
    _instances->erase(std::static_pointer_cast<bireactor<bireactor_class>>(me));
  });
}

template <class bireactor_class>
void bireactor<bireactor_class>::shutdown() {
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} {}::shutdown", static_cast<void*>(this),
                      _class_name);
}

namespace com::centreon::agent {

template class bireactor<
    ::grpc::ClientBidiReactor<MessageFromAgent, MessageToAgent>>;

template class bireactor<
    ::grpc::ServerBidiReactor<MessageToAgent, MessageFromAgent>>;

}  // namespace com::centreon::agent