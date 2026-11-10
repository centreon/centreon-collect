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

#include <absl/algorithm/container.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "com/centreon/broker/multiplexing/publisher.hh"

using namespace com::centreon::broker;
using config::applier::broker_state;

/**
 * @brief When a round of configuration is over, and when it is not.
 *
 * The question matters because closing a round makes the caller build the
 * global diff, and that consumes every diff-<N>.prot of the directory. Closing
 * it while a poller has a diff prepared but not yet handed over destroys that
 * file: it is never written again, so the poller keeps asking to be updated and
 * Broker keeps failing to open a file that no longer exists.
 *
 * The race is a matter of tens of milliseconds in real life -- one poller
 * acknowledging while another's stream has not emitted yet -- so it cannot be
 * reproduced by a functional test. Hence the peers are forged here.
 */
class BrokerStateRound : public ::testing::Test {
 protected:
  broker_state* _state = nullptr;
  std::filesystem::path _dir;
  std::filesystem::path _cache_dir;

 public:
  void SetUp() override {
    config::applier::state::load<broker_state>("unittest");
    _state = static_cast<broker_state*>(&config::applier::state::instance());
    _dir = std::filesystem::temp_directory_path() / "ut_broker_state_round";
    _cache_dir =
        std::filesystem::temp_directory_path() / "ut_broker_state_round_cache";
    for (const auto& d : {_dir, _cache_dir}) {
      std::filesystem::remove_all(d);
      std::filesystem::create_directories(d);
    }
    _state->set_pollers_config_dir(_dir);
    /* Known and watched, as on a real central: the announcements PHP pushes
     * live there. None is left in these tests -- a configuration prepared for
     * an absent poller has already had its announcement consumed. */
    _state->set_cache_config_dir(_cache_dir);
  }

  void TearDown() override {
    config::applier::state::unload();
    std::filesystem::remove_all(_dir);
    std::filesystem::remove_all(_cache_dir);
  }

  /**
   * @brief Leave a prepared configuration for a poller, the way a cycle does
   * for one that was not connected at the time.
   *
   * `new-<N>.prot` alone, with no announcement beside it: the cycle that wrote
   * this file consumed the announcement in the same breath. Its presence is
   * what says a delivery is still pending.
   */
  void leave_prepared_conf(uint64_t poller_id, const std::string& version) {
    com::centreon::engine::configuration::State st;
    st.set_poller_id(poller_id);
    st.set_config_version(version);
    std::ofstream f(_dir / fmt::format("new-{}.prot", poller_id),
                    std::ios::binary);
    ASSERT_TRUE(st.SerializeToOstream(&f));
  }

  /**
   * @brief Connect an Engine peer announcing that it runs no configuration.
   *
   * With a new-<N>.prot left beforehand, this is the real path of a poller
   * starting after its configuration was pushed: add_peer() hands the prepared
   * state over without reading the sources again, which is what sets
   * available_conf while nothing has been sent yet.
   *
   * No <N>.prot is written, so the cache is not fed and nothing is published --
   * this test needs no multiplexing engine.
   */
  void connect(uint64_t poller_id) {
    _state->add_peer(poller_id, fmt::format("Poller{}", poller_id), "central",
                     com::centreon::common::ENGINE, true, "", "");
  }

  /** @brief Play out a delivery: sent, then acknowledged by the poller. */
  void deliver_and_acknowledge(uint64_t poller_id, const std::string& version) {
    _state->set_poller_conf_sent(poller_id, version);
    ASSERT_TRUE(_state->acknowledge_poller_conf(poller_id, version));
  }

  /**
   * @brief Prepare a configuration for a poller behind relay 10, the public
   * way to have a connected poller served without publishing anything.
   *
   * A diff already prepared is removed first, as the cycle replaces it: the
   * relay path only prepares when there is none.
   */
  void prepare_via_relay(uint64_t poller_id, const std::string& version) {
    std::filesystem::remove(_dir / fmt::format("diff-{}.prot", poller_id));
    leave_prepared_conf(poller_id, version);
    ASSERT_EQ(_state->prepare_relay_config_response(poller_id, ""),
              broker_state::relay_config_response::diff_ready);
  }
};

/**
 * Scenario: one poller acknowledges while two others are still owed their
 * configuration.
 * Given three connected pollers, each with a configuration prepared
 * When only the first one has been sent its configuration and acknowledges it
 * Then the round is not over, because two diffs are still waiting to be handed
 * over and the global diff would consume them.
 */
