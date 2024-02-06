/**
 * Copyright 2021 Centreon (https://www.centreon.com/)
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
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

extern int config_errors;
extern int config_warnings;

class ApplierSeverity : public ::testing::Test {
 public:
  void SetUp() override {
    config_errors = 0;
    config_warnings = 0;
    init_config_state();
  }

  void TearDown() override { deinit_config_state(); }

  configuration::severity valid_severity_config() const {
      // Add severity.
      {configuration::severity sv;
  sv.parse("id", "1");
  sv.parse("name", "foobar");
  configuration::applier::severity sv;
  aplyr.add_object(sv);
}
}
;

// Given a severity applier
// When we add a severity configuration
// Then it works.
// When we add a second severity configuration
// Then it works again.
// When we modify the first severity configuration
// Then it works.
TEST_F(ApplierSeverity, ModifyUnexistingContactConfigFromConfig) {
  configuration::applier::severity aply;
  configuration::severity sv(1);
  ASSERT_TRUE(sv.parse("name", "barfoo"));
  ASSERT_TRUE(sv.parse("level", "14"));
  ASSERT_NO_THROW(aply.add_object(sv));
  configuration::severity sv1(2);
  ASSERT_TRUE(sv1.parse("name", "barfoo1"));
  ASSERT_TRUE(sv1.parse("level", "13"));
  ASSERT_NO_THROW(aply.add_object(sv1));
  configuration::severity sv2(1);
  ASSERT_TRUE(sv2.parse("name", "theone"));
  ASSERT_TRUE(sv2.parse("level", "1"));
  ASSERT_NO_THROW(aply.modify_object(sv2));
}
