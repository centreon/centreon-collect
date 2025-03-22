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
#include <fstream>
#include "com/centreon/engine/globals.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/hostgroup_helper.hh"

#include "../helper.hh"
#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostgroup.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/serviceescalation.hh"
#include "com/centreon/engine/configuration/applier/servicegroup.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/hostescalation.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/grab_host.hh"
#include "com/centreon/engine/macros/process.hh"
#include "com/centreon/engine/timeperiod.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class MacroHostname : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();
    _tp = _creator.new_timeperiod();
    for (int i(0); i < 7; ++i)
      _creator.new_timerange(0, 0, 24, 0, i);
    _now = strtotimet("2016-11-24 08:00:00");
    set_time(_now);
  }

  void TearDown() override {
    _host.reset();
    _host2.reset();
    _host3.reset();
    _svc.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host, _host2, _host3;
  std::shared_ptr<engine::service> _svc;
  timeperiod_creator _creator;
  time_t _now;
  timeperiod* _tp;
};

TEST_F(MacroHostname, HostProblemId) {
  configuration::applier::host hst_aply, hst_aply2;
  configuration::Host hst, hst2;
  configuration::host_helper hst_hlp(&hst), hst2_hlp(&hst2);
  next_problem_id = 1;

  set_time(50000);
  // first host
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  // second host
  hst2.set_host_name("test_host2");
  hst2.set_host_id(13);
  hst2.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_NO_THROW(hst_aply2.add_object(hst2));
  ASSERT_EQ(2u, host::hosts.size());

  init_macros();
  _host = host::hosts.find("test_host")->second;
  _host->set_current_state(engine::host::state_up);
  _host->set_last_hard_state(engine::host::state_up);
  _host->set_last_hard_state_change(50000);
  _host->set_state_type(checkable::hard);

  _host2 = host::hosts.find("test_host2")->second;
  _host2->set_current_state(engine::host::state_up);
  _host2->set_last_hard_state(engine::host::state_up);
  _host2->set_last_hard_state_change(50000);
  _host2->set_state_type(checkable::hard);

  auto fn = [](std::shared_ptr<engine::host> hst, std::string firstcheck,
               std::string secondcheck) -> void {
    std::string out;
    nagios_macros* mac(get_global_macros());

    for (int i = 0; i < 3; i++) {
      // When i == 0, the state_down is soft => no notification
      // When i == 1, the state_down is soft => no notification
      // When i == 2, the state_down is hard down => notification
      set_time(50500 + i * 500);
      hst->set_last_state(hst->get_current_state());
      if (notifier::hard == hst->get_state_type())
        hst->set_last_hard_state(hst->get_current_state());
      hst->process_check_result_3x(engine::host::state_down, "The host is down",
                                   CHECK_OPTION_NONE, 0, true, 0);
    }

    process_macros_r(mac, fmt::format("$HOSTPROBLEMID:{}$", hst->name()), out,
                     0);
    ASSERT_EQ(out, firstcheck);

    for (int i = 0; i < 2; i++) {
      // When i == 0, the state_up is hard (return to up) => Recovery
      // notification When i == 1, the state_up is still here (no change) => no
      // notification
      set_time(52500 + i * 500);
      hst->set_last_state(hst->get_current_state());
      if (notifier::hard == hst->get_state_type())
        hst->set_last_hard_state(hst->get_current_state());
      hst->process_check_result_3x(engine::host::state_up, "The host is up",
                                   CHECK_OPTION_NONE, 0, true, 0);
    }

    process_macros_r(mac, "$HOSTPROBLEMID:test_host$", out, 0);
    ASSERT_EQ(out, secondcheck);
  };

  fn(_host, "1", "0");
  fn(_host2, "2", "0");
  fn(_host, "3", "0");
}

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(MacroHostname, TotalHostOk) {
  configuration::applier::host hst_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  init_macros();

  nagios_macros* mac(get_global_macros());
  std::string out;
  host::hosts["test_host"]->set_current_state(host::state_up);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$TOTALHOSTSUP$", out, 1);
  ASSERT_EQ(out, "1");
}

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(MacroHostname, TotalHostServicesCritical) {
  configuration::applier::host hst_aply;
  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  init_macros();

  nagios_macros* mac(get_global_macros());
  std::string out;
  host::hosts["test_host"]->set_current_state(host::state_up);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$TOTALHOSTSERVICESCRITICAL:test_host$", out, 1);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostName) {
  init_macros();
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  std::string out;
  nagios_macros* mac(get_global_macros());

  process_macros_r(mac, "$HOSTNAME:test_host$", out, 0);
  ASSERT_EQ(out, "test_host");
}

