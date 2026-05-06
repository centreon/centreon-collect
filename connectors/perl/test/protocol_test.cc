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

#include <absl/synchronization/mutex.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <cstring>
#include <memory>

#include "com/centreon/connector/perl/protocol.hh"

using namespace com::centreon::connector::perl;
namespace asio = boost::asio;

extern std::shared_ptr<asio::io_context> g_io_context;

/**
 * @brief Test fixture for the protocol class.
 *
 * Provides helpers to build ConnectorMess objects and to create connected
 * anonymous-pipe pairs for use with the synchronous send/recv API.
 */
class ProtocolTest : public ::testing::Test {
 protected:
  // ---- message factories -----------------------------------------------

  static ConnectorMess make_execute(uint64_t id = 1) {
    ConnectorMess msg;
    auto* ex = msg.mutable_execute();
    ex->set_cmd_id(id);
    ex->add_args("-H");
    ex->add_args("127.0.0.1");
    return msg;
  }

  static ConnectorMess make_result(uint64_t id = 1, int32_t status = 0) {
    ConnectorMess msg;
    auto* res = msg.mutable_result();
    res->set_cmd_id(id);
    res->set_status(status);
    return msg;
  }

  // ---- pipe helpers ----------------------------------------------------

  /**
   * @brief Pair of Asio pipe objects wrapping a POSIX anonymous pipe.
   *
   * Both ends are closed via their RAII destructors when the struct goes out
   * of scope.
   */
  struct PipePair {
    asio::readable_pipe rp;
    asio::writable_pipe wp;

    explicit PipePair(asio::io_context& io) : rp(io), wp(io) {}
  };

  /**
   * @brief Create a connected anonymous pipe and wrap both ends with Asio
   *        native-handle types.
   */
  PipePair make_pipe() {
    PipePair pp(*g_io_context);
    int fds[2];
    EXPECT_EQ(::pipe(fds), 0);
    boost::system::error_code ec;
    pp.rp.assign(fds[0], ec);
    EXPECT_FALSE(ec) << "rp.assign: " << ec.message();
    pp.wp.assign(fds[1], ec);
    EXPECT_FALSE(ec) << "wp.assign: " << ec.message();
    return pp;
  }
};

// ============================================================
//  send / recv round-trips
// ============================================================

/**
 * @brief Execute message survives a send → recv round-trip.
 *
 * Checks that every field (cmd_id, exe, args) is faithfully preserved after
 * serialisation, wire transfer, and deserialisation.
 */
TEST_F(ProtocolTest, SendRecvExecute) {
  auto pipe = make_pipe();
  protocol proto;

  ConnectorMess sent = make_execute(42);
  auto ec = proto.send(pipe.wp, sent);
  ASSERT_FALSE(ec) << "send: " << ec.message();

  ConnectorMess received;
  ec = proto.recv(pipe.rp, received);
  ASSERT_FALSE(ec) << "recv: " << ec.message();

  ASSERT_TRUE(received.has_execute());
  EXPECT_EQ(received.execute().cmd_id(), 42u);
  ASSERT_EQ(received.execute().args_size(), 2);
  EXPECT_EQ(received.execute().args(0), "-H");
  EXPECT_EQ(received.execute().args(1), "127.0.0.1");
}

/**
 * @brief Result message survives a send → recv round-trip.
 */
TEST_F(ProtocolTest, SendRecvResult) {
  auto pipe = make_pipe();
  protocol proto;

  ConnectorMess sent = make_result(99, 2);
  auto ec = proto.send(pipe.wp, sent);
  ASSERT_FALSE(ec) << "send: " << ec.message();

  ConnectorMess received;
  ec = proto.recv(pipe.rp, received);
  ASSERT_FALSE(ec) << "recv: " << ec.message();

  ASSERT_TRUE(received.has_result());
  EXPECT_EQ(received.result().cmd_id(), 99u);
  EXPECT_EQ(received.result().status(), 2);
}

/**
 * @brief N messages sent back-to-back are received in the same order.
 *
 * All frames fit in the kernel pipe buffer so no extra threading is needed.
 */
TEST_F(ProtocolTest, SendRecvMultipleOrdered) {
  auto pipe = make_pipe();
  protocol proto;

  constexpr int N = 20;
  for (int i = 0; i < N; ++i) {
    ASSERT_FALSE(proto.send(pipe.wp, make_execute(i))) << "send #" << i;
  }
  for (int i = 0; i < N; ++i) {
    ConnectorMess received;
    ASSERT_FALSE(proto.recv(pipe.rp, received)) << "recv #" << i;
    ASSERT_TRUE(received.has_execute());
    EXPECT_EQ(received.execute().cmd_id(), static_cast<uint64_t>(i));
  }
}

