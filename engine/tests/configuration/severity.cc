/*
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

#include "common/configuration/severity_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

extern configuration::state* config;

class ConfigSeverity : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// When I create a configuration::severity with a null id
// Then an exception is thrown.
TEST_F(ConfigSeverity, NewSeverityWithNoKey) {
  configuration::Severity sv;
  configuration::severity_helper sev_hlp(&sv);
  sev_hlp.hook("severity_id", "0");
  sev_hlp.hook("severity_type", "service");
  ASSERT_THROW(sev_hlp.check_validity(), std::exception);
}

// When I create a configuration::severity with a null level
// Then an exception is thrown.
TEST_F(ConfigSeverity, NewSeverityWithNoLevel) {
  configuration::Severity sv;
  configuration::severity_helper sv_hlp(&sv);
  sv_hlp.hook("severity_id", "1");
  sv_hlp.hook("severity_type", "service");
  ASSERT_THROW(sv_hlp.check_validity(), std::exception);
}

// When I create a configuration::severity with an empty name
// Then an exception is thrown.
TEST_F(ConfigSeverity, NewSeverityWithNoName) {
  configuration::Severity sv;
  configuration::severity_helper sv_hlp(&sv);
  sv_hlp.hook("severity_id", "1");
  sv_hlp.hook("severity_type", "service");
  sv.set_level(2);
  ASSERT_THROW(sv_hlp.check_validity(), std::exception);
}

// When I create a configuration::severity with a non empty name,
// non null id and non null level.
// Then no exception is thrown.
TEST_F(ConfigSeverity, NewSeverityWellFilled) {
  configuration::Severity sv;
  configuration::severity_helper sv_hlp(&sv);
  sv_hlp.hook("severity_id", "1");
  sv_hlp.hook("severity_type", "service");
  sv.set_level(2);
  sv.set_severity_name("foobar");
  ASSERT_EQ(sv.key().id(), 1);
  ASSERT_EQ(sv.level(), 2);
  ASSERT_EQ(sv.severity_name(), "foobar");
  ASSERT_EQ(sv.key().type(), configuration::severity::service);
  ASSERT_NO_THROW(sv_hlp.check_validity());
}

// When I create a configuration::severity with an icon id.
// Then we can get its value.
TEST_F(ConfigSeverity, NewSeverityIconId) {
  configuration::Severity sv;
  configuration::severity_helper sv_hlp(&sv);
  sv_hlp.hook("severity_id", "1");
  sv_hlp.hook("severity_type", "host");
  sv.set_level(2);
  sv.set_severity_name("foobar");
  ASSERT_EQ(sv.key().id(), 1);
  ASSERT_EQ(sv.level(), 2);
  ASSERT_EQ(sv.severity_name(), "foobar");
  sv.set_icon_id(18);
  ASSERT_NO_THROW(sv_hlp.check_validity());
}
