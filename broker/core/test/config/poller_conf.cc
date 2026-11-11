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

#include "broker/core/config/applier/poller_conf.hh"

using com::centreon::broker::config::applier::poller_conf;
using delivery = poller_conf::delivery_state;
using db = poller_conf::db_state;

/**
 * @brief The transitions of the two automata of poller_conf, numbered as in
 * doc/conf-delivery-simplification-fr.md, section 1.2. Where the current
 * behaviour still differs from the plan (transition 4, until step 4), the test
 * describes the current one.
 */
class PollerConfDelivery : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger =
      std::make_shared<spdlog::logger>("poller_conf_test");

  /** @brief A poller running v0, that Broker knows. */
  poller_conf up_to_date() { return poller_conf{1, _logger, "v0"}; }

  /** @brief A poller running v0, owed v1. */
  poller_conf owed() {
    poller_conf conf = up_to_date();
    conf.prepare("v1");
    return conf;
  }

  /** @brief A poller running v0, sent v1. */
  poller_conf in_flight() {
    poller_conf conf = owed();
    conf.on_sent("v1");
    return conf;
  }

  /** @brief A poller running v0, that could not apply v1. */
  poller_conf rejected() {
    poller_conf conf = in_flight();
    conf.on_rejected("v0", "v1", "boom");
    return conf;
  }
};

/**
 * Scenario: transition 2, Broker does not know what the poller runs.
 * Given a connected poller
 * When Broker finds nothing stored for it
 * Then it is unknown, and owes nothing.
 */
TEST_F(PollerConfDelivery, T02_ConfigurationUnknown) {
  poller_conf conf = up_to_date();
  conf.on_conf_unknown();
  EXPECT_EQ(conf.delivery_status(), delivery::unknown);
  EXPECT_FALSE(conf.broker_knows_conf());
  EXPECT_FALSE(conf.owes_conf());
}

/**
 * Scenario: transition 4, an export while Broker does not know what the poller
 * runs.
 * Given an unknown poller
 * When v1 is prepared for it
 * Then, until step 4 of the plan, it is owed at once.
 */
TEST_F(PollerConfDelivery, T04_ExportWhileUnknownIsOwedForNow) {
  poller_conf conf = up_to_date();
  conf.on_conf_unknown();
  conf.prepare("v1");
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
  EXPECT_TRUE(conf.needs_update());
}

/**
 * Scenario: transition 5, Broker gets the configuration back.
 * Given an unknown poller
 * When its configuration is stored again
 * Then it is up to date.
 */
TEST_F(PollerConfDelivery, T05_ConfigurationRecovered) {
  poller_conf conf = up_to_date();
  conf.on_conf_unknown();
  conf.on_conf_recovered();
  EXPECT_EQ(conf.delivery_status(), delivery::up_to_date);
  EXPECT_TRUE(conf.broker_knows_conf());
}

/**
 * Scenario: transition 6, the configuration comes back while an export waits.
 * Given an unknown poller, owed v1 since (transition 4)
 * When its configuration is stored again
 * Then v1 is still owed.
 */
TEST_F(PollerConfDelivery, T06_RecoveredWithAnExportWaiting) {
  poller_conf conf = up_to_date();
  conf.on_conf_unknown();
  conf.prepare("v1");
  conf.on_conf_recovered();
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
}

/**
 * Scenario: transition 7, an export.
 * Given a poller up to date
 * When v1 is prepared
 * Then v1 is owed and can be sent.
 */
TEST_F(PollerConfDelivery, T07_Export) {
  poller_conf conf = owed();
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
  EXPECT_TRUE(conf.owes_conf());
  EXPECT_TRUE(conf.needs_update());
  EXPECT_EQ(conf.available_conf(), "v1");
}

/**
 * Scenario: transition 8, an export replaces one not sent yet.
 * Given a poller owed v1
 * When v2 is prepared
 * Then v2 is owed instead.
 */
TEST_F(PollerConfDelivery, T08_ExportReplacesOneNotSent) {
  poller_conf conf = owed();
  conf.prepare("v2");
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
  EXPECT_EQ(conf.available_conf(), "v2");
}

/**
 * Scenario: transition 9, the owed version is sent.
 * Given a poller owed v1
 * When v1 is sent
 * Then it is in flight, its acknowledgement awaited, and nothing else is sent.
 */
