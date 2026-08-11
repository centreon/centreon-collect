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

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include "broker/core/config/applier/broker_state.hh"

using namespace com::centreon::broker;
using com::centreon::broker::config::applier::broker_state;
namespace cfg = com::centreon::engine::configuration;

namespace cccommon = com::centreon::common;

class BrokerStateRelayTest : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

  void SetUp() override { _logger = spdlog::default_logger(); }
};

/* Tests of load_foreign_objects(), which reads the configurations Broker stores
 * for the other pollers so a per-poller validation can tell "undefined" from
 * "defined on another poller". They need a real directory of .prot files. */
class BrokerStateForeignTest : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;
  std::filesystem::path _dir;

  void SetUp() override {
    _logger = spdlog::default_logger();
    /* One directory per test, so a leftover of a previous run or of another
     * test can never be read as a poller configuration. */
    _dir = std::filesystem::temp_directory_path() /
           fmt::format(
               "broker_state_foreign_{}_{}", getpid(),
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
    std::filesystem::remove_all(_dir);
    std::filesystem::create_directories(_dir);
  }

  void TearDown() override { std::filesystem::remove_all(_dir); }

  /* Write a serialized State holding one host and one of its services under
   * @a file_name, the way Broker stores a poller configuration. */
  void write_state(const std::string& file_name,
                   const std::string& host_name,
                   const std::string& service_description) {
    cfg::State s;
    auto* h = s.add_hosts();
    h->set_host_name(host_name);
    auto* svc = s.add_services();
    svc->set_host_name(host_name);
    svc->set_service_description(service_description);
    std::ofstream f(_dir / file_name, std::ios::binary);
    ASSERT_TRUE(s.SerializeToOstream(&f));
  }
};

/* The work files of a push in progress are not stored configurations and must
 * not be read as such. The store itself is read whole; it is `self` that keeps
 * the validated poller out of the answers, so the same load serves a whole
 * batch. */
TEST_F(BrokerStateForeignTest, SkipsWorkFilesAndFiltersOnSelf) {
  broker_state state(_logger);
  state.set_pollers_config_dir(_dir);
  write_state("1.prot", "host_of_1", "svc_of_1");
  write_state("3.prot", "host_of_3", "svc_of_3");
  write_state("new-4.prot", "host_of_4", "svc_of_4");
  write_state("diff-5.prot", "host_of_5", "svc_of_5");

  auto foreign = state.load_foreign_objects();
  foreign.objects.self = 1;

  EXPECT_EQ(foreign.objects.poller_of_host("host_of_3"), 3u);
  EXPECT_EQ(foreign.objects.poller_of_host("host_of_1"), 0u);
  EXPECT_EQ(foreign.objects.poller_of_host("host_of_4"), 0u);
  EXPECT_EQ(foreign.objects.poller_of_host("host_of_5"), 0u);
  EXPECT_EQ(foreign.objects.hosts.size(), 2u);

  /* Same load, next poller of the batch: only `self` moves. */
  foreign.objects.self = 3;
  EXPECT_EQ(foreign.objects.poller_of_host("host_of_1"), 1u);
  EXPECT_EQ(foreign.objects.poller_of_host("host_of_3"), 0u);
}

/* Services are indexed by {host name, service description}. Reading them after
 * load_foreign_objects() has returned also proves the borrowed strings survive
 * the return: the messages live behind unique_ptr, so moving the result never
 * moves them. */
TEST_F(BrokerStateForeignTest, IndexesServicesByHostAndDescription) {
  broker_state state(_logger);
  state.set_pollers_config_dir(_dir);
  write_state("2.prot", "host_of_2", "svc_of_2");
  write_state("7.prot", "host_of_7", "svc_of_7");

  auto foreign = state.load_foreign_objects();
  foreign.objects.self = 2;

  EXPECT_EQ(foreign.objects.poller_of_service("host_of_7", "svc_of_7"), 7u);
  EXPECT_EQ(foreign.objects.poller_of_service("host_of_7", "ghost"), 0u);
  EXPECT_EQ(foreign.objects.poller_of_service("host_of_2", "svc_of_2"), 0u);
  EXPECT_EQ(foreign.objects.services.size(), 2u);
}

/* No pollers configuration directory (a relay, or a fresh central) yields an
 * empty index, and the validation then behaves as if it had no global view. */
TEST_F(BrokerStateForeignTest, EmptyWithoutPollersConfigDir) {
  broker_state state(_logger);

  auto foreign = state.load_foreign_objects();

  EXPECT_TRUE(foreign.objects.hosts.empty());
  EXPECT_TRUE(foreign.objects.services.empty());
  EXPECT_TRUE(foreign.states.empty());
}

/* An unreadable configuration costs the precision of the diagnostics about that
 * poller, nothing more: the others are still indexed. */
TEST_F(BrokerStateForeignTest, SkipsUnreadableConfiguration) {
  broker_state state(_logger);
  state.set_pollers_config_dir(_dir);
  write_state("8.prot", "host_of_8", "svc_of_8");
  {
    /* Wire type 7 does not exist, so protobuf is guaranteed to reject this —
     * unlike arbitrary text, which it may happen to parse. */
    std::ofstream f(_dir / "9.prot", std::ios::binary);
    f.put(static_cast<char>(0x0f));
  }

  auto foreign = state.load_foreign_objects();

  EXPECT_EQ(foreign.objects.poller_of_host("host_of_8"), 8u);
  EXPECT_EQ(foreign.states.size(), 1u);
}

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

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1",
                 "");

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

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1",
                 "");

  auto requests = state.pop_pending_config_requests();
  EXPECT_TRUE(requests.empty());
}

/* pop_pending_config_requests() drains the queue: second call returns empty. */
TEST_F(BrokerStateRelayTest, PopPendingConfigRequestsDrainsQueue) {
  broker_state state(_logger);

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "hash-v1",
                 "");

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
                 /*extended_negotiation=*/false, "hash-v1", "");

  auto requests = state.pop_pending_config_requests();
  EXPECT_TRUE(requests.empty());
}

/* Multiple ENGINE peers in relay mode: all are queued. */
TEST_F(BrokerStateRelayTest, RelayQueuesMultiplePollers) {
  broker_state state(_logger);

  state.add_peer(1u, "poller-1", "broker-1", cccommon::ENGINE, true, "v1", "");
  state.add_peer(2u, "poller-2", "broker-2", cccommon::ENGINE, true, "v2", "");

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

  state.add_peer(3u, "remote-broker", "broker-3", cccommon::BROKER, true, "",
                 "");

  auto requests = state.pop_pending_config_requests();
  EXPECT_TRUE(requests.empty());
}
