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

#include "broker/core/config/applier/broker_state.hh"
#include <gtest/gtest.h>

using namespace com::centreon::broker;
using com::centreon::broker::config::applier::broker_state;

namespace cccommon = com::centreon::common;

class BrokerStateRelayTest : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

  void SetUp() override { _logger = spdlog::default_logger(); }
};

/* is_relay() returns true when pollers_config_dir is not set (default). */
TEST_F(BrokerStateRelayTest, IsRelayWhenPollersDirEmpty) {
  broker_state state(_logger);
  EXPECT_TRUE(state.is_relay());
}

/* is_relay() returns false once a pollers_config_dir is configured. */
TEST_F(BrokerStateRelayTest, IsNotRelayWhenPollersDirSet) {
  broker_state state(_logger);
  state.set_pollers_config_dir("/tmp/fake-pollers");
  EXPECT_FALSE(state.is_relay());
}

/* supports_centralized_conf() mirrors !is_relay(). */
TEST_F(BrokerStateRelayTest, SupportsCentralizedConfWhenPollersDirSet) {
  broker_state state(_logger);
  state.set_pollers_config_dir("/tmp/fake-pollers");
  EXPECT_TRUE(state.supports_centralized_conf());
}

TEST_F(BrokerStateRelayTest, DoesNotSupportCentralizedConfWhenPollersDirEmpty) {
  broker_state state(_logger);
  EXPECT_FALSE(state.supports_centralized_conf());
}

/*
 * Relay detection: adding an ENGINE peer with extended_negotiation in relay
 * mode queues a ConfigRequest for that poller.
 */
TEST_F(BrokerStateRelayTest,
       RelayQueuesConfigRequestOnEnginePeerWithExtendedNegotiation) {
  broker_state state(_logger);
  /* pollers_config_dir is empty → is_relay() == true */

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1");

  auto requests = state.pop_pending_config_requests();
  ASSERT_EQ(requests.size(), 1u);
  EXPECT_EQ(std::get<0>(requests[0]), 1u);
  EXPECT_EQ(std::get<1>(requests[0]), "poller-1");
  EXPECT_EQ(std::get<2>(requests[0]), "hash-v1");
}

/*
 * Central broker: adding an ENGINE peer must NOT produce a ConfigRequest
 * (the central handles the config directly, it does not forward upstream).
 */
TEST_F(BrokerStateRelayTest, CentralDoesNotQueueConfigRequestOnEnginePeer) {
  broker_state state(_logger);
  state.set_pollers_config_dir("/tmp/fake-pollers");
  /* is_relay() == false */

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1");

  auto requests = state.pop_pending_config_requests();
  EXPECT_TRUE(requests.empty());
}

/* pop_pending_config_requests() drains the queue: second call returns empty. */
TEST_F(BrokerStateRelayTest, PopPendingConfigRequestsDrainsQueue) {
  broker_state state(_logger);

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1");

  auto first = state.pop_pending_config_requests();
  ASSERT_EQ(first.size(), 1u);

  auto second = state.pop_pending_config_requests();
  EXPECT_TRUE(second.empty());
}

/*
 * ENGINE peer without extended_negotiation must not be queued even in relay
 * mode (the peer does not participate in centralized config).
 */
TEST_F(BrokerStateRelayTest, RelayDoesNotQueueWhenNoExtendedNegotiation) {
  broker_state state(_logger);

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE,
                 /*extended_negotiation=*/false, "hash-v1");

  auto requests = state.pop_pending_config_requests();
  EXPECT_TRUE(requests.empty());
}

/* Multiple ENGINE peers in relay mode: all are queued. */
TEST_F(BrokerStateRelayTest, RelayQueuesMultiplePollers) {
  broker_state state(_logger);

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "v1");
  state.add_peer(2u, "poller-2", "broker-2", cccommon::ENGINE, true, "v2");

  auto requests = state.pop_pending_config_requests();
  ASSERT_EQ(requests.size(), 2u);

  bool found1 = false, found2 = false;
  for (const auto& [pid, pname, ver] : requests) {
    if (pid == 1u) {
      found1 = true;
      EXPECT_EQ(ver, "v1");
      EXPECT_EQ(pname, "poller-1");
    } else if (pid == 2u) {
      found2 = true;
      EXPECT_EQ(ver, "v2");
      EXPECT_EQ(pname, "poller-2");
    }
  }
  EXPECT_TRUE(found1);
  EXPECT_TRUE(found2);
}

/*
 * BROKER peers are never queued (only ENGINE peers connect through a relay
 * and need a ConfigRequest forwarded upstream).
 */
TEST_F(BrokerStateRelayTest, RelayDoesNotQueueBrokerPeer) {
  broker_state state(_logger);

  state.add_peer(3u, "remote-broker", "broker-3", cccommon::BROKER, true, "");

  auto requests = state.pop_pending_config_requests();
  EXPECT_TRUE(requests.empty());
}