TEST_F(BrokerStateRound, NotOverWhilePeersAreStillOwedTheirConfiguration) {
  for (uint64_t id = 1; id <= 3; ++id) {
    leave_prepared_conf(id, "v1");
    connect(id);
    ASSERT_TRUE(_state->poller_needs_update(id));
  }

  deliver_and_acknowledge(1, "v1");

  /* Pollers 2 and 3 have available_conf set, conf_round::out: their
   * diff-<N>.prot is on disk waiting for their stream to emit. */
  ASSERT_TRUE(_state->poller_needs_update(2));
  ASSERT_TRUE(_state->poller_needs_update(3));

  EXPECT_FALSE(_state->try_close_conf_round());
}

/**
 * Scenario: every poller has been served.
 * Given the same three pollers
 * When all three have been sent their configuration and have acknowledged it
 * Then the round is over and the global diff can be built.
 */
TEST_F(BrokerStateRound, OverOnceEveryPeerHasAcknowledged) {
  for (uint64_t id = 1; id <= 3; ++id) {
    leave_prepared_conf(id, "v1");
    connect(id);
    deliver_and_acknowledge(id, "v1");
  }

  for (uint64_t id = 1; id <= 3; ++id)
    ASSERT_FALSE(_state->poller_needs_update(id));

  EXPECT_TRUE(_state->try_close_conf_round());
}

/**
 * Scenario: a poller that is not connected owns no prepared diff.
 * Given one connected poller, served and acknowledged, and nothing else
 * When the round is looked at
 * Then it is over: a poller that never connected has no diff of its own to
 * lose, since preparing one requires a peer.
 */
TEST_F(BrokerStateRound, OverWhenTheOnlyServedPeerAcknowledged) {
  leave_prepared_conf(1, "v1");
  connect(1);
  deliver_and_acknowledge(1, "v1");

  EXPECT_TRUE(_state->try_close_conf_round());
}

/**
 * Scenario: the configuration sent is overtaken by a newer one.
 * Given a connected poller whose prepared configuration is v2
 * When the stream reports it sent v1, read from the diff file just before v2
 * replaced it
 * Then the poller is not marked as served
 * And v2 waits for the poller to say which version it runs
 * When the poller acknowledges v1
 * Then that acknowledgement does not count as its part of the round
 * And v2 is owed to it
 * And once v2 is sent, the poller is marked as served.
 */
TEST_F(BrokerStateRound, SentVersionMustBeTheLatestToCount) {
  leave_prepared_conf(1, "v2");
  connect(1);
  ASSERT_TRUE(_state->poller_needs_update(1));

  EXPECT_FALSE(_state->set_poller_conf_sent(1, "v1"));
  EXPECT_FALSE(_state->poller_needs_update(1));

  EXPECT_FALSE(_state->acknowledge_poller_conf(1, "v1"));
  EXPECT_TRUE(_state->poller_needs_update(1));

  EXPECT_TRUE(_state->set_poller_conf_sent(1, "v2"));
  EXPECT_FALSE(_state->poller_needs_update(1));
}

/**
 * Scenario: a new export arrives while the poller applies the previous one.
 * Given a poller behind a relay that was sent v1 and has not acknowledged it
 * When v2 is prepared for it
 * Then v2 is not sent: Engine would apply it on top of v1, while it was
 * computed from what Broker stores
 * And the round is not over, v2 being owed
 * When the poller acknowledges v1
 * Then that acknowledgement does not count as its part of the round
 * And the poller is known to run v1, so v2 can be sent
 * When v2 is sent and acknowledged
 * Then the round is over.
 */
TEST_F(BrokerStateRound, NothingSentWhileAConfigurationIsInFlight) {
  _state->register_poller_via_relay(1, "Poller1", 10, "");
  prepare_via_relay(1, "v1");
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));

  prepare_via_relay(1, "v2");
  EXPECT_TRUE(_state->pollers_via_relay_needing_update(10).empty());
  EXPECT_FALSE(_state->try_close_conf_round());

  EXPECT_FALSE(_state->acknowledge_poller_conf(1, "v1"));
  EXPECT_EQ(_state->poller_engine_conf(1), "v1");
  EXPECT_EQ(_state->pollers_via_relay_needing_update(10),
            std::vector<uint64_t>{1});

  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v2"));
  EXPECT_TRUE(_state->acknowledge_poller_conf(1, "v2"));
  EXPECT_TRUE(_state->try_close_conf_round());
}