TEST_F(PollerConfDelivery, T09_Sent) {
  poller_conf conf = owed();
  EXPECT_TRUE(conf.on_sent("v1"));
  EXPECT_EQ(conf.delivery_status(), delivery::in_flight);
  EXPECT_TRUE(conf.awaits_acknowledgement());
  EXPECT_FALSE(conf.owes_conf());
  EXPECT_FALSE(conf.needs_update());
}

/**
 * Scenario: transition 10, the poller announces the version prepared for it.
 * Given a poller running v1, owed v1 by a Broker that never saw its
 * acknowledgement
 * When the announcement is taken as the acknowledgement
 * Then it is up to date, and its difference waits for the database.
 */
TEST_F(PollerConfDelivery, T10_AnnouncementAcknowledges) {
  poller_conf conf{1, _logger, "v1"};
  conf.on_announced("v1");
  EXPECT_EQ(conf.delivery_status(), delivery::up_to_date);
  EXPECT_EQ(conf.db_status(), db::pending);
  EXPECT_FALSE(conf.owes_conf());
}

/**
 * Scenario: transition 11, an export while a version is in flight.
 * Given a poller sent v1
 * When v2 is prepared
 * Then it stays in flight: nothing is sent before it answers
 * And v2 is owed, which keeps the round open.
 */
TEST_F(PollerConfDelivery, T11_ExportWhileInFlight) {
  poller_conf conf = in_flight();
  conf.prepare("v2");
  EXPECT_EQ(conf.delivery_status(), delivery::in_flight);
  EXPECT_FALSE(conf.awaits_acknowledgement());
  EXPECT_TRUE(conf.owes_conf());
  EXPECT_FALSE(conf.needs_update());
}

/**
 * Scenario: the same export announced twice while it is in flight.
 * Given a poller sent v1
 * When v1 is prepared again
 * Then its acknowledgement is still awaited, and counts for the round.
 */
TEST_F(PollerConfDelivery, SameExportTwiceKeepsTheAcknowledgement) {
  poller_conf conf = in_flight();
  conf.prepare("v1");
  EXPECT_TRUE(conf.awaits_acknowledgement());
  EXPECT_TRUE(conf.on_acknowledged("v1"));
}

/**
 * Scenario: transition 13, the acknowledgement.
 * Given a poller sent v1
 * When it acknowledges v1
 * Then it is up to date on v1, and its difference waits for the database.
 */
TEST_F(PollerConfDelivery, T13_Acknowledged) {
  poller_conf conf = in_flight();
  EXPECT_TRUE(conf.on_acknowledged("v1"));
  EXPECT_EQ(conf.delivery_status(), delivery::up_to_date);
  EXPECT_EQ(conf.db_status(), db::pending);
  EXPECT_EQ(conf.engine_conf(), "v1");
  EXPECT_TRUE(conf.conf_in_flight().empty());
}

/**
 * Scenario: transition 14, the acknowledgement of an overtaken version.
 * Given a poller sent v1, and owed v2 since
 * When it acknowledges v1
 * Then that does not count for the round
 * And v2 is owed, and can be sent now.
 */
TEST_F(PollerConfDelivery, T14_AcknowledgedWithANewerExport) {
  poller_conf conf = in_flight();
  conf.prepare("v2");
  EXPECT_FALSE(conf.on_acknowledged("v1"));
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
  EXPECT_EQ(conf.db_status(), db::up_to_date);
  EXPECT_EQ(conf.engine_conf(), "v1");
  EXPECT_TRUE(conf.needs_update());
}

/**
 * Scenario: transition 16, the poller could not apply the version.
 * Given a poller sent v1
 * When it reports it could not apply it
 * Then it is rejected, owes nothing, and the failure is kept.
 */
TEST_F(PollerConfDelivery, T16_Rejected) {
  poller_conf conf = rejected();
  EXPECT_EQ(conf.delivery_status(), delivery::rejected);
  EXPECT_FALSE(conf.owes_conf());
  ASSERT_TRUE(conf.last_rejection());
  EXPECT_EQ(conf.last_rejection()->version, "v1");
  EXPECT_EQ(conf.last_rejection()->error, "boom");
}

/**
 * Scenario: transition 17, the failure of an overtaken version.
 * Given a poller sent v1, and owed v2 since
 * When it reports it could not apply v1
 * Then v2 is owed, and can be sent now.
 */
