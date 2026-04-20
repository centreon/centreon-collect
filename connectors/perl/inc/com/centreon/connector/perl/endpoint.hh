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

#ifndef CCCP_ENDPOINT_HH
#define CCCP_ENDPOINT_HH

#include "boost/asio/buffer.hpp"
#include "boost/asio/compose.hpp"
#include "boost/asio/read.hpp"
#include "boost/asio/readable_pipe.hpp"
#include "boost/asio/write.hpp"
#include "boost/system/detail/error_code.hpp"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "connectors/perl/src/perl_connector.pb.h"

namespace com::centreon::connector::perl {

class endpoint;

namespace detail {

class async_receive_impl {
  const std::shared_ptr<endpoint> _parent;
  const std::shared_ptr<spdlog::logger> _logger;
  const asio::readable_pipe& _pipe;
  size_t _data_len = 0;
  std::unique_ptr<unsigned char[]> _data_buff;

  enum { e_wait_len, e_wait_data, e_data } _state = e_wait_len;

 public:
  async_receive_impl(const std::shared_ptr<endpoint> parent,
                     const std::shared_ptr<spdlog::logger>& logger,
                     const asio::readable_pipe& pipe)
      : _parent(parent), _logger(logger), _pipe(pipe) {}

  template <typename self_type>
  void operator()(self_type& self,
                  const boost::system::error_code err,
                  size_t) {
    if (err) {
      self.complete(err, {});
      return;
    }
    switch (_state) {
      case e_wait_len:
        _state = e_data;
        asio::async_read(_pipe, asio::buffer(&_data_len, sizeof(_data_len)),
                         std::move(self));
        break;
      case e_wait_data:
        _data_buff = std::make_unique<unsigned char[]>(_data_len);
        _state = e_wait_data;
        asio::async_read(_pipe, asio::buffer(_data_buff.get(), _data_len),
                         std::move(self));
        break;
      default:
        try {
          ConnectorMess received;
          received.ParseFromArray(_data_buff.get(), _data_len);
          self.complete({}, received);
        } catch (const std::exception& e) {
          SPDLOG_LOGGER_ERROR(_logger, "Fail to decode pb message: {}",
                              e.what());
          self.complete(boost::system::errc::make_error_code(
                            boost::system::errc::protocol_error),
                        {});
        }
    }
  }
};

}  // namespace detail

class endpoint : public std::enable_shared_from_this<endpoint> {
 public:
  enum stream_id : unsigned { stdin, stdout, stderr };

 private:
  const std::shared_ptr<asio::io_context> _io_context;
  const std::shared_ptr<spdlog::logger> _logger;
  const bool _is_parent;

  // initialized in case of child side
  std::unique_ptr<asio::readable_pipe> _child_stdin;
  std::unique_ptr<asio::writable_pipe> _child_stdout;
  std::unique_ptr<asio::writable_pipe> _child_stderr;

  // initialized in case of parent side
  std::unique_ptr<asio::writable_pipe> _parent_stdin;
  std::unique_ptr<asio::readable_pipe> _parent_stdout;
  std::unique_ptr<asio::readable_pipe> _parent_stderr;

  struct proto_buffer {
    size_t len;
    unsigned char data[];
  };

  static std::shared_ptr<proto_buffer> _serialize(
      const ::google::protobuf::Message& mess);

  static size_t _parse(const void* buff, size_t len, ConnectorMess& mess);

  boost::system::error_code _send(stream_id id,
                                  const ConnectorMess& mess,
                                  std::unique_ptr<asio::writable_pipe>& stream);

  template <typename handler_type>
  void _async_send(stream_id id,
                   const ::google::protobuf::Message& mess,
                   std::unique_ptr<asio::writable_pipe>& stream,
                   handler_type&& handler);

  boost::system::error_code _recv(stream_id id,
                                  std::unique_ptr<asio::readable_pipe>& stream,
                                  ConnectorMess& received);

