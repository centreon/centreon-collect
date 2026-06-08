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
#include <chrono>
#include <future>
#include <thread>

#include "com/centreon/connector/perl/endpoint.hh"

using namespace com::centreon::connector::perl;

extern std::shared_ptr<asio::io_context> g_io_context;

/**
 * @brief Test fixture for endpoint.
 *
 * Creates an io_context and a logger shared by all tests, and provides helpers
 * to build message objects and connected endpoint pairs over anonymous pipes.
 *
 * Wire layout for a pair:
 *   stdin  pipe [1] → parent._parent_stdin (writable)
 *   stdin  pipe [0] → child._child_stdin   (readable)
 *   stdout pipe [1] → child._child_stdout  (writable)
 *   stdout pipe [0] → parent._parent_stdout(readable)
 *   stderr pipe [1] → child._child_stderr  (writable)  [optional]
 *   stderr pipe [0] → parent._parent_stderr(readable)  [optional]
 */
class EndpointTest : public ::testing::Test {
 protected:
  static std::shared_ptr<spdlog::logger> _logger;

  static void SetUpTestSuite() { _logger = spdlog::default_logger(); }

  struct EndpointPair {
    std::shared_ptr<endpoint> parent;
    std::shared_ptr<endpoint> child;
  };

  /**
   * @brief Build a pair of pipe-connected endpoints.
   *
   * @param with_stderr  When true, a third pipe is created for stderr.
   */
  EndpointPair make_connected_pair(bool with_stderr = false) {
    int stdin_fds[2], stdout_fds[2];
    EXPECT_EQ(::pipe(stdin_fds), 0);
    EXPECT_EQ(::pipe(stdout_fds), 0);

    int parent_stderr_fd = -1, child_stderr_fd = -1;
    if (with_stderr) {
      int stderr_fds[2];
      EXPECT_EQ(::pipe(stderr_fds), 0);
      parent_stderr_fd = stderr_fds[0];
      child_stderr_fd = stderr_fds[1];
    }

    // Parent owns: write end of stdin pipe, read ends of stdout (and stderr).
    auto par =
        std::make_shared<endpoint>(g_io_context, _logger, /*is_parent=*/true,
                                   stdin_fds[1],   // stdin  write end
                                   stdout_fds[0],  // stdout read  end
                                   parent_stderr_fd);

    // Child owns: read end of stdin pipe, write ends of stdout (and stderr).
    auto ch =
        std::make_shared<endpoint>(g_io_context, _logger, /*is_parent=*/false,
                                   stdin_fds[0],   // stdin  read  end
                                   stdout_fds[1],  // stdout write end
                                   child_stderr_fd);

    return {par, ch};
  }

  // --- Message factories ---

  static ConnectorMess make_execute(uint64_t id = 1) {
    ConnectorMess msg;
    auto* ex = msg.mutable_execute();
    ex->set_check_id(id);
    ex->set_exe("/usr/lib/nagios/plugins/check_ping");
    ex->add_args("-H");
    ex->add_args("127.0.0.1");
    return msg;
  }

  static ConnectorMess make_result(uint64_t id = 1, int32_t status = 0) {
    ConnectorMess msg;
    auto* res = msg.mutable_result();
    res->set_check_id(id);
    res->set_status(status);
    return msg;
  }

  static ConnectorMess make_terminate() {
    ConnectorMess msg;
    msg.mutable_terminate();
    return msg;
  }
};

std::shared_ptr<spdlog::logger> EndpointTest::_logger;

// ============================================================
//  Synchronous tests
// ============================================================

/**
 * @brief Parent writes an Execute message on stdin; child reads it back.
 *
 * Verifies that the serialise → wire → parse round-trip preserves every field.
 */
TEST_F(EndpointTest, SyncParentWriteChildRead) {
  auto [par, ch] = make_connected_pair();

  ConnectorMess sent = make_execute(42);

  // Write fits in the pipe kernel buffer, so it is non-blocking.
  auto ec = par->write(endpoint::stdin, sent);
  ASSERT_FALSE(ec) << "parent write: " << ec.message();

  ConnectorMess received;
  ec = ch->read(endpoint::stdin, received);
  ASSERT_FALSE(ec) << "child read: " << ec.message();

  ASSERT_TRUE(received.has_execute());
  EXPECT_EQ(received.execute().check_id(), 42u);
  EXPECT_EQ(received.execute().exe(), "/usr/lib/nagios/plugins/check_ping");
  ASSERT_EQ(received.execute().args_size(), 2);
  EXPECT_EQ(received.execute().args(0), "-H");
  EXPECT_EQ(received.execute().args(1), "127.0.0.1");
}

/**
 * @brief Child writes a Result message on stdout; parent reads it back.
 */
