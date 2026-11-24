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
#include <thread>

#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/proto_file/proto_file.hh"

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
    /* Never started: what a connection or the end of a round publishes waits
     * in its queue, and goes away with it. */
    multiplexing::engine::load();
    /* A muxer needs the events registry, which needs the protocols. The test
     * environment loads both, but a suite run before may have unloaded them
     * (StatsTest does): load() keeps the ones already there. */
    io::protocols::load();
    io::events::load();
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
    multiplexing::engine::unload();
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
   * @brief Leave a configuration exported while another one is in flight, the
   * way a cycle does: `next-<N>.prot`, whole, with no difference computed.
   */
  void leave_next_conf(uint64_t poller_id, const std::string& version) {
    com::centreon::engine::configuration::State st;
    st.set_poller_id(poller_id);
    st.set_config_version(version);
    std::ofstream f(_dir / fmt::format("next-{}.prot", poller_id),
                    std::ios::binary);
    ASSERT_TRUE(st.SerializeToOstream(&f));
  }

  /**
   * @brief Leave the configuration a poller acknowledged: `<N>.prot`.
   */
  void leave_stored_conf(uint64_t poller_id, const std::string& version) {
    com::centreon::engine::configuration::State st;
    st.set_poller_id(poller_id);
    st.set_config_version(version);
    std::ofstream f(_dir / fmt::format("{}.prot", poller_id), std::ios::binary);
    ASSERT_TRUE(st.SerializeToOstream(&f));
  }

  /**
   * @brief Leave an acknowledged difference, the way an acknowledgement does:
   * `diff-<N>.prot`, waiting for the round to close.
   */
  void leave_acknowledged_diff(uint64_t poller_id,
                               const std::string& from,
                               const std::string& to) {
    com::centreon::engine::configuration::DiffState diff;
    diff.set_from_version(from);
    diff.set_config_version(to);
    std::ofstream f(_dir / fmt::format("diff-{}.prot", poller_id),
                    std::ios::binary);
    ASSERT_TRUE(diff.SerializeToOstream(&f));
  }

  /** @brief The version a stored protobuf State names. */
  std::string version_of(const std::string& name) {
    com::centreon::engine::configuration::State st;
    std::error_code read_ec;
    com::centreon::common::read_proto_file(_dir / name, st, read_ec);
    EXPECT_FALSE(read_ec) << name << ": " << read_ec.message();
    return st.config_version();
  }

  /** @brief Whether a stored difference is a full configuration. */
  bool diff_is_full(const std::string& name) {
    com::centreon::engine::configuration::DiffState diff;
    std::error_code read_ec;
    com::centreon::common::read_proto_file(_dir / name, diff, read_ec);
    EXPECT_FALSE(read_ec) << name << ": " << read_ec.message();
    return diff.has_state();
  }

  /** @brief The versions a stored difference goes from and to. */
  std::pair<std::string, std::string> versions_of_diff(
      const std::string& name) {
    com::centreon::engine::configuration::DiffState diff;
    std::error_code read_ec;
    com::centreon::common::read_proto_file(_dir / name, diff, read_ec);
    EXPECT_FALSE(read_ec) << name << ": " << read_ec.message();
    return {diff.from_version(), diff.config_version()};
  }

  /**
   * @brief Connect an Engine peer announcing that it runs no configuration.
   *
   * With a new-<N>.prot left beforehand, this is the real path of a poller
   * starting after its configuration was pushed: add_peer() hands the prepared
   * state over without reading the sources again, which is what sets
   * available_conf while nothing has been sent yet.
   *
   * No <N>.prot is written, so the cache is not fed and nothing is published.
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
   * @brief Play an export arriving for a poller already connected: the cycle
   * writes `new-<N>.prot` from the sources, then prepares its delivery. Only
   * the sources are skipped here.
   */
  void export_conf(uint64_t poller_id, const std::string& version) {
    leave_prepared_conf(poller_id, version);
    const std::optional<bool> owed =
        _state->_prepare_diff_from_new_prot_file(poller_id);
    ASSERT_TRUE(owed && *owed);
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

  /* Pollers 2 and 3 are owed their configuration: their
   * new-diff-<N>.prot is on disk waiting for their stream to emit. */
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
 * Scenario: a version exported while another one is in flight waits its turn.
 * Given a connected poller that was sent v1 and has not acknowledged it
 * And v2 exported since, left in next-<N>.prot by the cycle
 * Then nothing is owed to the poller: v1 is in flight, and v2 is not even
 * prepared, new-<N>.prot still holding v1
 * And the round is not over
 * When the poller acknowledges v1
 * Then v1 is installed: <N>.prot holds v1, diff-<N>.prot the difference to it
 * And v2 takes its place: new-<N>.prot holds v2, new-diff-<N>.prot the
 * difference from v1 to v2, next-<N>.prot is gone
 * And v2 is owed to the poller, which keeps the round open.
 */
TEST_F(BrokerStateRound, VersionExportedDuringAFlightWaitsInNext) {
  _state->initialize_cache();
  leave_prepared_conf(1, "v1");
  connect(1);
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  leave_next_conf(1, "v2");

  EXPECT_FALSE(_state->poller_needs_update(1));
  EXPECT_EQ(version_of("new-1.prot"), "v1");
  EXPECT_FALSE(_state->try_close_conf_round());

  _state->on_poller_conf_acknowledged(1, "v1");

  EXPECT_EQ(version_of("1.prot"), "v1");
  EXPECT_TRUE(diff_is_full("diff-1.prot"));
  EXPECT_EQ(version_of("new-1.prot"), "v2");
  EXPECT_EQ(versions_of_diff("new-diff-1.prot"),
            (std::pair<std::string, std::string>{"v1", "v2"}));
  EXPECT_FALSE(std::filesystem::exists(_dir / "next-1.prot"));
  EXPECT_TRUE(_state->poller_needs_update(1));
  EXPECT_FALSE(_state->try_close_conf_round());
}

/**
 * Scenario: the failure of a version closes the round that the poller held
 * open, and the version it acknowledged before goes to the database.
 * Given a connected poller that was sent v1, with v2 exported since
 * And the poller acknowledged v1: diff-<N>.prot waits for the database, and v2
 * is owed, which keeps the round open
 * When v2 is sent and the poller reports it could not apply it
 * Then new-<N>.prot and new-diff-<N>.prot are gone, <N>.prot still holds v1
 * And the round is closed: diff-<N>.prot, which the publication removes, is
 * gone
 * And there is no round left to close.
 */
TEST_F(BrokerStateRound, RejectionClosesTheRoundWithTheAcknowledgedDiff) {
  _state->initialize_cache();
  leave_prepared_conf(1, "v1");
  connect(1);
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  leave_next_conf(1, "v2");
  _state->on_poller_conf_acknowledged(1, "v1");
  ASSERT_TRUE(std::filesystem::exists(_dir / "diff-1.prot"));
  ASSERT_TRUE(_state->poller_needs_update(1));

  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v2"));
  _state->on_poller_conf_rejected(1, "v1", "v2", "boom");

  EXPECT_FALSE(std::filesystem::exists(_dir / "new-1.prot"));
  EXPECT_FALSE(std::filesystem::exists(_dir / "new-diff-1.prot"));
  EXPECT_EQ(version_of("1.prot"), "v1");
  EXPECT_FALSE(std::filesystem::exists(_dir / "diff-1.prot"));
  EXPECT_FALSE(_state->try_close_conf_round());
}

/**
 * Scenario: a new export arrives while the poller behind a relay applies the
 * previous one.
 * Given a poller behind a relay that was sent v1 and has not acknowledged it
 * And v2 exported since, left in next-<N>.prot by the cycle
 * Then the relay has nothing to send: v1 is in flight
 * And the round is not over
 * When the poller acknowledges v1
 * Then that acknowledgement is its part of the round for v1
 * And v2 is now prepared, and owed through the relay
 * When v2 is sent and acknowledged
 * Then the round is over.
 */
TEST_F(BrokerStateRound, NothingSentWhileAConfigurationIsInFlight) {
  _state->initialize_cache();
  _state->register_poller_via_relay(1, "Poller1", 10, "");
  export_conf(1, "v1");
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));

  leave_next_conf(1, "v2");
  EXPECT_FALSE(_state->poller_needs_update(1));
  EXPECT_FALSE(_state->try_close_conf_round());

  _state->on_poller_conf_acknowledged(1, "v1");
  EXPECT_EQ(_state->poller_engine_conf(1), "v1");
  EXPECT_TRUE(_state->poller_needs_update(1));
  EXPECT_EQ(version_of("new-1.prot"), "v2");

  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v2"));
  EXPECT_TRUE(_state->acknowledge_poller_conf(1, "v2"));
  EXPECT_TRUE(_state->try_close_conf_round());
}

