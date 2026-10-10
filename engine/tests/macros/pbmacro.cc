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

#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <gtest/gtest.h>
#include <fstream>
#include "../helper.hh"
#include "../test_engine.hh"
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
// #include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/hostescalation.hh"
#include "com/centreon/engine/macros.hh"
// #include "com/centreon/engine/macros/grab_host.hh"
// #include "com/centreon/engine/macros/grab_service.hh"
// #include "com/centreon/engine/macros/process.hh"
// #include "common/engine_conf/contact_helper.hh"
// #include "common/engine_conf/host_helper.hh"
// #include "common/engine_conf/hostgroup_helper.hh"
// #include "common/engine_conf/message_helper.hh"
#include "common/engine_conf/parser.hh"
// #include "common/engine_conf/service_helper.hh"
// #include "common/engine_conf/servicegroup_helper.hh"
// #include "common/engine_conf/state.pb.h"
// #include "common/engine_conf/timeperiod_helper.hh"
#include "common/tests/timeperiods/utils.hh"
// #include "common/timeperiods/timeperiod.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::common::timeperiods;
namespace notifications = com::centreon::common::notifications;

class Macro : public TestEngine {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override {
    _state_hlp = init_config_state();
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
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "poller_name=poller-test" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$POLLERNAME$", out, 0);
  ASSERT_EQ(out, "poller-test");
}

TEST_F(Macro, PbPollerId) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "poller_id=42" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);
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
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "admin_email=contactadmin@centreon.com" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);

  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$ADMINEMAIL:test_host$", out, 1);
  ASSERT_EQ(out, "contactadmin@centreon.com");
}

TEST_F(Macro, PbAdminPager) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "admin_pager=04737293866" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);

  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$ADMINPAGER:test_host$", out, 1);
  ASSERT_EQ(out, "04737293866");
}

TEST_F(Macro, PbMainConfigFile) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$MAINCONFIGFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/tmp/test-config.cfg");
}

TEST_F(Macro, PbStatusDataFile) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "status_file=/usr/local/var/status.dat" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$STATUSDATAFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/usr/local/var/status.dat");
}

TEST_F(Macro, PbRetentionDataFile) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "state_retention_file=/var/log/centreon-engine/retention.dat"
      << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$RETENTIONDATAFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/var/log/centreon-engine/retention.dat");
}

TEST_F(Macro, PbTempFile) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$TEMPFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/tmp/centengine.tmp");
}

TEST_F(Macro, PbLogFile) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=/tmp/centengine.log" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$LOGFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/tmp/centengine.log");
}

TEST_F(Macro, PbCommandFile) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "command_file=/usr/local/var/rw/centengine.cmd" << std::endl;
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);
  init_macros();

  std::string out;
  nagios_macros* mac(get_global_macros());
  process_macros_r(mac, "$COMMANDFILE:test_host$", out, 1);
  ASSERT_EQ(out, "/usr/local/var/rw/centengine.cmd");
}