TEST_F(MacroHostname, HostAlias) {
  init_macros();
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  std::string out;
  nagios_macros* mac(get_global_macros());

  process_macros_r(mac, "$HOSTALIAS:test_host$", out, 0);
  ASSERT_EQ(out, "test_host");
}

TEST_F(MacroHostname, HostAddress) {
  init_macros();
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  std::string out;
  nagios_macros* mac(get_global_macros());

  process_macros_r(mac, "$HOSTADDRESS:test_host$", out, 0);
  ASSERT_EQ(out, "127.0.0.1");
}

TEST_F(MacroHostname, LastHostCheck) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$LASTHOSTCHECK:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, LastHostStateChange) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$LASTHOSTSTATECHANGE:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostOutput) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_plugin_output("foo bar!");
  process_macros_r(mac, "$HOSTOUTPUT:test_host$", out, 0);
  ASSERT_EQ(out, "foo bar!");
}

TEST_F(MacroHostname, HostPerfData) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_perf_data("test");
  process_macros_r(mac, "$HOSTPERFDATA:test_host$", out, 0);
  ASSERT_EQ(out, "test");
}

TEST_F(MacroHostname, HostState) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  hst_hlp.hook("contacts", "user");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_current_state(host::state_up);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTSTATE:test_host$", out, 1);
  ASSERT_EQ(out, "UP");
}

TEST_F(MacroHostname, HostStateID) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  hst_hlp.hook("contacts", "user");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_current_state(host::state_down);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTSTATEID:test_host$", out, 1);
  ASSERT_EQ(out, "1");
}

TEST_F(MacroHostname, HostAttempt) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  hst_hlp.hook("contacts", "user");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_current_state(host::state_up);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTATTEMPT:test_host$", out, 1);
  ASSERT_EQ(out, "1");
}

TEST_F(MacroHostname, HostExecutionTime) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  hst_hlp.hook("contacts", "user");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  init_macros();

  int now{500000000};
  set_time(now);

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_execution_time(10.0);
  process_macros_r(mac, "$HOSTEXECUTIONTIME:test_host$", out, 1);
  ASSERT_EQ(out, "10.000");
}

TEST_F(MacroHostname, HostLatency) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_latency(100);
  process_macros_r(mac, "$HOSTLATENCY:test_host$", out, 1);
  ASSERT_EQ(out, "100.000");
}

TEST_F(MacroHostname, HostDuration) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTDURATION:test_host$", out, 1);
  ASSERT_EQ(out, "5787d 0h 53m 20s");
}

TEST_F(MacroHostname, HostDurationSec) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTDURATIONSEC:test_host$", out, 1);
  ASSERT_EQ(out, "500000000");
}

TEST_F(MacroHostname, HostDownTime) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTDOWNTIME:test_host$", out, 1);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostStateType) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTSTATETYPE:test_host$", out, 1);
  ASSERT_EQ(out, "HARD");
}

TEST_F(MacroHostname, HostPercentChange) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  hst.set_host_name("test_host");
  hst.set_host_id(12);
  hst.set_address("127.0.0.1");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());
  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTPERCENTCHANGE:test_host$", out, 1);
  ASSERT_EQ(out, "0.00");
}

TEST_F(MacroHostname, HostGroupName) {
  configuration::error_cnt err;
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  hst_a.set_host_name("a");
  hst_a.set_host_id(1);
  hst_a.set_address("127.0.0.1");

  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);
  hst_c.set_host_name("c");
  hst_c.set_host_id(2);
  hst_c.set_address("127.0.0.1");

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,c");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hg_aply.expand_objects(pb_indexed_config.state()));

  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTGROUPNAME:a$", out, 1);
  ASSERT_EQ(out, "temphg");
}

TEST_F(MacroHostname, HostGroupAlias) {
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_address("127.0.0.1");
  hst_a.set_host_id(1);

  hst_c.set_host_name("c");
  hst_c.set_address("127.0.0.1");
  hst_c.set_host_id(2);

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg.set_alias("temphgal");
  hg_hlp.hook("members", "a,c");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hg_aply.expand_objects(pb_indexed_config.state()));

  configuration::error_cnt err;
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTGROUPALIAS:temphg$", out, 1);
  ASSERT_EQ(out, "temphgal");
}

TEST_F(MacroHostname, LastHostUP) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_last_time_up(30);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$LASTHOSTUP:test_host$", out, 1);
  ASSERT_EQ(out, "30");
}

TEST_F(MacroHostname, LastHostDown) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  host::hosts["test_host"]->set_last_time_down(40);
  host::hosts["test_host"]->set_has_been_checked(true);
  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$LASTHOSTDOWN:test_host$", out, 1);
  ASSERT_EQ(out, "40");
}