/**
 * Scenario: a version exported while another one is in flight is replaced by
 * the next export before it leaves.
 * Given a connected poller that was sent v1 and has not acknowledged it
 * And v2 exported since, waiting in next-<N>.prot
 * When the poller disconnects, and comes back announcing v1
 * Then its announcement installs v1, and v2 takes its place: new-<N>.prot
 * holds v2, owed to it
 * And next-<N>.prot is gone.
 */
TEST_F(BrokerStateRound, ReconnectionAnnouncingTheFlightVersionPromotesNext) {
  _state->initialize_cache();
  leave_prepared_conf(1, "v1");
  connect(1);
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  leave_next_conf(1, "v2");
  _state->remove_peer(1, "Poller1", "central");

  /* Behind a relay, which announces with the version its Engine runs. */
  _state->register_poller_via_relay(1, "Poller1", 10, "v1");

  EXPECT_EQ(version_of("1.prot"), "v1");
  EXPECT_EQ(version_of("new-1.prot"), "v2");
  EXPECT_EQ(versions_of_diff("new-diff-1.prot"),
            (std::pair<std::string, std::string>{"v1", "v2"}));
  EXPECT_FALSE(std::filesystem::exists(_dir / "next-1.prot"));
  EXPECT_TRUE(_state->poller_needs_update(1));
}

