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
#include "com/centreon/broker/bam/configuration/kpi.hh"
#include "com/centreon/broker/bam/hst_svc_mapping.hh"
#include "com/centreon/broker/bam/service_book.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

/* A reload applies the configuration read from the DB by difference with the
 * previous one. What the DB also carries -- the state each KPI was last seen
 * in -- is not configuration: the applied objects know it better. It used to
 * be part of the comparison, and a reload then recreated every KPI whose state
 * had moved, seeding it again from the DB. */
class ApplierReload : public ::testing::Test {
 public:
  void SetUp() override {
    config::applier::init<com::centreon::broker::config::applier::broker_state>(
        "", 0, "test_broker", 0);
    _logger = log_v2::instance().get(log_v2::BAM);
    _aply_state = std::make_unique<bam::configuration::applier::state>(_logger);
    _state = std::make_unique<bam::configuration::state>(_logger);
  }

  void TearDown() override { config::applier::deinit(); }

  /* One BA of type worst, whose single KPI is the service (1, 5). */
  void build() {
    auto* mapping = _state->get_local_hst_svc_mapping();
    ASSERT_NE(mapping, nullptr) << "this test needs the local mapping";
    mapping->set_service("host_1", "service_5", 1, 5, true);

    bam::configuration::ba b(1, "test", "_Module_BAM_1",
                             bam::configuration::ba::state_source_worst);
    b.set_host_id(1);
    b.set_service_id(1001);
    _state->get_bas().insert({1, b});

    bam::configuration::kpi k;
    k.set_id(10);
    k.set_ba_id(1);
    k.set_host_id(1);
    k.set_service_id(5);
    k.set_impact_critical(100);
    k.set_status(0);  // OK, as the DB says at first
    _state->get_kpis().insert({10, k});
  }

  void service_goes(ServiceStatus_State state) {
    auto status = std::make_shared<neb::pb_service_status>();
    auto& o = status->mut_obj();
    o.set_host_id(1);
    o.set_service_id(5);
    o.set_last_check(time(nullptr));
    o.set_last_hard_state(state);
    o.set_state(state);
    o.set_state_type(ServiceStatus_StateType_HARD);
    _aply_state->book_service().update(status, nullptr);
  }

 protected:
  std::shared_ptr<spdlog::logger> _logger;
  std::unique_ptr<bam::configuration::applier::state> _aply_state;
  std::unique_ptr<bam::configuration::state> _state;
};

// Given a BA whose service KPI went CRITICAL after the configuration was
// applied When the same configuration is applied again, the DB now saying the
// KPI is CRITICAL with a new opened event Then the KPI is kept as it is, and
// the BA stays CRITICAL.
TEST_F(ApplierReload, UnchangedKpiIsKept) {
  build();
  ASSERT_NO_THROW(_aply_state->apply(*_state));
  auto ba = _aply_state->find_ba(1);
  ASSERT_TRUE(ba);
  EXPECT_EQ(ba->get_state_hard(), bam::state_ok);

  service_goes(ServiceStatus_State_CRITICAL);
  ASSERT_EQ(ba->get_state_hard(), bam::state_critical);

  /* What reader_v2 would now read for this KPI: its state moved, and
   * mod_bam_kpi carries a new last_state_change. Its configuration did not
   * move. */
  auto& k = _state->get_kpis().at(10);
  k.set_status(2);
  k.set_downtimed(true);
  KpiEvent e;
  e.set_kpi_id(10);
  e.set_ba_id(1);
  e.set_start_time(time(nullptr) + 60);
  e.set_end_time(-1);
  e.set_status(com::centreon::broker::State::CRITICAL);
  k.set_opened_event(e);

  ASSERT_NO_THROW(_aply_state->apply(*_state));
  /* Recreating the KPI would seed it from the DB (status 2, in downtime), then
   * the BA would be recomputed from that seed: still CRITICAL here, so also
   * check the other way round with a state the DB has wrong. */
  EXPECT_EQ(_aply_state->find_ba(1)->get_state_hard(), bam::state_critical);

  k.set_status(0);
  k.set_downtimed(false);
  ASSERT_NO_THROW(_aply_state->apply(*_state));
  /* The DB says OK; the applied KPI, fed by the service book, knows better. A
   * recreated KPI would have made the BA OK. */
  EXPECT_EQ(_aply_state->find_ba(1)->get_state_hard(), bam::state_critical);
}

// Given the same BA
// When the KPI's configuration really changes (its critical impact)
// Then it is recreated, and seeded from the DB again.
TEST_F(ApplierReload, ChangedKpiIsRecreated) {
  build();
  ASSERT_NO_THROW(_aply_state->apply(*_state));
  service_goes(ServiceStatus_State_CRITICAL);
  ASSERT_EQ(_aply_state->find_ba(1)->get_state_hard(), bam::state_critical);

  auto& k = _state->get_kpis().at(10);
  k.set_impact_critical(50);  // a configuration change
  k.set_status(0);            // and the DB seed says OK
  ASSERT_NO_THROW(_aply_state->apply(*_state));
  EXPECT_EQ(_aply_state->find_ba(1)->get_state_hard(), bam::state_ok);
}