/**
 * Scenario: the acknowledgement of an overtaken configuration touches no file.
 * Given a poller behind a relay that was sent v1
 * And v2 prepared for it since, in new-<N>.prot and diff-<N>.prot
 * When the poller acknowledges v1
 * Then new-<N>.prot is not renamed: it holds v2, which the poller does not run
 * And diff-<N>.prot is still there to be sent.
 */
TEST_F(BrokerStateRound, OvertakenAcknowledgementKeepsTheNewerFiles) {
  _state->register_poller_via_relay(1, "Poller1", 10, "");
  prepare_via_relay(1, "v1");
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  prepare_via_relay(1, "v2");

  _state->on_poller_conf_acknowledged(1, "v1");

  EXPECT_TRUE(std::filesystem::exists(_dir / "new-1.prot"));
  EXPECT_FALSE(std::filesystem::exists(_dir / "1.prot"));
  EXPECT_TRUE(std::filesystem::exists(_dir / "diff-1.prot"));
}

/**
 * Scenario: a diff file is written whole or not at all.
 * Given a connected poller with a prepared configuration
 * When its diff file is written
 * Then diff-<N>.prot can be parsed and names the prepared version
 * And no temporary file is left in the directory.
 */
TEST_F(BrokerStateRound, DiffFileIsWrittenWholeWithoutLeftover) {
  leave_prepared_conf(1, "v1");
  connect(1);

  com::centreon::engine::configuration::DiffState diff;
  std::ifstream f(_dir / "diff-1.prot", std::ios::binary);
  ASSERT_TRUE(f);
  ASSERT_TRUE(diff.ParseFromIstream(&f));
  EXPECT_EQ(diff.state().config_version(), "v1");

  for (const auto& entry : std::filesystem::directory_iterator(_dir))
    EXPECT_EQ(entry.path().filename().string().find(".tmp"), std::string::npos)
        << entry.path();
}

/**
 * Scenario: a poller that does not run the stored configuration still gets a
 * real difference prepared.
 * Given a poller whose acknowledged configuration <N>.prot is v1
 * And a new configuration v2 waiting in new-<N>.prot
 * And the poller running no configuration at all (it lost its state.prot)
 * When the diff is prepared
 * Then diff-<N>.prot is the difference from v1 to v2, not the whole v2
 * And it says it starts from v1, so that the stream knows the poller needs the
 * full configuration.
 */
TEST_F(BrokerStateRound, DiffStartsFromStoredConfEvenIfPollerRunsAnother) {
  for (const auto& [name, version] :
       {std::pair{"1.prot", "v1"}, std::pair{"new-1.prot", "v2"}}) {
    com::centreon::engine::configuration::State st;
    st.set_poller_id(1);
    st.set_config_version(version);
    std::ofstream f(_dir / name, std::ios::binary);
    ASSERT_TRUE(st.SerializeToOstream(&f));
  }
  /* Behind a relay: the public way to have a connected poller whose
   * configuration is prepared without publishing anything. */
  _state->register_poller_via_relay(1, "Poller1", 10, "");

  ASSERT_EQ(_state->prepare_relay_config_response(1, ""),
            broker_state::relay_config_response::diff_ready);

  com::centreon::engine::configuration::DiffState diff;
  std::ifstream f(_dir / "diff-1.prot", std::ios::binary);
  ASSERT_TRUE(f);
  ASSERT_TRUE(diff.ParseFromIstream(&f));
  EXPECT_FALSE(diff.has_state());
  EXPECT_EQ(diff.from_version(), "v1");
  EXPECT_EQ(diff.config_version(), "v2");
}

/**
 * Scenario: a poller that cannot apply its configuration says so.
 * Given two pollers behind a relay, each sent v1
 * When the first one reports it could not apply v1, still running nothing
 * Then new-<N>.prot and diff-<N>.prot of that poller are removed, so that the
 * closing of the round does not carry its diff to the database
 * And v1 is not sent to it again, it would fail the same way
 * And the failure is kept with its reason
 * When the second poller acknowledges v1
 * Then the round is over, the first poller no longer holding it open.
 */
