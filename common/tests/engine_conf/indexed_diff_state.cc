/**
 * Copyright 2025-2026 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/indexed_diff_state.hh"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include "common/engine_conf/command_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/hostgroup_helper.hh"
#include "common/engine_conf/state.pb.h"
#include "common/engine_conf/tag_helper.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine;
using com::centreon::common::log_v2::log_v2;

class IndexedDiffState : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    log_v2::load("indexedDiffState");
    _logger = log_v2::instance().get(log_v2::BBDO);
  }
  void TearDown() override { log_v2::unload(); }
};

TEST_F(IndexedDiffState, AddCommand) {
  configuration::indexed_diff_state global_diff;
  configuration::DiffState diff_state;
  auto cmd = std::make_unique<configuration::Command>();
  configuration::command_helper cmd_helper(cmd.get());
  cmd->set_command_line("test");
  cmd->set_command_name("test1");
  cmd->set_connector("super connector");

  diff_state.mutable_commands()->mutable_added()->AddAllocated(cmd.release());
  global_diff.add_diff_state(diff_state, _logger);
  ASSERT_EQ(global_diff.added_commands().size(), 1);
}

TEST_F(IndexedDiffState, AddRemoveCommand) {
  configuration::indexed_diff_state global_diff;
  configuration::DiffState diff_state;
  auto cmd = std::make_unique<configuration::Command>();
  configuration::command_helper cmd_helper(cmd.get());
  cmd->set_command_line("test");
  cmd->set_command_name("test1");
  cmd->set_connector("super connector");

  diff_state.mutable_commands()->mutable_added()->AddAllocated(cmd.release());
  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(diff_state.commands().added_size(), 0);
  diff_state.mutable_commands()->add_removed("test1");
  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(global_diff.added_commands().size(), 0);
  ASSERT_EQ(global_diff.removed_commands().size(), 0);
  ASSERT_EQ(global_diff.modified_commands().size(), 1);
}

TEST_F(IndexedDiffState, AddRemoveCommandAddTag) {
  configuration::indexed_diff_state global_diff;
  configuration::DiffState diff_state;
  auto cmd = std::make_unique<configuration::Command>();
  configuration::command_helper cmd_helper(cmd.get());
  cmd->set_command_line("test");
  cmd->set_command_name("test1");
  cmd->set_connector("super connector");
  diff_state.mutable_commands()->mutable_added()->AddAllocated(cmd.release());

  auto tag = std::make_unique<configuration::Tag>();
  configuration::tag_helper tag_hlp(tag.get());
  tag->mutable_key()->set_id(12);
  tag->mutable_key()->set_type(2);
  tag->set_tag_name("tag");
  diff_state.mutable_tags()->mutable_added()->AddAllocated(tag.release());

  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(diff_state.commands().added_size(), 0);
  diff_state.mutable_commands()->add_removed("test1");
  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(global_diff.added_commands().size(), 0);
  ASSERT_EQ(global_diff.removed_commands().size(), 0);
  ASSERT_EQ(global_diff.modified_commands().size(), 1);

  ASSERT_EQ(global_diff.added_tags().size(), 1);
  ASSERT_EQ(global_diff.removed_tags().size(), 0);
  ASSERT_EQ(global_diff.modified_tags().size(), 0);
}

TEST_F(IndexedDiffState, ModifyRemoveTag) {
  configuration::indexed_diff_state global_diff;
  configuration::DiffState diff_state;

  auto tag = std::make_unique<configuration::Tag>();
  configuration::tag_helper tag_hlp(tag.get());
  tag->mutable_key()->set_id(12);
  tag->mutable_key()->set_type(2);
  tag->set_tag_name("tag");
  diff_state.mutable_tags()->mutable_modified()->AddAllocated(tag.release());

  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(diff_state.commands().modified_size(), 0);
  auto key = std::make_unique<configuration::TagKeyWithPoller>();
  key->set_id(12);
  key->set_type(2);
  key->set_poller_id(0);
  diff_state.mutable_tags()->mutable_removed()->AddAllocated(key.release());
  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(global_diff.added_tags().size(), 0);
  ASSERT_EQ(global_diff.removed_tags().size(), 0);
  ASSERT_EQ(global_diff.modified_tags().size(), 1);
}

TEST_F(IndexedDiffState, RemoveModifyTag) {
  configuration::indexed_diff_state global_diff;
  configuration::DiffState diff_state;

  auto key = std::make_unique<configuration::TagKeyWithPoller>();
  key->set_id(12);
  key->set_type(2);
  key->set_poller_id(0);
  diff_state.mutable_tags()->mutable_removed()->AddAllocated(key.release());
  global_diff.add_diff_state(diff_state, _logger);

  auto tag = std::make_unique<configuration::Tag>();
  configuration::tag_helper tag_hlp(tag.get());
  tag->mutable_key()->set_id(12);
  tag->mutable_key()->set_type(2);
  tag->set_tag_name("tag");
  diff_state.mutable_tags()->mutable_modified()->AddAllocated(tag.release());

  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(global_diff.added_tags().size(), 0);
  ASSERT_EQ(global_diff.removed_tags().size(), 0);
  ASSERT_EQ(global_diff.modified_tags().size(), 1);
}

TEST_F(IndexedDiffState, AddModifyTag) {
  configuration::indexed_diff_state global_diff;
  configuration::DiffState diff_state;

  auto tag = std::make_unique<configuration::Tag>();
  configuration::tag_helper tag_hlp(tag.get());
  tag->mutable_key()->set_id(12);
  tag->mutable_key()->set_type(2);
  tag->set_tag_name("tag1");
  diff_state.mutable_tags()->mutable_modified()->AddAllocated(tag.release());

  global_diff.add_diff_state(diff_state, _logger);

  auto tag1 = std::make_unique<configuration::Tag>();
  configuration::tag_helper tag1_hlp(tag1.get());
  tag1->mutable_key()->set_id(12);
  tag1->mutable_key()->set_type(2);
  tag1->set_tag_name("tag2");
  diff_state.mutable_tags()->mutable_modified()->AddAllocated(tag1.release());

  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(global_diff.added_tags().size(), 0);
  ASSERT_EQ(global_diff.removed_tags().size(), 0);
  ASSERT_EQ(global_diff.modified_tags().size(), 1);
}

TEST_F(IndexedDiffState, ModifyAddTag) {
  configuration::indexed_diff_state global_diff;
  configuration::DiffState diff_state;

  auto tag1 = std::make_unique<configuration::Tag>();
  configuration::tag_helper tag1_hlp(tag1.get());
  tag1->mutable_key()->set_id(12);
  tag1->mutable_key()->set_type(2);
  tag1->set_tag_name("tag2");
  diff_state.mutable_tags()->mutable_modified()->AddAllocated(tag1.release());

  global_diff.add_diff_state(diff_state, _logger);

  auto tag = std::make_unique<configuration::Tag>();
  configuration::tag_helper tag_hlp(tag.get());
  tag->mutable_key()->set_id(12);
  tag->mutable_key()->set_type(2);
  tag->set_tag_name("tag1");
  diff_state.mutable_tags()->mutable_modified()->AddAllocated(tag.release());

  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(global_diff.added_tags().size(), 0);
  ASSERT_EQ(global_diff.removed_tags().size(), 0);
  ASSERT_EQ(global_diff.modified_tags().size(), 1);
}

/* We want to start with a diff just containing a state. Then we add a diff with
 * added hosts. */
