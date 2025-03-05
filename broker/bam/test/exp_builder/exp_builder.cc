/**
 * Copyright 2016,2023 Centreon
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

#include "com/centreon/broker/bam/exp_builder.hh"
#include <gtest/gtest.h>
#include <memory>
#include "bbdo/neb.pb.h"
#include "com/centreon/broker/bam/ba_impact.hh"
#include "com/centreon/broker/bam/bool_expression.hh"
#include "com/centreon/broker/bam/bool_value.hh"
#include "com/centreon/broker/bam/exp_parser.hh"
#include "com/centreon/broker/bam/kpi_boolexp.hh"
#include "com/centreon/broker/bam/service_book.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/neb/service_status.hh"
#include "common/log_v2/log_v2.hh"
#include "test-visitor.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

class BamExpBuilder : public ::testing::Test {
 protected:
  std::unique_ptr<test_visitor> _visitor;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    _logger = log_v2::instance().get(log_v2::BAM);
    try {
      config::applier::init(com::centreon::common::BROKER, 0, "test_broker", 0);
      _logger->set_level(spdlog::level::debug);
      _logger->flush_on(spdlog::level::debug);
    } catch (std::exception const& e) {
      (void)e;
    }
    _visitor = std::make_unique<test_visitor>("test-visitor");
  }

  void TearDown() override {
    // The cache must be destroyed before the applier deinit() call.
    config::applier::deinit();
  }
};

TEST_F(BamExpBuilder, Valid1) {
  bam::exp_parser p("OK IS OK");
  bam::hst_svc_mapping mapping(_logger);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid2) {
  bam::exp_parser p("OK IS NOT OK");
  bam::hst_svc_mapping mapping(_logger);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 0);
  ASSERT_FALSE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid3) {
  bam::exp_parser p("OK AND CRITICAL");
  bam::hst_svc_mapping mapping(_logger);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 0);
  ASSERT_FALSE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid4) {
  bam::exp_parser p("OK OR CRITICAL");
  bam::hst_svc_mapping mapping(_logger);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid5) {
  bam::exp_parser p("OK XOR CRITICAL");
  bam::hst_svc_mapping mapping(_logger);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid6) {
  bam::exp_parser p("2 + 3 * 2 == 8");
  bam::hst_svc_mapping mapping(_logger);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid7) {
  bam::exp_parser p("2 - 3 * (2 - 6 / 3) == 2");
  bam::hst_svc_mapping mapping(_logger);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid8) {
  bam::exp_parser p("2 % 3 == 20 % 6");
  bam::hst_svc_mapping mapping(_logger);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, UnknownService1) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("{host_1 service_1} {IS} {OK}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, UnknownService2) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("{host_1 service_1} {IS} {CRITICAL}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, OkService2) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("{host_1 service_1} {IS} {CRITICAL}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  auto svc1 = std::make_shared<neb::service_status>();
  svc1->host_id = 1;
  svc1->service_id = 1;
  svc1->current_state = 0;    // OK
  svc1->last_hard_state = 0;  // OK

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, CritService2) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("{host_1 service_1} {IS} {CRITICAL}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());
}

TEST_F(BamExpBuilder, CritOkService1) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {OR} {host_1 service_2} {IS} {OK}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);

  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());
}

TEST_F(BamExpBuilder, CritOkService2) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {OR} {host_1 service_2} {IS} {OK}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc1);

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, CritOkService3) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {OR} {host_1 service_2} {IS} {OK}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);

  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  svc2->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc2);

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());
}

TEST_F(BamExpBuilder, CritAndOkService1) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {AND} {host_1 service_2} {IS} {OK}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);

  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());
}

TEST_F(BamExpBuilder, CritAndOkService2) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {AND} {host_1 service_2} {IS} {OK}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc1);

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, CritAndOkService3) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {AND} {host_1 service_2} {IS} {OK}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::WARNING);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::WARNING);

  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc2);

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());
}

TEST_F(BamExpBuilder, NotCritService3) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("({host_1 service_1} {NOT} {CRITICAL})");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::WARNING);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::WARNING);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, ExpressionWithService) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("({host_1 service_1} {NOT} {CRITICAL})");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::bool_expression exp(1, true, _logger);
  exp.set_expression(b);

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(exp.state_known());
  ASSERT_FALSE(exp.get_state());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::WARNING);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::WARNING);

  book.update(svc1);

  ASSERT_TRUE(exp.state_known());
  ASSERT_TRUE(exp.get_state());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(exp.state_known());
  ASSERT_FALSE(exp.get_state());
}

TEST_F(BamExpBuilder, ReverseExpressionWithService) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("({host_1 service_1} {NOT} {CRITICAL})");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::bool_expression exp(1, false, _logger);
  exp.set_expression(b);

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(exp.state_known());
  ASSERT_TRUE(exp.get_state());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::WARNING);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::WARNING);

  book.update(svc1);

  ASSERT_TRUE(exp.state_known());
  ASSERT_FALSE(exp.get_state());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(exp.state_known());
  ASSERT_TRUE(exp.get_state());
}

TEST_F(BamExpBuilder, KpiBoolexpWithService) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("({host_1 service_1} {NOT} {CRITICAL})");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  auto exp = std::make_shared<bam::bool_expression>(1, true, _logger);
  exp->set_expression(b);
  b->add_parent(exp);

  auto kpi =
      std::make_shared<bam::kpi_boolexp>(1, 1, "test_boool_exp", _logger);
  kpi->link_boolexp(exp);
  exp->add_parent(kpi);

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(exp->state_known());
  ASSERT_TRUE(kpi->ok_state());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::WARNING);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::WARNING);

  book.update(svc1);

  ASSERT_TRUE(exp->state_known());
  EXPECT_FALSE(kpi->ok_state());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_TRUE(exp->state_known());
  ASSERT_TRUE(kpi->ok_state());
}

TEST_F(BamExpBuilder, KpiBoolexpReversedImpactWithService) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("({host_1 service_1} {NOT} {CRITICAL})");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  auto exp = std::make_shared<bam::bool_expression>(1, false, _logger);
  exp->set_expression(b);
  b->add_parent(exp);

  auto kpi =
      std::make_shared<bam::kpi_boolexp>(1, 1, "test_boool_exp", _logger);
  kpi->link_boolexp(exp);
  exp->add_parent(kpi);

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(exp->state_known());
  ASSERT_TRUE(kpi->ok_state());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::WARNING);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::WARNING);

  book.update(svc1);

  ASSERT_TRUE(kpi->ok_state());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);

  book.update(svc1);

  ASSERT_FALSE(kpi->ok_state());
}

TEST_F(BamExpBuilder, BoolexpServiceXorService) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "({host_1 service_1} {IS} {CRITICAL}) {XOR} ({host_1 service_2} {IS} "
      "{CRITICAL})");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc1);

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  book.update(svc1);

  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, BoolexpLTWithServiceStatus) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p("{host_1 service_1} < {host_1 service_2}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc1);

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc1);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  book.update(svc1);

  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc2);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, BoolexpKpiService) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {OR} {host_1 service_2} {IS} "
      "{CRITICAL}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc1, nullptr);

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc2, nullptr);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc1, nullptr);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  book.update(svc1, nullptr);

  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc2, nullptr);

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc1, nullptr);

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

/* Same test as BoolexpKpiService but with the point of view of a boolean
 * expression. */
