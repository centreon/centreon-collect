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

#include "com/centreon/engine/configuration/severity.hh"
#include <gtest/gtest.h>

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
  configuration::severity sv({0, 0});
  ASSERT_THROW(sv.check_validity(), std::exception);
}

// When I create a configuration::severity with a null level
// Then an exception is thrown.
TEST_F(ConfigSeverity, NewSeverityWithNoLevel) {
  configuration::severity sv({1, 0});
  ASSERT_THROW(sv.check_validity(), std::exception);
}

// When I create a configuration::severity with an empty name
// Then an exception is thrown.
TEST_F(ConfigSeverity, NewSeverityWithNoName) {
  configuration::severity sv({1, 0});
  sv.parse("level", "2");
  ASSERT_THROW(sv.check_validity(), std::exception);
}

// When I create a configuration::severity with a non empty name,
// non null id and non null level.
// Then no exception is thrown.
TEST_F(ConfigSeverity, NewSeverityWellFilled) {
  configuration::severity sv({1, 0});
  sv.parse("level", "2");
  sv.parse("severity_name", "foobar");
  sv.parse("type", "service");
  ASSERT_EQ(sv.key().first, 1);
  ASSERT_EQ(sv.level(), 2);
  ASSERT_EQ(sv.severity_name(), "foobar");
  ASSERT_EQ(sv.type(), configuration::severity::service);
  ASSERT_NO_THROW(sv.check_validity());
}

// When I create a configuration::severity with an icon id.
// Then we can get its value.
TEST_F(ConfigSeverity, NewSeverityIconId) {
  configuration::severity sv({1, 0});
  sv.parse("level", "2");
  sv.parse("icon_id", "18");
  sv.parse("severity_name", "foobar");
  sv.parse("type", "host");
  ASSERT_EQ(sv.icon_id(), 18);
  ASSERT_NO_THROW(sv.check_validity());
}