/**
 * Scenario: the version in flight was never applied, and a newer one waits.
 * Given a connected poller that was sent v1 and has not acknowledged it
 * And v2 exported since, waiting in next-<N>.prot
 * When the poller disconnects, and comes back announcing the version before v1
 * Then v1 is dropped: nobody runs it, and v2 supersedes it
 * And v2 is prepared from what the poller runs, and owed to it
 * And next-<N>.prot is gone.
 */
TEST_F(BrokerStateRound, ReconnectionAnnouncingTheOldVersionSkipsTheFlight) {
  leave_prepared_conf(1, "v1");
  connect(1);
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  leave_next_conf(1, "v2");
  _state->remove_peer(1, "Poller1", "central");

  _state->register_poller_via_relay(1, "Poller1", 10, "");

  EXPECT_FALSE(std::filesystem::exists(_dir / "1.prot"));
  EXPECT_EQ(version_of("new-1.prot"), "v2");
  EXPECT_TRUE(diff_is_full("new-diff-1.prot"));
  EXPECT_FALSE(std::filesystem::exists(_dir / "next-1.prot"));
  EXPECT_TRUE(_state->poller_needs_update(1));
}

/**
 * Scenario: a diff file is written whole or not at all.
 * Given a connected poller with a prepared configuration
 * When its diff file is written
 * Then new-diff-<N>.prot can be parsed and names the prepared version
 * And no temporary file is left in the directory.
 */
TEST_F(BrokerStateRound, DiffFileIsWrittenWholeWithoutLeftover) {
  leave_prepared_conf(1, "v1");
  connect(1);

  com::centreon::engine::configuration::DiffState diff;
  std::error_code read_ec;
  com::centreon::common::read_proto_file(_dir / "new-diff-1.prot", diff,
                                         read_ec);
  ASSERT_FALSE(read_ec) << read_ec.message();
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
 * Then new-diff-<N>.prot is the difference from v1 to v2, not the whole v2
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
  /* Its relay asks for it, announcing no version. */
  _state->register_poller_via_relay(1, "Poller1", 10, "");
  ASSERT_TRUE(_state->poller_needs_update(1));

  com::centreon::engine::configuration::DiffState diff;
  std::error_code read_ec;
  com::centreon::common::read_proto_file(_dir / "new-diff-1.prot", diff,
                                         read_ec);
  ASSERT_FALSE(read_ec) << read_ec.message();
  EXPECT_FALSE(diff.has_state());
  EXPECT_EQ(diff.from_version(), "v1");
  EXPECT_EQ(diff.config_version(), "v2");
}

