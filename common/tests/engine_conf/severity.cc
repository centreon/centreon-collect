/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include "common/engine_conf/severity_helper.hh"
#include "common/engine_conf/state_helper.hh"
#include "engine/inc/com/centreon/engine/configuration/applier/pb_difference.hh"

using namespace com::centreon::engine;

class TestDiffSeverity : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    _logger = spdlog::stdout_color_mt("TestDiffSeverity");
  }
  void TearDown() override { spdlog::drop("TestDiffSeverity"); }
};

/**
 * @brief Test the diff between two states, the first one with no severity, the
 * second one with one severity. The field added should be filled with one
 * severity.
 */
TEST_F(TestDiffSeverity, AddSeverity) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);
  configuration::State new_state(old_state);
  auto* sev = new_state.add_severities();
  configuration::severity_helper sev_hlp(sev);
  sev->mutable_key()->set_id(12);
  sev->mutable_key()->set_type(1);
  sev->set_severity_name("severity_12");
  sev->set_level(1);
  sev->set_icon_id(15);

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);
  ASSERT_EQ(diff.severities().added_size(), 1);
  ASSERT_EQ(diff.severities().modified_size(), 0);
  ASSERT_EQ(diff.severities().deleted_size(), 0);
  ASSERT_EQ(diff.severities().added(0).key().id(), 12);
  ASSERT_EQ(diff.severities().added(0).key().type(), 1);
  ASSERT_EQ(diff.severities().added(0).severity_name(), "severity_12");
  ASSERT_EQ(diff.severities().added(0).level(), 1);
  ASSERT_EQ(diff.severities().added(0).icon_id(), 15);
  ASSERT_FALSE(diff.has_state());
}

/**
 * @brief Test the diff between two states, the first one with one severity, the
 * second one no severity. The diff should have a field deleted filled with one
 * index (0).
 */
TEST_F(TestDiffSeverity, DelSeverity) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);
  configuration::State new_state(old_state);
  auto* sev = old_state.add_severities();
  configuration::severity_helper sev_hlp(sev);
  sev->mutable_key()->set_id(12);
  sev->mutable_key()->set_type(1);
  sev->set_severity_name("severity_12");
  sev->set_level(1);
  sev->set_icon_id(15);

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);
  ASSERT_EQ(diff.severities().added_size(), 0);
  ASSERT_EQ(diff.severities().modified_size(), 0);
  ASSERT_EQ(diff.severities().deleted_size(), 1);
  ASSERT_TRUE(
      MessageDifferencer::Equals(diff.severities().deleted(0), sev->key()));
  ASSERT_FALSE(diff.has_state());
}

TEST_F(TestDiffSeverity, ModifySeverity) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);
  configuration::State new_state(old_state);
  auto* sev1 = old_state.add_severities();
  configuration::severity_helper sev_hlp1(sev1);
  sev1->mutable_key()->set_id(12);
  sev1->mutable_key()->set_type(1);
  sev1->set_severity_name("severity_11");
  sev1->set_level(2);
  sev1->set_icon_id(14);

  auto* sev2 = new_state.add_severities();
  configuration::severity_helper sev_hlp2(sev2);
  sev2->mutable_key()->set_id(12);
  sev2->mutable_key()->set_type(1);
  sev2->set_severity_name("severity_12");
  sev2->set_level(1);
  sev2->set_icon_id(15);

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);
  ASSERT_EQ(diff.severities().added_size(), 0);
  ASSERT_EQ(diff.severities().modified_size(), 1);
  ASSERT_EQ(diff.severities().deleted_size(), 0);
  // We check the key
  ASSERT_EQ(diff.severities().modified(0).key().id(), 12);
  ASSERT_EQ(diff.severities().modified(0).key().type(), 1);
  // We check the object
  ASSERT_EQ(diff.severities().modified(0).object().key().id(), 12);
  ASSERT_EQ(diff.severities().modified(0).object().key().type(), 1);
  ASSERT_EQ(diff.severities().modified(0).object().severity_name(),
            "severity_12");
  ASSERT_EQ(diff.severities().modified(0).object().level(), 1);
  ASSERT_EQ(diff.severities().modified(0).object().icon_id(), 15);
  ASSERT_FALSE(diff.has_state());
}