/**
 * @brief recv returns an error when the write end is closed before any data.
 *
 * Closing the write end makes the pipe return EOF.  recv should propagate the
 * I/O error rather than block.
 */
TEST_F(ProtocolTest, RecvOnClosedPipeReturnsError) {
  auto pipe = make_pipe();
  protocol proto;

  // Close the write end so the next read hits EOF immediately.
  pipe.wp.close();

  ConnectorMess received;
  auto ec = proto.recv(pipe.rp, received);
  EXPECT_TRUE(ec) << "expected an error on EOF pipe, got none";
}

/**
 * @brief recv returns a protocol_error when the length header encodes a frame
 *        that is too small to hold any payload (len <= sizeof(size_t)).
 *
 * This exercises the sanity check in recv() that guards against malformed or
 * truncated frames.
 */
TEST_F(ProtocolTest, RecvEmptyFrameReturnsProtocolError) {
  auto pipe = make_pipe();
  protocol proto;

  // Write a length-only header with value == sizeof(size_t), meaning zero
  // payload bytes.  The implementation must reject this as a protocol error.
  size_t bad_len = sizeof(size_t);
  boost::system::error_code write_ec;
  asio::write(pipe.wp, asio::buffer(&bad_len, sizeof(bad_len)), write_ec);
  ASSERT_FALSE(write_ec) << "write bad header: " << write_ec.message();

  ConnectorMess received;
  auto ec = proto.recv(pipe.rp, received);
  EXPECT_TRUE(ec) << "expected protocol_error for zero-payload frame";
  EXPECT_EQ(ec, boost::system::errc::protocol_error);
}

// ============================================================
//  on_recv — partial / empty buffer edge cases
// ============================================================

/**
 * @brief on_recv on an empty string returns an empty vector.
 */
TEST_F(ProtocolTest, OnRecvEmptyString) {
  protocol proto;
  std::vector<ConnectorMess> received;
  proto.on_recv("", received);
  EXPECT_TRUE(received.empty());
}

/**
 * @brief on_recv returns an empty vector when the buffer is shorter than the
 *        size_t length header (i.e. the frame is incomplete).
 */
TEST_F(ProtocolTest, OnRecvIncompleteHeader) {
  protocol proto;
  // Feed sizeof(size_t) - 1 bytes — not enough for a complete length header.
  std::string short_buf(sizeof(size_t) - 1, '\x01');
  std::vector<ConnectorMess> result;
  proto.on_recv(short_buf, result);
  EXPECT_TRUE(result.empty());
}

/**
 * @brief on_recv returns an empty vector when the length header is present but
 *        the announced frame body has not yet been received.
 *
 * The header claims 100 bytes total, but only 20 bytes (header + truncated
 * payload) are available.
 */
TEST_F(ProtocolTest, OnRecvTruncatedFrame) {
  protocol proto;
  constexpr size_t claimed_len = 100;
  std::string data(20, '\x00');
  std::memcpy(data.data(), &claimed_len, sizeof(size_t));
  std::vector<ConnectorMess> result;
  proto.on_recv(data, result);
  EXPECT_TRUE(result.empty());
}

/**
 * @brief on_recv decodes a fully-framed ConnectorMess and returns it with all
 *        fields intact.
 *
 * Builds a Result message with every sub-field populated (cmd_id, status,
 * stdout, stderr, AfterFirstCheck, AfterLastCheck), serialises it into the
 * length-prefixed wire format used by the protocol, feeds it to on_recv, and
 * verifies that the returned vector contains exactly one message whose fields
 * match the original.
 */
