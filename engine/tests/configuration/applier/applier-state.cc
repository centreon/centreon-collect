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
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "configuration/state.pb.h"

using namespace com::centreon::engine;

class ApplierState : public ::testing::Test {
 protected:
  configuration::State pb_config;

 public:
  void SetUp() override {
    auto tps = pb_config.mutable_timeperiods();
    for (int i = 0; i < 10; i++) {
      auto* tp = tps->Add();
      tp->set_alias(fmt::format("timeperiod {}", i));
      tp->set_name(fmt::format("Timeperiod {}", i));
    }
  }
};

TEST_F(ApplierState, DiffOnTimeperiod) {
  configuration::State new_config;
  auto tps = new_config.mutable_timeperiods();
  for (int i = 0; i < 10; i++) {
    auto* tp = tps->Add();
    tp->set_alias(fmt::format("timeperiod {}", i));
    tp->set_name(fmt::format("Timeperiod {}", i));
  }
  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  ASSERT_TRUE(dstate.dtimeperiods().to_remove().empty());
  ASSERT_TRUE(dstate.dtimeperiods().to_add().empty());
  ASSERT_TRUE(dstate.dtimeperiods().to_modify().empty());
}

TEST_F(ApplierState, DiffOnTimeperiodOneRemoved) {
  configuration::State new_config;
  auto tps = new_config.mutable_timeperiods();
  for (int i = 0; i < 9; i++) {
    auto* tp = tps->Add();
    tp->set_alias(fmt::format("timeperiod {}", i));
    tp->set_name(fmt::format("Timeperiod {}", i));
  }
  configuration::DiffState dstate =
      configuration::applier::state::instance().build_difference(pb_config,
                                                                 new_config);
  ASSERT_EQ(dstate.dtimeperiods().to_remove().size(), 1u);
  ASSERT_TRUE(dstate.dtimeperiods().to_add().empty());
  ASSERT_TRUE(dstate.dtimeperiods().to_modify().empty());
}
