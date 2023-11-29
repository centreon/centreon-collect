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
#include "com/centreon/engine/configuration/applier/severity.hh"
#include "com/centreon/engine/severity.hh"
#include "common/configuration/severity_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierSeverity : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(LEGACY); }

  void TearDown() override { deinit_config_state(); }
};

// Given a severity applier
// And a severity configuration just with a name, an id and a type.
// Then the applier add_object adds the severity in the configuration set
// and in the severities map.
TEST_F(ApplierSeverity, AddSeverityFromConfig) {
  configuration::applier::severity aply;
  configuration::severity sv;
  sv.parse("severity_id", "1");
  sv.parse("severity_type", "service");
  sv.parse("severity_name", "severity");
  aply.add_object(sv);
  const set_severity& s = config->severities();
  ASSERT_EQ(s.size(), 1u);
  ASSERT_EQ(engine::severity::severities.size(), 1u);
  ASSERT_NO_THROW(aply.expand_objects(*config));
  ASSERT_NO_THROW(aply.resolve_object(sv));
}

// Given a severity applier
// And a severity configuration just with a name, an id and a type.
// Then the applier add_object adds the severity in the configuration set
// and in the severities map.
TEST_F(ApplierSeverity, PbAddSeverityFromConfig) {
  configuration::applier::severity aply;
  configuration::Severity sv;
  configuration::severity_helper sv_hlp(&sv);
  sv_hlp.hook("severity_id", "1");
  sv_hlp.hook("severity_type", "service");
  sv.set_severity_name("severity");
  aply.add_object(sv);
  const auto& s = pb_config.severities();
  ASSERT_EQ(s.size(), 1u);
  ASSERT_EQ(engine::severity::severities.size(), 1u);
  ASSERT_NO_THROW(aply.expand_objects(pb_config));
  ASSERT_NO_THROW(aply.resolve_object(sv));
}

// Given a severity applier
// And a severity configuration is added
// Then it is modified
// Then the real object is well modified.
TEST_F(ApplierSeverity, ModifySeverityFromConfig) {
  configuration::applier::severity aply;
  configuration::severity sv;
  sv.parse("severity_id", "1");
  sv.parse("severity_type", "service");
  sv.parse("severity_name", "severity");
  aply.add_object(sv);

  const set_severity& s = config->severities();
  ASSERT_EQ(s.size(), 1u);

  sv.parse("severity_name", "severity1");
  sv.parse("level", "12");
  aply.modify_object(sv);

  ASSERT_EQ(engine::severity::severities.size(), 1u);
  ASSERT_EQ(engine::severity::severities.begin()->second->name(),
            std::string_view("severity1"));
}

// Given a severity applier
// And a severity configuration is added
// Then it is modified
// Then the real object is well modified.
// Then it is modified by doing nothing
// Then it is not modified at all.
TEST_F(ApplierSeverity, PbModifySeverityFromConfig) {
  configuration::applier::severity aply;
  configuration::Severity sv;
  configuration::severity_helper sv_hlp(&sv);
  sv_hlp.hook("severity_id", "1");
  sv_hlp.hook("severity_type", "service");
  sv.set_severity_name("severity");
  aply.add_object(sv);

  const auto& s = pb_config.severities();
  ASSERT_EQ(s.size(), 1u);

  sv.set_severity_name("severity1");
  sv.set_level(12);
  sv.set_icon_id(14);

  aply.modify_object(&pb_config.mutable_severities()->at(0), sv);

  ASSERT_EQ(engine::severity::severities.size(), 1u);
  ASSERT_EQ(engine::severity::severities.begin()->second->level(), 12);
  ASSERT_EQ(engine::severity::severities.begin()->second->icon_id(), 14);
  ASSERT_EQ(engine::severity::severities.begin()->second->name(),
            std::string_view("severity1"));

  // No change here
  aply.modify_object(&pb_config.mutable_severities()->at(0), sv);
  ASSERT_EQ(engine::severity::severities.begin()->second->level(), 12);
  ASSERT_EQ(engine::severity::severities.begin()->second->icon_id(), 14);
  ASSERT_EQ(engine::severity::severities.begin()->second->name(),
            std::string_view("severity1"));
}

// Given a severity applier
// And a severity configuration just with a name, an id and a type.
// This configuration is added and then removed.
// There is no more severity.
TEST_F(ApplierSeverity, RemoveSeverityFromConfig) {
  configuration::applier::severity aply;
  configuration::severity sv;
  sv.parse("severity_id", "1");
  sv.parse("severity_type", "service");
  sv.parse("severity_name", "severity");
  aply.add_object(sv);
  const set_severity& s = config->severities();
  ASSERT_EQ(s.size(), 1u);

  aply.remove_object(sv);
  ASSERT_TRUE(s.empty());
}

// Given a severity applier
// And a severity configuration just with a name, an id and a type.
// This configuration is added and then removed.
// There is no more severity.
TEST_F(ApplierSeverity, PbRemoveSeverityFromConfig) {
  configuration::applier::severity aply;
  configuration::Severity sv;
  configuration::severity_helper sv_hlp(&sv);
  sv_hlp.hook("severity_id", "1");
  sv_hlp.hook("severity_type", "service");
  sv.set_severity_name("severity");
  aply.add_object(sv);
  const auto& s = pb_config.severities();
  ASSERT_EQ(s.size(), 1u);

  aply.remove_object(0);
  ASSERT_TRUE(s.empty());
}
