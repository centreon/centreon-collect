/**
 * Copyright 2014, 2021 Centreon
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

#include "com/centreon/broker/bam/kpi_service.hh"
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <regex>
#include "bbdo/bam/state.hh"
#include "com/centreon/broker/bam/ba_best.hh"
#include "com/centreon/broker/bam/ba_impact.hh"
#include "com/centreon/broker/bam/ba_ratio_number.hh"
#include "com/centreon/broker/bam/ba_ratio_percent.hh"
#include "com/centreon/broker/bam/ba_worst.hh"
#include "com/centreon/broker/bam/configuration/applier/state.hh"
#include "com/centreon/broker/bam/kpi_ba.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/service_status.hh"
#include "common/log_v2/log_v2.hh"
#include "test-visitor.hh"

using namespace com::centreon::broker;

class BamBA : public ::testing::Test {
 protected:
  std::unique_ptr<bam::configuration::applier::state> _aply_state;
  std::unique_ptr<bam::configuration::state> _state;
  std::unique_ptr<test_visitor> _visitor;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    // Initialization.
    _logger = log_v2::instance().get(log_v2::BAM);
    config::applier::init(com::centreon::common::BROKER, 0, "test_broker", 0);

    _aply_state = std::make_unique<bam::configuration::applier::state>(_logger);
    _state = std::make_unique<bam::configuration::state>(_logger);
    _visitor = std::make_unique<test_visitor>("test-visitor");
  }

  void TearDown() override {
    // Cleanup.
    config::applier::deinit();
  }
};

/**
 * Check that KPI change at BA recompute does not mess with the BA
 * value.
 */
TEST_F(BamBA, KpiServiceRecompute) {
  // Build BAM objects.
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_impact>(1, 1, 1, true, _logger)};
  test_ba->set_level_critical(0);
  test_ba->set_level_warning(25);

  std::shared_ptr<bam::kpi_service> kpi{
      std::make_shared<bam::kpi_service>(1, 1, 1, 1, "host_1/serv_1", _logger)};

  kpi->set_impact_critical(100.0);
  kpi->set_state_hard(bam::state_ok);
  kpi->set_state_soft(kpi->get_state_hard());
  test_ba->add_impact(kpi);
  kpi->add_parent(test_ba);

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));
  for (int i = 0; i < 100 + 2; ++i) {
    auto ss{std::make_shared<neb::service_status>()};
    ss->host_id = 1;
    ss->service_id = 1;
    ss->last_check = now + i;
    ss->last_hard_state = ((i & 1) ? 0 : 2);
    ss->current_state = ss->last_hard_state;
    kpi->service_update(ss, _visitor.get());
    if (i == 0) {
      /* Here is an occasion to checkout output from ba when it is critical */
      ASSERT_EQ(test_ba->get_output(),
                "Status is CRITICAL - Level = 0 - 1 KPI out of 1 impacts the "
                "BA for 100 points - KPI host_1/serv_1 (impact: 100)");
      ASSERT_EQ(test_ba->get_perfdata(), "BA_Level=0;25;0;0;100");
    }
  }

  ASSERT_EQ(test_ba->get_state_hard(), 0);
}

