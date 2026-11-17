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
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "common/engine_conf/indexed_state.hh"
#include "common/engine_conf/message_helper.hh"
#include "common/engine_conf/service_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

extern configuration::state* config;

class HostDependency : public TestEngine {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override {
    _state_hlp = init_config_state();

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    configuration::error_cnt err;
    ct_aply.add_object(ctct);
    _state_hlp->expand(err);
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
  _state_hlp->expand(err);
  hd_aply.add_object(hd1);
  hd_aply.resolve_object(hd1, err);

  configuration::Hostdependency hd2{
      new_pb_configuration_hostdependency("host2", "host1")};
  _state_hlp->expand(err);
  hd_aply.add_object(hd2);
  hd_aply.resolve_object(hd2, err);

  ASSERT_EQ(pre_flight_circular_check(&err.config_warnings, &err.config_errors),
            ERROR);
}

TEST_F(HostDependency, PbCircularDependency3) {
  configuration::applier::hostdependency hd_aply;
  configuration::Hostdependency hd1{
      new_pb_configuration_hostdependency("host1", "host2")};
  configuration::error_cnt err;
  _state_hlp->expand(err);
  hd_aply.add_object(hd1);
  hd_aply.resolve_object(hd1, err);

  configuration::Hostdependency hd2{
      new_pb_configuration_hostdependency("host2", "host3")};
  _state_hlp->expand(err);
  hd_aply.add_object(hd2);
  hd_aply.resolve_object(hd2, err);

  configuration::Hostdependency hd3{
      new_pb_configuration_hostdependency("host3", "host1")};
  _state_hlp->expand(err);
  hd_aply.add_object(hd3);
  hd_aply.resolve_object(hd3, err);

  ASSERT_EQ(pre_flight_circular_check(&err.config_warnings, &err.config_errors),
            ERROR);
}

TEST_F(HostDependency, PbRemoveHostdependency) {
  configuration::applier::hostdependency hd_aply;
  configuration::Hostdependency hd1{
      new_pb_configuration_hostdependency("host1", "host2")};
  configuration::error_cnt err;
  _state_hlp->expand(err);
  uint64_t hash_key = hostdependency_key(hd1);
  hd_aply.add_object(hd1);
  hd_aply.resolve_object(hd1, err);

  ASSERT_EQ(engine::hostdependency::hostdependencies.size(), 1);
  hd_aply.remove_object(hash_key);
  ASSERT_EQ(engine::hostdependency::hostdependencies.size(), 0);
}

TEST_F(HostDependency, PbExpandHostdependency) {
  auto config = std::make_unique<configuration::State>();
  configuration::state_helper s_hlp(config.get());
  configuration::Hostdependency hd{
      new_pb_configuration_hostdependency("host1,host3,host5", "host2,host6")};
  auto* new_hd = config->add_hostdependencies();
  new_hd->CopyFrom(std::move(hd));
  configuration::error_cnt err;
  s_hlp.expand(err);
  configuration::indexed_state state(std::move(config));
  ASSERT_EQ(state.hostdependencies().size(), 6);
  ASSERT_TRUE(
      std::all_of(state.hostdependencies().begin(),
                  state.hostdependencies().end(), [](const auto& hd) {
                    return hd.second->hostgroups().data().empty() &&
                           hd.second->dependent_hostgroups().data().empty();
                  }));
}