/**
 * Scenario: a poller that cannot apply its configuration says so.
 * Given two pollers behind a relay, each sent v1
 * When the first one reports it could not apply v1, still running nothing
 * Then new-<N>.prot and new-diff-<N>.prot of that poller are removed: nothing
 * is left to send, and nothing to carry to the database
 * And v1 is not sent to it again, it would fail the same way
 * And the failure is kept with its reason
 * When the second poller acknowledges v1
 * Then the round is over, the first poller no longer holding it open.
 */
TEST_F(BrokerStateRound, RejectedConfigurationLeavesTheRound) {
  for (uint64_t id = 1; id <= 2; ++id) {
    _state->register_poller_via_relay(id, fmt::format("Poller{}", id), 10, "");
    export_conf(id, "v1");
    ASSERT_TRUE(_state->set_poller_conf_sent(id, "v1"));
  }

  _state->on_poller_conf_rejected(1, "", "v1", "boom");

  EXPECT_FALSE(std::filesystem::exists(_dir / "new-1.prot"));
  EXPECT_FALSE(std::filesystem::exists(_dir / "new-diff-1.prot"));
  EXPECT_TRUE(std::filesystem::exists(_dir / "new-diff-2.prot"));
  EXPECT_EQ(_state->poller_engine_conf(1), "");
  /* Nothing to send: v1 was refused by the first, and is in flight for the
   * second. */
  EXPECT_FALSE(_state->poller_needs_update(1));
  EXPECT_FALSE(_state->poller_needs_update(2));
  auto pollers = _state->connected_pollers();
  auto p1 = absl::c_find_if(
      pollers, [](const auto& p) { return p.peer.poller_id == 1; });
  ASSERT_NE(p1, pollers.end());
  ASSERT_TRUE(p1->conf.last_rejection());
  EXPECT_EQ(p1->conf.last_rejection()->version, "v1");
  EXPECT_EQ(p1->conf.last_rejection()->error, "boom");

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
  export_conf(1, "v1");
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  _state->on_poller_conf_rejected(1, "", "v1", "boom");

  export_conf(1, "v2");
  EXPECT_TRUE(_state->poller_needs_update(1));

  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v2"));
  EXPECT_TRUE(_state->acknowledge_poller_conf(1, "v2"));
  auto pollers = _state->connected_pollers();
  ASSERT_EQ(pollers.size(), 1u);
  EXPECT_FALSE(pollers[0].conf.last_rejection());
}

/**
 * Scenario: the version in flight fails, and a newer one waits.
 * Given a poller behind a relay that was sent v1
 * And v2 exported since, waiting in next-<N>.prot
 * When the poller reports it could not apply v1
 * Then v1 is dropped, and v2 takes its place: new-<N>.prot holds v2, and
 * new-diff-<N>.prot the difference from what the poller still runs
 * And v2 is owed to it: the failure of v1 does not hold it back
 * And next-<N>.prot is gone.
 */
TEST_F(BrokerStateRound, RejectionOfTheFlightVersionPromotesNext) {
  _state->register_poller_via_relay(1, "Poller1", 10, "");
  export_conf(1, "v1");
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  leave_next_conf(1, "v2");

  _state->on_poller_conf_rejected(1, "", "v1", "boom");

  EXPECT_EQ(version_of("new-1.prot"), "v2");
  EXPECT_TRUE(diff_is_full("new-diff-1.prot"));
  EXPECT_FALSE(std::filesystem::exists(_dir / "next-1.prot"));
  EXPECT_TRUE(_state->poller_needs_update(1));
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
 * And new-diff-<N>.prot is gone: nothing is left to send
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

  /* Behind a relay, which asks with the version its Engine announced. */
  _state->register_poller_via_relay(1, "Poller1", 10, "v2");

  EXPECT_FALSE(std::filesystem::exists(_dir / "new-1.prot"));
  com::centreon::engine::configuration::State st;
  {
    std::error_code read_ec;
    com::centreon::common::read_proto_file(_dir / "1.prot", st, read_ec);
    ASSERT_FALSE(read_ec) << read_ec.message();
  }
  EXPECT_EQ(st.config_version(), "v2");
  com::centreon::engine::configuration::DiffState diff;
  {
    std::error_code read_ec;
    com::centreon::common::read_proto_file(_dir / "diff-1.prot", diff, read_ec);
    ASSERT_FALSE(read_ec) << read_ec.message();
  }
  EXPECT_EQ(diff.from_version(), "v1");
  EXPECT_EQ(diff.config_version(), "v2");
  EXPECT_FALSE(std::filesystem::exists(_dir / "new-diff-1.prot"));
  EXPECT_FALSE(_state->poller_needs_update(1));

  EXPECT_FALSE(_state->try_close_conf_round());
  deliver_and_acknowledge(2, "v1");
  EXPECT_TRUE(_state->try_close_conf_round());
}