/**
 *  Check that a KPI change at BA recompute does not mess with the BA
 *  value.
 *
 *                 ----------------
 *         ________| BA(C40%:W70%)|___________
 *        /        ----------------           \
 *       |                  |                 |
 *  KPI1(C20%:W10%)   KPI2(C20%:W10%)  KPI3(C20%:W10%)
 *       |                  |                 |
 *      H1S1               H2S1             H3S1
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST_F(BamBA, KpiServiceImpactState) {
  // Build BAM objects.
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_impact>(1, 1, 2, true, _logger)};

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::vector<short> results{0, 0, 1, 1, 1, 2};

  for (int i = 0; i < 3; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("host_{}/serv_1", i + 1), _logger);
    s->set_impact_warning(10);
    s->set_impact_critical(20);
    s->set_state_hard(bam::state_ok);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  test_ba->set_level_warning(70.0);
  test_ba->set_level_critical(40.0);

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  std::shared_ptr<neb::service_status> ss{
      std::make_shared<neb::service_status>()};
  ss->service_id = 1;

  auto it = results.begin();
  for (int i = 0; i < 2; i++) {
    for (size_t j = 0; j < kpis.size(); j++) {
      ss->last_check = now + 1 + i;
      ss->host_id = j + 1;
      ss->last_hard_state = i + 1;
      ss->current_state = ss->last_hard_state;
      kpis[j]->service_update(ss, _visitor.get());

      if (i == 0) {
        if (j == 0) {
          /* Here is an occasion to test get_output for a status OK but not
           * totally */
          ASSERT_EQ(test_ba->get_output(),
                    "Status is OK - Level = 90 (warn: 70 - crit: 40) - 1 KPI "
                    "out of 3 impacts the BA: KPI host_1/serv_1 (impact: 10)");
          ASSERT_EQ(test_ba->get_perfdata(), "BA_Level=90;70;40;0;100");
        } else if (j == 2) {
          /* Here is an occasion to test get_output for a status WARNING */
          std::regex re(
              "Status is WARNING - Level = 70 - 3 KPIs out of 3 impact "
              "the BA for 30 points - KPI.+ \\(impact: 10\\), KPI.+ \\(impact: "
              "10\\), KPI.+ \\(impact: 10\\)");
          ASSERT_TRUE(std::regex_search(test_ba->get_output(), re));
          ASSERT_EQ(test_ba->get_perfdata(), "BA_Level=70;70;40;0;100");
        }
      }

      short val = *it;
      ASSERT_EQ(test_ba->get_state_soft(), val);
      ASSERT_EQ(test_ba->get_state_hard(), val);
      ++it;
    }
  }
  auto events = _visitor->queue();
  ASSERT_EQ(events.size(), 14u);

  _visitor->print_events();

  {
    auto it = events.begin();
    while (it->typ != test_visitor::test_event::kpi)
      ++it;
    /* The three kpi have a status 1 and downtime set to false */
    for (int i = 0; i < 3; i++) {
      ASSERT_EQ(it->end_time, -1);
      ASSERT_EQ(it->status, 1);
      ASSERT_FALSE(it->in_downtime);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);
    }

    /* We close the event with status 1 and we create a new one with status 2 */
    for (int i = 0; i < 3; i++) {
      ASSERT_EQ(it->start_time, now + 1);
      ASSERT_EQ(it->end_time, now + 2);
      ASSERT_EQ(it->status, 1);
      ASSERT_FALSE(it->in_downtime);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);

      ASSERT_EQ(it->start_time, now + 2);
      ASSERT_EQ(it->end_time, -1);
      ASSERT_EQ(it->status, 2);
      ASSERT_FALSE(it->in_downtime);
      while (++it != events.end()) {
        if (it->typ == test_visitor::test_event::kpi)
          break;
      }
    }
  }
}