TEST_F(IndexedDiffState, AddHosts) {
  configuration::indexed_diff_state global_diff;
  configuration::DiffState diff_state;

  diff_state.mutable_state()->set_poller_id(1);
  for (int i = 1; i <= 10; i++) {
    auto* host = diff_state.mutable_state()->add_hosts();
    configuration::host_helper host_hlp(host);
    host->set_host_id(i);
    host->set_host_name(fmt::format("host_{}", i));
    host->set_poller_id(1);
  }

  global_diff.add_diff_state(diff_state, _logger);

  ASSERT_EQ(global_diff.added_hosts().size(), 10);

  configuration::DiffState diff_state2;
  diff_state2.mutable_state()->set_poller_id(2);
  for (int i = 11; i <= 15; i++) {
    auto* host = diff_state2.mutable_state()->add_hosts();
    configuration::host_helper host_hlp(host);
    host->set_host_id(i);
    host->set_host_name(fmt::format("host_{}", i));
    host->set_poller_id(2);
  }
  global_diff.add_diff_state(diff_state2, _logger);

  ASSERT_EQ(global_diff.added_hosts().size(), 15);

  configuration::DiffState diff_state3;
  for (int i = 16; i <= 20; i++) {
    auto* host = diff_state3.mutable_hosts()->add_added();
    configuration::host_helper host_hlp(host);
    host->set_host_id(i);
    host->set_host_name(fmt::format("host_{}", i));
    host->set_poller_id(3);
  }
  global_diff.add_diff_state(diff_state3, _logger);

  ASSERT_EQ(global_diff.added_hosts().size(), 20);
}

