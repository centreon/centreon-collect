/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <gtest/gtest.h>

#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/init.hh"
#include "com/centreon/broker/bam/ba.hh"
#include "com/centreon/broker/bam/configuration/applier/state.hh"
#include "com/centreon/broker/bam/configuration/ba.hh"
#include "com/centreon/broker/bam/configuration/bool_expression.hh"
#include "com/centreon/broker/bam/configuration/kpi.hh"
#include "com/centreon/broker/bam/hst_svc_mapping.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

/* A configuration where a BA is its own ancestor is a user error. It used to
 * throw out of apply(), which took the whole BAM configuration down: no BA
 * computed at all until the user fixed it. Now the KPIs of the cycle are left
 * out, the BAs of the cycle are applied invalid, and everything else works. */
class ApplierCircular : public ::testing::Test {
 public:
  void SetUp() override {
    config::applier::init<com::centreon::broker::config::applier::broker_state>(
        "", 0, "test_broker", 0);
    _logger = log_v2::instance().get(log_v2::BAM);
    _aply_state = std::make_unique<bam::configuration::applier::state>(_logger);
    _state = std::make_unique<bam::configuration::state>(_logger);
  }

  void TearDown() override { config::applier::deinit(); }

  /* A BA of type impact with its virtual service (host 1, service 1000+id). */
  void add_ba(uint32_t id, const std::string& name) {
    bam::configuration::ba b(id, name, "_Module_BAM_1");
    b.set_host_id(1);
    b.set_service_id(1000 + id);
    b.set_warning_level(50);
    b.set_critical_level(25);
    _state->get_bas().insert({id, b});
  }

  /* A KPI of BA `ba_id` whose indicator is the BA `indicator`. */
  void add_ba_kpi(uint32_t id, uint32_t ba_id, uint32_t indicator) {
    bam::configuration::kpi k;
    k.set_id(id);
    k.set_ba_id(ba_id);
    k.set_indicator_ba_id(indicator);
    k.set_impact_critical(100);
    _state->get_kpis().insert({id, k});
  }

 protected:
  std::shared_ptr<spdlog::logger> _logger;
  std::unique_ptr<bam::configuration::applier::state> _aply_state;
  std::unique_ptr<bam::configuration::state> _state;
};

// Given BA 1 with BA 2 as KPI and BA 2 with BA 1 as KPI, and a sane BA 3
// When the configuration is applied
// Then apply() does not throw
// And BA 1 and BA 2 are UNKNOWN, their output naming the circular definition
// And BA 3 is computed as usual.
TEST_F(ApplierCircular, BaOfBaLoop) {
  add_ba(1, "test");
  add_ba(2, "child1");
  add_ba(3, "sane");
  add_ba_kpi(10, 1, 2);
  add_ba_kpi(11, 2, 1);
  add_ba_kpi(12, 3, 1);

  ASSERT_NO_THROW(_aply_state->apply(*_state));

  auto ba1 = _aply_state->find_ba(1);
  auto ba2 = _aply_state->find_ba(2);
  auto ba3 = _aply_state->find_ba(3);
  ASSERT_TRUE(ba1 && ba2 && ba3);
  EXPECT_EQ(ba1->get_state_hard(), bam::state_unknown);
  EXPECT_EQ(ba2->get_state_hard(), bam::state_unknown);
  EXPECT_EQ(ba1->get_output(),
            "Circular definition detected. BA test includes itself as a KPI.");
  EXPECT_EQ(
      ba2->get_output(),
      "Circular definition detected. BA child1 includes itself as a KPI.");
  /* BA 3 follows BA 1, which is UNKNOWN: its KPI counts an unknown impact of
   * 0 by default, so BA 3 is OK -- what matters is that it is computed. */
  EXPECT_NE(ba3->get_state_hard(), bam::state_unknown);
  EXPECT_EQ(ba3->get_output().find("Circular"), std::string::npos);

  /* A reload with the same configuration must behave the same. */
  ASSERT_NO_THROW(_aply_state->apply(*_state));
  EXPECT_EQ(_aply_state->find_ba(1)->get_state_hard(), bam::state_unknown);
  EXPECT_NE(_aply_state->find_ba(3)->get_state_hard(), bam::state_unknown);
}

// Given the same loop, then a configuration where the closing KPI is gone
// When the second configuration is applied
// Then BA 1 and BA 2 are valid again.
TEST_F(ApplierCircular, LoopFixedOnReload) {
  add_ba(1, "test");
  add_ba(2, "child1");
  add_ba_kpi(10, 1, 2);
  add_ba_kpi(11, 2, 1);
  ASSERT_NO_THROW(_aply_state->apply(*_state));
  ASSERT_EQ(_aply_state->find_ba(1)->get_state_hard(), bam::state_unknown);

  _state->get_kpis().erase(11);
  ASSERT_NO_THROW(_aply_state->apply(*_state));
  EXPECT_NE(_aply_state->find_ba(1)->get_state_hard(), bam::state_unknown);
  EXPECT_NE(_aply_state->find_ba(2)->get_state_hard(), bam::state_unknown);
  EXPECT_EQ(_aply_state->find_ba(1)->get_output().find("Circular"),
            std::string::npos);
}

// Given BA 1 whose only KPI is a boolean rule reading BA 1's own virtual
// service
// When the configuration is applied
// Then apply() does not throw and BA 1 is UNKNOWN with the reason as output.
TEST_F(ApplierCircular, BoolexpLoop) {
  auto* mapping = _state->get_local_hst_svc_mapping();
  ASSERT_NE(mapping, nullptr) << "this test needs the local mapping";
  add_ba(1, "test");
  mapping->set_service("_Module_BAM_1", "ba_1", 1, 1001, true);

  bam::configuration::bool_expression rule;
  rule.set_id(5);
  rule.set_name("reads myself");
  rule.set_expression("{_Module_BAM_1 ba_1} {IS} {CRITICAL}");
  _state->get_bool_exps().insert({5, rule});

  bam::configuration::kpi k;
  k.set_id(20);
  k.set_ba_id(1);
  k.set_boolexp_id(5);
  k.set_impact_critical(100);
  _state->get_kpis().insert({20, k});

  ASSERT_NO_THROW(_aply_state->apply(*_state));
  auto ba1 = _aply_state->find_ba(1);
  ASSERT_TRUE(ba1);
  EXPECT_EQ(ba1->get_state_hard(), bam::state_unknown);
  EXPECT_EQ(ba1->get_output(),
            "Circular definition detected. BA test includes itself as a KPI.");
}
