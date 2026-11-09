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

#include <gtest/gtest.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "broker/core/bbdo/stream.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/io/extension.hh"
#include "com/centreon/broker/io/raw.hh"

using namespace com::centreon::broker;

namespace {

using extension_list = std::list<std::shared_ptr<io::extension>>;

/**
 * @brief One direction of an in-memory connection: what one end writes, the
 * other end reads.
 */
class channel {
  std::mutex _m;
  std::condition_variable _cv;
  std::deque<std::vector<char>> _buffers;

 public:
  void push(const std::vector<char>& buffer) {
    {
      std::lock_guard<std::mutex> lck(_m);
      _buffers.push_back(buffer);
    }
    _cv.notify_all();
  }

  bool pop(std::vector<char>& buffer, time_t deadline) {
    std::unique_lock<std::mutex> lck(_m);
    auto has_data = [this] { return !_buffers.empty(); };
    if (deadline == static_cast<time_t>(-1))
      _cv.wait(lck, has_data);
    else if (!_cv.wait_until(lck,
                             std::chrono::system_clock::from_time_t(deadline),
                             has_data))
      return false;
    buffer = std::move(_buffers.front());
    _buffers.pop_front();
    return true;
  }
};

/**
 * @brief Transport stream standing for the TCP connection under a BBDO
 * stream.
 */
class pipe_end : public io::stream {
  std::shared_ptr<channel> _in;
  std::shared_ptr<channel> _out;

 public:
  pipe_end(std::shared_ptr<channel> in, std::shared_ptr<channel> out)
      : io::stream("pipe_end"), _in{std::move(in)}, _out{std::move(out)} {}

  bool read(std::shared_ptr<io::data>& d, time_t deadline) override {
    d.reset();
    std::vector<char> buffer;
    if (!_in->pop(buffer, deadline))
      return false;
    auto raw = std::make_shared<io::raw>();
    raw->get_buffer() = std::move(buffer);
    d = raw;
    return true;
  }

  int write(const std::shared_ptr<io::data>& d) override {
    _out->push(std::static_pointer_cast<io::raw>(d)->get_buffer());
    return 1;
  }

  int32_t stop() override { return 0; }
};

struct negotiation_result {
  std::string central_error;
  std::string poller_error;
};

extension_list make_extension(const std::string& name,
                              bool optional,
                              bool mandatory) {
  return {std::make_shared<io::extension>(name, optional, mandatory)};
}

/**
 * @brief Negotiate a BBDO connection between a central input (acceptor side)
 * and a poller output (connector side), each with its own extensions.
 *
 * @return The error raised by each side, empty when its negotiation succeeded.
 */
negotiation_result negotiate(const extension_list& central_extensions,
                             const extension_list& poller_extensions,
                             bool central_negotiates = true) {
  auto to_central = std::make_shared<channel>();
  auto to_poller = std::make_shared<channel>();

  bbdo::stream central(true, false, central_extensions);
  central.set_substream(std::make_shared<pipe_end>(to_central, to_poller));
  central.set_negotiate(central_negotiates);
  central.set_timeout(2);

  bbdo::stream poller(false, false, poller_extensions);
  poller.set_substream(std::make_shared<pipe_end>(to_poller, to_central));
  poller.set_timeout(2);

  negotiation_result result;
  std::thread poller_side([&poller, &result] {
    try {
      poller.negotiate(bbdo::stream::negotiate_first);
    } catch (const std::exception& e) {
      result.poller_error = e.what();
    }
  });
  try {
    central.negotiate(bbdo::stream::negotiate_second);
  } catch (const std::exception& e) {
    result.central_error = e.what();
  }
  poller_side.join();
  return result;
}

class BbdoNegotiation : public ::testing::Test {
 public:
  void SetUp() override {
    try {
      config::applier::init(com::centreon::common::BROKER, 0, "broker_test", 0);
    } catch (const std::exception&) {
    }
  }

  void TearDown() override { config::applier::deinit(); }
};

}  // namespace

/* Central input with TLS set to 'yes', poller output with TLS set to 'no':
 * the central must not accept the poller in clear. */
TEST_F(BbdoNegotiation, CentralRefusesPollerWithoutTls) {
  negotiation_result result = negotiate(make_extension("TLS", false, true), {});
  ASSERT_NE(result.central_error.find("TLS"), std::string::npos)
      << "central accepted a poller without TLS";
}

/* Poller output with TLS set to 'yes', central input with TLS set to 'no':
 * the poller must not send its data in clear. */
TEST_F(BbdoNegotiation, PollerRefusesCentralWithoutTls) {
  negotiation_result result = negotiate({}, make_extension("TLS", false, true));
  ASSERT_NE(result.poller_error.find("TLS"), std::string::npos)
      << "poller accepted a central without TLS";
  ASSERT_EQ(result.central_error, "");
}

/* With 'negotiation' set to 'no', the central still announces TLS since it is
 * mandatory, so a poller without TLS is still refused. */
TEST_F(BbdoNegotiation, CentralWithoutNegotiationRefusesPollerWithoutTls) {
  negotiation_result result =
      negotiate(make_extension("TLS", false, true), {}, false);
  ASSERT_NE(result.central_error.find("TLS"), std::string::npos)
      << "central accepted a poller without TLS";
}

/* TLS set to 'yes' on both sides: the connection is accepted. */
TEST_F(BbdoNegotiation, TlsOnBothSidesIsAccepted) {
  negotiation_result result = negotiate(make_extension("TLS", false, true),
                                        make_extension("TLS", false, true));
  ASSERT_EQ(result.central_error, "");
  ASSERT_EQ(result.poller_error, "");
}

/* TLS set to 'auto' means TLS if the peer supports it: a poller without TLS
 * is accepted. */
TEST_F(BbdoNegotiation, TlsAutoAcceptsPollerWithoutTls) {
  negotiation_result result = negotiate(make_extension("TLS", true, false), {});
  ASSERT_EQ(result.central_error, "");
  ASSERT_EQ(result.poller_error, "");
}

/* Compression is not a security setting: compression set to 'yes' on one side
 * only still falls back to an uncompressed connection. */
TEST_F(BbdoNegotiation, CompressionYesAcceptsPeerWithoutCompression) {
  negotiation_result result =
      negotiate(make_extension("COMPRESSION", false, true), {});
  ASSERT_EQ(result.central_error, "");
  ASSERT_EQ(result.poller_error, "");
}
