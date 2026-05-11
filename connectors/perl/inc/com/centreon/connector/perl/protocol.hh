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

#ifndef CCCP_PROTOCOL_HH
#define CCCP_PROTOCOL_HH

#include <boost/system/detail/error_code.hpp>
#include "connectors/perl/src/perl_connector.pb.h"

namespace com::centreon::connector::perl {

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
  asio::readable_pipe& _pipe;
  size_t _data_len = 0;
  std::unique_ptr<unsigned char[]> _data_buff;

  enum class e_state { read_len, read_data, decode };

  e_state _state = e_state::read_len;

 public:
  async_receive_impl(asio::readable_pipe& pipe) : _pipe(pipe) {}

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
      self.complete(err, std::shared_ptr<ConnectorMess>());
      return;
    }
    switch (_state) {
      case e_state::read_len:
        _state = e_state::read_data;
        asio::async_read(_pipe, asio::buffer(&_data_len, sizeof(_data_len)),
                         std::move(self));
        break;
      case e_state::read_data:
        if (_data_len > 0x100000) {
          self.complete(boost::system::errc::make_error_code(
                            boost::system::errc::protocol_error),
                        std::shared_ptr<ConnectorMess>());
          return;
        }
        // _data_len holds the total packet length (header + data).
        // Subtract the header size to get the actual protobuf payload length.
        _data_len -= sizeof(size_t);
        if (_data_len == 0) {
          self.complete(boost::system::errc::make_error_code(
                            boost::system::errc::protocol_error),
                        std::shared_ptr<ConnectorMess>());
          return;
        }
        _data_buff = std::make_unique<unsigned char[]>(_data_len);
        _state = e_state::decode;
        asio::async_read(_pipe, asio::buffer(_data_buff.get(), _data_len),
                         std::move(self));
        break;
      default:
        try {
          std::shared_ptr<ConnectorMess> received =
              std::make_shared<ConnectorMess>();
          received->ParseFromArray(_data_buff.get(), _data_len);
          self.complete({}, received);
        } catch (const std::exception& e) {
          self.complete(boost::system::errc::make_error_code(
                            boost::system::errc::protocol_error),
                        std::shared_ptr<ConnectorMess>());
        }
    }
  }
};

}  // namespace detail

/**
 * @brief Framing codec for ConnectorMess protobuf messages over POSIX pipes.
 *
 * ## Wire format
 * Every message is prefixed by a native-endian @c size_t that encodes the
 * total frame length (header + payload):
 * @code
 *   [ size_t  packet_len ][ <packet_len - sizeof(size_t)> protobuf bytes ]
 * @endcode
 *
 * ## Two usage modes
 * 1. **Synchronous pipe I/O** — `send()` / `recv()` block until the entire
 *    frame has been written or read.  Suitable for child processes that own
 *    both ends of a pipe.
 *
 * 2. **Streaming / async-friendly** — `on_recv()` accepts arbitrary chunks
 *    of raw data (e.g. from an async read callback), appends them to an
 *    internal reassembly buffer, and returns all complete messages decoded so
 *    far.  Partial frames are kept until more data arrives.
 *
 * Each `protocol` instance is *not* thread-safe; external synchronization is
 * required when `on_recv()` is called from multiple threads.
 */
class protocol {
  std::string _recv_buffer;

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

  using data_handler =
      std::pair<std::shared_ptr<proto_buffer>,
                std::function<void(const boost::system::error_code&)>>;

  std::deque<data_handler> _write_queue;
  bool _write_pending = false;

  /// Parse a single framed message from @p buff; returns the number of bytes
  /// consumed (0 if the buffer is incomplete).
  static size_t _parse(const void* buff, size_t len, ConnectorMess& to_parse);

  void _on_send(asio::writable_pipe& out_pipe,
                const boost::system::error_code& err);

 public:
  /// Serialize @p to_serialize into a heap-allocated proto_buffer ready to be
  /// sent.
  static std::shared_ptr<proto_buffer> serialize(
      const ::google::protobuf::Message& to_serialize);

  boost::system::error_code send(asio::writable_pipe& out_pipe,
                                 const ConnectorMess& to_send);

  boost::system::error_code recv(asio::readable_pipe& in_pipe,
                                 ConnectorMess& received);

  void on_recv(const std::string& raw_data,
               std::vector<ConnectorMess>& received);

  template <class send_handler_type>
  void async_send(asio::writable_pipe& out_pipe,
                  const ConnectorMess& to_send,
                  send_handler_type&& handler);

  template <typename handler_type>
  void async_recv(asio::readable_pipe& stream, handler_type&& handler);
};

template <class send_handler_type>
void protocol::async_send(asio::writable_pipe& out_pipe,
                          const ConnectorMess& mess_to_send,
                          send_handler_type&& handler) {
  auto to_send = serialize(mess_to_send);
  if (_write_pending) {
    _write_queue.emplace_back(to_send, std::move(handler));
  } else {
    _write_pending = true;
    asio::async_write(out_pipe, asio::buffer(to_send.get(), to_send->len),
                      [this, to_call = std::move(handler), to_send, &out_pipe](
                          const boost::system::error_code& err, size_t) {
                        to_call(err);
                        this->_on_send(out_pipe, err);
                      });
  }
}

template <typename handler_type>
void protocol::async_recv(asio::readable_pipe& stream, handler_type&& handler) {
  // async_compose may copy its initiating function on each async step, so the
  // actual state is kept in a separately heap-allocated async_receive_impl and
  // referenced through a shared_ptr captured by value.
  std::shared_ptr<detail::async_receive_impl> async_receiver =
      std::make_shared<detail::async_receive_impl>(stream);
  asio::async_compose<handler_type,
                      void(const boost::system::error_code,
                           const std::shared_ptr<ConnectorMess>&)>(
      [async_receiver](auto& self, const boost::system::error_code& error = {},
                       std::size_t nb_receive = 0) mutable {
        async_receiver->handle(self, error, nb_receive);
      },
      handler, stream);
}

}  // namespace com::centreon::connector::perl
#endif
