/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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

#include "../helper.hh"
#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/process.hh"
#include "common/engine_conf/command_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/service_helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class MacroCustomRecursive : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();
    _tp = _creator.new_timeperiod();
    for (int i = 0; i < 7; ++i)
      _creator.new_timerange(0, 0, 24, 0, i);
    _now = strtotimet("2016-11-24 08:00:00");
    set_time(_now);

    /* command needed by service */
    configuration::applier::command cmd_aply;
    configuration::Command cmd;
    configuration::command_helper cmd_hlp(&cmd);
    cmd.set_command_name("base_cmd");
    cmd.set_command_line("/bin/true");
    cmd_aply.add_object(cmd);

    /* host */
    configuration::applier::host hst_aply;
    configuration::Host hst;
    configuration::host_helper hst_hlp(&hst);
    hst.set_host_name("test_host");
    hst.set_host_id(12);
    hst.set_address("10.0.0.1");
    hst_hlp.set_default_values();
    hst.set_check_command("base_cmd");
    hst_aply.add_object(hst);

    _host = host::hosts.find("test_host")->second;

    /* service on that host */
    configuration::applier::service svc_aply;
    configuration::Service svc;
    configuration::service_helper svc_hlp(&svc);
    svc.set_host_name("test_host");
    svc.set_service_description("test_svc");
    svc.set_service_id(1);
    svc.set_host_id(12);
    svc_hlp.set_default_values();
    svc.set_check_command("base_cmd");
    svc_aply.add_object(svc);

    _svc = service::services.find({"test_host", "test_svc"})->second;

    init_macros();

    //macros_logger->set_level(spdlog::level::trace);
  }

  void TearDown() override {
    _svc.reset();
    _host.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;
  timeperiod_creator _creator;
  time_t _now;
  timeperiod* _tp;
};

/***********************************************************************
 *                     HOST CUSTOM MACRO TESTS                         *
 ***********************************************************************/

/* Host custom macro containing a standard host macro. */
TEST_F(MacroCustomRecursive, HostCustomContainsHostStandard) {
  _host->custom_variables["MYVAR"] = customvariable("host is $HOSTNAME$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTMYVAR$", out, 0);
  ASSERT_EQ(out, "host is test_host");
}

/* Host custom macro containing another host custom macro. */
TEST_F(MacroCustomRecursive, HostCustomContainsHostCustom) {
  _host->custom_variables["INNER"] = customvariable("inner_value");
  _host->custom_variables["OUTER"] = customvariable("got $_HOSTINNER$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTOUTER$", out, 0);
  ASSERT_EQ(out, "got inner_value");
}

/* Host custom macro containing $HOSTADDRESS$. */
TEST_F(MacroCustomRecursive, HostCustomContainsHostAddress) {
  _host->custom_variables["URL"] =
      customvariable("http://$HOSTADDRESS$/status");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTURL$", out, 0);
  ASSERT_EQ(out, "http://10.0.0.1/status");
}

/* Host custom macro without any $ should be returned as-is. */
TEST_F(MacroCustomRecursive, HostCustomNoRecursion) {
  _host->custom_variables["SIMPLE"] = customvariable("just_a_value");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTSIMPLE$", out, 0);
  ASSERT_EQ(out, "just_a_value");
}

/***********************************************************************
 *                   SERVICE CUSTOM MACRO TESTS                        *
 ***********************************************************************/
TEST_F(MacroCustomRecursive, ServiceCustomFullExample) {
  _host->custom_variables["HTTPPORT"] = customvariable("8080");
  _svc->custom_variables["ENDPOINT"] = customvariable("api/status");
  _svc->custom_variables["CUSTOMURL"] =
      customvariable("http://$HOSTADDRESS$:$_HOSTHTTPPORT$/$_SERVICEENDPOINT$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "$_SERVICECUSTOMURL$", out, 0);
  ASSERT_EQ(out, "http://10.0.0.1:8080/api/status");
}

TEST_F(MacroCustomRecursive, ServiceCustomFullExampleEndWithDollar) {
  _host->custom_variables["HTTPPORT"] = customvariable("8080");
  _svc->custom_variables["ENDPOINT"] = customvariable("api/status");
  _svc->custom_variables["CUSTOMURL"] = customvariable(
      "http://$HOSTADDRESS$:$_HOSTHTTPPORT$/$_SERVICEENDPOINT$/storage'^/$'");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "$_SERVICECUSTOMURL$", out, 0);
  ASSERT_EQ(out, "http://10.0.0.1:8080/api/status/storage'^/$'");
}

/* Service custom macro containing a standard service macro. */
TEST_F(MacroCustomRecursive, ServiceCustomContainsServiceStandard) {
  _svc->custom_variables["INFO"] = customvariable("svc=$SERVICEDESC$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "$_SERVICEINFO$", out, 0);
  ASSERT_EQ(out, "svc=test_svc");
}

/* Service custom macro can resolve host standard macros. */
TEST_F(MacroCustomRecursive, ServiceCustomContainsHostStandard) {
  _svc->custom_variables["HNAME"] = customvariable("host=$HOSTNAME$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "$_SERVICEHNAME$", out, 0);
  ASSERT_EQ(out, "host=test_host");
}

/* Service custom macro can resolve host custom macros. */
TEST_F(MacroCustomRecursive, ServiceCustomContainsHostCustom) {
  _host->custom_variables["PORT"] = customvariable("3306");
  _svc->custom_variables["DSN"] =
      customvariable("mysql://10.0.0.1:$_HOSTPORT$/db");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "$_SERVICEDSN$", out, 0);
  ASSERT_EQ(out, "mysql://10.0.0.1:3306/db");
}

/* Service custom macro can resolve $USERn$ resource macros. */
TEST_F(MacroCustomRecursive, ServiceCustomContainsUserMacro) {
  extern std::string macro_user[MAX_USER_MACROS];
  macro_user[0] = "/usr/lib/nagios/plugins";

  _svc->custom_variables["CHECK"] =
      customvariable("$USER1$/check_http -H $HOSTADDRESS$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "$_SERVICECHECK$", out, 0);
  ASSERT_EQ(out, "/usr/lib/nagios/plugins/check_http -H 10.0.0.1");

  macro_user[0].clear();
}

/***********************************************************************
 *                   RECURSION DEPTH LIMIT TESTS                       *
 ***********************************************************************/

/* Recursion depth limited to 2. A 3-level chain leaves the deepest
 * macro unresolved. */
TEST_F(MacroCustomRecursive, RecursionLimitedToDepth2) {
  _host->custom_variables["DEEP"] = customvariable("$HOSTNAME$");
  _host->custom_variables["MID"] = customvariable("$_HOSTDEEP$");
  _host->custom_variables["TOP"] = customvariable("$_HOSTMID$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();

  process_macros_r(mac, "$_HOSTTOP$", out, 0);
  ASSERT_EQ(out, "$HOSTNAME$");
}

/* 2-level chain resolves fully (within the limit). */
TEST_F(MacroCustomRecursive, TwoLevelChainResolvesCompletely) {
  _host->custom_variables["INNER"] = customvariable("$HOSTNAME$");
  _host->custom_variables["OUTER"] = customvariable("$_HOSTINNER$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTOUTER$", out, 0);
  ASSERT_EQ(out, "test_host");
}
