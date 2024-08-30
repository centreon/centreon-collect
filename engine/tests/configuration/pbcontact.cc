/*
 * Copyright 2017 - 2019 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/contact_helper.hh"

#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

class ConfigContact : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// When I create a configuration::Contact with an empty name
// Then an exception is thrown.
TEST_F(ConfigContact, NewPbContactWithNoName) {
  configuration::error_cnt err;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ASSERT_THROW(ctct_hlp.check_validity(err), std::exception);
}