TEST_F(Macro, PbTempPath) {
  configuration::parser parser;
  auto st = std::make_unique<configuration::State>();
  configuration::state_helper st_hlp(st.get());

  std::remove("/tmp/test-config.cfg");

  std::ofstream ofs("/tmp/test-config.cfg");
  ofs << "log_file=" << std::endl;
  ofs.close();

  configuration::error_cnt err;
  parser.parse("/tmp/test-config.cfg", st.get(), err);
  st_hlp.expand(err);
  configuration::applier::state::instance().apply(*st, err);
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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  _state_hlp->expand(err);
  cg_aply.resolve_object(cg);

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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  _state_hlp->expand(err);
  cg_aply.resolve_object(cg);
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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  _state_hlp->expand(err);
  cg_aply.resolve_object(cg);
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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "test_cg", "test_contact");
  cg_aply.add_object(cg);
  _state_hlp->expand(err);
  cg_aply.resolve_object(cg);
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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);
  ct_aply.resolve_object(ctct1);
  configuration::Contact ctct2;
  configuration::contact_helper ctct2_hlp(&ctct2);
  fill_pb_configuration_contact(&ctct2_hlp, "test_contact", false);
  ct_aply.add_object(ctct2);
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct2);

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

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

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

  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);
  ct_aply.resolve_object(ctct1);
  configuration::Contact ctct2;
  configuration::contact_helper ctct2_hlp(&ctct2);
  fill_pb_configuration_contact(&ctct2_hlp, "test_contact", false);
  ct_aply.add_object(ctct2);
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct2);

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

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

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

  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);
  ct_aply.resolve_object(ctct1);
  configuration::Contact ctct2;
  configuration::contact_helper ctct2_hlp(&ctct2);
  fill_pb_configuration_contact(&ctct2_hlp, "test_contact", false);
  ct_aply.add_object(ctct2);
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct2);

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

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

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

  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);
  ct_aply.resolve_object(ctct1);
  configuration::Contact ctct2{
      new_pb_configuration_contact("test_contact", false)};
  ct_aply.add_object(ctct2);
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct2);

  configuration::Host hst{new_pb_configuration_host("test_host", "admin")};
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc{
      new_pb_configuration_service("test_host", "test_svc", "admin,admin1")};
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

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

  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
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
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);
  ct_aply.resolve_object(ctct1);
  configuration::Contact ctct2{
      new_pb_configuration_contact("test_contact", false)};
  ct_aply.add_object(ctct2);
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct2);

  configuration::Host hst{new_pb_configuration_host("test_host", "admin")};
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc{
      new_pb_configuration_service("test_host", "test_svc", "admin,admin1")};
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

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

  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
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

/* Custom variable macros on a named object ($_HOSTFOO:h1$) or on a group
 * ($_HOSTFOO:hg:,$): two hosts h1/h2, each with a service s, two contacts
 * c1/c2, and a host, a service and a contact group holding them all. */
class MacroCustomOnDemand : public Macro {
 public:
  void SetUp() override {
    Macro::SetUp();
    configuration::error_cnt err;

    configuration::applier::contact ct_aply;
    configuration::Contact c1, c2;
    configuration::contact_helper c1_hlp(&c1), c2_hlp(&c2);
    fill_pb_configuration_contact(&c1_hlp, "c1", true);
    fill_pb_configuration_contact(&c2_hlp, "c2", false);
    c1_hlp.insert_customvariable("_BAZ", "baz-c1");
    c2_hlp.insert_customvariable("_BAZ", "baz-c2");
    ct_aply.add_object(c1);
    ct_aply.add_object(c2);

    configuration::applier::contactgroup cg_aply;
    configuration::Contactgroup cg;
    configuration::contactgroup_helper cg_hlp(&cg);
    fill_pb_configuration_contactgroup(&cg_hlp, "cg", "c1,c2");
    cg_aply.add_object(cg);

    configuration::applier::host hst_aply;
    configuration::Host h1, h2;
    configuration::host_helper h1_hlp(&h1), h2_hlp(&h2);
    fill_pb_configuration_host(&h1_hlp, "h1", "c1", 1);
    fill_pb_configuration_host(&h2_hlp, "h2", "c1", 2);
    h1_hlp.insert_customvariable("_FOO", "foo-h1");
    h2_hlp.insert_customvariable("_FOO", "foo-h2");
    hst_aply.add_object(h1);
    hst_aply.add_object(h2);

    configuration::applier::hostgroup hg_aply;
    configuration::Hostgroup hg;
    configuration::hostgroup_helper hg_hlp(&hg);
    hg.set_hostgroup_name("hg");
    hg_hlp.hook("members", "h1,h2");
    hg_aply.add_object(hg);

    configuration::applier::service svc_aply;
    configuration::Service s1, s2;
    configuration::service_helper s1_hlp(&s1), s2_hlp(&s2);
    fill_pb_configuration_service(&s1_hlp, "h1", "s", "c1", 1);
    fill_pb_configuration_service(&s2_hlp, "h2", "s", "c1", 2);
    s1_hlp.insert_customvariable("_BAR", "bar-h1");
    s2_hlp.insert_customvariable("_BAR", "bar-h2");
    svc_aply.add_object(s1);
    svc_aply.add_object(s2);

    configuration::applier::servicegroup sg_aply;
    configuration::Servicegroup sg;
    configuration::servicegroup_helper sg_hlp(&sg);
    sg.set_servicegroup_name("sg");
    fill_pair_string_group(sg.mutable_members(), "h1,s,h2,s");
    sg_aply.add_object(sg);

    _state_hlp->expand(err);
    ct_aply.resolve_object(c1);
    ct_aply.resolve_object(c2);
    cg_aply.resolve_object(cg);
    hst_aply.resolve_object(h1);
    hst_aply.resolve_object(h2);
    hg_aply.resolve_object(hg);
    svc_aply.resolve_object(s1);
    svc_aply.resolve_object(s2);
    sg_aply.resolve_object(sg);

    init_macros();
  }

