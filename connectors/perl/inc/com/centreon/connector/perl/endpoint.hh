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

#include "com/centreon/exceptions/msg_fmt.hh"
#include "connectors/perl/src/perl_connector.pb.h"

namespace com::centreon::connector::perl {

class endpoint;
class ConnectorMess;

namespace detail {

/**
 * @brief Async state machine for reading a single framed protobuf message.
 *
 * Wire format: [ size_t len ][ protobuf bytes ]
 *   - len  : total packet size in bytes, header included
 *   - data : serialized ConnectorMess payload
 *
 * The three states drive two successive async_read calls before decoding:
 *   read_len  -> read the 8-byte length header
 *   read_data -> read (len - sizeof(size_t)) payload bytes
 *   decode    -> ParseFromArray and complete the composed operation
 *
 * The object is heap-allocated and shared across the async_compose
 * continuations because async_compose may copy its initiating function.
 */
class async_receive_impl : std::enable_shared_from_this<async_receive_impl> {
  const std::shared_ptr<endpoint> _parent;
  const std::shared_ptr<spdlog::logger> _logger;
  asio::readable_pipe& _pipe;
  size_t _data_len = 0;
  std::unique_ptr<unsigned char[]> _data_buff;

  enum class e_state { read_len, read_data, decode };

  e_state _state = e_state::read_len;

 public:
  async_receive_impl(const std::shared_ptr<endpoint> parent,
                     const std::shared_ptr<spdlog::logger>& logger,
                     asio::readable_pipe& pipe)
      : _parent(parent), _logger(logger), _pipe(pipe) {}