TEST_F(EndpointTest, SyncChildWriteParentRead) {
  auto [par, ch] = make_connected_pair();

  ConnectorMess sent = make_result(99, 2);

  auto ec = ch->write(endpoint::stdout, sent);
  ASSERT_FALSE(ec) << "child write: " << ec.message();

  ConnectorMess received;
  ec = par->read(endpoint::stdout, received);
  ASSERT_FALSE(ec) << "parent read: " << ec.message();

  ASSERT_TRUE(received.has_result());
  EXPECT_EQ(received.result().check_id(), 99u);
  EXPECT_EQ(received.result().status(), 2);
}

/**
 * @brief Child sends a message on stderr; parent reads from its stderr pipe.
 */
TEST_F(EndpointTest, SyncChildWriteParentReadStderr) {
  auto [par, ch] = make_connected_pair(/*with_stderr=*/true);

  ConnectorMess sent = make_result(7, 1);

  auto ec = ch->write(endpoint::stderr, sent);
  ASSERT_FALSE(ec) << "child stderr write: " << ec.message();

  ConnectorMess received;
  ec = par->read(endpoint::stderr, received);
  ASSERT_FALSE(ec) << "parent stderr read: " << ec.message();

  ASSERT_TRUE(received.has_result());
  EXPECT_EQ(received.result().check_id(), 7u);
  EXPECT_EQ(received.result().status(), 1);
}

/**
 * @brief Full synchronous round-trip:
 *   parent → (Execute via stdin) → child → (Result via stdout) → parent.
 */
TEST_F(EndpointTest, SyncRoundTrip) {
  auto [par, ch] = make_connected_pair();

  // Step 1: parent sends a request.
  ConnectorMess request = make_execute(7);
  ASSERT_FALSE(par->write(endpoint::stdin, request)) << "parent write";

  // Step 2: child reads the request and sends a response.
  ConnectorMess req_received;
  ASSERT_FALSE(ch->read(endpoint::stdin, req_received)) << "child read";
  ASSERT_TRUE(req_received.has_execute());
  EXPECT_EQ(req_received.execute().check_id(), 7u);

  ConnectorMess response = make_result(7, 0);
  ASSERT_FALSE(ch->write(endpoint::stdout, response)) << "child write";

  // Step 3: parent reads the response.
  ConnectorMess resp_received;
  ASSERT_FALSE(par->read(endpoint::stdout, resp_received)) << "parent read";
  ASSERT_TRUE(resp_received.has_result());
  EXPECT_EQ(resp_received.result().check_id(), 7u);
  EXPECT_EQ(resp_received.result().status(), 0);
}

/**
 * @brief A Terminate message (no payload) survives the round-trip.
 */
TEST_F(EndpointTest, SyncTerminateMessage) {
  auto [par, ch] = make_connected_pair();

  ASSERT_FALSE(par->write(endpoint::stdin, make_terminate()));

  ConnectorMess received;
  ASSERT_FALSE(ch->read(endpoint::stdin, received));
  EXPECT_TRUE(received.has_terminate());
}

/**
 * @brief Send N messages back-to-back and verify each one is received in order.
 *
 * All messages fit in the pipe kernel buffer so no threading is needed.
 */
TEST_F(EndpointTest, SyncMultipleMessagesInOrder) {
  auto [par, ch] = make_connected_pair();

  constexpr int N = 20;

  for (int i = 0; i < N; ++i) {
    ASSERT_FALSE(par->write(endpoint::stdin, make_execute(i)))
        << "write #" << i;
  }

  for (int i = 0; i < N; ++i) {
    ConnectorMess received;
    ASSERT_FALSE(ch->read(endpoint::stdin, received)) << "read #" << i;
    ASSERT_TRUE(received.has_execute());
    EXPECT_EQ(received.execute().check_id(), static_cast<uint64_t>(i));
  }
}

// ============================================================
//  Asynchronous tests
// ============================================================

/**
 * @brief Parent async_write, child sync read (in a worker thread).
 *
 * The child thread blocks on a synchronous read; once io_context executes the
 * async write the data is in the pipe buffer and the child unblocks.
 */
TEST_F(EndpointTest, AsyncParentWriteChildRead) {
  auto [par, ch] = make_connected_pair();

  ConnectorMess sent = make_execute(100);

  // Child reads synchronously in a separate thread so it does not block the
  // io_context.
  auto child_future = std::async(
      std::launch::async,
      [&ch]() -> std::pair<boost::system::error_code, ConnectorMess> {
        ConnectorMess received;
        auto ec = ch->read(endpoint::stdin, received);
        return {ec, received};
      });

  // Parent schedules an async write.
  boost::system::error_code write_ec;
  bool write_done = false;
  absl::Mutex write_wait;

  par->async_write(endpoint::stdin, sent,
                   [&](const boost::system::error_code& ec) {
                     absl::MutexLock l(&write_wait);
                     write_ec = ec;
                     write_done = true;
                   });

  absl::MutexLock lck(&write_wait);
  write_wait.Await(absl::Condition(&write_done));
  ASSERT_TRUE(write_done);
  ASSERT_FALSE(write_ec) << "async write: " << write_ec.message();

  auto [read_ec, received] = child_future.get();
  ASSERT_FALSE(read_ec) << "child read: " << read_ec.message();
  ASSERT_TRUE(received.has_execute());
  EXPECT_EQ(received.execute().check_id(), 100u);
  EXPECT_EQ(received.execute().exe(), "/usr/lib/nagios/plugins/check_ping");
}

