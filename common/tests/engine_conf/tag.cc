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
#include "common/engine_conf/state.pb.h"
#include "common/engine_conf/state_helper.hh"
#include "common/engine_conf/tag_helper.hh"

using namespace com::centreon::engine;

class TestDiffTag : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override { _logger = spdlog::stdout_color_mt("TestDiffTag"); }
  void TearDown() override { spdlog::drop("TestDiffTag"); }
};

/**
 * @brief Test the diff between two states, the first one with no tag, the
 * second one with one tag. The field added should be filled with one
 * tag.
 */
TEST_F(TestDiffTag, AddTag) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);

  auto* tag1 = old_state.add_tags();
  configuration::tag_helper tag_hlp1(tag1);
  tag1->mutable_key()->set_id(3);
  tag1->mutable_key()->set_type(configuration::TagType::tag_hostgroup);
  tag1->set_tag_name("tag3");

  auto* tag2 = old_state.add_tags();
  configuration::tag_helper tag_hlp2(tag2);
  tag2->mutable_key()->set_id(4);
  tag2->mutable_key()->set_type(configuration::TagType::tag_servicegroup);
  tag2->set_tag_name("tag4");

  auto* tag3 = old_state.add_tags();
  configuration::tag_helper tag_hlp3(tag3);
  tag3->mutable_key()->set_id(5);
  tag3->mutable_key()->set_type(configuration::TagType::tag_hostcategory);
  tag3->set_tag_name("tag5");

  configuration::State new_state(old_state);

  auto* tag = new_state.add_tags();
  configuration::tag_helper tag_hlp(tag);
  tag->mutable_key()->set_id(1);
  tag->mutable_key()->set_type(configuration::TagType::tag_servicecategory);
  tag->set_tag_name("tag1");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);

  ASSERT_EQ(diff.tags().added_size(), 1);
  ASSERT_EQ(diff.tags().modified_size(), 0);
  ASSERT_EQ(diff.tags().deleted_size(), 0);
  ASSERT_EQ(diff.tags().added(0).key().id(), 1);
  ASSERT_EQ(diff.tags().added(0).key().type(), 2);
  ASSERT_EQ(diff.tags().added(0).tag_name(), "tag1");
}

/**
 * @brief Test the diff between two states, the first one with one tag, the
 * second one no tag. The diff should have a field deleted filled with one
 * index (0).
 */
TEST_F(TestDiffTag, DelTag) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);

  auto* tag1 = old_state.add_tags();
  configuration::tag_helper tag_hlp1(tag1);
  tag1->mutable_key()->set_id(3);
  tag1->mutable_key()->set_type(configuration::TagType::tag_hostgroup);
  tag1->set_tag_name("tag3");

  auto* tag2 = old_state.add_tags();
  configuration::tag_helper tag_hlp2(tag2);
  tag2->mutable_key()->set_id(4);
  tag2->mutable_key()->set_type(configuration::TagType::tag_servicegroup);
  tag2->set_tag_name("tag4");

  configuration::State new_state(old_state);

  auto* tag3 = old_state.add_tags();
  configuration::tag_helper tag_hlp3(tag3);
  tag3->mutable_key()->set_id(5);
  tag3->mutable_key()->set_type(configuration::TagType::tag_hostcategory);
  tag3->set_tag_name("tag5");

  auto* tag = old_state.add_tags();
  configuration::tag_helper tag_hlp(tag);
  tag->mutable_key()->set_id(1);
  tag->mutable_key()->set_type(configuration::TagType::tag_servicecategory);
  tag->set_tag_name("tag1");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);
  ASSERT_EQ(diff.tags().added_size(), 0);
  ASSERT_EQ(diff.tags().modified_size(), 0);
  ASSERT_EQ(diff.tags().deleted_size(), 2);
  ASSERT_EQ(diff.tags().deleted(0).id(), 5);
  ASSERT_EQ(diff.tags().deleted(0).type(), 3);
  ASSERT_EQ(diff.tags().deleted(1).id(), 1);
  ASSERT_EQ(diff.tags().deleted(1).type(), 2);
}

TEST_F(TestDiffTag, ModifyTag) {
  configuration::State old_state;
  configuration::state_helper old_state_hlp(&old_state);
  auto* tag1 = old_state.add_tags();

  configuration::tag_helper tag_hlp1(tag1);
  tag1->mutable_key()->set_id(3);
  tag1->mutable_key()->set_type(configuration::TagType::tag_hostgroup);
  tag1->set_tag_name("tag3");

  auto* tag2 = old_state.add_tags();
  configuration::tag_helper tag_hlp2(tag2);
  tag2->mutable_key()->set_id(4);
  tag2->mutable_key()->set_type(configuration::TagType::tag_servicegroup);
  tag2->set_tag_name("tag4");

  auto* tag3 = old_state.add_tags();
  configuration::tag_helper tag_hlp3(tag3);
  tag3->mutable_key()->set_id(5);
  tag3->mutable_key()->set_type(configuration::TagType::tag_hostcategory);
  tag3->set_tag_name("tag5");

  configuration::State new_state(old_state);

  auto* tag = old_state.add_tags();
  configuration::tag_helper tag_hlp(tag);
  tag->mutable_key()->set_id(1);
  tag->mutable_key()->set_type(configuration::TagType::tag_servicecategory);
  tag->set_tag_name("tag1");

  auto* tag_m = new_state.add_tags();
  configuration::tag_helper tag_hlp_m(tag_m);
  tag_m->mutable_key()->set_id(1);
  tag_m->mutable_key()->set_type(configuration::TagType::tag_servicecategory);
  tag_m->set_tag_name("tag_m");

  configuration::DiffState diff;
  configuration::state_helper::diff(old_state, new_state, _logger, &diff);

  ASSERT_EQ(diff.tags().added_size(), 0);
  ASSERT_EQ(diff.tags().modified_size(), 1);
  ASSERT_EQ(diff.tags().deleted_size(), 0);
  ASSERT_EQ(diff.tags().modified(0).key().id(), 1);
  ASSERT_EQ(diff.tags().modified(0).key().type(), 2);
  ASSERT_EQ(diff.tags().modified(0).tag_name(), "tag_m");
}
