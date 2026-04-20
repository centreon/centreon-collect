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

#include "com/centreon/connector/perl/endpoint.hh"
#include <boost/asio/buffer.hpp>
#include <boost/system/detail/errc.hpp>
#include "connectors/perl/src/perl_connector.pb.h"

using namespace com::centreon::connector::perl;

endpoint::endpoint(const std::shared_ptr<asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger> logger,
                   bool is_parent,
                   int stdin_fd,
                   int stdout_fd,
                   int stderr_fd)
    : _io_context(io_context), _logger(logger), _is_parent(is_parent) {
  if (is_parent) {
    if (stdin_fd > 0) {
      _parent_stdin =
          std::make_unique<asio::writable_pipe>(*io_context, stdin_fd);
    }
    if (stdout_fd > 0) {
      _parent_stdout =
          std::make_unique<asio::readable_pipe>(*io_context, stdout_fd);
    }
    if (stderr_fd > 0) {
      _parent_stderr =
          std::make_unique<asio::readable_pipe>(*io_context, stderr_fd);
    }
  } else {
    if (stdin_fd > 0) {
      _child_stdin =
          std::make_unique<asio::readable_pipe>(*io_context, stdin_fd);
    }
    if (stdout_fd > 0) {
      _child_stdout =
          std::make_unique<asio::writable_pipe>(*io_context, stdout_fd);
    }
    if (stderr_fd > 0) {
      _child_stderr =
          std::make_unique<asio::writable_pipe>(*io_context, stderr_fd);
    }
  }
}

std::shared_ptr<endpoint::proto_buffer> endpoint::_serialize(
    const ::google::protobuf::Message& mess) {
  size_t mess_len = mess.ByteSizeLong();
  size_t packet_len = mess_len + sizeof(proto_buffer);

  proto_buffer* b = static_cast<proto_buffer*>(malloc(packet_len));
  b->len = packet_len;
  mess.SerializeToArray(b->data, mess_len);
  return std::shared_ptr<proto_buffer>(
      b, [](proto_buffer* to_free) { free(to_free); });
}

size_t endpoint::_parse(const void* buff, size_t len, ConnectorMess& mess) {
  size_t consumed = 0;
  if (len >= sizeof(proto_buffer)) {
    const proto_buffer* pbuff = static_cast<const proto_buffer*>(buff);
    if (len >= pbuff->len) {
      consumed = pbuff->len;
      mess.ParseFromArray(pbuff->data, pbuff->len - sizeof(proto_buffer));
    }
  }
  return consumed;
}

boost::system::error_code endpoint::_send(
    stream_id id,
    const ConnectorMess& mess,
    std::unique_ptr<asio::writable_pipe>& stream) {
  if (!stream) {
    SPDLOG_LOGGER_ERROR(_logger, "send stream uninitialized {} {}",
                        static_cast<unsigned>(id),
                        _is_parent ? "parent side" : "child_side");
    return boost::system::errc::make_error_code(
        boost::system::errc::invalid_argument);
  }
  std::shared_ptr<proto_buffer> to_send = _serialize(mess);
  boost::system::error_code ec;
  asio::write(*stream, asio::buffer(to_send.get(), to_send->len), ec);
  return ec;
}

boost::system::error_code endpoint::write(stream_id id,
                                          const ConnectorMess& mess) {
  if (_is_parent) {
    switch (id) {
      case stream_id::stdin:
        return _send(id, mess, _parent_stdin);
      default:
        throw std::invalid_argument("parent can only write on stdin");
    }
  } else {
    switch (id) {
      case stream_id::stdin:
        throw std::invalid_argument("child can not write to stdin");
      case stream_id::stdout:
        return _send(id, mess, _child_stdout);
      case stream_id::stderr:
        return _send(id, mess, _child_stderr);
    }
  }
  return {};
}

boost::system::error_code endpoint::_recv(
    stream_id id,
    std::unique_ptr<asio::readable_pipe>& stream,
    ConnectorMess& received) {
  if (!stream) {
    SPDLOG_LOGGER_ERROR(_logger, "recv stream uninitialized {} {}",
                        static_cast<unsigned>(id),
                        _is_parent ? "parent side" : "child_side");
    return boost::system::errc::make_error_code(
        boost::system::errc::invalid_argument);
  }

  boost::system::error_code err;
  // first read packet length
  size_t len;
  asio::read(*stream, asio::mutable_buffer(&len, sizeof(len)), err);
  if (err) {
    return err;
  }
  if (len <= sizeof(proto_buffer)) {  // empty buffer => error
    return boost::system::errc::make_error_code(
        boost::system::errc::protocol_error);
  }
  size_t data_len = len - sizeof(proto_buffer);
  // then datas
  std::unique_ptr<unsigned char[]> raw =
      std::make_unique<unsigned char[]>(data_len);

  asio::read(*stream, asio::mutable_buffer(raw.get(), data_len), err);
  if (err) {
    return err;
  }
  const proto_buffer* raw_received = (const proto_buffer*)(raw.get());
  try {
    received.ParseFromArray(raw_received->data, data_len);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to parse response {} {}: {}",
                        static_cast<unsigned>(id),
                        _is_parent ? "parent side" : "child_side", e.what());
    return boost::system::errc::make_error_code(
        boost::system::errc::protocol_error);
  }
  return err;
}

boost::system::error_code endpoint::read(stream_id id,
                                         ConnectorMess& received) {
  if (!_is_parent) {
    switch (id) {
      case stream_id::stdin:
        return _recv(id, _child_stdin, received);
      default:
        throw std::invalid_argument("child can only read on stdin");
    }
  } else {
    switch (id) {
      case stream_id::stdin:
        throw std::invalid_argument("parent can not read on stdin");
      case stream_id::stdout:
        return _recv(id, _parent_stdout, received);
      case stream_id::stderr:
        return _recv(id, _parent_stderr, received);
    }
  }
}