/**
 * @brief Child sync write (in a worker thread), parent async_read.
 *
 * The child thread writes to the pipe buffer before (or while) the async_read
 * is waiting; the io_context wakes up and delivers the message.
 */
TEST_F(EndpointTest, AsyncChildWriteParentRead) {
  auto [par, ch] = make_connected_pair();

  ConnectorMess sent = make_result(200, 0);
  // Child writes synchronously from another thread.
  auto child_future = std::async(std::launch::async, [&ch, &sent]() {
    return ch->write(endpoint::stdout, sent);
  });

  // Parent schedules an async read.
  absl::Mutex recv_wait;
  boost::system::error_code read_ec;
  ConnectorMess received;
  bool read_done = false;
  par->async_read(endpoint::stdout,
                  [&](const boost::system::error_code& ec, ConnectorMess msg) {
                    absl::MutexLock l(&recv_wait);
                    read_ec = ec;
                    received = std::move(msg);
                    read_done = true;
                  });

  // Ensure the child has written before we start the event loop, so the pipe
  // buffer already contains data when the async_read poll fires.
  child_future.wait();
  absl::MutexLock lck(&recv_wait);
  recv_wait.Await(absl::Condition(&read_done));

  ASSERT_TRUE(read_done);
  ASSERT_FALSE(read_ec) << "async read: " << read_ec.message();
  ASSERT_TRUE(received.has_result());
  EXPECT_EQ(received.result().check_id(), 200u);
  EXPECT_EQ(received.result().status(), 0);
}

/**
 * @brief Chained async round-trip:
 *   parent async_write →
 *     (on write completion) parent async_read(stdout) →
 *       (on read completion) verify result.
 *
 * The child side uses sync I/O in a dedicated thread.
 */
TEST_F(EndpointTest, AsyncRoundTrip) {
  auto [par, ch] = make_connected_pair();

  ConnectorMess request = make_execute(300);
  ConnectorMess response_msg;
  boost::system::error_code final_ec;
  bool round_trip_done = false;
  absl::Mutex recv_wait;

  // Chain: async_write → async_read.
  par->async_write(
      endpoint::stdin, request, [&](const boost::system::error_code&) {
        par->async_read(
            endpoint::stdout,
            [&](const boost::system::error_code& read_ec, ConnectorMess msg) {
              absl::MutexLock l(&recv_wait);
              final_ec = read_ec;
              response_msg = std::move(msg);
              round_trip_done = true;
            });
      });

  // Child: sync read then sync write in its own thread.
  auto child_thread = std::thread([&ch]() {
    ConnectorMess req;
    if (ch->read(endpoint::stdin, req))
      return;  // I/O error, test will fail on the parent side

    ASSERT_TRUE(req.has_execute());

    ConnectorMess resp;
    resp.mutable_result()->set_check_id(req.execute().check_id());
    resp.mutable_result()->set_status(0);
    ch->write(endpoint::stdout, resp);
  });

  child_thread.join();

  absl::MutexLock lck(&recv_wait);
  recv_wait.Await(absl::Condition(&round_trip_done));

  ASSERT_TRUE(round_trip_done);
  ASSERT_FALSE(final_ec) << "final read: " << final_ec.message();
  ASSERT_TRUE(response_msg.has_result());
  EXPECT_EQ(response_msg.result().check_id(), 300u);
  EXPECT_EQ(response_msg.result().status(), 0);
}

/**
 * @brief Multiple async_write calls are issued back-to-back; a thread reads
 *        them all synchronously and verifies the order is preserved.
 */
TEST_F(EndpointTest, AsyncMultipleWritesOrdered) {
  auto [par, ch] = make_connected_pair();

  constexpr int N = 10;
  int completed = 0;

  // Issue N async writes sequentially (each write is independent).
  for (int i = 0; i < N; ++i) {
    par->async_write(endpoint::stdin, make_execute(i),
                     [&completed, i](const boost::system::error_code& ec) {
                       EXPECT_FALSE(ec)
                           << "async write #" << i << ": " << ec.message();
                       ++completed;
                     });
  }

  // Child reads all N messages in a separate thread.
  auto child_future = std::async(std::launch::async, [&ch, N]() {
    for (int i = 0; i < N; ++i) {
      ConnectorMess received;
      auto ec = ch->read(endpoint::stdin, received);
      if (ec)
        return false;
      if (!received.has_execute())
        return false;
      if (received.execute().check_id() != static_cast<uint64_t>(i))
        return false;
    }
    return true;
  });

  EXPECT_EQ(completed, N);
  EXPECT_TRUE(child_future.get());
}
