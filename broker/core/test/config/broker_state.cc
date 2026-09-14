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
    _state->set_poller_conf_sent(poller_id);
    _state->set_poller_engine_conf(poller_id, version);
    _state->set_poller_conf_acknowledged(poller_id);
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

  /* Pollers 2 and 3 have available_conf set, available_conf_sent false: their
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