TEST_F(ProtocolTest, OnRecvCompleteConnectorMess) {
  protocol proto;

  // Build a fully-populated Result message.
  ConnectorMess sent;
  auto* res = sent.mutable_result();
  res->set_cmd_id(77);
  res->set_status(1);
  res->set_stdout("PING OK - Packet loss = 0%");
  auto* after_first = res->mutable_afterfirstcheck();
  after_first->set_used_memory(1024);
  after_first->set_nb_opened_fd(5);
  after_first->set_nb_threads(2);
  auto* after_last = res->mutable_afterlastcheck();
  after_last->set_used_memory(2048);
  after_last->set_nb_opened_fd(7);
  after_last->set_nb_threads(3);

  // Produce a wire frame: [ size_t packet_len ][ serialised protobuf bytes ]
  std::string payload;
  ASSERT_TRUE(sent.SerializeToString(&payload));
  size_t packet_len = sizeof(size_t) + payload.size();
  std::string frame(sizeof(size_t), '\0');
  std::memcpy(frame.data(), &packet_len, sizeof(size_t));
  frame += payload;

  std::vector<ConnectorMess> result;
  proto.on_recv(frame, result);

  ASSERT_EQ(result.size(), 1u);
  const ConnectorMess& got = result[0];
  ASSERT_TRUE(got.has_result());
  EXPECT_EQ(got.result().cmd_id(), 77u);
  EXPECT_EQ(got.result().status(), 1);
  EXPECT_EQ(got.result().stdout(), "PING OK - Packet loss = 0%");
  EXPECT_EQ(got.result().afterfirstcheck().used_memory(), 1024u);
  EXPECT_EQ(got.result().afterfirstcheck().nb_opened_fd(), 5u);
  EXPECT_EQ(got.result().afterfirstcheck().nb_threads(), 2u);
  EXPECT_EQ(got.result().afterlastcheck().used_memory(), 2048u);
  EXPECT_EQ(got.result().afterlastcheck().nb_opened_fd(), 7u);
  EXPECT_EQ(got.result().afterlastcheck().nb_threads(), 3u);
}

// ============================================================
//  async_send / async_recv
// ============================================================

/**
 * @brief Execute message survives an async_send → async_recv round-trip.
 *
 * async_recv is scheduled first so it is ready when async_send fires data into
 * the pipe.  Both operations run on the global io_context thread; the test
 * thread waits on absl::Mutex conditions for each completion.
 */
TEST_F(ProtocolTest, AsyncSendRecvExecute) {
  auto pipe = make_pipe();
  protocol proto;

  absl::Mutex recv_mu;
  bool recv_done = false;
  boost::system::error_code recv_ec;
  std::shared_ptr<ConnectorMess> received;

  proto.async_recv(pipe.rp, [&](const boost::system::error_code& ec,
                                const std::shared_ptr<ConnectorMess>& msg) {
    absl::MutexLock l(&recv_mu);
    recv_ec = ec;
    received = msg;
    recv_done = true;
  });

  absl::Mutex send_mu;
  bool send_done = false;
  boost::system::error_code send_ec;

  proto.async_send(pipe.wp, make_execute(42),
                   [&](const boost::system::error_code& ec) {
                     absl::MutexLock l(&send_mu);
                     send_ec = ec;
                     send_done = true;
                   });

  {
    absl::MutexLock l(&send_mu);
    send_mu.Await(absl::Condition(&send_done));
  }
  ASSERT_FALSE(send_ec) << "async_send: " << send_ec.message();

  {
    absl::MutexLock l(&recv_mu);
    recv_mu.Await(absl::Condition(&recv_done));
  }
  ASSERT_FALSE(recv_ec) << "async_recv: " << recv_ec.message();

  ASSERT_TRUE(received->has_execute());
  EXPECT_EQ(received->execute().cmd_id(), 42u);
  ASSERT_EQ(received->execute().args_size(), 2);
  EXPECT_EQ(received->execute().args(0), "-H");
  EXPECT_EQ(received->execute().args(1), "127.0.0.1");
}

/**
 * @brief Result message survives an async_send → async_recv round-trip.
 */
TEST_F(ProtocolTest, AsyncSendRecvResult) {
  auto pipe = make_pipe();
  protocol proto;

  absl::Mutex recv_mu;
  bool recv_done = false;
  boost::system::error_code recv_ec;
  std::shared_ptr<ConnectorMess> received;

  proto.async_recv(pipe.rp, [&](const boost::system::error_code& ec,
                                const std::shared_ptr<ConnectorMess>& msg) {
    absl::MutexLock l(&recv_mu);
    recv_ec = ec;
    received = msg;
    recv_done = true;
  });

  absl::Mutex send_mu;
  bool send_done = false;
  boost::system::error_code send_ec;

  proto.async_send(pipe.wp, make_result(99, 2),
                   [&](const boost::system::error_code& ec) {
                     absl::MutexLock l(&send_mu);
                     send_ec = ec;
                     send_done = true;
                   });

  {
    absl::MutexLock l(&send_mu);
    send_mu.Await(absl::Condition(&send_done));
  }
  ASSERT_FALSE(send_ec) << "async_send: " << send_ec.message();

  {
    absl::MutexLock l(&recv_mu);
    recv_mu.Await(absl::Condition(&recv_done));
  }
  ASSERT_FALSE(recv_ec) << "async_recv: " << recv_ec.message();

  ASSERT_TRUE(received->has_result());
  EXPECT_EQ(received->result().cmd_id(), 99u);
  EXPECT_EQ(received->result().status(), 2);
}

