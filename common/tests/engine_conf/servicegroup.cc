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
#include <iostream>
#include "common/engine_conf/state.pb.h"
#include "common/engine_conf/state_helper.hh"

using namespace com::centreon::engine;

class TestDiffServicegroup : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    _logger = spdlog::stdout_color_mt("TestDiffServicegroup");
  }
  void TearDown() override { spdlog::drop("TestDiffServicegroup"); }
};

/**
 * @brief Test the diff between two states, the first one with no tag, the
 * second one with one tag. The field added should be filled with one
 * tag.
 */
TEST_F(TestDiffServicegroup, AddServicegroup) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);

  auto* sg1 = old_state.add_servicegroups();
  sg1->set_servicegroup_id(1);
  sg1->set_servicegroup_name("sg1");
  auto setpair1 = sg1->mutable_members()->add_data();
  setpair1->set_first("host1");
  setpair1->set_second("service1");

  configuration::State new_state(old_state);
  auto* sg = new_state.add_servicegroups();
  sg->set_servicegroup_id(4);
  sg->set_servicegroup_name("sg2");
  auto setpair = sg->mutable_members()->add_data();
  setpair->set_first("host2");
  setpair->set_second("service2");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);

  ASSERT_EQ(diff.servicegroups().added_size(), 1);
  ASSERT_EQ(diff.servicegroups().modified_size(), 0);
  ASSERT_EQ(diff.servicegroups().deleted_size(), 0);
  ASSERT_EQ(diff.servicegroups().added(0).servicegroup_id(), 4);
  ASSERT_EQ(diff.servicegroups().added(0).servicegroup_name(), "sg2");

  ASSERT_EQ(diff.servicegroups().added(0).members().data(0).first(), "host2");
  ASSERT_EQ(diff.servicegroups().added(0).members().data(0).second(),
            "service2");
}

/**
 * @brief Test the diff between two states, the first one with one tag, the
 * second one no tag. The diff should have a field deleted filled with one
 * index (0).
 */
TEST_F(TestDiffServicegroup, DelServicegroup) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);

  configuration::State new_state(old_state);

  auto* sg = old_state.add_servicegroups();
  sg->set_servicegroup_id(1);
  sg->set_servicegroup_name("sg1");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);

  ASSERT_EQ(diff.servicegroups().added_size(), 0);
  ASSERT_EQ(diff.servicegroups().modified_size(), 0);
  ASSERT_EQ(diff.servicegroups().deleted_size(), 1);
  ASSERT_EQ(diff.servicegroups().deleted(0), "sg1");
}

TEST_F(TestDiffServicegroup, ModifyServicegroup) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);

  configuration::State new_state(old_state);

  auto* sg = old_state.add_servicegroups();
  sg->set_servicegroup_id(1);
  sg->set_servicegroup_name("sg1");

  auto setpair = sg->mutable_members()->add_data();
  setpair->set_first("host1");
  setpair->set_second("service1");

  auto* sg_m = new_state.add_servicegroups();
  sg_m->set_servicegroup_id(16);
  sg_m->set_servicegroup_name("sg1");
  auto setpair_m = sg_m->mutable_members()->add_data();
  setpair_m->set_first("host4");
  setpair_m->set_second("service4");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);

  ASSERT_EQ(diff.servicegroups().added_size(), 0);
  ASSERT_EQ(diff.servicegroups().modified_size(), 1);
  ASSERT_EQ(diff.servicegroups().deleted_size(), 0);

  ASSERT_EQ(diff.servicegroups().modified(0).servicegroup_id(), 16);
  ASSERT_EQ(diff.servicegroups().modified(0).servicegroup_name(), "sg1");

  ASSERT_EQ(diff.servicegroups().modified(0).members().data(0).first(),
            "host4");
  ASSERT_EQ(diff.servicegroups().modified(0).members().data(0).second(),
            "service4");
}