  /**
   * @brief Continuation called by asio after each async operation.
   *
   * On the first entry (_state == read_len) it issues async_read for the
   * length header.  On the second entry (_state == read_data) it allocates
   * the payload buffer and issues async_read for the data bytes.  On the
   * third entry (_state == decode) it deserializes the protobuf message and
   * calls self.complete() to resolve the composed operation.
   */
  template <typename self_type>
  void handle(self_type& self,
              const boost::system::error_code err = {},
              size_t = 0) {
    if (err) {
      self.complete(err, {});
      return;
    }
    switch (_state) {
      case e_state::read_len:
        _state = e_state::read_data;
        asio::async_read(_pipe, asio::buffer(&_data_len, sizeof(_data_len)),
                         std::move(self));
        break;
      case e_state::read_data:
        // _data_len holds the total packet length (header + data).
        // Subtract the header size to get the actual protobuf payload length.
        _data_len -= sizeof(size_t);
        if (_data_len == 0) {
          self.complete(boost::system::errc::make_error_code(
                            boost::system::errc::protocol_error),
                        {});
          return;
        }
        _data_buff = std::make_unique<unsigned char[]>(_data_len);
        _state = e_state::decode;
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

/**
 * @brief IPC endpoint for parent<->child communication over pipes.
 *
 * An endpoint wraps three pipe file descriptors (stdin, stdout, stderr) and
 * exposes synchronous (read/write) and asynchronous (async_read/async_write)
 * operations to exchange ConnectorMess protobuf messages.
 *
 * The same class is used on both sides of the fork:
 *   - Parent side: writes to stdin (sends commands to the child) and reads
 *     from stdout/stderr (receives results/diagnostics from the child).
 *   - Child side:  reads from stdin (receives commands) and writes to
 *     stdout/stderr (sends back results/diagnostics).
 *
 * Wire format for every message:
 *   [ size_t len ][ serialized protobuf bytes ]
 * where len = sizeof(size_t) + ByteSizeLong(message).
 */
class endpoint : public std::enable_shared_from_this<endpoint> {
 public:
  /// Identifies which pipe of the pair to use for a given operation.
  enum stream_id : unsigned { stdin, stdout, stderr };

 private:
  const std::shared_ptr<asio::io_context> _io_context;
  const std::shared_ptr<spdlog::logger> _logger;
  /// True when this endpoint lives in the parent process.
  const bool _is_parent;

  // Child-side pipes: the child reads commands on stdin and writes results on
  // stdout/stderr.
  std::unique_ptr<asio::readable_pipe> _child_stdin;
  std::unique_ptr<asio::writable_pipe> _child_stdout;
  std::unique_ptr<asio::writable_pipe> _child_stderr;

  // Parent-side pipes: the parent writes commands on stdin and reads results
  // from stdout/stderr.
  std::unique_ptr<asio::writable_pipe> _parent_stdin;
  std::unique_ptr<asio::readable_pipe> _parent_stdout;
  std::unique_ptr<asio::readable_pipe> _parent_stderr;

  /**
   * @brief Length-prefixed buffer used on the wire.
   *
   * The flexible array member `data` immediately follows `len` in memory,
   * so the struct is always allocated with malloc(len) and freed with free().
   * `len` covers the full allocation: sizeof(size_t) + protobuf payload size.
   */
  struct proto_buffer {
    size_t len;
    unsigned char data[];
  };

  /// Serialize @p mess into a heap-allocated proto_buffer ready to be sent.
  static std::shared_ptr<proto_buffer> _serialize(
      const ::google::protobuf::Message& mess);

  /// Parse a single framed message from @p buff; returns the number of bytes
  /// consumed (0 if the buffer is incomplete).
  static size_t _parse(const void* buff, size_t len, ConnectorMess& mess);

  /// Synchronous send: serialize @p mess and write the full frame to @p stream.
  boost::system::error_code _send(stream_id id,
                                  const ConnectorMess& mess,
                                  std::unique_ptr<asio::writable_pipe>& stream);

  /// Async send: serialize @p mess and issue async_write; calls @p handler
  /// with the error code on completion.
  template <typename handler_type>
  void _async_send(stream_id id,
                   const ::google::protobuf::Message& mess,
                   std::unique_ptr<asio::writable_pipe>& stream,
                   handler_type&& handler);

  /// Synchronous receive: read a full framed message from @p stream into
  /// @p received.
  boost::system::error_code _recv(stream_id id,
                                  std::unique_ptr<asio::readable_pipe>& stream,
                                  ConnectorMess& received);

  /// Async receive: drive the detail::async_receive_impl state machine via
  /// asio::async_compose; calls @p handler(error, message) on completion.
  template <typename handler_type>
  void _async_recv(stream_id id,
                   std::unique_ptr<asio::readable_pipe>& stream,
                   handler_type&& handler);

 public:
  /**
   * @brief Construct an endpoint and wrap the given file descriptors.
   *
   * File descriptors <= 0 are silently ignored (the corresponding pipe
   * remains null and will trigger an error if used).
   *
   * @param io_context  The Asio I/O context driving async operations.
   * @param logger      spdlog logger for diagnostic messages.
   * @param is_parent   True for the parent process, false for the child.
   * @param stdin_fd    File descriptor for the stdin pipe.
   * @param stdout_fd   File descriptor for the stdout pipe.
   * @param stderr_fd   File descriptor for the stderr pipe.
   */
  endpoint(const std::shared_ptr<asio::io_context>& io_context,
           const std::shared_ptr<spdlog::logger> logger,
           bool is_parent,
           int stdin_fd,
           int stdout_fd,
           int stderr_fd);

  endpoint(const endpoint&) = delete;
  endpoint& operator=(const endpoint&) = delete;

  /**
   * @brief Asynchronously write @p mess to the stream identified by @p id.
   *
   * Routing rules (throws std::invalid_argument on violation):
   *   - Parent: may only write to stdin.
   *   - Child:  may only write to stdout or stderr.
   *
   * @p handler is called with the error code when the write completes.
   */
  template <typename handler_type>
  void async_write(stream_id id,
                   const ::google::protobuf::Message& mess,
                   handler_type&& handler);

  /**
   * @brief Synchronously write @p mess to the stream identified by @p id.
   *
   * Same routing rules as async_write.  Blocks until the full frame has been
   * written or an error occurs.
   */
  boost::system::error_code write(stream_id id, const ConnectorMess& mess);

  /**
   * @brief Asynchronously read one framed message from the stream @p id.
   *
   * Routing rules (throws std::invalid_argument on violation):
   *   - Parent: may only read from stdout or stderr.
   *   - Child:  may only read from stdin.
   *
   * @p handler is called with (error_code, ConnectorMess) on completion.
   */
  template <typename handler_type>
  void async_read(stream_id id, handler_type&& handler);

  /**
   * @brief Synchronously read one framed message from the stream @p id.
   *
   * Same routing rules as async_read.  Blocks until the full frame has been
   * received or an error occurs.
   */
  boost::system::error_code read(stream_id id, ConnectorMess& received);
};

// ---------------------------------------------------------------------------
// Template method definitions
// ---------------------------------------------------------------------------

template <typename handler_type>
void endpoint::async_write(stream_id id,
                           const ::google::protobuf::Message& mess,
                           handler_type&& handler) {
  if (_is_parent) {
    switch (id) {
      case stream_id::stdin:
        _async_send(id, mess, _parent_stdin, std::move(handler));
        break;
      default:
        throw std::invalid_argument("parent can only write on stdin");
    }
  } else {
    switch (id) {
      case stream_id::stdin:
        throw std::invalid_argument("child can not write to stdin");
      case stream_id::stdout:
        _async_send(id, mess, _child_stdout, std::move(handler));
        break;
      case stream_id::stderr:
        _async_send(id, mess, _child_stderr, std::move(handler));
        break;
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
  // Keep to_send alive until the write completes by capturing it in the lambda.
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
        break;
      default:
        throw std::invalid_argument("child can only read on stdin");
    }
  } else {
    switch (id) {
      case stream_id::stdin:
        throw std::invalid_argument("parent can not read on stdin");
      case stream_id::stdout:
        _async_recv(id, _parent_stdout, std::move(handler));
        break;
      case stream_id::stderr:
        _async_recv(id, _parent_stderr, std::move(handler));
        break;
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

  // async_compose may copy its initiating function on each async step, so the
  // actual state is kept in a separately heap-allocated async_receive_impl and
  // referenced through a shared_ptr captured by value.
  std::shared_ptr<detail::async_receive_impl> async_receiver =
      std::make_shared<detail::async_receive_impl>(shared_from_this(), _logger,
                                                   *stream);
  asio::async_compose<handler_type, void(const boost::system::error_code,
                                         const ConnectorMess&)>(
      [async_receiver](auto& self, const boost::system::error_code& error = {},
                       std::size_t nb_receive = 0) mutable {
        async_receiver->handle(self, error, nb_receive);
      },
      handler, *stream);
}

}  // namespace com::centreon::connector::perl

#endif