/**
 *  Check that a KPI change at BA recompute does not mess with the BA
 *  value.
 *
 *                 ----------------
 *         ________| BA(BEST)     |___________
 *        /        ----------------           \
 *       |                  |                 |
 *  KPI1(C20%:W10%)   KPI2(C20%:W10%)  KPI3(C20%:W10%)
 *       |                  |                 |
 *      H1S1               H2S1             H3S1
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST_F(BamBA, KpiServiceBestState) {
  // Build BAM objects.
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_best>(1, 1, 3, true, _logger)};

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::vector<short> results{0, 0, 1, 1, 1, 2};

  for (size_t i = 0; i < 3; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_ok);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  std::shared_ptr<neb::service_status> ss(new neb::service_status);
  ss->service_id = 1;

  auto it = results.begin();
  for (int i = 0; i < 2; i++) {
    for (size_t j = 0; j < kpis.size(); j++) {
      ss->last_check = now + 1;
      ss->host_id = j + 1;
      ss->last_hard_state = i + 1;
      ss->current_state = ss->last_hard_state;
      kpis[j]->service_update(ss, _visitor.get());

      short val = *it;
      ASSERT_EQ(test_ba->get_state_soft(), val);
      ASSERT_EQ(test_ba->get_state_hard(), val);
      ASSERT_EQ(test_ba->get_perfdata(), "");
      ++it;
    }
  }
}

/**
 *  Check that a KPI change at BA recompute does not mess with the BA
 *  value.
 *
 *                 ----------------
 *         ________| BA(WORST)     |___________
 *        /        ----------------           \
 *       |                  |                 |
 *  KPI1(C20%:W10%)   KPI2(C20%:W10%)  KPI3(C20%:W10%)
 *       |                  |                 |
 *      H1S1               H2S1             H3S1
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST_F(BamBA, KpiServiceWorstState) {
  // Build BAM objects.
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_worst>(1, 1, 4, true, _logger)};

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::vector<short> results{1, 1, 1, 2, 2, 2};

  for (int i = 0; i < 3; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_impact_warning(10);
    s->set_impact_critical(20);
    s->set_state_hard(bam::state_ok);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  std::shared_ptr<neb::service_status> ss(new neb::service_status);
  ss->service_id = 1;

  auto it = results.begin();
  for (int i = 0; i < 2; i++) {
    for (size_t j = 0; j < kpis.size(); j++) {
      ss->last_check = now + 1 + i;
      ss->host_id = j + 1;
      ss->last_hard_state = i + 1;
      ss->current_state = ss->last_hard_state;
      kpis[j]->service_update(ss, _visitor.get());

      short val = *it;
      ASSERT_EQ(test_ba->get_state_soft(), val);
      ASSERT_EQ(test_ba->get_state_hard(), val);
      ++it;
    }
  }

  auto events = _visitor->queue();
  ASSERT_EQ(events.size(), 12u);

  _visitor->print_events();

  {
    auto it = events.begin();
    /* The three kpi have a status 1 and downtime set to false */
    for (int i = 0; i < 3; i++) {
      ASSERT_EQ(it->end_time, -1);
      ASSERT_EQ(it->status, 1);
      ASSERT_FALSE(it->in_downtime);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);
    }

    /* We close the event with status 1 and we create a new one with status 2 */
    for (int i = 0; i < 3; i++) {
      ASSERT_EQ(it->start_time, now + 1);
      ASSERT_EQ(it->end_time, now + 2);
      ASSERT_EQ(it->status, 1);
      ASSERT_FALSE(it->in_downtime);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);

      ASSERT_EQ(it->start_time, now + 2);
      ASSERT_EQ(it->end_time, -1);
      ASSERT_EQ(it->status, 2);
      ASSERT_FALSE(it->in_downtime);
      while (++it != events.end()) {
        if (it->typ == test_visitor::test_event::kpi)
          break;
      }
    }
  }
}

/**
 *  Check that a KPI change at BA recompute does not mess with the BA
 *  value.
 *
 *                 ----------------
 *         ________| BA(RAtioNUm) |____________________________
 *        /        ----------------           \                \
 *       |                  |                 |                \
 *        ---------------2 C -> W , 4 C -> C -------------------
 *       |                  |                 |                \
 *      H1S1               H2S1             H3S1               H4S1
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST_F(BamBA, KpiServiceRatioNum) {
  // Build BAM objects.
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_number>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(4);
  test_ba->set_level_warning(2);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results{{2, 1, 1, 0}};

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_ok);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(std::move(s));
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    ss->current_state = ss->last_hard_state;
    kpis[j]->service_update(ss, _visitor.get());

    short val = results.top();
    std::cout << "val = " << val << std::endl;
    ASSERT_EQ(test_ba->get_state_soft(), val);
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

/**
 *  Check that a KPI change at BA recompute does not mess with the BA
 *  value.
 *
 *                 ----------------
 *         ________| BA(RAtio%  ) |____________________________
 *        /        ----------------           \                \
 *       |                  |                 |                \
 *        ---------------75% C -> W , 100% C -> C -------------------
 *       |                  |                 |                \
 *      H1S1               H2S1             H3S1               H4S1
 *
 *  @return EXIT_SUCCESS on success.
 */