/**
 * @brief Two differences of the same poller, one after the other: the second
 * one was computed from the configuration the first one led to.
 *
 * Unlike the differences of two pollers, which are merged as a union, here a
 * message added then removed never existed for whoever reads the result.
 */
class IndexedDiffStateSamePoller : public IndexedDiffState {
 protected:
  configuration::indexed_diff_state _diff;

  static configuration::Host* host(uint64_t id, const std::string& name) {
    auto* h = new configuration::Host();
    configuration::host_helper hlp(h);
    h->set_host_id(id);
    h->set_host_name(name);
    h->set_poller_id(1);
    return h;
  }
  void first(configuration::DiffState& d) { _diff.add_diff_state(d, _logger); }
  void next(configuration::DiffState& d) {
    _diff.add_diff_state_from_same_poller(d, _logger);
  }
};

/**
 * Scenario: a host added by the first difference is modified by the next one.
 * Then the host is added, with its latest definition.
 */
TEST_F(IndexedDiffStateSamePoller, AddedThenModifiedIsAddedWithTheLatest) {
  configuration::DiffState d1;
  d1.mutable_hosts()->mutable_added()->AddAllocated(host(1, "h1"));
  first(d1);
  configuration::DiffState d2;
  d2.mutable_hosts()->mutable_modified()->AddAllocated(host(1, "h1-renamed"));
  next(d2);

  ASSERT_EQ(_diff.added_hosts().size(), 1u);
  EXPECT_EQ(_diff.added_hosts().at(1)->host_name(), "h1-renamed");
  EXPECT_TRUE(_diff.modified_hosts().empty());
  EXPECT_TRUE(_diff.removed_hosts().empty());
}

/**
 * Scenario: a host added by the first difference is removed by the next one.
 * Then nothing is left of it: it was never seen.
 */
TEST_F(IndexedDiffStateSamePoller, AddedThenRemovedVanishes) {
  configuration::DiffState d1;
  d1.mutable_hosts()->mutable_added()->AddAllocated(host(1, "h1"));
  first(d1);
  configuration::DiffState d2;
  d2.mutable_hosts()->add_removed(1);
  next(d2);

  EXPECT_TRUE(_diff.added_hosts().empty());
  EXPECT_TRUE(_diff.modified_hosts().empty());
  EXPECT_TRUE(_diff.removed_hosts().empty());
}

/**
 * Scenario: a host modified by the first difference is removed by the next one.
 * Then the host is removed, and no modification is kept.
 */
TEST_F(IndexedDiffStateSamePoller, ModifiedThenRemovedIsRemoved) {
  configuration::DiffState d1;
  d1.mutable_hosts()->mutable_modified()->AddAllocated(host(1, "h1"));
  first(d1);
  configuration::DiffState d2;
  d2.mutable_hosts()->add_removed(1);
  next(d2);

  EXPECT_TRUE(_diff.added_hosts().empty());
  EXPECT_TRUE(_diff.modified_hosts().empty());
  EXPECT_EQ(_diff.removed_hosts().size(), 1u);
}

/**
 * Scenario: a host modified twice.
 * Then it is modified once, with its latest definition.
 */
TEST_F(IndexedDiffStateSamePoller, ModifiedTwiceKeepsTheLatest) {
  configuration::DiffState d1;
  d1.mutable_hosts()->mutable_modified()->AddAllocated(host(1, "h1"));
  first(d1);
  configuration::DiffState d2;
  d2.mutable_hosts()->mutable_modified()->AddAllocated(host(1, "h1-renamed"));
  next(d2);

  ASSERT_EQ(_diff.modified_hosts().size(), 1u);
  EXPECT_EQ(_diff.modified_hosts().at(1)->host_name(), "h1-renamed");
}

/**
 * Scenario: a host removed by the first difference is added again by the next
 * one.
 * Then the host is modified: whoever applied neither still has it.
 */
TEST_F(IndexedDiffStateSamePoller, RemovedThenAddedIsModified) {
  configuration::DiffState d1;
  d1.mutable_hosts()->add_removed(1);
  first(d1);
  configuration::DiffState d2;
  d2.mutable_hosts()->mutable_added()->AddAllocated(host(1, "h1-back"));
  next(d2);

  EXPECT_TRUE(_diff.added_hosts().empty());
  ASSERT_EQ(_diff.modified_hosts().size(), 1u);
  EXPECT_EQ(_diff.modified_hosts().at(1)->host_name(), "h1-back");
  EXPECT_TRUE(_diff.removed_hosts().empty());
}
