/*
** Copyright 2016 Centreon
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

#include "com/centreon/broker/bam/exp_builder.hh"
#include <gtest/gtest.h>
#include "com/centreon/broker/bam/bool_value.hh"
#include "com/centreon/broker/bam/exp_parser.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/modules.hh"
#include "bbdo/neb.pb.h"

using namespace com::centreon::broker;

extern std::shared_ptr<asio::io_context> g_io_context;

class BamExpBuilder : public ::testing::Test {
 public:
  void SetUp() override {
    g_io_context->restart();
    try {
      config::applier::init(0, "test_broker", 0);
    } catch (std::exception const& e) {
      (void)e;
    }
  }

  void TearDown() override {
    // The cache must be destroyed before the applier deinit() call.
    config::applier::deinit();
  }
};

TEST_F(BamExpBuilder, Valid1) {
  bam::exp_parser p("OK IS OK");
  bam::hst_svc_mapping mapping;
  bam::exp_builder builder(p.get_postfix(), mapping);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid2) {
  bam::exp_parser p("OK IS NOT OK");
  bam::hst_svc_mapping mapping;
  bam::exp_builder builder(p.get_postfix(), mapping);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 0);
  ASSERT_FALSE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid3) {
  bam::exp_parser p("OK AND CRITICAL");
  bam::hst_svc_mapping mapping;
  bam::exp_builder builder(p.get_postfix(), mapping);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 0);
  ASSERT_FALSE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid4) {
  bam::exp_parser p("OK OR CRITICAL");
  bam::hst_svc_mapping mapping;
  bam::exp_builder builder(p.get_postfix(), mapping);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid5) {
  bam::exp_parser p("OK XOR CRITICAL");
  bam::hst_svc_mapping mapping;
  bam::exp_builder builder(p.get_postfix(), mapping);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid6) {
  bam::exp_parser p("2 + 3 * 2 == 8");
  bam::hst_svc_mapping mapping;
  bam::exp_builder builder(p.get_postfix(), mapping);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid7) {
  bam::exp_parser p("2 - 3 * (2 - 6 / 3) == 2");
  bam::hst_svc_mapping mapping;
  bam::exp_builder builder(p.get_postfix(), mapping);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, Valid8) {
  bam::exp_parser p("2 % 3 == 20 % 6");
  bam::hst_svc_mapping mapping;
  bam::exp_builder builder(p.get_postfix(), mapping);
  ASSERT_EQ(builder.get_calls().size(), 0u);
  ASSERT_EQ(builder.get_services().size(), 0u);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_EQ(b->value_hard(), 1);
  ASSERT_TRUE(b->boolean_value());
  ASSERT_TRUE(b->state_known());
}

TEST_F(BamExpBuilder, UnknownService1) {
  config::applier::modules modules;
  modules.load_file("./lib/10-neb.so");
  bam::exp_parser p("{host_1 service_1} {IS} {OK}");
  bam::hst_svc_mapping mapping;
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, UnknownService2) {
  config::applier::modules modules;
  modules.load_file("./lib/10-neb.so");
  bam::exp_parser p("{host_1 service_1} {IS} {CRITICAL}");
  bam::hst_svc_mapping mapping;
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping);
  bam::bool_value::ptr b(builder.get_tree());
  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}

TEST_F(BamExpBuilder, OkService2) {
  config::applier::modules modules;
  modules.load_file("./lib/10-neb.so");
  bam::exp_parser p("{host_1 service_1} {IS} {CRITICAL}");
  bam::hst_svc_mapping mapping;
  mapping.set_service("host_1", "service_1", 1, 1, true);
  bam::exp_builder builder(p.get_postfix(), mapping);
  bam::bool_value::ptr b(builder.get_tree());
  auto svc1 = std::make_shared<neb::pb_service_status>();
  svc1->mut_obj().set_host_id(1);
  svc1->mut_obj().set_service_id(1);
  svc1->mut_obj().set_state(Service_State::Service_State_OK);
  svc1->mut_obj().set_last_hard_state(Service_State::OK);

  ASSERT_FALSE(b->state_known());
  ASSERT_FALSE(b->boolean_value());
}
