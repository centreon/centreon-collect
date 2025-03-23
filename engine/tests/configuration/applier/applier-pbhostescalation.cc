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

#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostescalation.hh"
#include "com/centreon/engine/hostescalation.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class PbApplierHostEscalation : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

TEST_F(PbApplierHostEscalation, PbAddEscalation) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::hostescalation he_apply;
  configuration::Hostescalation he;
  configuration::hostescalation_helper he_hlp(&he);
  he_hlp.hook("host_name", "test_host");
  he.set_first_notification(4);
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  he.set_first_notification(8);
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 2u);
}

TEST_F(PbApplierHostEscalation, PbRemoveEscalation) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::hostescalation he_apply;
  configuration::Hostescalation he;
  configuration::hostescalation_helper he_hlp(&he);
  he_hlp.hook("host_name", "test_host");
  he.set_first_notification(4);
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  he.set_first_notification(8);
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 2u);

  he_apply.remove_object<size_t>({1, 1});
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  he_apply.remove_object<size_t>({0, 1});
  ASSERT_EQ(hostescalation::hostescalations.size(), 0u);
}

TEST_F(PbApplierHostEscalation, RemoveEscalationFromRemovedHost) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_aply.add_object(hst);
  ASSERT_EQ(host::hosts.size(), 1u);

  configuration::applier::hostescalation he_apply;
  configuration::Hostescalation he;
  configuration::hostescalation_helper he_hlp(&he);
  he_hlp.hook("host_name", "test_host");
  he.set_first_notification(4);
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  he.set_first_notification(8);
  he_apply.add_object(he);
  ASSERT_EQ(hostescalation::hostescalations.size(), 2u);

  hst_aply.remove_object<size_t>({0, 12});
  ASSERT_EQ(host::hosts.size(), 0u);

  he_apply.remove_object<size_t>({0, 12});
  ASSERT_EQ(hostescalation::hostescalations.size(), 1u);
  he_apply.remove_object<size_t>({0, 12});
  ASSERT_EQ(hostescalation::hostescalations.size(), 0u);
}