/**
 * @brief An acknowledged difference waits for its round while the next export
 * comes in.
 *
 * Poller 1 runs v1, stored in 1.prot. Poller 2 is owed its configuration for
 * the whole test, so that the round never closes and nothing is published.
 */
class BrokerStateNextDiff : public BrokerStateRound {
 protected:
  using State = com::centreon::engine::configuration::State;
  using DiffState = com::centreon::engine::configuration::DiffState;

  /** @brief A configuration of poller 1 holding the hosts named. */
  static State conf(const std::string& version,
                    std::initializer_list<uint64_t> host_ids) {
    State st;
    st.set_poller_id(1);
    st.set_config_version(version);
    for (uint64_t id : host_ids) {
      auto* h = st.add_hosts();
      com::centreon::engine::configuration::host_helper hlp(h);
      h->set_host_id(id);
      h->set_host_name(fmt::format("host_{}", id));
      h->set_poller_id(1);
    }
    return st;
  }

  void write(const std::string& name, const State& st) {
    std::ofstream f(_dir / name, std::ios::binary);
    ASSERT_TRUE(st.SerializeToOstream(&f));
  }

  DiffState read_diff(const std::string& name) {
    DiffState diff;
    std::error_code read_ec;
    com::centreon::common::read_proto_file(_dir / name, diff, read_ec);
    EXPECT_FALSE(read_ec) << name << ": " << read_ec.message();
    return diff;
  }

  /**
   * @brief Poller 1 acknowledges v2, which adds host 2 to v1: diff-1.prot
   * waits for the round, held open by poller 2.
   */
  void acknowledge_v2() {
    _state->initialize_cache();
    write("1.prot", conf("v1", {1}));
    write("new-1.prot", conf("v2", {1, 2}));
    leave_prepared_conf(2, "v1");
    connect(2);
    /* Announcing the prepared version is acknowledging it. */
    _state->register_poller_via_relay(1, "Poller1", 10, "v2");
    ASSERT_TRUE(std::filesystem::exists(_dir / "diff-1.prot"));
  }

  /** @brief v3 is exported for poller 1: v2 without host 2. */
  void prepare_v3() {
    write("new-1.prot", conf("v3", {1}));
    const std::optional<bool> owed =
        _state->_prepare_diff_from_new_prot_file(1);
    ASSERT_TRUE(owed && *owed);
  }
};

/**
 * Scenario: a new export does not disturb the difference waiting for the
 * round.
 * Given poller 1 acknowledged v2, whose difference waits for the round
 * When v3 is exported for it
 * Then diff-<N>.prot still holds the difference from v1 to v2
 * And new-diff-<N>.prot holds the one from v2 to v3, to be sent
 * And the round is not over: poller 2 is still owed its configuration, and
 * poller 1 is owed v3.
 */
TEST_F(BrokerStateNextDiff, AcknowledgedDiffWaitsForTheRoundWhileV3IsPrepared) {
  acknowledge_v2();
  prepare_v3();

  const DiffState waiting = read_diff("diff-1.prot");
  EXPECT_EQ(waiting.from_version(), "v1");
  EXPECT_EQ(waiting.config_version(), "v2");
  const DiffState to_send = read_diff("new-diff-1.prot");
  EXPECT_EQ(to_send.from_version(), "v2");
  EXPECT_EQ(to_send.config_version(), "v3");
  EXPECT_TRUE(_state->poller_needs_update(1));
  EXPECT_FALSE(_state->try_close_conf_round());
}