  template <typename handler_type>
  void _async_recv(stream_id id,
                   std::unique_ptr<asio::readable_pipe>& stream,
                   handler_type&& handler);

 public:
  endpoint(const std::shared_ptr<asio::io_context>& io_context,
           const std::shared_ptr<spdlog::logger> logger,
           bool is_parent,
           int stdin_fd,
           int stdout_fd,
           int stderr_fd);

  endpoint(const endpoint&) = delete;
  endpoint& operator=(const endpoint&) = delete;

  template <typename handler_type>
  void async_write(stream_id id,
                   const ::google::protobuf::Message& mess,
                   handler_type&& handler);

  boost::system::error_code write(stream_id id, const ConnectorMess& mess);

  template <typename handler_type>
  void async_read(stream_id id, handler_type&& handler);

  boost::system::error_code read(stream_id id, ConnectorMess& received);
};

template <typename handler_type>
void endpoint::async_write(stream_id id,
                           const ::google::protobuf::Message& mess,
                           handler_type&& handler) {
  if (_is_parent) {
    switch (id) {
      case stream_id::stdin:
        _async_send(id, mess, _parent_stdin, std::move(handler));
      default:
        throw std::invalid_argument("parent can only write on stdin");
    }
  } else {
    switch (id) {
      case stream_id::stdin:
        throw std::invalid_argument("child can not write to stdin");
      case stream_id::stdout:
        return _async_send(id, mess, _child_stdout, std::move(handler));
      case stream_id::stderr:
        return _async_send(id, mess, _child_stderr, std::move(handler));
    }
  }
}

template <typename handler_type>
void endpoint::_async_send(stream_id id,
                           const ::google::protobuf::Message& mess,
                           std::unique_ptr<asio::writable_pipe>& stream,
                           handler_type&& handler) {
  if (!stream) {
    SPDLOG_LOGGER_ERROR(_logger, "send stream uninitialized {} {}",
                        static_cast<unsigned>(id),
                        _is_parent ? "parent side" : "child_side");
    throw com::centreon::exceptions::msg_fmt(
        "send stream uninitialized {} {}", static_cast<unsigned>(id),
        _is_parent ? "parent side" : "child_side");
  }
  std::shared_ptr<proto_buffer> to_send = _serialize(mess);
  asio::async_write(*stream, asio::buffer(to_send.get(), to_send->len),
                    [hand = std::move(handler), to_send](
                        const boost::system::error_code& error, std::size_t) {
                      hand(error);
                    });
}

template <typename handler_type>
void endpoint::async_read(stream_id id, handler_type&& handler) {
  if (!_is_parent) {
    switch (id) {
      case stream_id::stdin:
        _async_recv(id, _child_stdin, std::move(handler));
      default:
        throw std::invalid_argument("child can only read on stdin");
    }
  } else {
    switch (id) {
      case stream_id::stdin:
        throw std::invalid_argument("parent can not read on stdin");
      case stream_id::stdout:
        _async_recv(id, _parent_stdout, std::move(handler));
      case stream_id::stderr:
        _async_recv(id, _parent_stderr, std::move(handler));
    }
  }
}

template <typename handler_type>
void endpoint::_async_recv(stream_id id,
                           std::unique_ptr<asio::readable_pipe>& stream,
                           handler_type&& handler) {
  if (!stream) {
    SPDLOG_LOGGER_ERROR(_logger, "recv stream uninitialized {} {}",
                        static_cast<unsigned>(id),
                        _is_parent ? "parent side" : "child_side");
    throw com::centreon::exceptions::msg_fmt(
        "recv stream uninitialized {} {}", static_cast<unsigned>(id),
        _is_parent ? "parent side" : "child_side");
  }
  asio::async_compose<handler_type,
                      void(const boost::system::error_code, size_t)>(
      detail::async_receive_impl(shared_from_this(), _logger, *stream), handler,
      *stream);
}

}  // namespace com::centreon::connector::perl

#endif