TEST_F(MacroHostname, LastHostUnreachable) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_last_time_unreachable(50);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$LASTHOSTUNREACHABLE:test_host$", out, 1);
  ASSERT_EQ(out, "50");
}

TEST_F(MacroHostname, HostCheckCommand) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);

  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name("cmd");

  cmd.set_command_line("echo 'output| metric=12;50;75'");
  hst.set_check_command("cmd");
  configuration::applier::command cmd_aply;
  cmd_aply.add_object(cmd);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTCHECKCOMMAND:test_host$", out, 1);
  ASSERT_EQ(out, "cmd");
}

TEST_F(MacroHostname, HostDisplayName) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTDISPLAYNAME:test_host$", out, 1);
  ASSERT_EQ(out, "test_host");
}

TEST_F(MacroHostname, HostActionUrl) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_action_url("test_action_url");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTACTIONURL:test_host$", out, 1);
  ASSERT_EQ(out, "test_action_url");
}

TEST_F(MacroHostname, HostNotesUrl) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_notes_url("test_notes_url");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTNOTESURL:test_host$", out, 1);
  ASSERT_EQ(out, "test_notes_url");
}

TEST_F(MacroHostname, HostNotes) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTNOTES:test_host$", out, 1);
  ASSERT_EQ(out, "test_notes");
}

TEST_F(MacroHostname, TotalHostsDown) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_current_state(host::state_down);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$TOTALHOSTSDOWN:test_host$", out, 1);
  ASSERT_EQ(out, "1");
}

TEST_F(MacroHostname, TotalHostsUnreachable) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_current_state(host::state_unreachable);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$TOTALHOSTSUNREACHABLE:test_host$", out, 1);
  ASSERT_EQ(out, "1");
}

TEST_F(MacroHostname, TotalHostsDownUnhandled) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$TOTALHOSTSDOWNUNHANDLED:test_host$", out, 1);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, TotalHostsunreachableunhandled) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$TOTALHOSTSUNREACHABLEUNHANDLED:test_host$", out, 1);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, TotalHostProblems) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$TOTALHOSTPROBLEMS:test_host$", out, 1);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, TotalHostProblemsUnhandled) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$TOTALHOSTPROBLEMSUNHANDLED:test_host$", out, 1);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostCheckType) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_current_state(host::state_unreachable);
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTCHECKTYPE:test_host$", out, 0);
  ASSERT_EQ(out, "ACTIVE");
}

TEST_F(MacroHostname, LongHostOutput) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$LONGHOSTOUTPUT:test_host$", out, 0);
  ASSERT_EQ(out, "test_long_output");
}

TEST_F(MacroHostname, HostNotificationNumber) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTNOTIFICATIONNUMBER:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostNotificationID) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTNOTIFICATIONID:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostEventID) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTEVENTID:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, LastHostEventID) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$LASTHOSTEVENTID:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostGroupNames) {
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_address("127.0.0.1");
  hst_a.set_host_id(1);

  hst_c.set_host_name("c");
  hst_c.set_address("127.0.0.1");
  hst_c.set_host_id(2);

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,c");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hg_aply.expand_objects(pb_indexed_config.state()));

  configuration::error_cnt err;
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTGROUPNAMES:a$", out, 0);
  ASSERT_EQ(out, "temphg");
}

TEST_F(MacroHostname, MaxHostAttempts) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$MAXHOSTATTEMPTS:test_host$", out, 0);
  ASSERT_EQ(out, "3");
}

TEST_F(MacroHostname, TotalHostServices) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$TOTALHOSTSERVICES:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, TotalHostServicesOK) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$TOTALHOSTSERVICESOK:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, TotalHostServicesWarning) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$TOTALHOSTSERVICESWARNING:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, TotalHostServicesUnknown) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$TOTALHOSTSERVICESUNKNOWN:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostGroupNotes) {
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_address("127.0.0.1");
  hst_a.set_host_id(1);

  hst_c.set_host_name("c");
  hst_c.set_address("127.0.0.1");
  hst_c.set_host_id(2);

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,c");
  hg.set_notes("test_note");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hg_aply.expand_objects(pb_indexed_config.state()));

  configuration::error_cnt err;
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTGROUPNOTES:temphg$", out, 0);
  ASSERT_EQ(out, "test_note");
}

TEST_F(MacroHostname, HostGroupNotesUrl) {
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_helper(&hst_a);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_address("127.0.0.1");
  hst_a.set_host_id(1);

  hst_c.set_host_name("c");
  hst_c.set_address("127.0.0.1");
  hst_c.set_host_id(2);

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,c");
  hg.set_notes_url("test_note_url");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hg_aply.expand_objects(pb_indexed_config.state()));

  configuration::error_cnt err;
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTGROUPNOTESURL:temphg$", out, 0);
  ASSERT_EQ(out, "test_note_url");
}