/**
 * Scenario: a second acknowledgement before the round closes.
 * Given poller 1 acknowledged v2, whose difference waits for the round
 * And v3 sent to it
 * When it acknowledges v3
 * Then diff-<N>.prot holds one difference from v1 to v3, in which host 2,
 * added by v2 and removed by v3, does not appear
 * And new-diff-<N>.prot is gone
 * And 1.prot is v3.
 */
TEST_F(BrokerStateNextDiff, SecondAcknowledgementMergesIntoTheWaitingDiff) {
  acknowledge_v2();
  prepare_v3();
  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v3"));

  _state->on_poller_conf_acknowledged(1, "v3");

  EXPECT_FALSE(std::filesystem::exists(_dir / "new-diff-1.prot"));
  EXPECT_FALSE(std::filesystem::exists(_dir / "new-1.prot"));
  const DiffState merged = read_diff("diff-1.prot");
  EXPECT_EQ(merged.from_version(), "v1");
  EXPECT_EQ(merged.config_version(), "v3");
  EXPECT_EQ(merged.hosts().added_size(), 0);
  EXPECT_EQ(merged.hosts().modified_size(), 0);
  EXPECT_EQ(merged.hosts().removed_size(), 0);
  State stored;
  {
    std::error_code read_ec;
    com::centreon::common::read_proto_file(_dir / "1.prot", stored, read_ec);
    ASSERT_FALSE(read_ec) << read_ec.message();
  }
  EXPECT_EQ(stored.config_version(), "v3");
  EXPECT_FALSE(_state->try_close_conf_round());
}

/**
 * Scenario: the files of a poller configuration follow cache_compression.
 * Given a configuration prepared for poller 1
 * When poller 1 connects, and its difference is written
 * Then new-diff-1.prot starts with the compression header, which is the
 * default
 * And it is a plain protobuf message with cache_compression off
 * And it is read back the same in both cases.
 */
TEST_F(BrokerStateRound, ConfFilesFollowCacheCompression) {
  EXPECT_TRUE(_state->cache_compression());
  for (const bool compress : {true, false}) {
    SCOPED_TRACE(compress ? "compressed" : "plain");
    std::filesystem::remove(_dir / "new-diff-1.prot");
    _state->set_cache_compression(compress);
    leave_prepared_conf(1, "v1");
    connect(1);

    std::ifstream f(_dir / "new-diff-1.prot", std::ios::binary);
    ASSERT_TRUE(f);
    char magic[4] = {};
    f.read(magic, sizeof magic);
    EXPECT_EQ(
        std::string_view(magic, sizeof magic) == std::string_view("\0CBC", 4),
        compress);
    EXPECT_TRUE(diff_is_full("new-diff-1.prot"));
    _state->remove_peer(1, "Poller1", "central");
  }
}

/**
 * Scenario: a Broker peer that shares the id of a poller is not that poller.
 * Given 1.prot, the stored configuration of poller 1
 * When a Broker peer with the id 1 connects, as the rrd cbd of the central does
 * And then the Engine of poller 1 connects
 * Then 1.prot is published once, for the Engine: published for the Broker peer,
 * it would make unified_sql enable again in the database the objects of a
 * poller that may be stopped.
 */
