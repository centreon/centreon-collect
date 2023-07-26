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

#include "../../test_engine.hh"
#include "../../timeperiod/utils.hh"
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/configuration/applier/anomalydetection.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/host.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "common/configuration/anomalydetection_helper.hh"
#include "common/configuration/command_helper.hh"
#include "common/configuration/host_helper.hh"
#include "common/configuration/service_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierPbAnomalydetection : public TestEngine {
 public:
  void SetUp() override { init_config_state(PROTO); }

  void TearDown() override { deinit_config_state(); }
};

// Given an AD configuration with a host not defined
// Then the applier add_object throws an exception because it needs a service
// command.
TEST_F(ApplierPbAnomalydetection,
       PbNewAnomalydetectionWithHostNotDefinedFromConfig) {
  configuration::applier::anomalydetection ad_aply;
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper hlp(&ad);
  ad.set_host_name("test_host");
  ad.set_service_description("test description");
  ASSERT_THROW(ad_aply.add_object(ad), std::exception);
}

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(ApplierPbAnomalydetection, PbNewHostWithoutHostId) {
  configuration::applier::host hst_aply;
  configuration::applier::service ad_aply;
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper hlp(&ad);
  configuration::Host hst;
  configuration::host_helper hhlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  ASSERT_THROW(hst_aply.add_object(hst), std::exception);
}

// Given service configuration with a host defined
// Then the applier add_object creates the service
TEST_F(ApplierPbAnomalydetection, PbNewADFromConfig) {
  configuration::applier::host hst_aply;
  configuration::applier::anomalydetection ad_aply;
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper ad_hlp(&ad);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  // The host id is not given
  ASSERT_THROW(hst_aply.add_object(hst), std::exception);
  hst.set_host_id(12);
  ASSERT_NO_THROW(hst_aply.add_object(hst));

  configuration::applier::service svc_aply;
  configuration::Service svc;
  configuration::service_helper svc_hmlp(&svc);
  svc.set_host_name("test_host");
  svc.set_service_description("test_description");
  svc.set_host_id(12);
  svc.set_service_id(13);

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 'output| metric=12;50;75'");
  svc.set_check_command("cmd");

  configuration::applier::command cmd_aply;
  cmd_aply.add_object(cmd);
  ASSERT_NO_THROW(svc_aply.add_object(svc));

  ad.set_service_description("test description");
  ad.set_internal_id(112);
  ad.set_dependent_service_id(13);
  ad.set_service_id(4);
  ad.set_host_id(12);
  ad.set_host_name("test_host");
  ad.set_metric_name("foo");
  ad.set_thresholds_file("/etc/centreon-broker/thresholds.json");

  // No need here to call ad_aply.expand_objects(*config) because the
  // configuration service is not stored in configuration::state. We just have
  // to set the host_id manually.
  ad_aply.add_object(ad);
  service_id_map const& sm(engine::service::services_by_id);
  ASSERT_EQ(sm.size(), 2u);
  auto my_ad = sm.find({12u, 4u});
  ASSERT_EQ(my_ad->first.first, 12u);
  ASSERT_EQ(my_ad->first.second, 4u);

  // Service is not resolved, host is null now.
  ASSERT_TRUE(!my_ad->second->get_host_ptr());
  ASSERT_EQ(std::static_pointer_cast<com::centreon::engine::anomalydetection>(
                my_ad->second)
                ->get_internal_id(),
            112u);
  ASSERT_TRUE(my_ad->second->description() == "test description");
}

// Given service configuration without service_id
// Then the applier add_object throws an exception
TEST_F(ApplierPbAnomalydetection, PbNewADNoServiceId) {
  configuration::applier::host hst_aply;
  configuration::applier::anomalydetection ad_aply;
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper ad_hlp(&ad);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  // The host id is not given
  ASSERT_THROW(hst_aply.add_object(hst), std::exception);
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ad.set_service_description("test description");
  ad.set_host_id(1);
  ad.set_host_name("test_host");

  // No need here to call ad_aply.expand_objects(*config) because the
  // configuration service is not stored in configuration::state. We just have
  // to set the host_id manually.
  ASSERT_THROW(ad_aply.add_object(ad), std::exception);
}

// Given service configuration without host_id
// Then the applier add_object throws an exception
TEST_F(ApplierPbAnomalydetection, PbNewADNoHostId) {
  configuration::applier::host hst_aply;
  configuration::applier::anomalydetection ad_aply;
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper ad_hlp(&ad);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ad.set_service_description("test description");
  ad.set_service_id(4);
  ad.set_host_name("test_host");

  ASSERT_THROW(ad_aply.add_object(ad), std::exception);
}

// Given service configuration with bad host_id
// Then the applier add_object throws an exception
TEST_F(ApplierPbAnomalydetection, PbNewADBadHostId) {
  configuration::applier::host hst_aply;
  configuration::applier::anomalydetection ad_aply;
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper ad_hlp(&ad);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ad.set_service_description("test description");
  ad.set_host_id(2);
  ad.set_service_id(2);
  ad.set_dependent_service_id(3);
  ad.set_host_name("test_host");

  // No need here to call ad_aply.expand_objects(*config) because the
  // configuration service is not stored in configuration::state. We just have
  // to set the host_id manually.
  ASSERT_THROW(ad_aply.add_object(ad), std::exception);
}

// Given service configuration without metric_name
// Then the applier add_object throws an exception
TEST_F(ApplierPbAnomalydetection, PbNewADNoMetric) {
  configuration::applier::host hst_aply;
  configuration::applier::anomalydetection ad_aply;
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper ad_hlp(&ad);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ad.set_service_description("test description");
  ad.set_host_id(1);
  ad.set_service_id(4);
  ad.set_dependent_service_id(3);
  ad.set_host_name("test_host");

  // No need here to call ad_aply.expand_objects(*config) because the
  // configuration service is not stored in configuration::state. We just have
  // to set the host_id manually.
  ASSERT_THROW(ad_aply.add_object(ad), std::exception);
}

// Given service configuration without metric_name
// Then the applier add_object throws an exception
TEST_F(ApplierPbAnomalydetection, PbNewADNoThresholds) {
  configuration::applier::host hst_aply;
  configuration::applier::anomalydetection ad_aply;
  configuration::Anomalydetection ad;
  configuration::anomalydetection_helper ad_hlp(&ad);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(1);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ad.set_service_description("test description");
  ad.set_host_id(1);
  ad.set_service_id(4);
  ad.set_dependent_service_id(3);
  ad.set_host_name("test_host");
  ad.set_metric_name("bar");

  // No need here to call ad_aply.expand_objects(*config) because the
  // configuration service is not stored in configuration::state. We just have
  // to set the host_id manually.
  ASSERT_THROW(ad_aply.add_object(ad), std::exception);
}