TEST_F(BamBA, KpiServiceRatioPercent) {
  // Build BAM objects.
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({2, 1, 0, 0});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_ok);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  std::shared_ptr<neb::service_status> ss(new neb::service_status);
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    ss->current_state = ss->last_hard_state;
    kpis[j]->service_update(ss, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_soft(), val);
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtInheritAllCritical) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<bool> results({true, false, false, false});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->was_started = true;
    dt->actual_start_time = now + 2;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->in_downtime(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtInheritAllCriticalPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<bool> results({true, false, false, false});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& downtime = dt->mut_obj();

  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    downtime.set_host_id(ss->host_id);
    downtime.set_service_id(1);
    downtime.set_started(true);
    downtime.set_actual_start_time(now + 2);
    downtime.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->in_downtime(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtInheritOneOK) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(90);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<bool> results({false, false, false, false});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    if (i == 0)
      s->set_state_hard(bam::state_ok);
    else
      s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  time_t now = time(nullptr);

  auto ss = std::make_shared<neb::service_status>();
  auto dt = std::make_shared<neb::downtime>();
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->service_id = 1;
    if (j == 0)
      ss->last_hard_state = 0;
    else
      ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->was_started = true;
    dt->actual_start_time = now + 2;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->in_downtime(), val);
    results.pop();
  }

  auto events = _visitor->queue();
  _visitor->print_events();
  ASSERT_EQ(events.size(), 13u);
}

TEST_F(BamBA, KpiServiceDtInheritOneOKPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(90);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<bool> results({false, false, false, false});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    if (i == 0)
      s->set_state_hard(bam::state_ok);
    else
      s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  time_t now = time(nullptr);

  auto ss = std::make_shared<neb::pb_service_status>();
  auto dt = std::make_shared<neb::pb_downtime>();
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();
  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_service_id(1);
    if (j == 0)
      ss_obj.set_last_hard_state(ServiceStatus_State_OK);
    else
      ss_obj.set_last_hard_state(ServiceStatus_State_CRITICAL);
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_started(true);
    dt_obj.set_actual_start_time(now + 2);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->in_downtime(), val);
    results.pop();
  }

  auto events = _visitor->queue();
  _visitor->print_events();
  ASSERT_EQ(events.size(), 13u);
}

