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
#include <com/centreon/engine/host.hh>
#include <com/centreon/engine/hostescalation.hh>
#include <helper.hh>
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostescalation.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "common/engine_legacy_conf/state.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class ApplierHostEscalation : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

TEST_F(ApplierHostEscalation, AddEscalation) {
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::hostescalation he_apply;
  configuration::hostescalation he;
  ASSERT_TRUE(he.parse("host_name", "test_host"));
  ASSERT_TRUE(he.parse("first_notification", "4"));
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  ASSERT_TRUE(he.parse("first_notification", "8"));
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 2u);
}

TEST_F(ApplierHostEscalation, ResolveObject) {
  configuration::error_cnt err;
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::hostescalation he_apply;
  configuration::hostescalation he;
  ASSERT_TRUE(he.parse("host_name", "test_host"));
  ASSERT_TRUE(he.parse("first_notification", "4"));
  ASSERT_THROW(he_apply.resolve_object(he, err), std::exception);
  he_apply.add_object(he);
  ASSERT_NO_THROW(he_apply.resolve_object(he, err));
}

TEST_F(ApplierHostEscalation, RemoveEscalation) {
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::hostescalation he_apply;
  configuration::hostescalation he;
  ASSERT_TRUE(he.parse("host_name", "test_host"));
  ASSERT_TRUE(he.parse("first_notification", "4"));
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  ASSERT_TRUE(he.parse("first_notification", "8"));
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 2u);

  he_apply.remove_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  ASSERT_TRUE(he.parse("first_notification", "4"));
  he_apply.remove_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 0u);
}

TEST_F(ApplierHostEscalation, RemoveEscalationFromRemovedHost) {
  configuration::applier::host hst_aply;
  configuration::host hst;
  ASSERT_TRUE(hst.parse("host_name", "test_host"));
  ASSERT_TRUE(hst.parse("address", "127.0.0.1"));
  ASSERT_TRUE(hst.parse("_HOST_ID", "12"));
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::hostescalation he_apply;
  configuration::hostescalation he;
  ASSERT_TRUE(he.parse("host_name", "test_host"));
  ASSERT_TRUE(he.parse("first_notification", "4"));
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  ASSERT_TRUE(he.parse("first_notification", "8"));
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 2u);

  hst_aply.remove_object(hst);
  ASSERT_EQ(host::hosts.size(), 0u);

  he_apply.remove_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  ASSERT_TRUE(he.parse("first_notification", "4"));
  he_apply.remove_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 0u);
}