TEST_F(PollerConfDelivery, T17_RejectedWithANewerExport) {
  poller_conf conf = in_flight();
  conf.prepare("v2");
  EXPECT_FALSE(conf.on_rejected("v0", "v1", "boom"));
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
  EXPECT_FALSE(conf.last_rejection());
}

/**
 * Scenario: transition 18, an export after a failure.
 * Given a poller that could not apply v1
 * When v2 is prepared
 * Then v2 is owed, the failure of v1 still told
 * And once v2 is acknowledged, the failure is forgotten.
 */
TEST_F(PollerConfDelivery, T18_ExportAfterARejection) {
  poller_conf conf = rejected();
  conf.prepare("v2");
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
  ASSERT_TRUE(conf.last_rejection());
  EXPECT_EQ(conf.last_rejection()->version, "v1");
  EXPECT_TRUE(conf.on_sent("v2"));
  EXPECT_TRUE(conf.on_acknowledged("v2"));
  EXPECT_FALSE(conf.last_rejection());
}

/**
 * Scenario: transition 19, a reconnection after a failure.
 * Given a poller that could not apply v1
 * When it connects again
 * Then nothing is left of the failure: the files decide what is owed.
 */
TEST_F(PollerConfDelivery, T19_ReconnectionAfterARejection) {
  poller_conf conf = rejected();
  conf.on_connected("v0");
  EXPECT_EQ(conf.delivery_status(), delivery::up_to_date);
  EXPECT_FALSE(conf.last_rejection());
  EXPECT_TRUE(conf.available_conf().empty());
}

/**
 * Scenario: the round closes.
 * Given a poller whose acknowledged difference waits for the database
 * When the round closes
 * Then the database automaton is back to up to date.
 */
TEST_F(PollerConfDelivery, RoundClosed) {
  poller_conf conf = in_flight();
  conf.on_acknowledged("v1");
  conf.leave_round();
  EXPECT_EQ(conf.db_status(), db::up_to_date);
}

/**
 * Scenario: a relay asks for the configuration of a poller sent the version
 * prepared for it.
 * Given a poller behind a relay, sent v1
 * When the relay asks for its configuration
 * Then v1 is still awaited: the answer sends it again.
 */
TEST_F(PollerConfDelivery, RelayRequestKeepsThePreparedVersionInFlight) {
  poller_conf conf = in_flight();
  conf.on_relay_request("v0");
  EXPECT_EQ(conf.delivery_status(), delivery::in_flight);
  EXPECT_TRUE(conf.awaits_acknowledgement());
}

/**
 * Scenario: a relay asks for the configuration of a poller sent an overtaken
 * version.
 * Given a poller behind a relay, sent v1, and owed v2 since
 * When the relay asks for its configuration, saying the poller runs v0
 * Then v1 will not be acknowledged through this exchange: v2 is owed now.
 */
TEST_F(PollerConfDelivery, RelayRequestDropsAnOvertakenVersion) {
  poller_conf conf = in_flight();
  conf.prepare("v2");
  conf.on_relay_request("v0");
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
  EXPECT_TRUE(conf.needs_update());
}

/**
 * Scenario: a poller_conf created from files or an export is not registered.
 * Given a poller_conf just created
 * Then the registry has no Engine entry for it.
 */
TEST_F(PollerConfDelivery, CreatedUnregistered) {
  EXPECT_FALSE(up_to_date().is_registered());
}

/**
 * Scenario: the connection registers the poller, the disconnection unregisters
 * it.
 * Given a poller_conf not registered
 * When the poller connects, then disconnects
 * Then it is registered, then no more.
 */
TEST_F(PollerConfDelivery, ConnectionRegisters) {
  poller_conf conf = up_to_date();
  conf.on_connected("v0");
  EXPECT_TRUE(conf.is_registered());
  conf.on_disconnected();
  EXPECT_FALSE(conf.is_registered());
}

/**
 * Scenario: a poller listed in topology.cache is expected.
 * Given a poller_conf owed v1
 * When topology.cache says the poller is expected
 * Then it is registered, and is still owed v1.
 */
TEST_F(PollerConfDelivery, ExpectedRegistersAndKeepsTheState) {
  poller_conf conf = owed();
  conf.on_expected();
  EXPECT_TRUE(conf.is_registered());
  EXPECT_EQ(conf.delivery_status(), delivery::owed);
  EXPECT_EQ(conf.available_conf(), "v1");
}