TEST_F(BrokerStateRound, RejectedConfigurationLeavesTheRound) {
  for (uint64_t id = 1; id <= 2; ++id) {
    _state->register_poller_via_relay(id, fmt::format("Poller{}", id), 10, "");
    prepare_via_relay(id, "v1");
    ASSERT_TRUE(_state->set_poller_conf_sent(id, "v1"));
  }

  _state->on_poller_conf_rejected(1, "", "v1", "boom");

  EXPECT_FALSE(std::filesystem::exists(_dir / "new-1.prot"));
  EXPECT_FALSE(std::filesystem::exists(_dir / "diff-1.prot"));
  EXPECT_TRUE(std::filesystem::exists(_dir / "diff-2.prot"));
  EXPECT_EQ(_state->poller_engine_conf(1), "");
  EXPECT_TRUE(_state->pollers_via_relay_needing_update(10).empty());
  auto pollers = _state->connected_pollers();
  auto p1 = absl::c_find_if(
      pollers, [](const auto& p) { return p.peer.poller_id == 1; });
  ASSERT_NE(p1, pollers.end());
  EXPECT_EQ(p1->conf.rejected_conf, "v1");
  EXPECT_EQ(p1->conf.rejection_error, "boom");

  EXPECT_TRUE(_state->acknowledge_poller_conf(2, "v1"));
  EXPECT_TRUE(_state->try_close_conf_round());
}

/**
 * Scenario: a new export is sent to a poller that rejected the previous one.
 * Given a poller behind a relay that could not apply v1
 * When v2 is prepared for it
 * Then v2 is owed and sent to it
 * And once acknowledged, the failure is forgotten.
 */
TEST_F(BrokerStateRound, NewExportIsSentAfterARejection) {
  _state->register_poller_via_relay(1, "Poller1", 10, "");
  prepare_via_relay(1, "v1");
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  _state->on_poller_conf_rejected(1, "", "v1", "boom");

  prepare_via_relay(1, "v2");
  EXPECT_EQ(_state->pollers_via_relay_needing_update(10),
            std::vector<uint64_t>{1});

  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v2"));
  EXPECT_TRUE(_state->acknowledge_poller_conf(1, "v2"));
  auto pollers = _state->connected_pollers();
  ASSERT_EQ(pollers.size(), 1u);
  EXPECT_TRUE(pollers[0].conf.rejected_conf.empty());
  EXPECT_TRUE(pollers[0].conf.rejection_error.empty());
}

/**
 * Scenario: the failure of an overtaken configuration touches no file.
 * Given a poller behind a relay that was sent v1
 * And v2 prepared for it since, in new-<N>.prot and diff-<N>.prot
 * When the poller reports it could not apply v1
 * Then new-<N>.prot and diff-<N>.prot are kept: they hold v2
 * And v2 is owed to it, computed from what the poller still runs.
 */
TEST_F(BrokerStateRound, OvertakenRejectionKeepsTheNewerFiles) {
  _state->register_poller_via_relay(1, "Poller1", 10, "");
  prepare_via_relay(1, "v1");
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  prepare_via_relay(1, "v2");

  _state->on_poller_conf_rejected(1, "", "v1", "boom");

  EXPECT_TRUE(std::filesystem::exists(_dir / "new-1.prot"));
  EXPECT_TRUE(std::filesystem::exists(_dir / "diff-1.prot"));
  EXPECT_EQ(_state->pollers_via_relay_needing_update(10),
            std::vector<uint64_t>{1});
}

/**
 * Scenario: a poller that connects announcing the configuration prepared for
 * it has applied it, and that announcement is its acknowledgement.
 * Given a poller whose acknowledged configuration <N>.prot is v1
 * And v2 waiting in new-<N>.prot, applied by the poller whose acknowledgement
 * never reached Broker
 * And a second poller still owed its configuration, so that the round stays
 * open and nothing is published
 * When the first poller connects announcing v2
 * Then new-<N>.prot becomes <N>.prot
 * And diff-<N>.prot holds the difference from v1 to v2, waiting for the round
 * And v2 is not owed to the poller: it runs it
 * When the second poller is served and acknowledges
 * Then the round is over, the first poller having played its part.
 */