  /* Evaluate a line; with a context host h1, its service s and contact c1. */
  std::string eval(const std::string& line, bool with_context = false) {
    nagios_macros* mac(get_global_macros());
    clear_volatile_macros_r(mac);
    if (with_context) {
      grab_host_macros_r(mac, engine::host::hosts.find("h1")->second.get());
      grab_service_macros_r(
          mac, engine::service::services.find({"h1", "s"})->second.get());
      grab_contact_macros_r(mac,
                            engine::contact::contacts.find("c1")->second.get());
    }
    std::string out;
    process_macros_r(mac, line, out, 0);
    clear_volatile_macros_r(mac);
    return out;
  }

  /* The members of a group are concatenated in no particular order. */
  static std::vector<std::string_view> sorted(const std::string& s) {
    std::vector<std::string_view> retval = absl::StrSplit(s, ',');
    std::sort(retval.begin(), retval.end());
    return retval;
  }
};

TEST_F(MacroCustomOnDemand, Host) {
  ASSERT_EQ(eval("$_HOSTFOO$", true), "foo-h1");
  ASSERT_EQ(eval("$_HOSTFOO:h2$"), "foo-h2");
  ASSERT_EQ(eval("$_HOSTFOO:unknown$"), "");
  std::string out = eval("$_HOSTFOO:hg:,$");
  ASSERT_EQ(sorted(out), (std::vector<std::string_view>{"foo-h1", "foo-h2"}));
}

TEST_F(MacroCustomOnDemand, Service) {
  ASSERT_EQ(eval("$_SERVICEBAR$", true), "bar-h1");
  /* The host is given: it is not the context host. */
  ASSERT_EQ(eval("$_SERVICEBAR:h2:s$", true), "bar-h2");
  ASSERT_EQ(eval("$_SERVICEBAR:h2:s$"), "bar-h2");
  /* No host given: the context host. */
  ASSERT_EQ(eval("$_SERVICEBAR::s$", true), "bar-h1");
  ASSERT_EQ(eval("$_SERVICEBAR::s$"), "");
  /* A service group with a separator, its members are on both hosts. */
  std::string out = eval("$_SERVICEBAR:sg:,$", true);
  ASSERT_EQ(sorted(out), (std::vector<std::string_view>{"bar-h1", "bar-h2"}));
}

TEST_F(MacroCustomOnDemand, Contact) {
  ASSERT_EQ(eval("$_CONTACTBAZ$", true), "baz-c1");
  ASSERT_EQ(eval("$_CONTACTBAZ:c2$"), "baz-c2");
  std::string out = eval("$_CONTACTBAZ:cg:,$");
  ASSERT_EQ(sorted(out), (std::vector<std::string_view>{"baz-c1", "baz-c2"}));
}

/* The $TOTALHOSTS*$ summary macros: a DOWN or UNREACHABLE host is unhandled
 * unless it is acknowledged, in downtime or with active checks disabled.
 * Hosts: one UP, two DOWN (one acknowledged), four UNREACHABLE (two unhandled,
 * one in downtime, one with active checks disabled). */
