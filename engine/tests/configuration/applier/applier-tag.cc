/*
 * Copyright 2023 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/configuration/applier/tag.hh"
#include "com/centreon/engine/tag.hh"
#include "common/configuration/tag_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierTag : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(LEGACY); }

  void TearDown() override { deinit_config_state(); }
};

// Given a tag applier
// And a tag configuration just with a name, an id and a type.
// Then the applier add_object adds the tag in the configuration set
// and in the tags map.
TEST_F(ApplierTag, AddTagFromConfig) {
  configuration::applier::tag aply;
  configuration::tag tg;
  tg.parse("tag_id", "1");
  tg.parse("tag_type", "servicecategory");
  tg.parse("tag_name", "tag");
  aply.add_object(tg);
  const set_tag& s = config->tags();
  ASSERT_EQ(s.size(), 1u);
  ASSERT_EQ(engine::tag::tags.size(), 1u);
  ASSERT_NO_THROW(aply.expand_objects(*config));
  ASSERT_NO_THROW(aply.resolve_object(tg));
}

// Given a tag applier
// And a tag configuration just with a name, an id and a type.
// Then the applier add_object adds the tag in the configuration set
// and in the tags map.
TEST_F(ApplierTag, PbAddTagFromConfig) {
  configuration::applier::tag aply;
  configuration::Tag tg;
  configuration::tag_helper sv_hlp(&tg);
  sv_hlp.hook("tag_id", "1");
  sv_hlp.hook("tag_type", "servicecategory");
  tg.set_tag_name("tag");
  aply.add_object(tg);
  const auto& s = pb_config.tags();
  ASSERT_EQ(s.size(), 1u);
  ASSERT_EQ(engine::tag::tags.size(), 1u);
  ASSERT_NO_THROW(aply.expand_objects(pb_config));
  ASSERT_NO_THROW(aply.resolve_object(tg));
}

// Given a tag applier
// And a tag configuration is added
// Then it is modified
// Then the real object is well modified.
TEST_F(ApplierTag, ModifyTagFromConfig) {
  configuration::applier::tag aply;
  configuration::tag tg;
  tg.parse("tag_id", "1");
  tg.parse("tag_type", "service");
  tg.parse("tag_name", "tag");
  aply.add_object(tg);

  const set_tag& s = config->tags();
  ASSERT_EQ(s.size(), 1u);

  tg.parse("tag_name", "tag1");
  tg.parse("level", "12");
  aply.modify_object(tg);

  ASSERT_EQ(engine::tag::tags.size(), 1u);
  ASSERT_EQ(engine::tag::tags.begin()->second->name(),
            absl::string_view("tag1"));
}

// Given a tag applier
// And a tag configuration is added
// Then it is modified
// Then the real object is well modified.
// Then it is modified by doing nothing
// Then it is not modified at all.
TEST_F(ApplierTag, PbModifyTagFromConfig) {
  configuration::applier::tag aply;
  configuration::Tag tg;
  configuration::tag_helper sv_hlp(&tg);
  sv_hlp.hook("tag_id", "1");
  sv_hlp.hook("tag_type", "service");
  tg.set_tag_name("tag");
  aply.add_object(tg);

  const auto& s = pb_config.tags();
  ASSERT_EQ(s.size(), 1u);

  tg.set_tag_name("tag1");

  aply.modify_object(&pb_config.mutable_tags()->at(0), tg);

  ASSERT_EQ(engine::tag::tags.size(), 1u);
  ASSERT_EQ(engine::tag::tags.begin()->second->name(),
            absl::string_view("tag1"));

  // No change here
  aply.modify_object(&pb_config.mutable_tags()->at(0), tg);
  ASSERT_EQ(engine::tag::tags.begin()->second->name(),
            absl::string_view("tag1"));
}

// Given a tag applier
// And a tag configuration just with a name, an id and a type.
// This configuration is added and then removed.
// There is no more tag.
TEST_F(ApplierTag, RemoveTagFromConfig) {
  configuration::applier::tag aply;
  configuration::tag tg;
  tg.parse("tag_id", "1");
  tg.parse("tag_type", "service");
  tg.parse("tag_name", "tag");
  aply.add_object(tg);
  const set_tag& s = config->tags();
  ASSERT_EQ(s.size(), 1u);

  aply.remove_object(tg);
  ASSERT_TRUE(s.empty());
}

// Given a tag applier
// And a tag configuration just with a name, an id and a type.
// This configuration is added and then removed.
// There is no more tag.
TEST_F(ApplierTag, PbRemoveTagFromConfig) {
  configuration::applier::tag aply;
  configuration::Tag tg;
  configuration::tag_helper sv_hlp(&tg);
  sv_hlp.hook("tag_id", "1");
  sv_hlp.hook("tag_type", "service");
  tg.set_tag_name("tag");
  aply.add_object(tg);
  const auto& s = pb_config.tags();
  ASSERT_EQ(s.size(), 1u);

  aply.remove_object(0);
  ASSERT_TRUE(s.empty());
}
