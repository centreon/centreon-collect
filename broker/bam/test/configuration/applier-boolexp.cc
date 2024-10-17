/**
 * Copyright 2018 Centreon
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 **
 ** For more information : contact@centreon.com
 */

#include <gtest/gtest.h>
#include "com/centreon/broker/bam/configuration/applier/state.hh"
#include "com/centreon/broker/bam/configuration/bool_expression.hh"
#include "com/centreon/broker/bam/configuration/kpi.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

class ApplierBoolexp : public ::testing::Test {
 public:
  void SetUp() override {
    // Initialization.
    config::applier::init(com::centreon::common::BROKER, 0, "test_broker", 0);
    auto logger = log_v2::instance().get(log_v2::BAM);

    _aply_state = std::make_unique<bam::configuration::applier::state>(logger);
    _state = std::make_unique<bam::configuration::state>(logger);
  }

  void TearDown() override {
    // Cleanup.
    config::applier::deinit();
  }

 protected:
  std::unique_ptr<bam::configuration::applier::state> _aply_state;
  std::unique_ptr<bam::configuration::state> _state;
};

// Given an empty boolexp with its kpi
// When the configuration is applied
// Then no exception is thrown (and no crash).
TEST_F(ApplierBoolexp, Simple) {
  bam::configuration::bool_expression boolexp;
  boolexp.set_id(23);
  bam::configuration::kpi kpi_boolexp;
  kpi_boolexp.set_id(17);
  kpi_boolexp.set_boolexp_id(23);
  _state->get_bool_exps().insert({boolexp.get_id(), boolexp});
  _state->get_kpis().insert({kpi_boolexp.get_id(), kpi_boolexp});

  ASSERT_NO_THROW(_aply_state->apply(*_state));
}
