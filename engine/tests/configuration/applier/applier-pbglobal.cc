/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

#include <fstream>
#include "com/centreon/engine/configuration/applier/hostescalation.hh"
#include "common/engine_conf/parser.hh"
#include "common/engine_conf/state_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class ApplierGlobal : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(ApplierGlobal, PbPollerName) {
  configuration::parser parser;
  configuration::State st;
  configuration::state_helper st_hlp(&st);

  ASSERT_EQ(st.poller_name(), "unknown");

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "poller_name=poller-test" << std::endl;
  ofs.close();
  configuration::error_cnt err;

  parser.parse("/tmp/test-config.cfg", &st, err);
  std::remove("/tmp/test-config.cfg");

  ASSERT_EQ(st.poller_name(), "poller-test");
}
