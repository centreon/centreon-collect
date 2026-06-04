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

/***********************************************************************
 *         UNKNOWN MACRO RESOLUTION BEHAVIOR TESTS                     *
 ***********************************************************************/

/* Old-style $MACRO$: unknown at depth 0 (top-level call) is replaced by
 * an empty string. */
TEST_F(MacroCustomRecursive, UnknownSimpleMacroAtTopLevelBecomesEmpty) {
  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "A$UNKNOWNMACRO$B", out, 0);
  ASSERT_EQ(out, "AB");
}

/* Old-style $MACRO$: unknown at depth > 0 (inside a custom macro value)
 * is kept as-is for backward compatibility with SQL-like filters such as
 * "name in ('MSSQL$SIG','MSSQL$RH')". */
TEST_F(MacroCustomRecursive, UnknownSimpleMacroInsideCustomValueIsKeptAsIs) {
  _svc->custom_variables["FILTER"] =
      customvariable("name in ('MSSQL$SIG','MSSQL$RH')");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "$_SERVICEFILTER$", out, 0);
  ASSERT_EQ(out, "name in ('MSSQL$SIG','MSSQL$RH')");
}

/* New-style {{$MACRO$}}: unknown at depth 0 (top-level call) is replaced
 * by an empty string. */
TEST_F(MacroCustomRecursive, UnknownBraceMacroAtTopLevelBecomesEmpty) {
  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "A{{$UNKNOWNMACRO$}}B", out, 0);
  ASSERT_EQ(out, "AB");
}

/* New-style {{$MACRO$}}: unknown at depth > 0 (inside a custom macro value)
 * is still replaced by an empty string — no backward-compat exception applies
 * to the brace style. */
TEST_F(MacroCustomRecursive, UnknownBraceMacroInsideCustomValueBecomesEmpty) {
  _svc->custom_variables["EXTRAOPTIONS"] =
      customvariable("'perf-config=none{{$TITI$}}'");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->service_ptr = _svc.get();
  process_macros_r(mac, "{{$_SERVICEEXTRAOPTIONS$}}", out, 0);
  ASSERT_EQ(out, "'perf-config=none'");
}

/***********************************************************************
 *                       ARG MACRO TESTS                               *
 ***********************************************************************/

/* ARG0 is invalid (ARG macros are 1-based) → ERROR at depth 0 → empty. */
TEST_F(MacroCustomRecursive, Arg0InvalidAtTopLevelBecomesEmpty) {
  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "A$ARG0$B", out, 0);
  ASSERT_EQ(out, "AB");
}

/* ARG5 is valid but not set → OK with empty value → empty. */
TEST_F(MacroCustomRecursive, Arg5NotSetAtTopLevelBecomesEmpty) {
  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "A$ARG5$B", out, 0);
  ASSERT_EQ(out, "AB");
}

/* ARG0 (ERROR) inside a custom macro value (old style) → kept as-is at
 * depth > 0. */
TEST_F(MacroCustomRecursive, Arg0InvalidInsideCustomValueOldStyleKeptAsIs) {
  _host->custom_variables["CMD"] = customvariable("check$ARG0$end");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTCMD$", out, 0);
  ASSERT_EQ(out, "check$ARG0$end");
}

/* ARG0 (ERROR) inside a custom macro value (new style) → empty even at
 * depth > 0. */
TEST_F(MacroCustomRecursive, Arg0InvalidInsideCustomValueBraceStyleBecomesEmpty) {
  _host->custom_variables["CMD"] = customvariable("check{{$ARG0$}}end");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTCMD$", out, 0);
  ASSERT_EQ(out, "checkend");
}

/* ARG5 (OK, empty value) inside a custom macro value → empty even at depth > 0
 * because it goes through the OK branch, not the ERROR/kept-as-is branch. */
TEST_F(MacroCustomRecursive, Arg5NotSetInsideCustomValueBecomesEmpty) {
  _host->custom_variables["CMD"] = customvariable("check$ARG5$end");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTCMD$", out, 0);
  ASSERT_EQ(out, "checkend");
}

/***********************************************************************
 *                       USER MACRO TESTS                              *
 ***********************************************************************/

/* USER0 is invalid (USER macros are 1-based) → ERROR at depth 0 → empty. */
TEST_F(MacroCustomRecursive, User0InvalidAtTopLevelBecomesEmpty) {
  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "A$USER0$B", out, 0);
  ASSERT_EQ(out, "AB");
}

/* USER1 and USER2 not set → OK with empty value → empty. */
TEST_F(MacroCustomRecursive, User1AndUser2NotSetAtTopLevelBecomeEmpty) {
  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "A$USER1$$USER2$B", out, 0);
  ASSERT_EQ(out, "AB");
}

/* USER1 set to a value is resolved correctly. */
TEST_F(MacroCustomRecursive, User1SetIsResolved) {
  extern std::string macro_user[MAX_USER_MACROS];
  macro_user[0] = "/usr/lib/plugins";

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$USER1$/check_foo", out, 0);
  ASSERT_EQ(out, "/usr/lib/plugins/check_foo");

  macro_user[0].clear();
}

/* USER0 (ERROR) inside a custom macro value (old style) → kept as-is. */
TEST_F(MacroCustomRecursive, User0InvalidInsideCustomValueOldStyleKeptAsIs) {
  _host->custom_variables["VAL"] = customvariable("x$USER0$y");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTVAL$", out, 0);
  ASSERT_EQ(out, "x$USER0$y");
}