TEST_F(BamBA, KpiServiceIgnoreDt) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<bool> results({false, false, false, false});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->actual_start_time = now + 2;
    dt->was_started = true;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->in_downtime(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceIgnoreDtPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<bool> results({false, false, false, false});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();

  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(ServiceStatus_State_CRITICAL);
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_actual_start_time(now + 2);
    dt_obj.set_started(true);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->in_downtime(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpi) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<bool> results({false, false, false, false});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->actual_start_time = now + 2;
    dt->was_started = true;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->in_downtime(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<bool> results({false, false, false, false});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();

  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(ServiceStatus_State_CRITICAL);
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_actual_start_time(now + 2);
    dt_obj.set_started(true);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->in_downtime(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiImpact) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_impact>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(50);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({0, 0, 1, 2});

  results.push(0);
  results.push(0);
  results.push(1);
  results.push(2);

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    if (i == 3)
      s->set_state_hard(bam::state_ok);
    else
      s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    s->set_impact_critical(25);
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    if (j == 3)
      ss->last_hard_state = 0;
    else
      ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->actual_start_time = now + 2;
    dt->was_started = true;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiImpactPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_impact>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(50);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({0, 0, 1, 2});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    if (i == 3)
      s->set_state_hard(bam::state_ok);
    else
      s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    s->set_impact_critical(25);
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();

  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    if (j == 3)
      ss_obj.set_last_hard_state(ServiceStatus_State_OK);
    else
      ss_obj.set_last_hard_state(ServiceStatus_State_CRITICAL);
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_actual_start_time(now + 2);
    dt_obj.set_started(true);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiBest) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_best>(1, 1, 4, true, _logger)};
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({0, 2, 1, 0});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    switch (i) {
      case 0:
      case 1:
        s->set_state_hard(bam::state_ok);
        break;
      case 2:
        s->set_state_hard(bam::state_warning);
        break;
      case 3:
        s->set_state_hard(bam::state_critical);
        break;
    }
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss(std::make_shared<neb::service_status>());
  auto dt(std::make_shared<neb::downtime>());
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = kpis[j]->get_state_hard();
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->actual_start_time = now + 2;
    dt->was_started = true;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiBestPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_best>(1, 1, 4, true, _logger)};
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({0, 2, 1, 0});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    switch (i) {
      case 0:
      case 1:
        s->set_state_hard(bam::state_ok);
        break;
      case 2:
        s->set_state_hard(bam::state_warning);
        break;
      case 3:
        s->set_state_hard(bam::state_critical);
        break;
    }
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();

  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(
        static_cast<ServiceStatus_State>(kpis[j]->get_state_hard()));
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_actual_start_time(now + 2);
    dt_obj.set_started(true);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiWorst) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_worst>(1, 1, 4, true, _logger)};
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({0, 0, 1, 2});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    switch (i) {
      case 0:
      case 1:
        s->set_state_hard(bam::state_critical);
        break;
      case 2:
        s->set_state_hard(bam::state_warning);
        break;
      case 3:
        s->set_state_hard(bam::state_ok);
        break;
    }
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = kpis[j]->get_state_hard();
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->was_started = true;
    dt->actual_start_time = now + 2;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiWorstPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_worst>(1, 1, 4, true, _logger)};
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({0, 0, 1, 2});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    switch (i) {
      case 0:
      case 1:
        s->set_state_hard(bam::state_critical);
        break;
      case 2:
        s->set_state_hard(bam::state_warning);
        break;
      case 3:
        s->set_state_hard(bam::state_ok);
        break;
    }
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();
  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(
        static_cast<ServiceStatus_State>(kpis[j]->get_state_hard()));
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_started(true);
    dt_obj.set_actual_start_time(now + 2);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiRatio) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_number>(1, 1, 4, true, _logger)};
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);
  test_ba->set_level_warning(1);
  test_ba->set_level_critical(2);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({0, 1, 2, 2});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = kpis[j]->get_state_hard();
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->actual_start_time = now + 2;
    dt->was_started = true;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDtIgnoreKpiRatioPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_number>(1, 1, 4, true, _logger)};
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_ignore_kpi);
  test_ba->set_level_warning(1);
  test_ba->set_level_critical(2);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::stack<short> results({0, 1, 2, 2});

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();

  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(
        static_cast<ServiceStatus_State>(kpis[j]->get_state_hard()));
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_actual_start_time(now + 2);
    dt_obj.set_started(true);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = results.top();
    ASSERT_EQ(test_ba->get_state_hard(), val);
    results.pop();
  }
}

