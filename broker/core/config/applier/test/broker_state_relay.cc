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
#include "com/centreon/common/file.hh"
#include "common/engine_conf/state.pb.h"
#include <absl/cleanup/cleanup.h>
#include <gtest/gtest.h>

using namespace com::centreon::broker;
using com::centreon::broker::config::applier::broker_state;

namespace cccommon = com::centreon::common;

class BrokerStateRelayTest : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;
  /* Never created: no .prot file can be found there, whatever the machine. */
  const std::filesystem::path _pollers_dir =
      std::filesystem::temp_directory_path() /
      ("broker_state_relay_" + std::to_string(getpid()) + "_missing");

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
  state.set_pollers_config_dir(_pollers_dir);
  EXPECT_FALSE(state.is_relay());
}

/* supports_centralized_conf() mirrors !is_relay(). */
TEST_F(BrokerStateRelayTest, SupportsCentralizedConfWhenPollersDirSet) {
  broker_state state(_logger);
  state.set_pollers_config_dir(_pollers_dir);
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
  state.set_pollers_config_dir(_pollers_dir);
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

/*
 * config_generation(): every change of the state read by broker_stream::read()
 * increments it, so that streams know they have something to check.
 */
TEST_F(BrokerStateRelayTest, ConfigGenerationIncrementsOnChanges) {
  broker_state state(_logger);
  uint64_t gen = state.config_generation();

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1");
  EXPECT_GT(state.config_generation(), gen);
  gen = state.config_generation();

  state.push_pending_diff_state(1u, nullptr);
  EXPECT_GT(state.config_generation(), gen);
  gen = state.config_generation();

  state.push_pending_diff_state_ack(nullptr);
  EXPECT_GT(state.config_generation(), gen);
  gen = state.config_generation();

  state.set_poller_engine_conf(1u, "hash-v2");
  EXPECT_GT(state.config_generation(), gen);
}

/*
 * config_generation(): the pop_*() methods only remove work, they must not
 * increment it, otherwise each stream would rescan after its own pops.
 */
TEST_F(BrokerStateRelayTest, ConfigGenerationUnchangedByPops) {
  broker_state state(_logger);
  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1");
  state.push_pending_diff_state(1u, nullptr);
  state.push_pending_diff_state_ack(nullptr);
  const uint64_t gen = state.config_generation();

  state.pop_pending_config_requests();
  state.pop_pending_diff_state_for_engine(1u);
  state.pop_pending_diff_state_acks();
  state.pop_pending_config_revokes(1u);
  /* Not a pop but also only removes work. */
  state.clear_pending_for_poller(1u);
  EXPECT_EQ(state.config_generation(), gen);
}

/*
 * config_generation(): changes that give no new work to broker_stream::read()
 * must not increment it.
 */
TEST_F(BrokerStateRelayTest, ConfigGenerationUnchangedWithoutNewWork) {
  broker_state state(_logger);
  state.set_pollers_config_dir(_pollers_dir);
  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1");
  state.set_poller_engine_conf(1u, "hash-v2");
  const uint64_t gen = state.config_generation();

  /* Same values as before. */
  state.set_pollers_config_dir(_pollers_dir);
  state.set_poller_engine_conf(1u, "hash-v2");
  /* Unknown poller. */
  state.set_poller_engine_conf(42u, "hash-v3");
  /* Fields not read by broker_stream::read() or removing work. */
  state.set_instance_running(1u, true);
  state.set_poller_engine_conf_unknown(1u, true);
  state.acknowledge_engine_peer(1u);
  state.set_available_conf_sent_to_engine_peer(1u);
  /* Not all peers acknowledged: nothing reset. */
  state.add_peer(2u, "poller-2", "broker-2", cccommon::ENGINE, true, "hash-v1");
  const uint64_t gen2 = state.config_generation();
  state.set_available_conf_sent_to_engine_peer(2u);
  EXPECT_FALSE(state.all_engine_peers_acknowledged());

  EXPECT_GT(gen2, gen); /* add_peer() bumps */
  EXPECT_EQ(state.config_generation(), gen2);
}

/*
 * config_generation(): when all peers acknowledged, available_conf_sent flags
 * are reset. If no peer has an available_conf different from its engine_conf,
 * this gives no work to broker_stream::read(): no increment.
 */
TEST_F(BrokerStateRelayTest, ConfigGenerationUnchangedOnAcknowledgeResetNoWork) {
  broker_state state(_logger);
  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1");
  state.set_available_conf_sent_to_engine_peer(1u);
  state.acknowledge_engine_peer(1u);
  const uint64_t gen = state.config_generation();

  EXPECT_TRUE(state.all_engine_peers_acknowledged());
  EXPECT_EQ(state.config_generation(), gen);
}

/*
 * config_generation(): when all peers acknowledged and one of them has an
 * available_conf different from its engine_conf, the reset makes it need an
 * update again: it must be incremented.
 */
TEST_F(BrokerStateRelayTest, ConfigGenerationIncrementsOnAcknowledgeReset) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() /
      ("broker_state_relay_" + std::to_string(getpid()) + "_ack");
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  absl::Cleanup cleanup = [&dir] { std::filesystem::remove_all(dir); };

  /* {N}.prot gives available_conf = "hash-v2" to the peer known in
   * "hash-v1" (see prepare_relay_config_response(), case 3). */
  com::centreon::engine::configuration::State conf;
  conf.set_config_version("hash-v2");
  cccommon::save_proto_to_disk(dir / "1.prot", conf);

  broker_state state(_logger);
  state.set_pollers_config_dir(dir);
  state.register_engine_peer_via_relay(1u, "poller-1", 10u, "hash-v1");
  ASSERT_EQ(state.prepare_relay_config_response(1u, "hash-v1"),
            broker_state::relay_config_response::diff_ready);
  state.set_available_conf_sent_to_engine_peer(1u);
  state.acknowledge_engine_peer(1u);
  const uint64_t gen = state.config_generation();

  EXPECT_TRUE(state.all_engine_peers_acknowledged());
  EXPECT_GT(state.config_generation(), gen);
}
