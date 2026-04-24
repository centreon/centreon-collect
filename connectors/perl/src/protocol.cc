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

#include "com/centreon/connector/perl/protocol.hh"

using namespace com::centreon::connector::perl;

/**
 * @brief Serialize a protobuf message into a length-prefixed proto_buffer.
 *
 * The returned buffer is laid out as:
 *   [ size_t len ][ serialized protobuf bytes ]
 * where len = sizeof(size_t) + ByteSizeLong(mess).
 *
 * The buffer is heap-allocated with malloc so that the flexible array member
 * `data` is contiguous with `len`; it is freed via a custom deleter.
 */
std::shared_ptr<protocol::proto_buffer> protocol::serialize(
    const ::google::protobuf::Message& mess) {
  size_t mess_len = mess.ByteSizeLong();
  size_t packet_len = mess_len + sizeof(proto_buffer);

  proto_buffer* b = static_cast<proto_buffer*>(malloc(packet_len));
  b->len = packet_len;
  mess.SerializeToArray(b->data, mess_len);
  return std::shared_ptr<proto_buffer>(
      b, [](proto_buffer* to_free) { free(to_free); });
}

/**
 * @brief Parse one framed message from a raw buffer (not currently used by
 *        async paths; kept for potential synchronous bulk-read scenarios).
 *
 * @return Number of bytes consumed from @p buff, or 0 if the buffer does not
 *         yet hold a complete frame.
 */
size_t protocol::_parse(const void* buff, size_t len, ConnectorMess& mess) {
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

/**
 * @brief Synchronously send a framed ConnectorMess on @p stream.
 *
 * Serializes the message, then calls asio::write which loops internally until
 * either the full frame is written or an error occurs.
 */
boost::system::error_code protocol::send(asio::writable_pipe& out_pipe,
                                         const ConnectorMess& mess) {
  std::shared_ptr<proto_buffer> to_send = serialize(mess);
  boost::system::error_code ec;
  asio::write(out_pipe, asio::buffer(to_send.get(), to_send->len), ec);
  return ec;
}

void protocol::_on_send(asio::writable_pipe& out_pipe,
                        const boost::system::error_code& err) {
  if (err || _write_queue.empty()) {
    _write_pending = false;
    return;
  }
  auto to_send = _write_queue.front();
  _write_queue.pop_front();
  asio::async_write(
      out_pipe, asio::buffer(to_send.first.get(), to_send.first->len),
      [this, buff_saved = to_send.first, to_call = std::move(to_send.second),
       &out_pipe](const boost::system::error_code& err, size_t) {
        to_call(err);
        _on_send(out_pipe, err);
      });
}

/**
 * @brief Synchronously receive one framed ConnectorMess from @p stream.
 *
 * Reads the length header first, then allocates a buffer and reads the
 * protobuf payload.  Returns a protocol_error if the decoded length indicates
 * an empty or malformed frame.
 */
boost::system::error_code protocol::recv(asio::readable_pipe& in_pipe,
                                         ConnectorMess& received) {
  boost::system::error_code err;
  // Step 1: read the 8-byte length header to know how many payload bytes
  // follow.
  size_t len;
  asio::read(in_pipe, asio::mutable_buffer(&len, sizeof(len)), err);
  if (err) {
    return err;
  }
  if (len <= sizeof(proto_buffer)) {  // empty payload => protocol error
    return boost::system::errc::make_error_code(
        boost::system::errc::protocol_error);
  }
  size_t data_len = len - sizeof(proto_buffer);

  // Step 2: read exactly data_len bytes of serialized protobuf.
  std::unique_ptr<unsigned char[]> raw =
      std::make_unique<unsigned char[]>(data_len);

  asio::read(in_pipe, asio::mutable_buffer(raw.get(), data_len), err);
  if (err) {
    return err;
  }

  // Step 3: deserialize the protobuf message.
  try {
    received.ParseFromArray(raw.get(), data_len);
  } catch (const std::exception& e) {
    return boost::system::errc::make_error_code(
        boost::system::errc::protocol_error);
  }
  return err;
}

/**
 * @brief Feed a chunk of raw data into the reassembly buffer and return all
 *        complete messages decoded so far.
 *
 * Appends @p raw_data to the internal `_recv_buffer`, then walks the buffer
 * extracting every complete length-prefixed frame in sequence.  Bytes that
 * belong to a partial frame at the end of the buffer are retained for the
 * next call, making the method suitable for use in async read callbacks where
 * data may arrive in arbitrary chunks.
 *
 * @param raw_data  Arbitrary-length chunk of bytes received from the pipe or
 *                  socket (may span multiple frames or hold only part of one).
 * @return          Zero or more fully decoded `ConnectorMess` objects, in
 *                  arrival order.
 * @throws com::centreon::exceptions::msg_fmt if a complete frame cannot be
 *         parsed as a valid protobuf message.
 */
void protocol::on_recv(const std::string& raw_data,
                       std::vector<ConnectorMess>& received) {
  _recv_buffer += raw_data;
  size_t offset = 0;
  while (true) {
    size_t remaining = _recv_buffer.size() - offset;
    if (remaining < sizeof(size_t)) {
      break;
    }
    size_t packet_len =
        *(reinterpret_cast<const size_t*>(_recv_buffer.data() + offset));
    if (remaining < packet_len) {
      break;
    }
    size_t data_len = packet_len - sizeof(size_t);
    try {
      ConnectorMess mess;
      if (!mess.ParseFromArray(_recv_buffer.data() + offset + sizeof(size_t),
                               data_len)) {
        throw exceptions::msg_fmt("fail to decode protobuf message");
      }
      received.push_back(std::move(mess));
    } catch (const exceptions::msg_fmt& e) {
      throw;
    } catch (const std::exception& e) {
      throw exceptions::msg_fmt("fail to decode protobuf message: {}",
                                e.what());
    }
    offset += packet_len;
  }
  _recv_buffer.erase(0, offset);
}