TEST_F(Macro, PbTotalHostsUnhandled) {
  configuration::applier::host hst_aply;
  const char* names[] = {"up",   "down",   "down_ack", "unr1",
                         "unr2", "unr_dt", "unr_nochk"};
  std::vector<configuration::Host> hosts(std::size(names));
  for (size_t i = 0; i < hosts.size(); ++i) {
    configuration::host_helper hlp(&hosts[i]);
    fill_pb_configuration_host(&hlp, names[i], "", i + 1);
    hst_aply.add_object(hosts[i]);
  }
  configuration::error_cnt err;
  _state_hlp->expand(err);
  for (auto& h : hosts)
    hst_aply.resolve_object(h);

  auto set = [](const char* name, engine::host::host_state st) {
    auto& h = engine::host::hosts.find(name)->second;
    h->set_current_state(st);
    h->set_has_been_checked(true);
    h->set_acknowledgement(AckType::NONE);
    return h;
  };
  set("up", engine::host::state_up);
  set("down", engine::host::state_down);
  set("down_ack", engine::host::state_down)
      ->set_acknowledgement(AckType::NORMAL);
  set("unr1", engine::host::state_unreachable);
  set("unr2", engine::host::state_unreachable);
  set("unr_dt", engine::host::state_unreachable)
      ->set_scheduled_downtime_depth(1);
  set("unr_nochk", engine::host::state_unreachable)->set_checks_enabled(false);

  init_macros();
  nagios_macros* mac(get_global_macros());
  std::string out;
  process_macros_r(mac,
                   "$TOTALHOSTSUP$ $TOTALHOSTSDOWN$ $TOTALHOSTSDOWNUNHANDLED$ "
                   "$TOTALHOSTSUNREACHABLE$ $TOTALHOSTSUNREACHABLEUNHANDLED$ "
                   "$TOTALHOSTPROBLEMS$ $TOTALHOSTPROBLEMSUNHANDLED$",
                   out, 0);
  ASSERT_EQ(out, "1 2 1 4 2 6 3");
}

/* $CONTACTADDRESSn$ with n greater than the number of addresses of the
 * contact (but not than MAX_CONTACT_ADDRESSES) used to read out of the
 * addresses vector: c1 has two addresses, c2 none. */
TEST_F(Macro, PbContactAddressOutOfRange) {
  configuration::applier::contact ct_aply;
  configuration::Contact c1, c2;
  configuration::contact_helper c1_hlp(&c1), c2_hlp(&c2);
  fill_pb_configuration_contact(&c1_hlp, "c1", true);
  fill_pb_configuration_contact(&c2_hlp, "c2", false);
  c1.add_address("address-one");
  c1.add_address("address-two");
  ct_aply.add_object(c1);
  ct_aply.add_object(c2);
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(c1);
  ct_aply.resolve_object(c2);

  init_macros();
  nagios_macros* mac(get_global_macros());
  std::string out;

  grab_contact_macros_r(mac,
                        engine::contact::contacts.find("c1")->second.get());
  process_macros_r(mac,
                   "[$CONTACTADDRESS1$] [$CONTACTADDRESS2$] "
                   "[$CONTACTADDRESS3$] [$CONTACTADDRESS6$]",
                   out, 0);
  ASSERT_EQ(out, "[address-one] [address-two] [] []");
  clear_volatile_macros_r(mac);

  grab_contact_macros_r(mac,
                        engine::contact::contacts.find("c2")->second.get());
  process_macros_r(mac, "[$CONTACTADDRESS1$]", out, 0);
  ASSERT_EQ(out, "[]");
  clear_volatile_macros_r(mac);

  process_macros_r(mac, "[$CONTACTADDRESS2:c1$] [$CONTACTADDRESS3:c1$]", out,
                   0);
  ASSERT_EQ(out, "[address-two] []");
  process_macros_r(mac, "[$CONTACTADDRESS1:c2$]", out, 0);
  ASSERT_EQ(out, "[]");
}

/* $SERVICEGROUPMEMBERS$ used to overwrite its buffer with each member instead
 * of appending it: only the last member was left, behind a comma. */