TEST_F(BrokerStateRound, BrokerPeerSharingAPollerIdPublishesNothing) {
  leave_stored_conf(1, "v1");
  /* The muxer opens a queue file, which goes through the disk accessor, and
   * names its filters through the event registry, which registers its
   * protocols. The global environment loads the two registries, but every test
   * going through config::applier::deinit() unloads them: load() only creates
   * them when they are missing. */
  file::disk_accessor::load(0);
  io::protocols::load();
  io::events::load();
  multiplexing::muxer_filter filter{neb::pb_engine_state::static_type()};
  std::shared_ptr<multiplexing::muxer> mux = multiplexing::muxer::create(
      "ut_broker_state_round_engine_state",
      multiplexing::engine::instance_ptr(), filter, filter);

  /* The rrd cbd of the central carries the id of the central poller. */
  _state->add_peer(1, "Central", "central-rrd-master",
                   com::centreon::common::BROKER, true, "", "");
  connect(1);
  multiplexing::engine::instance_ptr()->start();

  int published = 0;
  for (int i = 0; i < 50; ++i) {
    std::shared_ptr<io::data> d;
    if (mux->read(d, 0) && d &&
        d->type() == neb::pb_engine_state::static_type())
      ++published;
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_EQ(published, 1);
  multiplexing::engine::instance_ptr()->stop();
  mux.reset();
  file::disk_accessor::unload();
}

/**
 * Scenario: a stored configuration that cannot be read is neither published nor
 * taken for lost.
 * Given poller 1 whose 1.prot is not a protobuf message, and poller 2 with no
 * stored configuration at all
 * When both connect
 * Then nothing is published for poller 1: unified_sql would take what was read
 * for its whole configuration, and disable in the database every object it
 * lacks.
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
    export_conf(id, "v1");
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
  EXPECT_FALSE(p1->conf.last_rejection());
  EXPECT_TRUE(p1->conf.conf_in_flight().empty());
  EXPECT_TRUE(p1->conf.available_conf().empty());
}

/**
 * Scenario: Broker stopped between an acknowledgement and the end of the
 * round, while another poller was still to be served.
 * Given diff-1.prot, the difference poller 1 acknowledged
 * And new-2.prot, a configuration prepared for poller 2, not connected
 * When Broker loads the stored configurations
 * Then nothing is published: diff-1.prot, which a publication removes, is
 * still there
 * And poller 1 waits for the database, as it shows once connected
 * And that mark alone would close the round, poller 2 being owed nothing
 * before it connects: this is why the load does not try when a new-<ID>.prot
 * exists.
 */
TEST_F(BrokerStateRound, AcknowledgedDiffFoundAtStartWaitsForTheOpenRound) {
  leave_acknowledged_diff(1, "v0", "v1");
  leave_prepared_conf(2, "v1");

  _state->load_pollers_config_in_cache();

  EXPECT_TRUE(std::filesystem::exists(_dir / "diff-1.prot"));
  connect(1);
  auto pollers = _state->connected_pollers();
  auto p1 = absl::c_find_if(
      pollers, [](const auto& p) { return p.peer.poller_id == 1; });
  ASSERT_NE(p1, pollers.end());
  EXPECT_EQ(p1->conf.db_status(),
            config::applier::poller_conf::db_state::pending);
  EXPECT_TRUE(_state->try_close_conf_round());
}

/**
 * Scenario: without an acknowledged difference left behind, the load marks
 * nobody.
 * Given new-2.prot alone, a configuration prepared for poller 2, not connected
 * When Broker loads the stored configurations
 * Then there is no round to close.
 */
TEST_F(BrokerStateRound, NoRoundAtStartWithoutAnAcknowledgedDiff) {
  leave_prepared_conf(2, "v1");

  _state->load_pollers_config_in_cache();

  EXPECT_FALSE(_state->try_close_conf_round());
}

/**
 * Scenario: a poller connects announcing another version than the one Broker
 * stores for it, with nothing newer prepared (transition 20).
 * Given 1.prot and 2.prot, both v1
 * When poller 1 connects directly, announcing no version
 * And poller 2 comes through a relay, announcing v0
 * Then both are owed v1, the stored configuration itself
 * And no file was written for that
 * When poller 1 acknowledges v1
 * Then it is owed nothing any more, and 1.prot is untouched.
 */
TEST_F(BrokerStateRound, PollerNotRunningTheStoredConfIsOwedIt) {
  leave_stored_conf(1, "v1");
  leave_stored_conf(2, "v1");

  connect(1);
  _state->register_poller_via_relay(2, "Poller2", 10, "v0");

  for (uint64_t id = 1; id <= 2; ++id) {
    EXPECT_TRUE(_state->poller_needs_update(id)) << id;
    EXPECT_EQ(_state->poller_available_conf(id), "v1") << id;
    EXPECT_TRUE(_state->broker_knows_poller_conf(id)) << id;
    EXPECT_FALSE(
        std::filesystem::exists(_dir / fmt::format("new-diff-{}.prot", id)));
    EXPECT_FALSE(
        std::filesystem::exists(_dir / fmt::format("new-{}.prot", id)));
  }

  ASSERT_TRUE(_state->set_poller_conf_sent(1, "v1"));
  _state->on_poller_conf_acknowledged(1, "v1");
  EXPECT_FALSE(_state->poller_needs_update(1));
  EXPECT_EQ(_state->poller_engine_conf(1), "v1");
  EXPECT_EQ(version_of("1.prot"), "v1");
}

/**
 * Scenario: a poller that runs the stored configuration is owed nothing.
 * Given 1.prot and 2.prot, both v1
 * When poller 1 connects directly and poller 2 through a relay, both
 * announcing v1
 * Then neither is owed a configuration.
 */
TEST_F(BrokerStateRound, PollerRunningTheStoredConfIsOwedNothing) {
  leave_stored_conf(1, "v1");
  leave_stored_conf(2, "v1");

  _state->add_peer(1, "Poller1", "central", com::centreon::common::ENGINE, true,
                   "v1", "");
  _state->register_poller_via_relay(2, "Poller2", 10, "v1");

  for (uint64_t id = 1; id <= 2; ++id) {
    EXPECT_FALSE(_state->poller_needs_update(id)) << id;
    EXPECT_TRUE(_state->broker_knows_poller_conf(id)) << id;
  }
}

/**
 * @brief RemovePoller on the pollers Broker knows without them being
 * connected: those topology.cache restores behind a relay.
 */
class BrokerStateRemovePoller : public BrokerStateRound {
 public:
  /* RemovePoller empties the global cache, which only apply() creates. */
  void SetUp() override {
    BrokerStateRound::SetUp();
    _state->initialize_cache();
  }

 protected:
  /** @brief Restore poller 5 behind relay 10, as a start would. */
  void restore_poller_behind_relay() {
    TopologyCache cache;
    auto* e = cache.add_entries();
    e->set_poller_id(5);
    e->set_relay_id(10);
    {
      std::ofstream f(_dir / "topology.cache", std::ios::binary);
      ASSERT_TRUE(cache.SerializeToOstream(&f));
    }
    _state->load_topology_cache();
  }

  /** @brief The topology.cache Broker writes when it stops. */
  TopologyCache written_topology() {
    _state->save_topology_cache();
    TopologyCache cache;
    std::error_code read_ec;
    com::centreon::common::read_proto_file(_dir / "topology.cache", cache,
                                           read_ec);
    EXPECT_FALSE(read_ec) << read_ec.message();
    return cache;
  }
};

/**
 * Scenario: a poller restored from topology.cache is removed.
 * Given poller 5 restored from topology.cache behind relay 10
 * And a configuration prepared for it
 * When it is removed
 * Then its configuration files are gone
 * And the topology.cache written when Broker stops no longer leads to it, so
 * the next start forgets it too.
 */
TEST_F(BrokerStateRemovePoller, ExpectedPollerIsForgottenWithItsRelay) {
  restore_poller_behind_relay();
  leave_prepared_conf(5, "v1");
  leave_next_conf(5, "v2");

  EXPECT_TRUE(_state->remove_poller(5));
  EXPECT_FALSE(std::filesystem::exists(_dir / "new-5.prot"));
  EXPECT_FALSE(std::filesystem::exists(_dir / "next-5.prot"));
  EXPECT_EQ(written_topology().entries_size(), 0);
}

/**
 * Scenario: a poller restored from topology.cache came back through its relay.
 * Given poller 5 restored from topology.cache behind relay 10
 * And a configuration prepared for it
 * When relay 10 asks for its configuration
 * Then removing it is refused, and its files are kept.
 */
TEST_F(BrokerStateRemovePoller, ExpectedPollerBackThroughARelayIsKept) {
  restore_poller_behind_relay();
  leave_prepared_conf(5, "v1");
  _state->register_poller_via_relay(5, "Poller5", 10, "");

  EXPECT_FALSE(_state->remove_poller(5));
  EXPECT_TRUE(_state->is_poller_connected(5));
  EXPECT_TRUE(std::filesystem::exists(_dir / "new-5.prot"));
}

/**
 * Scenario: a connected poller is not removed.
 * Given poller 5 connected directly, with a configuration prepared for it
 * When it is removed
 * Then the removal is refused, and its files are kept.
 */
TEST_F(BrokerStateRemovePoller, ConnectedPollerIsKept) {
  leave_prepared_conf(5, "v1");
  connect(5);

  EXPECT_FALSE(_state->remove_poller(5));
  EXPECT_TRUE(_state->is_poller_connected(5));
  EXPECT_TRUE(std::filesystem::exists(_dir / "new-5.prot"));
}
