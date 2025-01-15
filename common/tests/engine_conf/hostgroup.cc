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
#include "common/engine_conf/hostgroup_helper.hh"
#include "common/engine_conf/state.pb.h"
#include "common/engine_conf/state_helper.hh"

using namespace com::centreon::engine;

class TestDiffHostgroup : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    _logger = spdlog::stdout_color_mt("TestDiffHostgroup");
  }
  void TearDown() override { spdlog::drop("TestDiffHostgroup"); }
};

/**
 * @brief Test the diff between two states, the first one with no tag, the
 * second one with one tag. The field added should be filled with one
 * tag.
 */
TEST_F(TestDiffHostgroup, AddHostgroup) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);

  auto* hg1 = old_state.add_hostgroups();
  hg1->set_hostgroup_id(1);
  hg1->set_hostgroup_name("hg1");
  hg1->mutable_members()->add_data("host1");
  hg1->mutable_members()->add_data("host2");

  auto* hg2 = old_state.add_hostgroups();
  hg2->set_hostgroup_id(2);
  hg2->set_hostgroup_name("hg2");
  hg2->mutable_members()->add_data("host3");
  hg2->mutable_members()->add_data("host4");

  auto* hg3 = old_state.add_hostgroups();
  hg3->set_hostgroup_id(3);
  hg3->set_hostgroup_name("hg3");
  hg3->mutable_members()->add_data("host5");
  hg3->mutable_members()->add_data("host6");

  configuration::State new_state(old_state);

  auto* hg = new_state.add_hostgroups();
  hg->set_hostgroup_id(4);
  hg->set_hostgroup_name("hg4");
  hg->mutable_members()->add_data("host7");
  hg->mutable_members()->add_data("host8");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);

  ASSERT_EQ(diff.hostgroups().added_size(), 1);
  ASSERT_EQ(diff.hostgroups().modified_size(), 0);
  ASSERT_EQ(diff.hostgroups().deleted_size(), 0);
  ASSERT_EQ(diff.hostgroups().added(0).hostgroup_id(), 4);
  ASSERT_EQ(diff.hostgroups().added(0).hostgroup_name(), "hg4");

  std::vector<std::string> expected_members = {"host7", "host8"};
  std::vector<std::string> actual_members(
      diff.hostgroups().added(0).members().data().begin(),
      diff.hostgroups().added(0).members().data().end());
  ASSERT_EQ(actual_members, expected_members);
}

/**
 * @brief Test the diff between two states, the first one with one tag, the
 * second one no tag. The diff should have a field deleted filled with one
 * index (0).
 */
TEST_F(TestDiffHostgroup, DelHostgroup) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);

  auto* hg1 = old_state.add_hostgroups();
  hg1->set_hostgroup_id(1);
  hg1->set_hostgroup_name("hg1");
  hg1->mutable_members()->add_data("host1");
  hg1->mutable_members()->add_data("host2");

  auto* hg2 = old_state.add_hostgroups();
  hg2->set_hostgroup_id(2);
  hg2->set_hostgroup_name("hg2");
  hg2->mutable_members()->add_data("host3");
  hg2->mutable_members()->add_data("host4");

  auto* hg3 = old_state.add_hostgroups();
  hg3->set_hostgroup_id(3);
  hg3->set_hostgroup_name("hg3");
  hg3->mutable_members()->add_data("host5");
  hg3->mutable_members()->add_data("host6");

  configuration::State new_state(old_state);

  auto* hg = old_state.add_hostgroups();
  hg->set_hostgroup_id(4);
  hg->set_hostgroup_name("hg4");
  hg->mutable_members()->add_data("host7");
  hg->mutable_members()->add_data("host8");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);

  ASSERT_EQ(diff.hostgroups().added_size(), 0);
  ASSERT_EQ(diff.hostgroups().modified_size(), 0);
  ASSERT_EQ(diff.hostgroups().deleted_size(), 1);
  ASSERT_EQ(diff.hostgroups().deleted(0), 4);
}

TEST_F(TestDiffHostgroup, ModifyHostgroup) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);

  auto* hg1 = old_state.add_hostgroups();
  hg1->set_hostgroup_id(1);
  hg1->set_hostgroup_name("hg1");
  hg1->mutable_members()->add_data("host1");
  hg1->mutable_members()->add_data("host2");

  auto* hg2 = old_state.add_hostgroups();
  hg2->set_hostgroup_id(2);
  hg2->set_hostgroup_name("hg2");
  hg2->mutable_members()->add_data("host3");
  hg2->mutable_members()->add_data("host4");

  auto* hg3 = old_state.add_hostgroups();
  hg3->set_hostgroup_id(3);
  hg3->set_hostgroup_name("hg3");
  hg3->mutable_members()->add_data("host5");
  hg3->mutable_members()->add_data("host6");

  configuration::State new_state(old_state);

  auto* hg = old_state.add_hostgroups();
  hg->set_hostgroup_id(4);
  hg->set_hostgroup_name("hg4");
  hg->mutable_members()->add_data("host7");
  hg->mutable_members()->add_data("host8");

  auto* hg_m = new_state.add_hostgroups();
  hg_m->set_hostgroup_id(4);
  hg_m->set_hostgroup_name("hg4");
  hg_m->mutable_members()->add_data("host10");
  hg_m->mutable_members()->add_data("host11");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);

  ASSERT_EQ(diff.hostgroups().added_size(), 0);
  ASSERT_EQ(diff.hostgroups().modified_size(), 1);
  ASSERT_EQ(diff.hostgroups().deleted_size(), 0);

  ASSERT_EQ(diff.hostgroups().modified(0).hostgroup_id(), 4);
  ASSERT_EQ(diff.hostgroups().modified(0).hostgroup_name(), "hg4");

  std::vector<std::string> expected_members = {"host10", "host11"};
  std::vector<std::string> actual_members(
      diff.hostgroups().modified(0).members().data().begin(),
      diff.hostgroups().modified(0).members().data().end());
  ASSERT_EQ(actual_members, expected_members);
}