TEST_F(Macro, PbServicegroupMembers) {
  configuration::applier::host hst_aply;
  configuration::applier::service svc_aply;
  configuration::Host h1, h2;
  configuration::host_helper h1_hlp(&h1), h2_hlp(&h2);
  fill_pb_configuration_host(&h1_hlp, "h1", "", 1);
  fill_pb_configuration_host(&h2_hlp, "h2", "", 2);
  hst_aply.add_object(h1);
  hst_aply.add_object(h2);
  configuration::Service s1, s2, s3;
  configuration::service_helper s1_hlp(&s1), s2_hlp(&s2), s3_hlp(&s3);
  fill_pb_configuration_service(&s1_hlp, "h1", "s", "", 1);
  fill_pb_configuration_service(&s2_hlp, "h2", "s", "", 2);
  fill_pb_configuration_service(&s3_hlp, "h2", "t", "", 3);
  svc_aply.add_object(s1);
  svc_aply.add_object(s2);
  svc_aply.add_object(s3);

  configuration::applier::servicegroup sg_aply;
  configuration::Servicegroup sg;
  configuration::servicegroup_helper sg_hlp(&sg);
  sg.set_servicegroup_name("sg");
  fill_pair_string_group(sg.mutable_members(), "h1,s,h2,s,h2,t");
  sg_aply.add_object(sg);

  configuration::error_cnt err;
  _state_hlp->expand(err);
  hst_aply.resolve_object(h1);
  hst_aply.resolve_object(h2);
  svc_aply.resolve_object(s1);
  svc_aply.resolve_object(s2);
  svc_aply.resolve_object(s3);
  sg_aply.resolve_object(sg);

  init_macros();
  nagios_macros* mac(get_global_macros());
  std::string out;
  process_macros_r(mac, "$SERVICEGROUPMEMBERS:sg$", out, 0);

  /* The members come in no particular order: compare the sorted pairs. */
  std::vector<std::string_view> items = absl::StrSplit(out, ',');
  ASSERT_EQ(items.size(), 6u) << out;
  std::vector<std::string> pairs;
  for (size_t i = 0; i < items.size(); i += 2)
    pairs.push_back(absl::StrCat(items[i], ",", items[i + 1]));
  std::sort(pairs.begin(), pairs.end());
  ASSERT_EQ(absl::StrJoin(pairs, ";"), "h1,s;h2,s;h2,t");
}

/* The $TOTALHOSTSERVICES*$ macros used to be cached for any host: once
 * computed for an on-demand host, they were returned for the context host and
 * for every member of a host group. h1 (context) has one critical service, h2
 * two warning services. */
TEST_F(Macro, PbTotalHostServicesOnDemand) {
  configuration::applier::host hst_aply;
  configuration::applier::service svc_aply;
  configuration::Host h1, h2;
  configuration::host_helper h1_hlp(&h1), h2_hlp(&h2);
  fill_pb_configuration_host(&h1_hlp, "h1", "", 1);
  fill_pb_configuration_host(&h2_hlp, "h2", "", 2);
  hst_aply.add_object(h1);
  hst_aply.add_object(h2);
  configuration::Service s1, s2, s3;
  configuration::service_helper s1_hlp(&s1), s2_hlp(&s2), s3_hlp(&s3);
  fill_pb_configuration_service(&s1_hlp, "h1", "s", "", 1);
  fill_pb_configuration_service(&s2_hlp, "h2", "s", "", 2);
  fill_pb_configuration_service(&s3_hlp, "h2", "t", "", 3);
  svc_aply.add_object(s1);
  svc_aply.add_object(s2);
  svc_aply.add_object(s3);

  configuration::applier::hostgroup hg_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  hg.set_hostgroup_name("hg");
  hg_hlp.hook("members", "h1,h2");
  hg_aply.add_object(hg);

  configuration::error_cnt err;
  _state_hlp->expand(err);
  hst_aply.resolve_object(h1);
  hst_aply.resolve_object(h2);
  svc_aply.resolve_object(s1);
  svc_aply.resolve_object(s2);
  svc_aply.resolve_object(s3);
  hg_aply.resolve_object(hg);

  engine::service::services.find({"h1", "s"})
      ->second->set_current_state(engine::service::state_critical);
  engine::service::services.find({"h2", "s"})
      ->second->set_current_state(engine::service::state_warning);
  engine::service::services.find({"h2", "t"})
      ->second->set_current_state(engine::service::state_warning);

  init_macros();
  nagios_macros* mac(get_global_macros());
  std::string out;
  grab_host_macros_r(mac, engine::host::hosts.find("h1")->second.get());
  process_macros_r(mac,
                   "$TOTALHOSTSERVICESWARNING:h2$ $TOTALHOSTSERVICESWARNING$ "
                   "$TOTALHOSTSERVICESWARNING:h2$",
                   out, 0);
  ASSERT_EQ(out, "2 0 2");
  process_macros_r(mac,
                   "$TOTALHOSTSERVICESWARNING$ $TOTALHOSTSERVICESWARNING:h2$ "
                   "$TOTALHOSTSERVICESCRITICAL$",
                   out, 0);
  ASSERT_EQ(out, "0 2 1");
  clear_volatile_macros_r(mac);

  /* One value per member, in no particular order. */
  process_macros_r(mac, "$TOTALHOSTSERVICESWARNING:hg:,$", out, 0);
  std::vector<std::string_view> values = absl::StrSplit(out, ',');
  std::sort(values.begin(), values.end());
  ASSERT_EQ(absl::StrJoin(values, ","), "0,2");
}

