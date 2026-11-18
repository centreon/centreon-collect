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

#include <gtest/gtest.h>
#include "common/engine_conf/tag_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

class ConfigTag : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// When I create a configuration::tag with a null id
// Then an exception is thrown.
TEST_F(ConfigTag, NewTagWithNoKey) {
  configuration::error_cnt err;
  configuration::Tag tg;
  configuration::tag_helper tag_hlp(&tg);
  tg.mutable_key()->set_id(0);
  tg.mutable_key()->set_type(0);
  ASSERT_THROW(tag_hlp.check_validity(err), std::exception);
}

// When I create a configuration::tag with a null type
// Then an exception is thrown.
TEST_F(ConfigTag, NewTagWithNoLevel) {
  configuration::error_cnt err;
  configuration::Tag tg;
  configuration::tag_helper tg_hlp(&tg);
  tg.mutable_key()->set_id(1);
  tg.mutable_key()->set_type(0);
  ASSERT_THROW(tg_hlp.check_validity(err), std::exception);
}

// When I create a configuration::tag with an empty name
// Then an exception is thrown.
TEST_F(ConfigTag, NewTagWithNoName) {
  configuration::error_cnt err;
  configuration::Tag tg;
  configuration::tag_helper tg_hlp(&tg);
  tg.mutable_key()->set_id(1);
  tg_hlp.hook("type", "hostcategory");
  ASSERT_THROW(tg_hlp.check_validity(err), std::exception);
}

// When I create a configuration::tag with a non empty name,
// non null id and non null type
// Then no exception is thrown.
TEST_F(ConfigTag, NewTagWellFilled) {
  configuration::error_cnt err;
  configuration::Tag tg;
  configuration::tag_helper tg_hlp(&tg);
  tg.mutable_key()->set_id(1);
  tg.mutable_key()->set_type(0);
  tg_hlp.hook("type", "servicegroup");
  tg.set_tag_name("foobar");
  ASSERT_EQ(tg.key().id(), 1);
  ASSERT_EQ(tg.key().type(), tag_servicegroup);
  ASSERT_EQ(tg.tag_name(), "foobar");
  ASSERT_NO_THROW(tg_hlp.check_validity(err));
}

// When I create a configuration::tag with a non empty name,
// non null id and non null type.
// Then we can get the type value.
TEST_F(ConfigTag, NewTagIconId) {
  configuration::error_cnt err;
  configuration::Tag tg;
  configuration::tag_helper tg_hlp(&tg);
  tg.mutable_key()->set_id(1);
  tg.mutable_key()->set_type(0);
  tg_hlp.hook("type", "hostgroup");
  tg.set_tag_name("foobar");
  ASSERT_EQ(tg.key().type(), tag_hostgroup);
  ASSERT_NO_THROW(tg_hlp.check_validity(err));
}