TEST_F(BamBA, KpiServiceDt) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::vector<bool> results{false, false, false, true};

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};
  ss->service_id = 1;

  auto it = results.begin();
  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->was_started = true;
    dt->actual_start_time = now + 1;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = *it;
    ASSERT_EQ(test_ba->in_downtime(), val);
    ++it;
  }

  for (int i = 0; i < 3; i++) {
    dt->host_id = 1;
    dt->service_id = 1;
    dt->actual_start_time = now + 2 + 10 * i;
    dt->actual_end_time = 0;
    dt->was_started = true;
    std::cout << "service_update 1" << std::endl;
    kpis[0]->service_update(dt, _visitor.get());

    dt->deletion_time = now + 2 + 10 * i + 5;
    dt->actual_end_time = now + 2 + 10 * i + 5;
    dt->was_cancelled = true;
    std::cout << "service_update 2" << std::endl;
    kpis[0]->service_update(dt, _visitor.get());
  }
  auto events = _visitor->queue();

  _visitor->print_events();

  ASSERT_EQ(events.size(), 41u);
  {
    auto it = events.begin();
    /* For each kpi... */
    for (int i = 0; i < 4; i++) {
      /* the kpi is set to hard critical and not in downtime */
      ASSERT_EQ(it->start_time, now + 1);
      ASSERT_EQ(it->end_time, -1);
      ASSERT_EQ(it->in_downtime, false);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);

      /* the kpi is set in downtime */
      /* The previous event is closed */
      ASSERT_EQ(it->start_time, now + 1);
      ASSERT_EQ(it->end_time, now + 1);
      ASSERT_EQ(it->in_downtime, false);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);

      /* The new event on downtime is added and open */
      ASSERT_EQ(it->start_time, now + 1);
      ASSERT_EQ(it->end_time, -1);
      ASSERT_EQ(it->in_downtime, true);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);
    }

    ////////////////////////////////////////////////////////////////

    /* We set the downtime to true for the kpi1, but it is already in downtime,
     * the event is skipped. Then we remove the downtime.
     *   1. closure of the event concerning the downtime open
     *   2. new event for downtime.
     *   3. closure of the downtime event.
     *   4. new event with downtime set to false.
     */

    ASSERT_EQ(it->start_time, now + 1);
    ASSERT_EQ(it->end_time, now + 7);
    ASSERT_TRUE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    ASSERT_EQ(it->start_time, now + 7);
    ASSERT_EQ(it->end_time, -1);
    ASSERT_FALSE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    /* New downtime:
     *   1. closure of the event concerning the downtime off.
     *   2. new event with downtime on.
     */
    ASSERT_EQ(it->start_time, now + 7);
    ASSERT_EQ(it->end_time, now + 12);
    ASSERT_FALSE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    ASSERT_EQ(it->start_time, now + 12);
    ASSERT_EQ(it->end_time, -1);
    ASSERT_TRUE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    /* New downtime:
     *   1. closure of the event concerning the downtime on.
     *   2. new event with downtime on.
     */
    ASSERT_EQ(it->start_time, now + 12);
    ASSERT_EQ(it->end_time, now + 17);
    ASSERT_TRUE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    ASSERT_EQ(it->start_time, now + 17);
    ASSERT_EQ(it->end_time, -1);
    ASSERT_FALSE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);
  }
  ASSERT_FALSE(test_ba->in_downtime());
}

