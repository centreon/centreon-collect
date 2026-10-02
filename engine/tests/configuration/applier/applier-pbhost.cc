/**
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

#include <fstream>
#include <gtest/gtest.h>
#include <unistd.h>

#include "../../timeperiod/utils.hh"
#include "cbmod_test.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "common/engine_conf/command_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/service_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierPbHost : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

class ProtobufHostEvents : public ApplierPbHost {
 protected:
  class recording_cbmod : public com::centreon::broker::neb::cbmod {
   public:
    explicit recording_cbmod(const std::string& path) : cbmod(path) {}
    bool record = false;
    std::vector<std::shared_ptr<com::centreon::broker::io::data>> written;
    void write(
        const std::shared_ptr<com::centreon::broker::io::data>& event) override {
      if (record)
        written.push_back(event);
    }
  };

  std::string _config_file;

 public:
  void SetUp() override {
    // The default cbmod mock uses BBDO 2. Exercise the actual BBDO 3 sender.
    cbm.reset();
    char config_path[] = "/tmp/engine-macro-events-XXXXXX";
    int fd = mkstemp(config_path);
    ASSERT_NE(fd, -1);
    close(fd);
    _config_file = config_path;
    {
      std::ofstream config(_config_file);
      ASSERT_TRUE(config.is_open());
      config << R"({"centreonBroker": {
        "broker_id": 999,
        "broker_name": "macro-events-test",
        "poller_id": 20,
        "uid": 4294967316,
        "poller_name": "test-poller",
        "bbdo_version": "3.0.0",
        "cache_directory": "/tmp"
      }})";
    }
    cbm = std::make_unique<recording_cbmod>(_config_file);
    ApplierPbHost::SetUp();
  }

  void TearDown() override {
    ApplierPbHost::TearDown();
    cbm.reset();
    ::remove(_config_file.c_str());
  }
};

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(ApplierPbHost, PbNewHostWithoutHostId) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst_hlp.set_default_values();
  ASSERT_THROW(hst_aply.add_object(hst), std::exception);
}

// Given a host configuration
// When we change the host name in the configuration
// Then the applier modify_object changes the host name without changing
// the host id.
TEST_F(ApplierPbHost, HostRenamed) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.set_default_values();
  hst_aply.add_object(hst);
  host_map const& hm(engine::host::hosts);
  ASSERT_EQ(hm.size(), 1u);
  std::shared_ptr<com::centreon::engine::host> h1(hm.begin()->second);
  ASSERT_TRUE(h1->name() == "test_host");

  hst.set_host_name("test_host1");
  hst_aply.modify_object(&pb_config.mutable_hosts()->at(0), hst);
  ASSERT_EQ(hm.size(), 1u);
  h1 = hm.begin()->second;
  ASSERT_TRUE(h1->name() == "test_host1");
  ASSERT_EQ(get_host_id(h1->name()), 12u);
}

// Given a host with a custom variable sent to broker
// When its custom variables are modified
// Then the variable is still sent, so a later removal reaches broker.
TEST_F(ApplierPbHost, ModifiedCustomVariableKeepsIsSent) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.set_default_values();
  configuration::CustomVariable* cv = hst.add_customvariables();
  cv->set_name("OTEL_SERVICE_NAME");
  cv->set_value("payment-api");
  cv->set_is_sent(true);
  hst_aply.add_object(hst);
  std::shared_ptr<com::centreon::engine::host> h1(
      engine::host::hosts.begin()->second);
  ASSERT_TRUE(h1->custom_variables["OTEL_SERVICE_NAME"].is_sent());

  hst.mutable_customvariables(0)->set_value("billing");
  hst_aply.modify_object(&pb_config.mutable_hosts()->at(0), hst);
  h1 = engine::host::hosts.begin()->second;
  ASSERT_EQ(h1->custom_variables["OTEL_SERVICE_NAME"].value(), "billing");
  ASSERT_TRUE(h1->custom_variables["OTEL_SERVICE_NAME"].is_sent());
}

// Given a host configuration with a custom variable
// When the applier adds the host
// Then broker receives the host before its custom variables, as in the
// startup dump, so it can tell them from the ones of a previous poller.
TEST_F(ApplierPbHost, AddedHostIsSentBeforeItsCustomVariables) {
  auto* test_cbm =
      static_cast<com::centreon::broker::neb::cbmod_test*>(cbm.get());
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.set_default_values();
  configuration::CustomVariable* cv = hst.add_customvariables();
  cv->set_name("OTEL_SERVICE_NAME");
  cv->set_value("payment-api");
  cv->set_is_sent(true);

  test_cbm->written.clear();
  test_cbm->record = true;
  hst_aply.add_object(hst);
  test_cbm->record = false;

  namespace neb = com::centreon::broker::neb;
  auto first_of = [&](std::initializer_list<uint32_t> types) {
    for (size_t i = 0; i < test_cbm->written.size(); ++i)
      for (uint32_t t : types)
        if (test_cbm->written[i]->type() == t)
          return i;
    return test_cbm->written.size();
  };
  const size_t host_pos =
      first_of({neb::pb_host::static_type(), neb::host::static_type()});
  const size_t cv_pos = first_of({neb::pb_custom_variable::static_type(),
                                  neb::custom_variable::static_type()});
  const size_t nb_written = test_cbm->written.size();
  test_cbm->written.clear();
  ASSERT_LT(host_pos, nb_written);
  ASSERT_LT(cv_pos, nb_written);
  ASSERT_LT(host_pos, cv_pos);
}

TEST_F(ProtobufHostEvents, CustomVariableEventsIdentifyOriginatingPoller) {
  namespace neb = com::centreon::broker::neb;
  auto* test_cbm = static_cast<recording_cbmod*>(cbm.get());
  ASSERT_TRUE(test_cbm->use_protobuf());
  ASSERT_EQ(test_cbm->poller_id(), 4294967316ULL);

  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.set_default_values();
  hst_aply.add_object(hst);

  configuration::applier::command cmd_aply;
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");
  cmd.set_command_line("echo 1");
  cmd_aply.add_object(cmd);
  configuration::applier::service svc_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  svc.set_host_name("test_host");
  svc.set_host_id(12);
  svc.set_service_description("test_service");
  svc.set_service_id(3);
  svc.set_check_command("cmd");
  svc_hlp.set_default_values();
  svc_aply.add_object(svc);

  auto host = engine::host::hosts_by_id.at(12);
  auto service = engine::service::services_by_id.at({12, 3});
  test_cbm->written.clear();
  test_cbm->record = true;
  for (int type : {NEBTYPE_HOSTCUSTOMVARIABLE_ADD,
                   NEBTYPE_HOSTCUSTOMVARIABLE_DELETE})
    broker_custom_variable(type, host.get(), "OTEL_SERVICE_NAME", "api",
                           nullptr);
  for (int type : {NEBTYPE_SERVICECUSTOMVARIABLE_ADD,
                   NEBTYPE_SERVICECUSTOMVARIABLE_DELETE})
    broker_custom_variable(type, service.get(), "OTEL_SERVICE_NAME", "api",
                           nullptr);
  char host_args[] = "test_host;OTEL_SERVICE_NAME;updated";
  broker_external_command(NEBTYPE_EXTERNALCOMMAND_START,
                          CMD_CHANGE_CUSTOM_HOST_VAR, host_args);
  char service_args[] = "test_host;test_service;OTEL_SERVICE_NAME;updated";
  broker_external_command(NEBTYPE_EXTERNALCOMMAND_START,
                          CMD_CHANGE_CUSTOM_SVC_VAR, service_args);
  test_cbm->record = false;
  auto written = std::move(test_cbm->written);
  test_cbm->written.clear();

  ASSERT_EQ(written.size(), 6u);
  for (size_t i = 0; i < 4; ++i) {
    ASSERT_EQ(written[i]->type(), neb::pb_custom_variable::static_type());
    const auto& event =
        std::static_pointer_cast<neb::pb_custom_variable>(written[i])->obj();
    EXPECT_EQ(event.instance_id(), test_cbm->poller_id());
    EXPECT_EQ(event.host_id(), 12u);
    EXPECT_EQ(event.service_id(), i < 2 ? 0u : 3u);
    EXPECT_EQ(event.enabled(), i % 2 == 0);
  }
  for (size_t i = 4; i < 6; ++i) {
    ASSERT_EQ(written[i]->type(), neb::pb_custom_variable_status::static_type());
    const auto& event =
        std::static_pointer_cast<neb::pb_custom_variable_status>(written[i])
            ->obj();
    EXPECT_EQ(event.instance_id(), test_cbm->poller_id());
    EXPECT_EQ(event.host_id(), 12u);
    EXPECT_EQ(event.service_id(), i == 4 ? 0u : 3u);
    EXPECT_EQ(event.value(), "updated");
  }
}

TEST_F(ApplierPbHost, PbHostRemoved) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.set_default_values();
  hst_aply.add_object(hst);
  host_map const& hm(engine::host::hosts);
  ASSERT_EQ(hm.size(), 1u);
  std::shared_ptr<com::centreon::engine::host> h1(hm.begin()->second);
  ASSERT_TRUE(h1->name() == "test_host");

  hst_aply.remove_object(0);

  ASSERT_EQ(hm.size(), 0u);
  hst.set_host_name("test_host1");
  hst_aply.add_object(hst);
  h1 = hm.begin()->second;
  ASSERT_EQ(hm.size(), 1u);
  ASSERT_TRUE(h1->name() == "test_host1");
  ASSERT_EQ(get_host_id(h1->name()), 12u);
}

TEST_F(ApplierPbHost, PbHostParentChildUnreachable) {
  configuration::error_cnt err;
  configuration::applier::host hst_aply;
  configuration::applier::command cmd_aply;
  configuration::Host hst_child;
  configuration::host_helper hst_child_hlp(&hst_child);
  configuration::Host hst_parent;
  configuration::host_helper hst_parent_hlp(&hst_parent);

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("base_centreon_ping");
  cmd.set_command_line(
      "$USER1$/check_icmp -H $HOSTADDRESS$ -n $_HOSTPACKETNUMBER$ -w "
      "$_HOSTWARNING$ -c $_HOSTCRITICAL$");
  cmd_aply.add_object(cmd);

  hst_child.set_host_name("child_host");
  hst_child.set_address("127.0.0.1");
  hst_child_hlp.hook("parents", "parent_host");
  hst_child.set_host_id(1);
  hst_child_hlp.hook("_PACKETNUMBER", "42");
  hst_child_hlp.hook("_WARNING", "200,20%");
  hst_child_hlp.hook("_CRITICAL", "400,50%");
  hst_child.set_check_command("base_centreon_ping");
  hst_child_hlp.set_default_values();
  hst_aply.add_object(hst_child);

  hst_parent.set_host_name("parent_host");
  hst_parent.set_address("127.0.0.1");
  hst_parent.set_host_id(2);
  hst_parent_hlp.hook("_PACKETNUMBER", "42");
  hst_parent_hlp.hook("_WARNING", "200,20%");
  hst_parent_hlp.hook("_CRITICAL", "400,50%");
  hst_parent.set_check_command("base_centreon_ping");
  hst_parent_hlp.set_default_values();
  hst_aply.add_object(hst_parent);

  ASSERT_EQ(engine::host::hosts.size(), 2u);

  hst_aply.expand_objects(pb_config);
  hst_aply.resolve_object(hst_child, err);
  hst_aply.resolve_object(hst_parent, err);

  host_map::iterator child = engine::host::hosts.find("child_host");
  host_map::iterator parent = engine::host::hosts.find("parent_host");

  ASSERT_EQ(parent->second->child_hosts.size(), 1u);
  ASSERT_EQ(child->second->parent_hosts.size(), 1u);

  engine::host::host_state result;
  parent->second->run_sync_check_3x(&result, 0, 0, 0);
  ASSERT_EQ(parent->second->get_current_state(), engine::host::state_down);
  child->second->run_sync_check_3x(&result, 0, 0, 0);
  ASSERT_EQ(child->second->get_current_state(),
            engine::host::state_unreachable);
}
