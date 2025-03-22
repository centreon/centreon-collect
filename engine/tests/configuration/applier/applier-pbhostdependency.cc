/**
 * Copyright 2019, 2023 Centreon (https://www.centreon.com/)
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

#include <cstring>

#include "../../test_engine.hh"
#include "../../timeperiod/utils.hh"
#include "com/centreon/clib.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostdependency.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/serviceescalation.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/hostdependency_helper.hh"
#include "common/engine_conf/service_helper.hh"
#include "common/engine_conf/state_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

extern configuration::state* config;

class HostDependency : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    configuration::error_cnt err;
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_indexed_config.state());
    ct_aply.resolve_object(ctct, err);

    configuration::applier::host hst_aply;

    configuration::Host hst1{new_pb_configuration_host("host1", "admin", 18)};
    hst_aply.add_object(hst1);
    hst_aply.resolve_object(hst1, err);

    configuration::Host hst2{new_pb_configuration_host("host2", "admin", 19)};
    hst_aply.add_object(hst2);
    hst_aply.resolve_object(hst2, err);

    configuration::Host hst3{new_pb_configuration_host("host3", "admin", 20)};
    hst_aply.add_object(hst3);
    hst_aply.resolve_object(hst3, err);
  }

  void TearDown() override { deinit_config_state(); }
};

TEST_F(HostDependency, PbCircularDependency2) {
  configuration::error_cnt err;
  configuration::applier::hostdependency hd_aply;
  configuration::Hostdependency hd1{
      new_pb_configuration_hostdependency("host1", "host2")};
  hd_aply.expand_objects(pb_indexed_config.state());
  hd_aply.add_object(hd1);
  hd_aply.resolve_object(hd1, err);

  configuration::Hostdependency hd2{
      new_pb_configuration_hostdependency("host2", "host1")};
  hd_aply.expand_objects(pb_indexed_config.state());
  hd_aply.add_object(hd2);
  hd_aply.resolve_object(hd2, err);

  ASSERT_EQ(pre_flight_circular_check(&err.config_warnings, &err.config_errors),
            ERROR);
}

TEST_F(HostDependency, PbCircularDependency3) {
  configuration::applier::hostdependency hd_aply;
  configuration::Hostdependency hd1{
      new_pb_configuration_hostdependency("host1", "host2")};
  hd_aply.expand_objects(pb_indexed_config.state());
  hd_aply.add_object(hd1);
  configuration::error_cnt err;
  hd_aply.resolve_object(hd1, err);

  configuration::Hostdependency hd2{
      new_pb_configuration_hostdependency("host2", "host3")};
  hd_aply.expand_objects(pb_indexed_config.state());
  hd_aply.add_object(hd2);
  hd_aply.resolve_object(hd2, err);

  configuration::Hostdependency hd3{
      new_pb_configuration_hostdependency("host3", "host1")};
  hd_aply.expand_objects(pb_indexed_config.state());
  hd_aply.add_object(hd3);
  hd_aply.resolve_object(hd3, err);

  ASSERT_EQ(pre_flight_circular_check(&err.config_warnings, &err.config_errors),
            ERROR);
}

TEST_F(HostDependency, PbRemoveHostdependency) {
  configuration::applier::hostdependency hd_aply;
  configuration::Hostdependency hd1{
      new_pb_configuration_hostdependency("host1", "host2")};
  hd_aply.expand_objects(pb_indexed_config.state());
  hd_aply.add_object(hd1);
  configuration::error_cnt err;
  hd_aply.resolve_object(hd1, err);

  ASSERT_EQ(engine::hostdependency::hostdependencies.size(), 1);
  hd_aply.remove_object(0);
  ASSERT_EQ(engine::hostdependency::hostdependencies.size(), 0);
}

TEST_F(HostDependency, PbExpandHostdependency) {
  configuration::State s;
  configuration::Hostdependency hd{
      new_pb_configuration_hostdependency("host1,host3,host5", "host2,host6")};
  auto* new_hd = s.add_hostdependencies();
  new_hd->CopyFrom(std::move(hd));
  configuration::applier::hostdependency hd_aply;
  hd_aply.expand_objects(s);
  ASSERT_EQ(s.hostdependencies().size(), 6);
  ASSERT_TRUE(std::all_of(s.hostdependencies().begin(),
                          s.hostdependencies().end(), [](const auto& hd) {
                            return hd.hostgroups().data().empty() &&
                                   hd.dependent_hostgroups().data().empty();
                          }));
}