TEST_F(BamBA, KpiServiceDtPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::vector<bool> results{false, false, false, true};

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();

  ss_obj.set_service_id(1);

  auto it = results.begin();
  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(ServiceStatus_State_CRITICAL);
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_started(true);
    dt_obj.set_actual_start_time(now + 1);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = *it;
    ASSERT_EQ(test_ba->in_downtime(), val);
    ++it;
  }

  for (int i = 0; i < 3; i++) {
    dt_obj.set_host_id(1);
    dt_obj.set_service_id(1);
    dt_obj.set_actual_start_time(now + 2 + 10 * i);
    dt_obj.set_actual_end_time(0);
    dt_obj.set_started(true);
    std::cout << "service_update 1" << std::endl;
    kpis[0]->service_update(dt, _visitor.get());

    dt_obj.set_deletion_time(now + 2 + 10 * i + 5);
    dt_obj.set_actual_end_time(now + 2 + 10 * i + 5);
    dt_obj.set_cancelled(true);
    std::cout << "service_update 2" << std::endl;
    kpis[0]->service_update(dt, _visitor.get());
  }
  auto events = _visitor->queue();

  _visitor->print_events();

  ASSERT_EQ(events.size(), 41u);
  {
    auto it = events.begin();
    /* For each kpi... */
    for (int i = 0; i < 4; i++) {
      /* the kpi is set to hard critical and not in downtime */
      ASSERT_EQ(it->start_time, now + 1);
      ASSERT_EQ(it->end_time, -1);
      ASSERT_EQ(it->in_downtime, false);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);

      /* the kpi is set in downtime */
      /* The previous event is closed */
      ASSERT_EQ(it->start_time, now + 1);
      ASSERT_EQ(it->end_time, now + 1);
      ASSERT_EQ(it->in_downtime, false);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);

      /* The new event on downtime is added and open */
      ASSERT_EQ(it->start_time, now + 1);
      ASSERT_EQ(it->end_time, -1);
      ASSERT_EQ(it->in_downtime, true);
      do {
        ++it;
      } while (it->typ != test_visitor::test_event::kpi);
    }

    ////////////////////////////////////////////////////////////////

    /* We set the downtime to true for the kpi1, but it is already in downtime,
     * the event is skipped. Then we remove the downtime.
     *   1. closure of the event concerning the downtime open
     *   2. new event for downtime.
     *   3. closure of the downtime event.
     *   4. new event with downtime set to false.
     */

    ASSERT_EQ(it->start_time, now + 1);
    ASSERT_EQ(it->end_time, now + 7);
    ASSERT_TRUE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    ASSERT_EQ(it->start_time, now + 7);
    ASSERT_EQ(it->end_time, -1);
    ASSERT_FALSE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    /* New downtime:
     *   1. closure of the event concerning the downtime off.
     *   2. new event with downtime on.
     */
    ASSERT_EQ(it->start_time, now + 7);
    ASSERT_EQ(it->end_time, now + 12);
    ASSERT_FALSE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    ASSERT_EQ(it->start_time, now + 12);
    ASSERT_EQ(it->end_time, -1);
    ASSERT_TRUE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    /* New downtime:
     *   1. closure of the event concerning the downtime on.
     *   2. new event with downtime on.
     */
    ASSERT_EQ(it->start_time, now + 12);
    ASSERT_EQ(it->end_time, now + 17);
    ASSERT_TRUE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);

    ASSERT_EQ(it->start_time, now + 17);
    ASSERT_EQ(it->end_time, -1);
    ASSERT_FALSE(it->in_downtime);
    do {
      ++it;
    } while (it->typ != test_visitor::test_event::kpi);
  }
  ASSERT_FALSE(test_ba->in_downtime());
}

TEST_F(BamBA, KpiServiceDtInherited_set) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::vector<bool> results{false, false, false, true};

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};
  ss->service_id = 1;

  auto it = results.begin();
  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->was_started = true;
    dt->actual_start_time = now + 1;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());

    short val = *it;
    ASSERT_EQ(test_ba->in_downtime(), val);
    ++it;
  }

  for (int i = 0; i < 3; i++) {
    dt->host_id = 1;
    dt->service_id = 1;
    dt->actual_start_time = now + 2 + 10 * i;
    dt->actual_end_time = 0;
    dt->was_started = true;
    kpis[0]->service_update(dt, _visitor.get());
  }
  ASSERT_TRUE(test_ba->in_downtime());
}

TEST_F(BamBA, KpiServiceDtInherited_setPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 1, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;
  std::vector<bool> results{false, false, false, true};

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();

  ss_obj.set_service_id(1);

  auto it = results.begin();
  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(ServiceStatus_State_CRITICAL);
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_started(true);
    dt_obj.set_actual_start_time(now + 1);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());

    short val = *it;
    ASSERT_EQ(test_ba->in_downtime(), val);
    ++it;
  }

  for (int i = 0; i < 3; i++) {
    dt_obj.set_host_id(1);
    dt_obj.set_service_id(1);
    dt_obj.set_actual_start_time(now + 2 + 10 * i);
    dt_obj.set_actual_end_time(0);
    dt_obj.set_started(true);
    kpis[0]->service_update(dt, _visitor.get());
  }
  ASSERT_TRUE(test_ba->in_downtime());
}