TEST_F(BamExpBuilder, BoolexpKpiServiceAndBoolExpression) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {OR} {host_1 service_2} {IS} "
      "{CRITICAL}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  auto exp = std::make_shared<bam::bool_expression>(1, true, _logger);
  exp->set_expression(b);
  b->add_parent(exp);

  auto kpi =
      std::make_shared<bam::kpi_boolexp>(1, 1, "test_boool_exp", _logger);
  kpi->set_impact(100);
  kpi->link_boolexp(exp);
  exp->add_parent(kpi);

  auto ba = std::make_shared<bam::ba_impact>(1, 30, 300, false, _logger);
  ba->set_name("ba-kpi-service");
  ba->set_level_warning(70);
  ba->set_level_warning(80);
  ba->add_impact(kpi);
  kpi->add_parent(ba);

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  ASSERT_EQ(exp->get_state(), 0);
  //  ASSERT_TRUE(kpi->ok_state());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc1, nullptr);

  ba->dump("/tmp/ba1");
  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  ASSERT_FALSE(exp->state_known());
  ASSERT_TRUE(kpi->ok_state());
  ASSERT_EQ(ba->get_state_hard(), 0);

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc2, nullptr);
  ba->dump("/tmp/ba2");

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  ASSERT_EQ(exp->get_state(), 2);
  ASSERT_FALSE(kpi->ok_state());
  ASSERT_EQ(ba->get_state_hard(), 2);

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc1, nullptr);
  ba->dump("/tmp/ba3");

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  book.update(svc1, nullptr);
  ba->dump("/tmp/ba4");

  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc2, nullptr);
  ba->dump("/tmp/ba5");

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  ASSERT_EQ(exp->get_state(), 2);
  ASSERT_FALSE(kpi->ok_state());
  ASSERT_EQ(ba->get_state_hard(), 2);

  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc1, nullptr);
  ba->dump("/tmp/ba6");

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  ASSERT_EQ(exp->get_state(), 0);
  ASSERT_TRUE(kpi->ok_state());
  ASSERT_EQ(ba->get_state_hard(), 0);
}

