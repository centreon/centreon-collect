/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/configuration/tag.hh"
#include <gtest/gtest.h>

#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

extern configuration::state* config;

class ConfigTag : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(LEGACY); }

  void TearDown() override { deinit_config_state(); }
};

// When I create a configuration::tag with a null id
// Then an exception is thrown.
TEST_F(ConfigTag, NewTagWithNoKey) {
  configuration::tag tg({0, 0});
  ASSERT_THROW(tg.check_validity(), std::exception);
}

// When I create a configuration::tag with a null type
// Then an exception is thrown.
TEST_F(ConfigTag, NewTagWithNoLevel) {
  configuration::tag tg({1, 0});
  ASSERT_THROW(tg.check_validity(), std::exception);
}

// When I create a configuration::tag with an empty name
// Then an exception is thrown.
TEST_F(ConfigTag, NewTagWithNoName) {
  configuration::tag tg({1, 0});
  tg.parse("type", "hostcategory");
  ASSERT_THROW(tg.check_validity(), std::exception);
}

// When I create a configuration::tag with a non empty name,
// non null id and non null type
// Then no exception is thrown.
TEST_F(ConfigTag, NewTagWellFilled) {
  configuration::tag tg({1, 0});
  tg.parse("type", "servicegroup");
  tg.parse("tag_name", "foobar");
  ASSERT_EQ(tg.key().first, 1);
  ASSERT_EQ(tg.key().second, engine::configuration::tag::servicegroup);
  ASSERT_EQ(tg.tag_name(), "foobar");
  ASSERT_NO_THROW(tg.check_validity());
}

// When I create a configuration::tag with a non empty name,
// non null id and non null type.
// Then we can get the type value.
TEST_F(ConfigTag, NewTagIconId) {
  configuration::tag tg({1, 0});
  tg.parse("type", "hostgroup");
  tg.parse("tag_name", "foobar");
  ASSERT_EQ(tg.key().second, engine::configuration::tag::hostgroup);
  ASSERT_NO_THROW(tg.check_validity());
}