TEST_F(BamBA, KpiServiceDtInherited_unset) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 2, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto dt{std::make_shared<neb::downtime>()};

  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    dt->host_id = ss->host_id;
    dt->service_id = 1;
    dt->was_started = false;
    dt->actual_start_time = 0;
    dt->actual_end_time = 0;
    kpis[j]->service_update(dt, _visitor.get());
  }

  ASSERT_FALSE(test_ba->in_downtime());
}

TEST_F(BamBA, KpiServiceDtInherited_unsetPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 2, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto dt{std::make_shared<neb::pb_downtime>()};
  auto& ss_obj = ss->mut_obj();
  auto& dt_obj = dt->mut_obj();
  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(ServiceStatus_State_CRITICAL);
    kpis[j]->service_update(ss, _visitor.get());

    dt_obj.set_host_id(ss_obj.host_id());
    dt_obj.set_service_id(1);
    dt_obj.set_started(false);
    dt_obj.set_actual_start_time(0);
    dt_obj.set_actual_end_time(0);
    kpis[j]->service_update(dt, _visitor.get());
  }

  ASSERT_FALSE(test_ba->in_downtime());
}

TEST_F(BamBA, KpiServiceAcknowledgement) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 2, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(s);
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::service_status>()};
  auto ack{std::make_shared<neb::acknowledgement>()};
  ss->service_id = 1;

  for (size_t j = 0; j < kpis.size(); j++) {
    ss->last_check = now + 1;
    ss->host_id = j + 1;
    ss->last_hard_state = 2;
    kpis[j]->service_update(ss, _visitor.get());

    ack->poller_id = 1;
    ack->host_id = ss->host_id;
    ack->service_id = 1;
    ack->entry_time = now + 2;
    ack->deletion_time = -1;
    kpis[j]->service_update(ack, _visitor.get());
  }

  auto events = _visitor->queue();
  ASSERT_EQ(events.size(), 5u);

  _visitor->print_events();
}

TEST_F(BamBA, KpiServiceAcknowledgementPb) {
  std::shared_ptr<bam::ba> test_ba{
      std::make_shared<bam::ba_ratio_percent>(1, 2, 4, true, _logger)};
  test_ba->set_level_critical(100);
  test_ba->set_level_warning(75);
  test_ba->set_downtime_behaviour(bam::configuration::ba::dt_inherit);

  std::vector<std::shared_ptr<bam::kpi_service>> kpis;

  for (int i = 0; i < 4; i++) {
    auto s = std::make_shared<bam::kpi_service>(
        i + 1, 1, i + 1, 1, fmt::format("service {}", i), _logger);
    s->set_state_hard(bam::state_critical);
    s->set_state_soft(s->get_state_hard());
    test_ba->add_impact(s);
    s->add_parent(test_ba);
    kpis.push_back(std::move(s));
  }

  // Change KPI state as much time as needed to trigger a
  // recomputation. Note that the loop must terminate on a odd number
  // for the test to be correct.
  time_t now(time(nullptr));

  auto ss{std::make_shared<neb::pb_service_status>()};
  auto ack{std::make_shared<neb::pb_acknowledgement>()};
  auto& ss_obj = ss->mut_obj();

  ss_obj.set_service_id(1);

  for (size_t j = 0; j < kpis.size(); j++) {
    ss_obj.set_last_check(now + 1);
    ss_obj.set_host_id(j + 1);
    ss_obj.set_last_hard_state(ServiceStatus_State_CRITICAL);
    kpis[j]->service_update(ss, _visitor.get());

    ack->mut_obj().set_instance_id(1);
    ack->mut_obj().set_host_id(ss_obj.host_id());
    ack->mut_obj().set_service_id(1);
    ack->mut_obj().set_entry_time(now + 2);
    ack->mut_obj().set_deletion_time(-1);
    kpis[j]->service_update(ack, _visitor.get());
  }

  auto events = _visitor->queue();
  ASSERT_EQ(events.size(), 5u);

  _visitor->print_events();
}