/* Some macros are always stripped of the illegal output characters, whatever
 * the cleaning options given by the caller: outputs, perfdata, notification
 * authors and comments. The ranges of ids used to select them came from the
 * Nagios numbering: they missed the notification authors and comments, and
 * cleaned the notes and URLs of the host groups and of the service groups. */
TEST_F(Macro, PbForcedCleaning) {
  pb_indexed_config.mut_state().set_illegal_output_chars("`~$&|'\"<>");

  configuration::applier::host hst_aply;
  configuration::applier::service svc_aply;
  configuration::Host h1;
  configuration::host_helper h1_hlp(&h1);
  fill_pb_configuration_host(&h1_hlp, "h1", "", 1);
  hst_aply.add_object(h1);
  configuration::Service s1;
  configuration::service_helper s1_hlp(&s1);
  fill_pb_configuration_service(&s1_hlp, "h1", "s", "", 1);
  svc_aply.add_object(s1);

  configuration::applier::hostgroup hg_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  hg.set_hostgroup_name("hg");
  hg.set_notes("notes & <co>");
  hg.set_notes_url("http://example.com/hg-notes?a=1&b=2");
  hg.set_action_url("http://example.com/hg-action?a=1&b=2");
  hg_hlp.hook("members", "h1");
  hg_aply.add_object(hg);

  configuration::applier::servicegroup sg_aply;
  configuration::Servicegroup sg;
  configuration::servicegroup_helper sg_hlp(&sg);
  sg.set_servicegroup_name("sg");
  sg.set_notes("notes & <co>");
  sg.set_notes_url("http://example.com/sg-notes?a=1&b=2");
  fill_pair_string_group(sg.mutable_members(), "h1,s");
  sg_aply.add_object(sg);

  configuration::error_cnt err;
  _state_hlp->expand(err);
  hst_aply.resolve_object(h1);
  svc_aply.resolve_object(s1);
  hg_aply.resolve_object(hg);
  sg_aply.resolve_object(sg);

  init_macros();
  nagios_macros* mac(get_global_macros());
  mac->x[MACRO_NOTIFICATIONAUTHOR] = "ad`min";
  mac->x[MACRO_NOTIFICATIONAUTHORNAME] = "Admin|istrator";
  mac->x[MACRO_NOTIFICATIONAUTHORALIAS] = "admin <alias>";
  mac->x[MACRO_NOTIFICATIONCOMMENT] = "it's a \"comment\" & more";
  std::string out;

  /* The notes and URLs of the groups are given as they are. */
  process_macros_r(mac,
                   "[$HOSTGROUPNOTES:hg$] [$HOSTGROUPNOTESURL:hg$] "
                   "[$HOSTGROUPACTIONURL:hg$] [$SERVICEGROUPNOTES:sg$] "
                   "[$SERVICEGROUPNOTESURL:sg$]",
                   out, 0);
  ASSERT_EQ(out,
            "[notes & <co>] [http://example.com/hg-notes?a=1&b=2] "
            "[http://example.com/hg-action?a=1&b=2] [notes & <co>] "
            "[http://example.com/sg-notes?a=1&b=2]");

  /* The notification authors and comment are cleaned. */
  process_macros_r(mac,
                   "[$NOTIFICATIONAUTHOR$] [$NOTIFICATIONAUTHORNAME$] "
                   "[$NOTIFICATIONAUTHORALIAS$] [$NOTIFICATIONCOMMENT$]",
                   out, 0);
  ASSERT_EQ(out, "[admin] [Administrator] [admin alias] [its a comment  more]");

  mac->x[MACRO_NOTIFICATIONAUTHOR].clear();
  mac->x[MACRO_NOTIFICATIONAUTHORNAME].clear();
  mac->x[MACRO_NOTIFICATIONAUTHORALIAS].clear();
  mac->x[MACRO_NOTIFICATIONCOMMENT].clear();
}