/**
 * @brief async_recv propagates an error when the write end is closed before
 *        any data is written (EOF on the pipe).
 */
TEST_F(ProtocolTest, AsyncRecvOnClosedPipeReturnsError) {
  auto pipe = make_pipe();
  protocol proto;

  pipe.wp.close();

  absl::Mutex mu;
  bool done = false;
  boost::system::error_code result_ec;

  proto.async_recv(pipe.rp, [&](const boost::system::error_code& ec,
                                const std::shared_ptr<ConnectorMess>&) {
    absl::MutexLock l(&mu);
    result_ec = ec;
    done = true;
  });

  absl::MutexLock l(&mu);
  mu.Await(absl::Condition(&done));
  EXPECT_TRUE(result_ec) << "expected an error on EOF pipe, got none";
}

/**
 * @brief async_recv returns protocol_error when the length header encodes a
 *        zero-payload frame (len == sizeof(size_t)).
 */
TEST_F(ProtocolTest, AsyncRecvEmptyFrameReturnsProtocolError) {
  auto pipe = make_pipe();
  protocol proto;

  size_t bad_len = sizeof(size_t);
  boost::system::error_code write_ec;
  asio::write(pipe.wp, asio::buffer(&bad_len, sizeof(bad_len)), write_ec);
  ASSERT_FALSE(write_ec) << "write bad header: " << write_ec.message();
  pipe.wp.close();

  absl::Mutex mu;
  bool done = false;
  boost::system::error_code result_ec;

  proto.async_recv(pipe.rp, [&](const boost::system::error_code& ec,
                                const std::shared_ptr<ConnectorMess>&) {
    absl::MutexLock l(&mu);
    result_ec = ec;
    done = true;
  });

  absl::MutexLock l(&mu);
  mu.Await(absl::Condition(&done));
  EXPECT_TRUE(result_ec) << "expected protocol_error for zero-payload frame";
  EXPECT_EQ(result_ec, boost::system::errc::protocol_error);
}

/**
 * @brief N consecutive async_send calls are queued and all complete in order.
 *
 * The first call goes directly to asio::async_write; the remaining N-1 are
 * held in _write_queue and drained by _on_send.  After all sends complete the
 * messages are read back synchronously to verify ordering.
 */
TEST_F(ProtocolTest, AsyncSendQueuesMultiple) {
  auto pipe = make_pipe();
  protocol proto;

  constexpr int N = 5;
  absl::Mutex mu;
  int completed = 0;
  bool all_done = false;

  for (int i = 0; i < N; ++i) {
    proto.async_send(
        pipe.wp, make_execute(i), [&, i](const boost::system::error_code& ec) {
          EXPECT_FALSE(ec) << "async_send #" << i << ": " << ec.message();
          absl::MutexLock l(&mu);
          if (++completed == N)
            all_done = true;
        });
  }

  {
    absl::MutexLock l(&mu);
    mu.Await(absl::Condition(&all_done));
  }
  ASSERT_EQ(completed, N);

  for (int i = 0; i < N; ++i) {
    ConnectorMess received;
    ASSERT_FALSE(proto.recv(pipe.rp, received)) << "recv #" << i;
    ASSERT_TRUE(received.has_execute());
    EXPECT_EQ(received.execute().cmd_id(), static_cast<uint64_t>(i));
  }
}

/**
 * @brief Chained async round-trip: async_send, then async_recv in the send
 *        completion handler.
 *
 * Once async_send completes the pipe buffer already holds the frame, so the
 * subsequent async_recv resolves immediately without blocking.
 */
TEST_F(ProtocolTest, AsyncRoundTrip) {
  auto pipe = make_pipe();
  protocol proto;

  absl::Mutex mu;
  bool done = false;
  boost::system::error_code final_ec;
  std::shared_ptr<ConnectorMess> received_msg;

  proto.async_send(
      pipe.wp, make_execute(77), [&](const boost::system::error_code& send_ec) {
        if (send_ec) {
          absl::MutexLock l(&mu);
          final_ec = send_ec;
          done = true;
          return;
        }
        proto.async_recv(pipe.rp,
                         [&](const boost::system::error_code& recv_ec,
                             const std::shared_ptr<ConnectorMess>& msg) {
                           absl::MutexLock l(&mu);
                           final_ec = recv_ec;
                           received_msg = msg;
                           done = true;
                         });
      });

  absl::MutexLock l(&mu);
  mu.Await(absl::Condition(&done));

  ASSERT_FALSE(final_ec) << "round-trip: " << final_ec.message();
  ASSERT_TRUE(received_msg->has_execute());
  EXPECT_EQ(received_msg->execute().cmd_id(), 77u);
}