/* USER1 (OK, empty value) inside a custom macro value → empty at depth > 0
 * (goes through the OK branch, not kept as-is). */
TEST_F(MacroCustomRecursive, User1NotSetInsideCustomValueBecomesEmpty) {
  _host->custom_variables["VAL"] = customvariable("x$USER1$y");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTVAL$", out, 0);
  ASSERT_EQ(out, "xy");
}

/***********************************************************************
 *                  CONTACTADDRESS MACRO TESTS                         *
 ***********************************************************************/

/* CONTACTADDRESS0: index 0 subtracts to UINT_MAX → ERROR → empty at depth 0. */
TEST_F(MacroCustomRecursive, ContactAddress0InvalidAtTopLevelBecomesEmpty) {
  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "A$CONTACTADDRESS0$B", out, 0);
  ASSERT_EQ(out, "AB");
}

/* CONTACTADDRESS1: valid index but no contact_ptr → ERROR → empty at depth 0. */
TEST_F(MacroCustomRecursive, ContactAddress1NoContactAtTopLevelBecomesEmpty) {
  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "A$CONTACTADDRESS1$B", out, 0);
  ASSERT_EQ(out, "AB");
}

/* CONTACTADDRESS1 (ERROR) inside a custom macro value (old style) → kept. */
TEST_F(MacroCustomRecursive, ContactAddress1InsideCustomValueOldStyleKeptAsIs) {
  _host->custom_variables["INFO"] = customvariable("addr=$CONTACTADDRESS1$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTINFO$", out, 0);
  ASSERT_EQ(out, "addr=$CONTACTADDRESS1$");
}

/* CONTACTADDRESS1 (ERROR) inside a custom macro value (new style) → empty. */
TEST_F(MacroCustomRecursive,
       ContactAddress1InsideCustomValueBraceStyleBecomesEmpty) {
  _host->custom_variables["INFO"] = customvariable("addr={{$CONTACTADDRESS1$}}");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  process_macros_r(mac, "$_HOSTINFO$", out, 0);
  ASSERT_EQ(out, "addr=");
}

/***********************************************************************
 *              NON-EMPTY MACRO RESOLUTION TESTS                       *
 ***********************************************************************/

/* ARG1 set to a value is resolved at top level. */
TEST_F(MacroCustomRecursive, Arg1SetAtTopLevelIsResolved) {
  nagios_macros* mac(get_global_macros());
  mac->argv[0] = "myhost";

  std::string out;
  process_macros_r(mac, "check -H $ARG1$", out, 0);
  ASSERT_EQ(out, "check -H myhost");

  mac->argv[0].clear();
}

/* ARG5 set to a value is resolved at top level. */
TEST_F(MacroCustomRecursive, Arg5SetAtTopLevelIsResolved) {
  nagios_macros* mac(get_global_macros());
  mac->argv[4] = "5000";

  std::string out;
  process_macros_r(mac, "port=$ARG5$", out, 0);
  ASSERT_EQ(out, "port=5000");

  mac->argv[4].clear();
}

/* ARG1 set, inside a custom macro value (depth > 0) → resolved (goes
 * through the OK branch, not the kept-as-is ERROR branch). */
TEST_F(MacroCustomRecursive, Arg1SetInsideCustomValueIsResolved) {
  _host->custom_variables["CMD"] = customvariable("check -H $ARG1$");

  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->argv[0] = "myhost";

  std::string out;
  process_macros_r(mac, "$_HOSTCMD$", out, 0);
  ASSERT_EQ(out, "check -H myhost");

  mac->argv[0].clear();
}

/* USER2 set to a value is resolved at top level. */
TEST_F(MacroCustomRecursive, User2SetAtTopLevelIsResolved) {
  extern std::string macro_user[MAX_USER_MACROS];
  macro_user[1] = "secretpass";

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "pass=$USER2$", out, 0);
  ASSERT_EQ(out, "pass=secretpass");

  macro_user[1].clear();
}

/* CONTACTADDRESS1 with a contact having address[0] set is resolved at top
 * level. The contact is inserted directly into contact::contacts, which
 * deinit_config_state() clears in TearDown. */
TEST_F(MacroCustomRecursive, ContactAddress1WithContactAtTopLevelIsResolved) {
  auto ctct = std::make_shared<engine::contact>();
  ctct->set_name("test_contact");
  ctct->set_addresses({"192.168.1.1", "", "", "", "", ""});
  engine::contact::contacts["test_contact"] = ctct;

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->contact_ptr = ctct.get();
  process_macros_r(mac, "$CONTACTADDRESS1$", out, 0);
  ASSERT_EQ(out, "192.168.1.1");
}

/* CONTACTADDRESS1 with a contact, inside a custom macro value (depth > 0)
 * → resolved (OK branch). */
TEST_F(MacroCustomRecursive,
       ContactAddress1WithContactInsideCustomValueIsResolved) {
  auto ctct = std::make_shared<engine::contact>();
  ctct->set_name("test_contact");
  ctct->set_addresses({"192.168.1.1", "", "", "", "", ""});
  engine::contact::contacts["test_contact"] = ctct;

  _host->custom_variables["INFO"] = customvariable("addr=$CONTACTADDRESS1$");

  std::string out;
  nagios_macros* mac(get_global_macros());
  mac->host_ptr = _host.get();
  mac->contact_ptr = ctct.get();
  process_macros_r(mac, "$_HOSTINFO$", out, 0);
  ASSERT_EQ(out, "addr=192.168.1.1");
}