TEST_F(MacroHostname, HostGroupActionUrl) {
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_address("127.0.0.1");
  hst_a.set_host_id(1);

  hst_c.set_host_name("c");
  hst_c.set_address("127.0.0.1");
  hst_c.set_host_id(2);

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,c");
  hg.set_action_url("test_action_url");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hg_aply.expand_objects(pb_indexed_config.state()));

  configuration::error_cnt err;
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTGROUPACTIONURL:temphg$", out, 0);
  ASSERT_EQ(out, "test_action_url");
}

TEST_F(MacroHostname, HostGroupMembers) {
  configuration::applier::hostgroup hg_aply;
  configuration::applier::host hst_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  configuration::Host hst_a;
  configuration::host_helper hst_a_hlp(&hst_a);
  configuration::Host hst_c;
  configuration::host_helper hst_c_hlp(&hst_c);

  hst_a.set_host_name("a");
  hst_a.set_host_id(1);
  hst_a.set_address("127.0.0.1");

  hst_c.set_host_name("c");
  hst_c.set_address("127.0.0.1");
  hst_c.set_host_id(2);

  hst_aply.add_object(hst_a);
  hst_aply.add_object(hst_c);

  hg.set_hostgroup_name("temphg");
  hg_hlp.hook("members", "a,c");
  hg.set_action_url("test_action_url");
  ASSERT_NO_THROW(hg_aply.add_object(hg));

  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hst_aply.expand_objects(pb_indexed_config.state()));
  ASSERT_NO_THROW(hg_aply.expand_objects(pb_indexed_config.state()));

  configuration::error_cnt err;
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_a, err));
  ASSERT_NO_THROW(hst_aply.resolve_object(hst_c, err));
  ASSERT_NO_THROW(hg_aply.resolve_object(hg, err));

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTGROUPMEMBERS:temphg$", out, 0);
  ASSERT_TRUE(out == "c,a" || out == "a,c");
}

TEST_F(MacroHostname, LastHostProblemId) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$LASTHOSTPROBLEMID:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, LastHostState) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$LASTHOSTSTATE:test_host$", out, 0);
  ASSERT_EQ(out, "UP");
}

TEST_F(MacroHostname, LastHostStateID) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$LASTHOSTSTATEID:test_host$", out, 0);
  ASSERT_EQ(out, "0");
}

TEST_F(MacroHostname, HostParents) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst_hlp.hook("parents", "test_parent");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTPARENTS:test_host$", out, 0);
  ASSERT_EQ(out, "test_parent");
}

TEST_F(MacroHostname, HostChildren) {
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
  hst_child_hlp.insert_customvariable("_PACKETNUMBER", "42");
  hst_child_hlp.insert_customvariable("_WARNING", "200,20%");
  hst_child_hlp.insert_customvariable("_CRITICAL", "400,50%");
  hst_child.set_check_command("base_centreon_ping");
  hst_aply.add_object(hst_child);

  hst_parent.set_host_name("parent_host");
  hst_parent.set_address("127.0.0.1");
  hst_parent.set_host_id(2);
  hst_parent_hlp.insert_customvariable("_PACKETNUMBER", "42");
  hst_parent_hlp.insert_customvariable("_WARNING", "200,20%");
  hst_parent_hlp.insert_customvariable("_CRITICAL", "400,50%");
  hst_parent.set_check_command("base_centreon_ping");
  hst_aply.add_object(hst_parent);

  ASSERT_EQ(engine::host::hosts.size(), 2u);

  configuration::error_cnt err;
  hst_aply.expand_objects(pb_indexed_config.state());
  hst_aply.resolve_object(hst_child, err);
  hst_aply.resolve_object(hst_parent, err);

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$HOSTCHILDREN:parent_host$", out, 0);
  ASSERT_EQ(out, "child_host");
}

TEST_F(MacroHostname, HostID) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_notes("test_notes");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTID:test_host$", out, 0);
  ASSERT_EQ(out, "12");
}

TEST_F(MacroHostname, HostTimeZone) {
  configuration::applier::host hst_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  hst_hlp.hook("contacts", "user");
  hst.set_timezone("test_timezone");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  host::hosts["test_host"]->set_long_plugin_output("test_long_output");
  host::hosts["test_host"]->set_has_been_checked(true);
  process_macros_r(mac, "$HOSTTIMEZONE:test_host$", out, 0);
  ASSERT_EQ(out, "test_timezone");
}