/* Same test as BoolexpKpiServiceAndBoolExpression but with AND operator and
 * with impact_if set to false. */
TEST_F(BamExpBuilder, BoolexpKpiServiceAndBoolExpressionAndOperator) {
  config::applier::modules modules(_logger);
  modules.load_file("./broker/neb/10-neb.so");
  bam::exp_parser p(
      "{host_1 service_1} {IS} {CRITICAL} {AND} {host_1 service_2} {IS} "
      "{CRITICAL}");
  bam::hst_svc_mapping mapping(_logger);
  mapping.set_service("host_1", "service_1", 1, 1, true);
  mapping.set_service("host_1", "service_2", 1, 2, true);
  bam::exp_builder builder(p.get_postfix(), mapping, _logger);
  bam::bool_value::ptr b(builder.get_tree());

  auto exp = std::make_shared<bam::bool_expression>(1, false, _logger);
  exp->set_expression(b);
  b->add_parent(exp);

  auto kpi =
      std::make_shared<bam::kpi_boolexp>(1, 1, "test_boool_exp", _logger);
  kpi->set_impact(100);
  kpi->link_boolexp(exp);
  exp->add_parent(kpi);

  auto ba = std::make_shared<bam::ba_impact>(1, 30, 300, false, _logger);
  ba->set_name("ba-kpi-service");
  ba->set_level_warning(70);
  ba->set_level_warning(80);
  ba->add_impact(kpi);
  kpi->add_parent(ba);

  bam::service_book book(_logger);
  for (auto& svc : builder.get_services())
    book.listen(svc->get_host_id(), svc->get_service_id(), svc.get());

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  ASSERT_EQ(exp->get_state(), 2);
  ASSERT_TRUE(kpi->ok_state());

  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc1, nullptr);

  ba->dump("/tmp/ba1");
  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  ASSERT_FALSE(exp->state_known());
  ASSERT_FALSE(kpi->ok_state());
  ASSERT_EQ(ba->get_state_hard(), 2);

  auto svc2 = std::make_shared<neb::pb_service_status>();
  svc2->mut_obj().set_host_id(1);
  svc2->mut_obj().set_service_id(2);
  svc2->mut_obj().set_state(ServiceStatus::OK);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc2, nullptr);
  ba->dump("/tmp/ba2");

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  ASSERT_EQ(exp->get_state(), 2);
  ASSERT_FALSE(kpi->ok_state());
  ASSERT_EQ(ba->get_state_hard(), 2);

  svc1->mut_obj().set_state(ServiceStatus::OK);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::OK);
  book.update(svc1, nullptr);
  ba->dump("/tmp/ba3");

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  book.update(svc1, nullptr);
  ba->dump("/tmp/ba4");

  svc2->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc2->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc2, nullptr);
  ba->dump("/tmp/ba5");

  ASSERT_TRUE(b->state_known());
  ASSERT_FALSE(b->boolean_value());

  ASSERT_EQ(exp->get_state(), 2);
  ASSERT_FALSE(kpi->ok_state());
  ASSERT_EQ(ba->get_state_hard(), 2);

  svc1->mut_obj().set_state(ServiceStatus::CRITICAL);
  svc1->mut_obj().set_last_hard_state(ServiceStatus::CRITICAL);
  book.update(svc1, nullptr);
  ba->dump("/tmp/ba6");

  ASSERT_TRUE(b->state_known());
  ASSERT_TRUE(b->boolean_value());

  ASSERT_EQ(exp->get_state(), 0);
  ASSERT_TRUE(kpi->ok_state());
  ASSERT_EQ(ba->get_state_hard(), 0);
}
