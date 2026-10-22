/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

#include <fstream>

#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/macros.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/macros.hh"
#include "common/engine_conf/command_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

extern configuration::State pb_config;

/* The host check command prints the macros it receives in its environment. */
static constexpr const char* env_script = "/tmp/pbmacro_global_context.sh";

/**
 * @brief The engine shares one global macro context between its checks,
 * notifications and event handlers. A notification or a CMA service result
 * leaves a service in it. These tests check that this service never reaches
 * a following host check, and that a reload removing this service does not
 * leave a dangling pointer in the context (centengine crashed in
 * _build_environment_macros() on the next host check when
 * enable_environment_macros was set).
 */
class GlobalMacrosContext : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();
    // The engine creates it at startup. Created by the reload, its constructor
    // would clear the macros and hide the bug.
    configuration::applier::macros::instance();
    init_macros();
    set_time(50000);

    std::ofstream script(env_script);
    script << "echo \"HOSTNAME=[${NAGIOS_HOSTNAME}] "
              "SERVICEDESC=[${NAGIOS_SERVICEDESC}]\"\n";
    script.close();

    pb_config.set_enable_environment_macros(true);

    configuration::error_cnt err;
    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_config);
    ct_aply.resolve_object(ctct, err);

    configuration::Command env_cmd;
    configuration::command_helper env_cmd_hlp(&env_cmd);
    env_cmd.set_command_name("env_cmd");
    env_cmd.set_command_line(fmt::format("/bin/sh {}", env_script));
    configuration::applier::command cmd_aply;
    cmd_aply.add_object(env_cmd);

    configuration::Host hst{new_pb_configuration_host("test_host", "admin")};
    hst.set_check_command("env_cmd");
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

    configuration::Service svc{
        new_pb_configuration_service("test_host", "test_svc", "admin")};
    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);

    hst_aply.resolve_object(hst, err);
    svc_aply.resolve_object(svc, err);

    _host = engine::host::hosts.begin()->second;
    _host->set_current_state(engine::host::state_up);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));
    // Otherwise a service result triggers a first check of the host.
    _host->set_has_been_checked(true);

    _svc = engine::service::services.begin()->second;
    _svc->set_current_state(engine::service::state_ok);
    _svc->set_last_state(engine::service::state_ok);
    _svc->set_state_type(checkable::hard);
    _svc->set_acknowledgement(AckType::NONE);
    _svc->set_notify_on(static_cast<uint32_t>(-1));
    _svc->set_accept_passive_checks(true);
  }

  void TearDown() override {
    _svc.reset();
    _host.reset();
    deinit_config_state();
    ::unlink(env_script);
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;

  void notify_service() {
    _svc->set_current_state(engine::service::state_critical);
    _svc->set_last_state(engine::service::state_critical);
    ASSERT_EQ(_svc->notify(notifier::reason_normal, "admin", "comment",
                           notifier::notification_option_forced),
              OK);
  }

  /* The result is OK as the service: no host check is triggered on demand. */
  void receive_cma_service_result() {
    timeval now{.tv_sec = std::time(nullptr), .tv_usec = 0};
    auto res = std::make_shared<check_result>(
        service_check, _svc.get(), checkable::check_passive,
        CHECK_OPTION_PASSIVE_IS_HARD | CHECK_OPTION_CMA_RESULT, false, 0.0, now,
        now, false, true, engine::service::state_ok, "OK");
    checks::checker::instance().add_check_result_to_reap(res);
    checks::checker::instance().reap();
  }

  /* Reload the configuration without any service. */
  void reload_without_services() {
    configuration::State new_cfg;
    new_cfg.CopyFrom(pb_config);
    new_cfg.mutable_services()->Clear();

    // We must not keep the service alive, the reload has to destroy it.
    _svc.reset();

    configuration::error_cnt err;
    configuration::applier::state::instance().apply(new_cfg, err);
    ASSERT_TRUE(engine::service::services.empty());
    _host = engine::host::hosts.begin()->second;
  }

  /* Objects may have been destroyed by a reload, no pointer must remain. */
  static void no_object_in_global_macros() {
    nagios_macros* mac = get_global_macros();
    ASSERT_EQ(mac->service_ptr, nullptr);
    ASSERT_EQ(mac->servicegroup_ptr, nullptr);
    ASSERT_EQ(mac->host_ptr, nullptr);
    ASSERT_EQ(mac->hostgroup_ptr, nullptr);
    ASSERT_EQ(mac->contact_ptr, nullptr);
    ASSERT_EQ(mac->contactgroup_ptr, nullptr);
  }

  /* Execute an active check of the host and return its output. */
  std::string run_host_check() {
    if (_host->run_async_check(CHECK_OPTION_FORCE_EXECUTION, 0.0, false, false,
                               nullptr, nullptr) != OK)
      return "host check not executed";
    checks::checker::instance().wait_completion(
        checks::checker::e_completion_filter::host);
    checks::checker::instance().reap();
    return _host->get_plugin_output();
  }
};

// Given a service that has just been notified
// When its host is checked with environment macros enabled
// Then the host check command does not get the service macros.
TEST_F(GlobalMacrosContext, HostCheckAfterServiceNotification) {
  ASSERT_NO_FATAL_FAILURE(notify_service());
  ASSERT_EQ(run_host_check(), "HOSTNAME=[test_host] SERVICEDESC=[]");
}

// Given a CMA service whose result has just been received
// When its host is checked with environment macros enabled
// Then the host check command does not get the service macros.
TEST_F(GlobalMacrosContext, HostCheckAfterCmaServiceResult) {
  ASSERT_NO_FATAL_FAILURE(receive_cma_service_result());
  ASSERT_EQ(run_host_check(), "HOSTNAME=[test_host] SERVICEDESC=[]");
}

// Given a service that has just been notified
// When a reload removes this service
// Then the global macros do not reference any object anymore
// And the next host check is executed without the service macros.
TEST_F(GlobalMacrosContext, ReloadRemovingNotifiedService) {
  ASSERT_NO_FATAL_FAILURE(notify_service());
  ASSERT_NO_FATAL_FAILURE(reload_without_services());
  ASSERT_NO_FATAL_FAILURE(no_object_in_global_macros());

  ASSERT_EQ(run_host_check(), "HOSTNAME=[test_host] SERVICEDESC=[]");
}

// Given a CMA service whose result has just been received
// When a reload removes this service
// Then the global macros do not reference any object anymore
// And the next host check is executed without the service macros.
TEST_F(GlobalMacrosContext, ReloadRemovingCmaService) {
  ASSERT_NO_FATAL_FAILURE(receive_cma_service_result());
  ASSERT_NO_FATAL_FAILURE(reload_without_services());
  ASSERT_NO_FATAL_FAILURE(no_object_in_global_macros());

  ASSERT_EQ(run_host_check(), "HOSTNAME=[test_host] SERVICEDESC=[]");
}