TEST_F(BrokerStateRound, AnnouncementOfThePreparedVersionAcknowledgesIt) {
  /* The acknowledgement feeds the cache with the difference. */
  _state->initialize_cache();
  for (const auto& [name, version] :
       {std::pair{"1.prot", "v1"}, std::pair{"new-1.prot", "v2"}}) {
    com::centreon::engine::configuration::State st;
    st.set_poller_id(1);
    st.set_config_version(version);
    std::ofstream f(_dir / name, std::ios::binary);
    ASSERT_TRUE(st.SerializeToOstream(&f));
  }
  leave_prepared_conf(2, "v1");
  connect(2);
  ASSERT_TRUE(_state->poller_needs_update(2));

  /* Behind a relay, as <N>.prot would otherwise be published at the
   * connection of a direct poller: the relay asks with the version its Engine
   * announced. */
  _state->register_poller_via_relay(1, "Poller1", 10, "v2");
  EXPECT_EQ(_state->prepare_relay_config_response(1, "v2"),
            broker_state::relay_config_response::up_to_date);

  EXPECT_FALSE(std::filesystem::exists(_dir / "new-1.prot"));
  com::centreon::engine::configuration::State st;
  {
    std::ifstream f(_dir / "1.prot", std::ios::binary);
    ASSERT_TRUE(f);
    ASSERT_TRUE(st.ParseFromIstream(&f));
  }
  EXPECT_EQ(st.config_version(), "v2");
  com::centreon::engine::configuration::DiffState diff;
  {
    std::ifstream f(_dir / "diff-1.prot", std::ios::binary);
    ASSERT_TRUE(f);
    ASSERT_TRUE(diff.ParseFromIstream(&f));
  }
  EXPECT_EQ(diff.from_version(), "v1");
  EXPECT_EQ(diff.config_version(), "v2");
  EXPECT_FALSE(_state->poller_needs_update(1));
  EXPECT_TRUE(_state->pollers_via_relay_needing_update(10).empty());

  EXPECT_FALSE(_state->try_close_conf_round());
  deliver_and_acknowledge(2, "v1");
  EXPECT_TRUE(_state->try_close_conf_round());
}

/**
 * Scenario: a stored configuration that cannot be read is neither published nor
 * taken for lost.
 * Given poller 1 whose 1.prot is not a protobuf message, and poller 2 with no
 * stored configuration at all
 * When both connect
 * Then nothing is published for poller 1: unified_sql would take what was read
 * for its whole configuration, and disable in the database every object it
 * lacks. No multiplexing engine is loaded here, so a publication would crash.
 * And Broker still says it knows the configuration of poller 1: asking Engine
 * for it would not help, since 1.prot is only recreated when it does not exist
 * And it says it does not know the one of poller 2, which is asked for.
 */
TEST_F(BrokerStateRound, UnreadableStoredConfigurationIsNotPublished) {
  {
    /* Field number 0 does not exist: no message starts with a null byte. */
    std::ofstream f(_dir / "1.prot", std::ios::binary);
    f.write("\0\1\2\3\4\5\6\7", 8);
  }
  connect(1);
  connect(2);

  EXPECT_TRUE(_state->broker_knows_poller_conf(1));
  EXPECT_TRUE(std::filesystem::exists(_dir / "1.prot"));
  EXPECT_FALSE(_state->broker_knows_poller_conf(2));
}

/**
 * Scenario: a poller that disconnects during a round leaves it, and comes back
 * with a clean slate.
 * Given two pollers behind a relay, each sent v1
 * And the first one could not apply it
 * When the first one disconnects
 * Then it no longer holds the round open
 * And the round is over once the second one acknowledges v1
 * When the first one connects again
 * Then nothing is in flight for it, and its failure is forgotten: v1 is tried
 * again.
 */
TEST_F(BrokerStateRound, DisconnectionResetsThePollerConfiguration) {
  for (uint64_t id = 1; id <= 2; ++id) {
    _state->register_poller_via_relay(id, fmt::format("Poller{}", id), 10, "");
    prepare_via_relay(id, "v1");
    ASSERT_TRUE(_state->set_poller_conf_sent(id, "v1"));
  }
  _state->on_poller_conf_rejected(1, "", "v1", "boom");

  _state->remove_peer(1, "Poller1", "central");
  EXPECT_FALSE(_state->is_poller_connected(1));
  EXPECT_EQ(_state->poller_engine_conf(1), "");

  EXPECT_TRUE(_state->acknowledge_poller_conf(2, "v1"));
  EXPECT_TRUE(_state->try_close_conf_round());

  _state->register_poller_via_relay(1, "Poller1", 10, "");
  auto pollers = _state->connected_pollers();
  auto p1 = absl::c_find_if(
      pollers, [](const auto& p) { return p.peer.poller_id == 1; });
  ASSERT_NE(p1, pollers.end());
  EXPECT_TRUE(p1->conf.rejected_conf.empty());
  EXPECT_TRUE(p1->conf.conf_in_flight.empty());
  EXPECT_TRUE(p1->conf.available_conf.empty());
}
