/*
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
#include "com/centreon/engine/configuration/indexed_state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/hostescalation.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/grab_host.hh"
#include "com/centreon/engine/macros/process.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "common/engine_conf/contact_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/message_helper.hh"
#include "common/engine_conf/parser.hh"
#include "common/engine_conf/service_helper.hh"
#include "common/engine_conf/state.pb.h"
#include "common/engine_conf/timeperiod_helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

class Macro : public TestEngine {
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

// Given host configuration without host_id
// Then the applier add_object throws an exception.
TEST_F(Macro, PbPollerName) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "poller_name=poller-test" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$POLLERNAME$", out, 0);
  ASSERT_EQ(out, "poller-test");
}

TEST_F(Macro, PbPollerId) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "poller_id=42" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);
  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$POLLERID$", out, 0);
  ASSERT_EQ(out, "42");
}

TEST_F(Macro, PbLongDateTime) {
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
  process_macros_r(mac, "$LONGDATETIME:test_host$", out, 0);
  ASSERT_EQ(out, "Tue Nov 5 01:53:20 CET 1985");
}

TEST_F(Macro, PbShortDateTime) {
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
  process_macros_r(mac, "$SHORTDATETIME:test_host$", out, 0);
  ASSERT_EQ(out, "11-05-1985 01:53:20");
}

TEST_F(Macro, PbDate) {
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
  process_macros_r(mac, "$DATE:test_host$", out, 0);
  ASSERT_EQ(out, "11-05-1985");
}

TEST_F(Macro, PbTime) {
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
  process_macros_r(mac, "$TIME:test_host$", out, 0);
  ASSERT_EQ(out, "01:53:20");
}

TEST_F(Macro, PbTimeT) {
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
  process_macros_r(mac, "$TIMET:test_host$", out, 0);
  ASSERT_EQ(out, "500000000");
}

TEST_F(Macro, PbContactName) {
  configuration::applier::host hst_aply;
  configuration::applier::contact cnt_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Contact cnt;
  configuration::contact_helper cnt_hlp(&cnt);
  cnt.set_contact_name("user");
  cnt.set_email("contact@centreon.com");
  cnt.set_pager("0473729383");
  cnt_aply.add_object(cnt);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  fill_string_group(hst.mutable_contacts(), "user");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTNAME:user$", out, 1);
  ASSERT_EQ(out, "user");
}

TEST_F(Macro, PbContactAlias) {
  configuration::applier::host hst_aply;
  configuration::applier::contact cnt_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Contact cnt;
  configuration::contact_helper cnt_hlp(&cnt);
  cnt.set_contact_name("user");
  cnt.set_email("contact@centreon.com");
  cnt.set_pager("0473729383");
  cnt_aply.add_object(cnt);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  fill_string_group(hst.mutable_contacts(), "user");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTALIAS:user$", out, 1);
  ASSERT_EQ(out, "user");
}

TEST_F(Macro, PbContactEmail) {
  configuration::applier::host hst_aply;
  configuration::applier::contact cnt_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Contact cnt;
  configuration::contact_helper cnt_hlp(&cnt);
  cnt.set_contact_name("user");
  cnt.set_email("contact@centreon.com");
  cnt.set_pager("0473729383");
  cnt_aply.add_object(cnt);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  fill_string_group(hst.mutable_contacts(), "user");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTEMAIL:user$", out, 1);
  ASSERT_EQ(out, "contact@centreon.com");
}

TEST_F(Macro, PbContactPager) {
  configuration::applier::host hst_aply;
  configuration::applier::contact cnt_aply;
  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  configuration::Contact cnt;
  configuration::contact_helper cnt_hlp(&cnt);
  cnt.set_contact_name("user");
  cnt.set_email("contact@centreon.com");
  cnt.set_pager("0473729383");
  cnt_aply.add_object(cnt);

  hst.set_host_name("test_host");
  hst.set_address("127.0.0.1");
  hst.set_host_id(12);
  fill_string_group(hst.mutable_contacts(), "user");
  ASSERT_NO_THROW(hst_aply.add_object(hst));
  ASSERT_EQ(1u, host::hosts.size());

  int now{500000000};
  set_time(now);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTPAGER:user$", out, 1);
  ASSERT_EQ(out, "0473729383");
}

TEST_F(Macro, PbAdminEmail) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "admin_email=contactadmin@centreon.com" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);

  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$ADMINEMAIL:test_host$", out, 1);
  ASSERT_EQ(out, "contactadmin@centreon.com");
}

TEST_F(Macro, PbAdminPager) {
  configuration::indexed_state state;
  configuration::parser parser;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "admin_pager=04737293866" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);

  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$ADMINPAGER:test_host$", out, 1);
  ASSERT_EQ(out, "04737293866");
}

TEST_F(Macro, PbMainConfigFile) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$MAINCONFIGFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/tmp/test-config.cfg");
}

TEST_F(Macro, PbStatusDataFile) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "status_file=/usr/local/var/status.dat" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$STATUSDATAFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/usr/local/var/status.dat");
}

TEST_F(Macro, PbRetentionDataFile) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "state_retention_file=/var/log/centreon-engine/retention.dat"
      << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  state.index();
  configuration::applier::state::instance().apply(state, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$RETENTIONDATAFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/var/log/centreon-engine/retention.dat");
}

TEST_F(Macro, PbTempFile) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$TEMPFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/tmp/centengine.tmp");
}

TEST_F(Macro, PbLogFile) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=/tmp/centengine.log" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$LOGFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/tmp/centengine.log");
}

TEST_F(Macro, PbCommandFile) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State& st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "command_file=/usr/local/var/rw/centengine.cmd" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$COMMANDFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/usr/local/var/rw/centengine.cmd");
}

TEST_F(Macro, PbTempPath) {
  configuration::parser parser;
  configuration::indexed_state state;
  configuration::State st = state.mut_state();

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", &st, err);
  configuration::applier::state::instance().apply(state, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$TEMPPATH$", out, 0);
  ASSERT_EQ(out, "/tmp");
}

TEST_F(Macro, PbContactGroupName) {
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "test_contact", true);
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_indexed_config);
  cg_aply.resolve_object(cg, err);

  init_macros();
  int now{500000000};
  set_time(now);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTGROUPNAME:test_contact$", out, 1);
  ASSERT_EQ(out, "test_cg");
}

TEST_F(Macro, PbContactGroupAlias) {
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "test_contact", true);
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_indexed_config);
  cg_aply.resolve_object(cg, err);
  init_macros();
  int now{500000000};
  set_time(now);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTGROUPALIAS:test_cg$", out, 1);
  ASSERT_EQ(out, "test_cg");
}

TEST_F(Macro, PbContactGroupMembers) {
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "test_contact", true);
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_indexed_config);
  cg_aply.resolve_object(cg, err);
  init_macros();
  int now{500000000};
  set_time(now);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTGROUPMEMBERS:test_cg$", out, 1);
  ASSERT_EQ(out, "test_contact");
}

TEST_F(Macro, PbContactGroupNames) {
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "test_contact", true);
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_indexed_config);
  cg_aply.resolve_object(cg, err);
  init_macros();
  int now{500000000};
  set_time(now);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTGROUPNAMES:test_contact$", out, 1);
  ASSERT_EQ(out, "test_cg");
}

TEST_F(Macro, PbNotificationRecipients) {
  init_macros();
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "admin", true);
  ct_aply.add_object(ctct);
  configuration::Contact ctct1;
  configuration::contact_helper ctct1_hlp(&ctct1);
  fill_pb_configuration_contact(&ctct1_hlp, "admin1", false, "c,r");
  ct_aply.add_object(ctct1);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);
  ct_aply.resolve_object(ctct1, err);
  configuration::Contact ctct2;
  configuration::contact_helper ctct2_hlp(&ctct2);
  fill_pb_configuration_contact(&ctct2_hlp, "test_contact", false);
  ct_aply.add_object(ctct2);
  ct_aply.expand_objects(pb_indexed_config);
  ct_aply.resolve_object(ctct2, err);

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  fill_pb_configuration_host(&hst_hlp, "test_host", "admin");
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  fill_pb_configuration_service(&svc_hlp, "test_host", "test_svc",
                                "admin,admin1");
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst, err);
  svc_aply.resolve_object(svc, err);

  host_map const& hm{engine::host::hosts};
  _host3 = hm.begin()->second;
  _host3->set_current_state(engine::host::state_up);
  _host3->set_state_type(checkable::hard);
  _host3->set_acknowledgement(AckType::NONE);
  _host3->set_notify_on(static_cast<uint32_t>(-1));

  service_map const& sm{engine::service::services};
  _svc = sm.begin()->second;
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_state_type(checkable::hard);
  _svc->set_acknowledgement(AckType::NONE);
  _svc->set_notify_on(static_cast<uint32_t>(-1));

  nagios_macros* mac(get_global_macros());

  ASSERT_EQ(_svc->notify(notifier::reason_normal, "test_contact",
                         "test_comment", notifier::notification_option_forced),
            OK);

  std::string out;
  process_macros_r(mac, "$NOTIFICATIONRECIPIENTS:test_host:test_svc$", out, 1);
  ASSERT_TRUE(out == "admin,admin1" || out == "admin1,admin");
}

TEST_F(Macro, PbNotificationAuthor) {
  init_macros();
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "admin", true);
  ct_aply.add_object(ctct);
  configuration::Contact ctct1;
  configuration::contact_helper ctct1_hlp(&ctct1);
  fill_pb_configuration_contact(&ctct1_hlp, "admin1", false, "c,r");
  ct_aply.add_object(ctct1);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);
  ct_aply.resolve_object(ctct1, err);
  configuration::Contact ctct2;
  configuration::contact_helper ctct2_hlp(&ctct2);
  fill_pb_configuration_contact(&ctct2_hlp, "test_contact", false);
  ct_aply.add_object(ctct2);
  ct_aply.expand_objects(pb_indexed_config);
  ct_aply.resolve_object(ctct2, err);

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  fill_pb_configuration_host(&hst_hlp, "test_host", "admin");
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  fill_pb_configuration_service(&svc_hlp, "test_host", "test_svc",
                                "admin,admin1");
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst, err);
  svc_aply.resolve_object(svc, err);

  host_map const& hm{engine::host::hosts};
  _host3 = hm.begin()->second;
  _host3->set_current_state(engine::host::state_up);
  _host3->set_state_type(checkable::hard);
  _host3->set_acknowledgement(AckType::NONE);
  _host3->set_notify_on(static_cast<uint32_t>(-1));

  service_map const& sm{engine::service::services};
  _svc = sm.begin()->second;
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_state_type(checkable::hard);
  _svc->set_acknowledgement(AckType::NONE);
  _svc->set_notify_on(static_cast<uint32_t>(-1));

  nagios_macros* mac(get_global_macros());

  ASSERT_EQ(_svc->notify(notifier::reason_normal, "test_contact",
                         "test_comment", notifier::notification_option_forced),
            OK);

  std::string out;
  process_macros_r(mac, "$NOTIFICATIONAUTHOR$", out, 1);
  ASSERT_EQ(out, "test_contact");
}

TEST_F(Macro, PbNotificationAuthorName) {
  init_macros();
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "admin", true);
  ct_aply.add_object(ctct);
  configuration::Contact ctct1;
  configuration::contact_helper ctct1_hlp(&ctct1);
  fill_pb_configuration_contact(&ctct1_hlp, "admin1", false, "c,r");
  ct_aply.add_object(ctct1);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);
  ct_aply.resolve_object(ctct1, err);
  configuration::Contact ctct2;
  configuration::contact_helper ctct2_hlp(&ctct2);
  fill_pb_configuration_contact(&ctct2_hlp, "test_contact", false);
  ct_aply.add_object(ctct2);
  ct_aply.expand_objects(pb_indexed_config);
  ct_aply.resolve_object(ctct2, err);

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  fill_pb_configuration_host(&hst_hlp, "test_host", "admin");
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  fill_pb_configuration_service(&svc_hlp, "test_host", "test_svc",
                                "admin,admin1");
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst, err);
  svc_aply.resolve_object(svc, err);

  host_map const& hm{engine::host::hosts};
  _host3 = hm.begin()->second;
  _host3->set_current_state(engine::host::state_up);
  _host3->set_state_type(checkable::hard);
  _host3->set_acknowledgement(AckType::NONE);
  _host3->set_notify_on(static_cast<uint32_t>(-1));

  service_map const& sm{engine::service::services};
  _svc = sm.begin()->second;
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_state_type(checkable::hard);
  _svc->set_acknowledgement(AckType::NONE);
  _svc->set_notify_on(static_cast<uint32_t>(-1));

  nagios_macros* mac(get_global_macros());

  ASSERT_EQ(_svc->notify(notifier::reason_normal, "test_contact",
                         "test_comment", notifier::notification_option_forced),
            OK);
  std::string out;
  process_macros_r(mac, "$NOTIFICATIONAUTHORNAME:test_host:test_svc$", out, 1);
  ASSERT_EQ(out, "test_contact");
}

TEST_F(Macro, PbNotificationAuthorAlias) {
  init_macros();
  configuration::applier::contact ct_aply;
  configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
  ct_aply.add_object(ctct);
  configuration::Contact ctct1{
      new_pb_configuration_contact("admin1", false, "c,r")};
  ct_aply.add_object(ctct1);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);
  ct_aply.resolve_object(ctct1, err);
  configuration::Contact ctct2{
      new_pb_configuration_contact("test_contact", false)};
  ct_aply.add_object(ctct2);
  ct_aply.expand_objects(pb_indexed_config);
  ct_aply.resolve_object(ctct2, err);

  configuration::Host hst{new_pb_configuration_host("test_host", "admin")};
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc{
      new_pb_configuration_service("test_host", "test_svc", "admin,admin1")};
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst, err);
  svc_aply.resolve_object(svc, err);

  host_map const& hm{engine::host::hosts};
  _host3 = hm.begin()->second;
  _host3->set_current_state(engine::host::state_up);
  _host3->set_state_type(checkable::hard);
  _host3->set_acknowledgement(AckType::NONE);
  _host3->set_notify_on(static_cast<uint32_t>(-1));

  service_map const& sm{engine::service::services};
  _svc = sm.begin()->second;
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_state_type(checkable::hard);
  _svc->set_acknowledgement(AckType::NONE);
  _svc->set_notify_on(static_cast<uint32_t>(-1));

  nagios_macros* mac(get_global_macros());

  ASSERT_EQ(_svc->notify(notifier::reason_normal, "test_contact",
                         "test_comment", notifier::notification_option_forced),
            OK);
  std::string out;
  process_macros_r(mac, "$NOTIFICATIONAUTHORALIAS:test_host:test_svc$", out, 1);
  ASSERT_EQ(out, "test_contact");
}

TEST_F(Macro, PbNotificationComment) {
  init_macros();
  configuration::applier::contact ct_aply;
  configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
  ct_aply.add_object(ctct);
  configuration::Contact ctct1{
      new_pb_configuration_contact("admin1", false, "c,r")};
  ct_aply.add_object(ctct1);
  ct_aply.expand_objects(pb_indexed_config);
  configuration::error_cnt err;
  ct_aply.resolve_object(ctct, err);
  ct_aply.resolve_object(ctct1, err);
  configuration::Contact ctct2{
      new_pb_configuration_contact("test_contact", false)};
  ct_aply.add_object(ctct2);
  ct_aply.expand_objects(pb_indexed_config);
  ct_aply.resolve_object(ctct2, err);

  configuration::Host hst{new_pb_configuration_host("test_host", "admin")};
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc{
      new_pb_configuration_service("test_host", "test_svc", "admin,admin1")};
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst, err);
  svc_aply.resolve_object(svc, err);

  host_map const& hm{engine::host::hosts};
  _host3 = hm.begin()->second;
  _host3->set_current_state(engine::host::state_up);
  _host3->set_state_type(checkable::hard);
  _host3->set_acknowledgement(AckType::NONE);
  _host3->set_notify_on(static_cast<uint32_t>(-1));

  service_map const& sm{engine::service::services};
  _svc = sm.begin()->second;
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_state_type(checkable::hard);
  _svc->set_acknowledgement(AckType::NONE);
  _svc->set_notify_on(static_cast<uint32_t>(-1));

  nagios_macros* mac(get_global_macros());

  ASSERT_EQ(_svc->notify(notifier::reason_normal, "test_contact",
                         "test_comment", notifier::notification_option_forced),
            OK);

  std::string out;
  process_macros_r(mac, "$NOTIFICATIONCOMMENT$", out, 1);
  ASSERT_EQ(out, "test_comment");
}

TEST_F(Macro, PbIsValidTime) {
  configuration::applier::timeperiod time_aply;
  configuration::Timeperiod time;
  configuration::timeperiod_helper time_hlp(&time);

  time.set_timeperiod_name("test");
  time.set_alias("test");
  time_aply.add_object(time);

  init_macros();
  int now{500000000};
  set_time(now);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$ISVALIDTIME:test$", out, 1);
  ASSERT_EQ(out, "0");
}

TEST_F(Macro, PbNextValidTime) {
  configuration::applier::timeperiod time_aply;
  configuration::Timeperiod time;
  configuration::timeperiod_helper time_hlp(&time);

  time.set_alias("test");
  time.set_timeperiod_name("test");
  time_hlp.hook("monday", "23:00-24:00");
  time_hlp.hook("tuesday", "23:00-24:00");
  time_hlp.hook("wednesday", "23:00-24:00");
  time_hlp.hook("thursday", "23:00-24:00");
  time_hlp.hook("friday", "23:00-24:00");
  time_hlp.hook("saterday", "23:00-24:00");
  time_hlp.hook("sunday", "23:00-24:00");
  time_aply.add_object(time);

  init_macros();
  int now{500000000};
  set_time(now);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$NEXTVALIDTIME:test$", out, 1);
  ASSERT_EQ(out, "23:00:00");
}

TEST_F(Macro, PbContactTimeZone) {
  configuration::applier::contact cnt_aply;
  configuration::Contact cnt;
  cnt.set_contact_name("user");
  cnt.set_email("contact@centreon.com");
  cnt.set_pager("0473729383");
  cnt.set_timezone("time_test");
  cnt_aply.add_object(cnt);

  init_macros();
  int now{500000000};
  set_time(now);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$CONTACTTIMEZONE:user$", out, 1);
  ASSERT_EQ(out, "time_test");
}
