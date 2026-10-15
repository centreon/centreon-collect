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
#include "com/centreon/engine/notification_execution.hh"
#include "engine/macros/process.hh"
// #include "engine/macros/process.hh"
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
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$POLLERNAME$", 0);
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
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$POLLERID$", 0);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$LONGDATETIME:test_host$", 0);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$SHORTDATETIME:test_host$", 0);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$DATE:test_host$", 0);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$TIME:test_host$", 0);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$TIMET:test_host$", 0);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTNAME:user$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTALIAS:user$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTEMAIL:user$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTPAGER:user$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$ADMINEMAIL:test_host$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$ADMINPAGER:test_host$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$MAINCONFIGFILE:test_host$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$STATUSDATAFILE:test_host$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$RETENTIONDATAFILE:test_host$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$TEMPFILE:test_host$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$LOGFILE:test_host$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$COMMANDFILE:test_host$", 1);
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

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$TEMPPATH$", 0);
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

  int now{500000000};
  set_time(now);

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTGROUPNAME:test_contact$", 1);
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
  int now{500000000};
  set_time(now);

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTGROUPALIAS:test_cg$", 1);
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
  int now{500000000};
  set_time(now);

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTGROUPMEMBERS:test_cg$", 1);
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
  int now{500000000};
  set_time(now);

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTGROUPNAMES:test_contact$", 1);
  ASSERT_EQ(out, "test_cg");
}

TEST_F(Macro, PbNotificationRecipients) {
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

  /* Every contact sees all the recipients: the macro used to be filled
   * after each contact, so the first one saw none, the second one the first
   * one... */
  for (auto& [name, ctc] : engine::contact::contacts)
    for (auto& cmd : ctc->get_service_notification_commands())
      cmd->set_command_line("echo $NOTIFICATIONRECIPIENTS$");
  notification_commands_capture capture;
  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
      OK);

  ASSERT_EQ(capture.commands().size(), 2u);
  for (const std::string& cmd : capture.commands())
    ASSERT_TRUE(cmd == "echo admin,admin1" || cmd == "echo admin1,admin")
        << cmd;
  ASSERT_EQ(capture.commands()[0], capture.commands()[1]);
}

TEST_F(Macro, PbNotificationAuthor) {
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

  for (auto& [name, ctc] : engine::contact::contacts)
    for (auto& cmd : ctc->get_service_notification_commands())
      cmd->set_command_line("echo $NOTIFICATIONAUTHOR$");
  notification_commands_capture capture;
  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
      OK);

  ASSERT_FALSE(capture.commands().empty());
  for (const std::string& cmd : capture.commands())
    ASSERT_EQ(cmd, "echo test_contact");
}

TEST_F(Macro, PbNotificationAuthorName) {
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

  for (auto& [name, ctc] : engine::contact::contacts)
    for (auto& cmd : ctc->get_service_notification_commands())
      cmd->set_command_line("echo $NOTIFICATIONAUTHORNAME$");
  notification_commands_capture capture;
  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
      OK);

  ASSERT_FALSE(capture.commands().empty());
  for (const std::string& cmd : capture.commands())
    ASSERT_EQ(cmd, "echo test_contact");
}

TEST_F(Macro, PbNotificationAuthorAlias) {
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

  for (auto& [name, ctc] : engine::contact::contacts)
    for (auto& cmd : ctc->get_service_notification_commands())
      cmd->set_command_line("echo $NOTIFICATIONAUTHORALIAS$");
  notification_commands_capture capture;
  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
      OK);

  ASSERT_FALSE(capture.commands().empty());
  for (const std::string& cmd : capture.commands())
    ASSERT_EQ(cmd, "echo test_contact");
}

TEST_F(Macro, PbNotificationComment) {
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

  for (auto& [name, ctc] : engine::contact::contacts)
    for (auto& cmd : ctc->get_service_notification_commands())
      cmd->set_command_line("echo $NOTIFICATIONCOMMENT$");
  notification_commands_capture capture;
  ASSERT_EQ(
      _svc->notify(notifications::reason_normal, "test_contact", "test_comment",
                   notifications::notification_option_forced),
      OK);

  ASSERT_FALSE(capture.commands().empty());
  for (const std::string& cmd : capture.commands())
    ASSERT_EQ(cmd, "echo test_comment");
}

/* In notification_mode=broker, Broker decides the notification and gives its
 * id and its number: the service does not know them, its macros used to show
 * the ones it knew. The host macros are about the host, not the
 * notification. */
TEST_F(Macro, PbNotificationIdAndNumberFromBroker) {
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "admin", true);
  ct_aply.add_object(ctct);
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  fill_pb_configuration_host(&hst_hlp, "test_host", "admin");
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  fill_pb_configuration_service(&svc_hlp, "test_host", "test_svc", "admin");
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

  _svc = engine::service::services.begin()->second;

  for (auto& [name, ctc] : engine::contact::contacts)
    for (auto& cmd : ctc->get_service_notification_commands())
      cmd->set_command_line(
          "echo $SERVICENOTIFICATIONID$ $SERVICENOTIFICATIONNUMBER$ "
          "$NOTIFICATIONNUMBER$ $HOSTNOTIFICATIONID$ "
          "$HOSTNOTIFICATIONNUMBER$");
  ::google::protobuf::RepeatedPtrField<std::string> contacts;
  *contacts.Add() = "admin";
  notification_commands_capture capture;
  execute_broker_notification(
      _svc->host_id(), _svc->service_id(), notifications::reason_normal, 4242,
      7, false, "", "", notifications::notification_option_none, contacts);

  ASSERT_EQ(capture.commands().size(), 1u);
  ASSERT_EQ(capture.commands()[0],
            fmt::format("echo 4242 7 7 {} {}",
                        _svc->get_host_ptr()->get_current_notification_id(),
                        _svc->get_host_ptr()->get_notification_number()));
}

/* The user macros of the resource file are read in the configuration: a
 * reload changing or removing $USER5$ is taken into account at once. Before,
 * a removed $USERn$ kept its old value until Engine restarted. */
TEST_F(Macro, PbUserMacroFollowsReload) {
  auto& users = *pb_indexed_config.mut_state().mutable_users();
  macro_context mac;

  users["USER5"] = "/first";
  ASSERT_EQ(macros::process_macros(mac, "$USER5$", 0), "/first");
  users["USER5"] = "/second";
  ASSERT_EQ(macros::process_macros(mac, "$USER5$", 0), "/second");
  users.erase("USER5");
  ASSERT_EQ(macros::process_macros(mac, "$USER5$", 0), "");
}

/* $USERn$ is a user macro like the other ones: no limit on n, and the name
 * is taken as it is. */
TEST_F(Macro, PbUserMacroAnyName) {
  auto& users = *pb_indexed_config.mut_state().mutable_users();
  macro_context mac;

  users["USER300"] = "three hundred";
  users["USER3"] = "three";
  users["CENTREONPLUGINS"] = "/usr/lib/centreon/plugins";
  ASSERT_EQ(macros::process_macros(mac, "$USER300$", 0), "three hundred");
  ASSERT_EQ(macros::process_macros(mac, "$USER3$", 0), "three");
  ASSERT_EQ(macros::process_macros(mac, "$USER03$", 0), "");
  ASSERT_EQ(macros::process_macros(mac, "$CENTREONPLUGINS$", 0),
            "/usr/lib/centreon/plugins");
  /* A user macro takes no argument. */
  ASSERT_EQ(macros::process_macros(mac, "$USER3:h1$", 0), "");
  users.clear();
}

/* A command specification is "name!arg1!arg2": the name is skipped, '\'
 * escapes the next character, at most MAX_COMMAND_ARGUMENTS are kept. */
TEST_F(Macro, PbSplitArguments) {
  using args = std::vector<std::string>;
  ASSERT_EQ(macros::split_arguments(""), args{});
  ASSERT_EQ(macros::split_arguments("check_foo"), args{});
  ASSERT_EQ(macros::split_arguments("check_foo!"), args{""});
  ASSERT_EQ(macros::split_arguments("check_foo!a!b"), (args{"a", "b"}));
  ASSERT_EQ(macros::split_arguments("check_foo!a!!b!"),
            (args{"a", "", "b", ""}));
  ASSERT_EQ(macros::split_arguments(R"(check_foo!a\!b!c\\d)"),
            (args{"a!b", R"(c\d)"}));
  ASSERT_EQ(macros::split_arguments(R"(check_foo!end\)"), args{"end"});

  std::string spec("check_foo");
  for (int i = 0; i < MAX_COMMAND_ARGUMENTS + 5; ++i)
    spec += fmt::format("!{}", i);
  const args all = macros::split_arguments(spec);
  ASSERT_EQ(all.size(), static_cast<size_t>(MAX_COMMAND_ARGUMENTS));
  ASSERT_EQ(all.back(), fmt::format("{}", MAX_COMMAND_ARGUMENTS - 1));
}

/* The arguments have their macros processed, in order: an argument may use
 * the previous ones. The old arguments are cleared. */
TEST_F(Macro, PbSetArguments) {
  macro_context mac;
  mac.argv[5] = "old";
  mac.set_arguments("check_foo!$POLLERNAME$![$ARG1$]", 0);
  ASSERT_EQ(mac.argv[0], pb_indexed_config.state().poller_name());
  ASSERT_EQ(mac.argv[1], fmt::format("[{}]", mac.argv[0]));
  ASSERT_EQ(mac.argv[5], "");
}

/* The notification commands of a contact are configured by their name only,
 * they have no arguments. Their command line used to be split on '!' as if it
 * was "name!arg1": "echo [$ARG1$]!bang" gave "echo [bang]!bang". */
TEST_F(Macro, PbNotificationHasNoArguments) {
  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  fill_pb_configuration_contact(&ctct_hlp, "admin", true);
  ct_aply.add_object(ctct);
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(ctct);

  configuration::Host hst;
  configuration::host_helper hst_hlp(&hst);
  fill_pb_configuration_host(&hst_hlp, "test_host", "admin");
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc;
  configuration::service_helper svc_hlp(&svc);
  fill_pb_configuration_service(&svc_hlp, "test_host", "test_svc", "admin");
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);

  _svc = engine::service::services.begin()->second;

  for (auto& [name, ctc] : engine::contact::contacts)
    for (auto& cmd : ctc->get_service_notification_commands())
      cmd->set_command_line("echo [$ARG1$]!bang");
  ::google::protobuf::RepeatedPtrField<std::string> contacts;
  *contacts.Add() = "admin";
  notification_commands_capture capture;
  execute_broker_notification(
      _svc->host_id(), _svc->service_id(), notifications::reason_normal, 1, 1,
      false, "", "", notifications::notification_option_none, contacts);

  ASSERT_EQ(capture.commands().size(), 1u);
  ASSERT_EQ(capture.commands()[0], "echo []!bang");
}

TEST_F(Macro, PbIsValidTime) {
  configuration::applier::timeperiod time_aply;
  configuration::Timeperiod time;
  configuration::timeperiod_helper time_hlp(&time);

  time.set_timeperiod_name("test");
  time.set_alias("test");
  time_aply.add_object(time);

  int now{500000000};
  set_time(now);

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$ISVALIDTIME:test$", 1);
  ASSERT_EQ(out, "0");
}

/* $ISVALIDTIME$ is 1 when the instant is in the period, 0 otherwise. */
TEST_F(Macro, PbIsValidTimeRestricted) {
  configuration::applier::timeperiod time_aply;
  configuration::Timeperiod time;
  configuration::timeperiod_helper time_hlp(&time);

  time.set_timeperiod_name("workhours");
  time.set_alias("workhours");
  for (const char* day :
       {"monday", "tuesday", "wednesday", "thursday", "friday"})
    time_hlp.hook(day, "09:00-18:00");
  time_aply.add_object(time);

  /* Thursday 2016-11-24 08:00:00 (Europe/Paris). */
  const time_t now = 1479970800;
  set_time(now);

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$ISVALIDTIME:workhours$", 1);
  ASSERT_EQ(out, "0");
  out = macros::process_macros(
      *mac, absl::StrCat("$ISVALIDTIME:workhours:", now + 7200, "$"), 1);
  ASSERT_EQ(out, "1");
  out = macros::process_macros(
      *mac, absl::StrCat("$ISVALIDTIME:workhours:", now + 43200, "$"), 1);
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

  int now{500000000};
  set_time(now);

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$NEXTVALIDTIME:test$", 1);
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

  int now{500000000};
  set_time(now);

  std::string out;
  macro_context local_macros;
  macro_context* mac(&local_macros);
  out = macros::process_macros(*mac, "$CONTACTTIMEZONE:user$", 1);
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
  }

  /* Evaluate a line; with a context host h1, its service s and contact c1. */
  std::string eval(const std::string& line, bool with_context = false) {
    macro_context local_macros;
    macro_context* mac(&local_macros);
    if (with_context) {
      mac->set_host(engine::host::hosts.find("h1")->second.get());
      mac->set_service(
          engine::service::services.find({"h1", "s"})->second.get());
      mac->set_contact(engine::contact::contacts.find("c1")->second.get());
    }
    std::string out;
    out = macros::process_macros(*mac, line, 0);
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
  macro_context local_macros;
  macro_context* mac(&local_macros);
  std::string out;
  out = macros::process_macros(
      *mac,
      "$TOTALHOSTSUP$ $TOTALHOSTSDOWN$ $TOTALHOSTSDOWNUNHANDLED$ "
      "$TOTALHOSTSUNREACHABLE$ $TOTALHOSTSUNREACHABLEUNHANDLED$ "
      "$TOTALHOSTPROBLEMS$ $TOTALHOSTPROBLEMSUNHANDLED$",
      0);
  ASSERT_EQ(out, "1 2 1 4 2 6 3");
}

/* The $TOTAL*$ summary macros only count the objects the contact of the
 * context is notified for, directly or by a contactgroup. The contactgroup
 * test was inverted: an object was counted for the contacts that were not
 * members of its contactgroups. h1 notifies c2, h2 and the critical service
 * h1/s notify the contactgroup cg, whose only member is c1. */
TEST_F(Macro, PbTotalsFilteredByContactgroup) {
  configuration::applier::contact ct_aply;
  configuration::Contact c1, c2, c3;
  configuration::contact_helper c1_hlp(&c1), c2_hlp(&c2), c3_hlp(&c3);
  fill_pb_configuration_contact(&c1_hlp, "c1", true);
  fill_pb_configuration_contact(&c2_hlp, "c2", false);
  fill_pb_configuration_contact(&c3_hlp, "c3", false);
  ct_aply.add_object(c1);
  ct_aply.add_object(c2);
  ct_aply.add_object(c3);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "cg", "c1");
  cg_aply.add_object(cg);

  configuration::applier::host hst_aply;
  configuration::Host h1, h2;
  configuration::host_helper h1_hlp(&h1), h2_hlp(&h2);
  fill_pb_configuration_host(&h1_hlp, "h1", "c2", 1);
  fill_pb_configuration_host(&h2_hlp, "h2", "", 2);
  fill_string_group(h2.mutable_contactgroups(), "cg");
  hst_aply.add_object(h1);
  hst_aply.add_object(h2);

  configuration::applier::service svc_aply;
  configuration::Service s1;
  configuration::service_helper s1_hlp(&s1);
  fill_pb_configuration_service(&s1_hlp, "h1", "s", "", 1);
  fill_string_group(s1.mutable_contactgroups(), "cg");
  svc_aply.add_object(s1);

  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(c1);
  ct_aply.resolve_object(c2);
  ct_aply.resolve_object(c3);
  cg_aply.resolve_object(cg);
  hst_aply.resolve_object(h1);
  hst_aply.resolve_object(h2);
  svc_aply.resolve_object(s1);

  for (const char* name : {"h1", "h2"}) {
    auto& h = engine::host::hosts.find(name)->second;
    h->set_current_state(engine::host::state_up);
    h->set_has_been_checked(true);
  }
  engine::service::services.find({"h1", "s"})
      ->second->set_current_state(engine::service::state_critical);

  auto totals = [](const char* contact_name) {
    macro_context local_macros;
    macro_context* mac(&local_macros);
    if (contact_name)
      mac->set_contact(
          engine::contact::contacts.find(contact_name)->second.get());
    std::string out;
    out = macros::process_macros(*mac, "$TOTALHOSTSUP$ $TOTALSERVICESCRITICAL$",
                                 0);
    return out;
  };
  ASSERT_EQ(totals("c1"), "1 1");
  ASSERT_EQ(totals("c2"), "1 0");
  ASSERT_EQ(totals("c3"), "0 0");
  /* Without contact, everything is counted. */
  ASSERT_EQ(totals(nullptr), "2 1");
}

/* The objects of a contact are cached for the $TOTAL*$ macros: the cache must
 * follow the configuration changes. h1 notifies c1, h2 notifies the
 * contactgroup cg. Then h3 is added for c2, cg changes from c1 to c2 and h1 is
 * removed while it is still held. */
TEST_F(Macro, PbTotalsFollowConfigurationChanges) {
  configuration::applier::contact ct_aply;
  configuration::Contact c1, c2;
  configuration::contact_helper c1_hlp(&c1), c2_hlp(&c2);
  fill_pb_configuration_contact(&c1_hlp, "c1", true);
  fill_pb_configuration_contact(&c2_hlp, "c2", false);
  ct_aply.add_object(c1);
  ct_aply.add_object(c2);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  fill_pb_configuration_contactgroup(&cg_hlp, "cg", "c1");
  cg_aply.add_object(cg);

  configuration::applier::host hst_aply;
  configuration::Host h1, h2, h3;
  configuration::host_helper h1_hlp(&h1), h2_hlp(&h2), h3_hlp(&h3);
  fill_pb_configuration_host(&h1_hlp, "h1", "c1", 1);
  fill_pb_configuration_host(&h2_hlp, "h2", "", 2);
  fill_string_group(h2.mutable_contactgroups(), "cg");
  fill_pb_configuration_host(&h3_hlp, "h3", "c2", 3);
  hst_aply.add_object(h1);
  hst_aply.add_object(h2);

  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(c1);
  ct_aply.resolve_object(c2);
  cg_aply.resolve_object(cg);
  hst_aply.resolve_object(h1);
  hst_aply.resolve_object(h2);

  auto set_up = [](const char* name) {
    auto& h = engine::host::hosts.find(name)->second;
    h->set_current_state(engine::host::state_up);
    h->set_has_been_checked(true);
  };
  set_up("h1");
  set_up("h2");

  auto totals = [](const char* contact_name) {
    macro_context local_macros;
    macro_context* mac(&local_macros);
    mac->set_contact(
        engine::contact::contacts.find(contact_name)->second.get());
    std::string out;
    out = macros::process_macros(*mac, "$TOTALHOSTSUP$", 0);
    return out;
  };
  ASSERT_EQ(totals("c1"), "2");
  ASSERT_EQ(totals("c2"), "0");

  hst_aply.add_object(h3);
  hst_aply.resolve_object(h3);
  set_up("h3");
  ASSERT_EQ(totals("c1"), "2");
  ASSERT_EQ(totals("c2"), "1");

  configuration::Contactgroup new_cg;
  configuration::contactgroup_helper new_cg_hlp(&new_cg);
  fill_pb_configuration_contactgroup(&new_cg_hlp, "cg", "c2");
  cg_aply.modify_object(&cg, new_cg);
  ASSERT_EQ(totals("c1"), "1");
  ASSERT_EQ(totals("c2"), "2");

  /* h1 is still alive while someone holds it: it must no longer be counted
   * once removed from the configuration. */
  std::shared_ptr<engine::host> held = engine::host::hosts.find("h1")->second;
  hst_aply.remove_object(1);
  ASSERT_EQ(totals("c1"), "0");
  ASSERT_EQ(totals("c2"), "2");
  held.reset();
  ASSERT_EQ(totals("c1"), "0");
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
  macro_context local_macros;
  macro_context* mac(&local_macros);
  std::string out;

  mac->set_contact(engine::contact::contacts.find("c1")->second.get());
  out = macros::process_macros(*mac,
                               "[$CONTACTADDRESS1$] [$CONTACTADDRESS2$] "
                               "[$CONTACTADDRESS3$] [$CONTACTADDRESS6$]",
                               0);
  ASSERT_EQ(out, "[address-one] [address-two] [] []");
  mac->reset();

  mac->set_contact(engine::contact::contacts.find("c2")->second.get());
  out = macros::process_macros(*mac, "[$CONTACTADDRESS1$]", 0);
  ASSERT_EQ(out, "[]");
  mac->reset();

  out = macros::process_macros(
      *mac, "[$CONTACTADDRESS2:c1$] [$CONTACTADDRESS3:c1$]", 0);
  ASSERT_EQ(out, "[address-two] []");
  out = macros::process_macros(*mac, "[$CONTACTADDRESS1:c2$]", 0);
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
  macro_context local_macros;
  macro_context* mac(&local_macros);
  std::string out;
  out = macros::process_macros(*mac, "$SERVICEGROUPMEMBERS:sg$", 0);

  /* The members come in no particular order: compare the sorted pairs. */
  std::vector<std::string_view> items = absl::StrSplit(out, ',');
  ASSERT_EQ(items.size(), 6u) << out;
  std::vector<std::string> pairs;
  for (size_t i = 0; i < items.size(); i += 2)
    pairs.push_back(absl::StrCat(items[i], ",", items[i + 1]));
  std::sort(pairs.begin(), pairs.end());
  ASSERT_EQ(absl::StrJoin(pairs, ";"), "h1,s;h2,s;h2,t");
}

/* Without argument, the servicegroup and contactgroup macros used to look
 * for a group with an empty name instead of the group of the context, so they
 * were always empty. */
TEST_F(Macro, PbGroupOfContext) {
  configuration::applier::host hst_aply;
  configuration::applier::service svc_aply;
  configuration::applier::contact ct_aply;
  configuration::Host h1;
  configuration::host_helper h1_hlp(&h1);
  fill_pb_configuration_host(&h1_hlp, "h1", "", 1);
  hst_aply.add_object(h1);
  configuration::Service s1;
  configuration::service_helper s1_hlp(&s1);
  fill_pb_configuration_service(&s1_hlp, "h1", "s", "", 1);
  svc_aply.add_object(s1);
  configuration::Contact c1;
  configuration::contact_helper c1_hlp(&c1);
  fill_pb_configuration_contact(&c1_hlp, "c1", true);
  ct_aply.add_object(c1);

  configuration::applier::servicegroup sg_aply;
  configuration::Servicegroup sg;
  configuration::servicegroup_helper sg_hlp(&sg);
  sg.set_servicegroup_name("sg");
  sg.set_alias("Service group");
  fill_pair_string_group(sg.mutable_members(), "h1,s");
  sg_aply.add_object(sg);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  cg.set_contactgroup_name("cg");
  cg.set_alias("Contact group");
  fill_string_group(cg.mutable_members(), "c1");
  cg_aply.add_object(cg);

  configuration::error_cnt err;
  _state_hlp->expand(err);
  hst_aply.resolve_object(h1);
  svc_aply.resolve_object(s1);
  ct_aply.resolve_object(c1);
  sg_aply.resolve_object(sg);
  cg_aply.resolve_object(cg);
  macro_context local_macros;
  macro_context* mac(&local_macros);
  std::string out;
  const std::string line(
      "[$SERVICEGROUPALIAS$] [$SERVICEGROUPMEMBERS$] [$CONTACTGROUPALIAS$] "
      "[$CONTACTGROUPMEMBERS$]");

  mac->set_host(engine::host::hosts.find("h1")->second.get());
  mac->set_service(engine::service::services.find({"h1", "s"})->second.get());
  mac->set_contact(engine::contact::contacts.find("c1")->second.get());
  out = macros::process_macros(*mac, line, 0);
  ASSERT_EQ(out, "[Service group] [h1,s] [Contact group] [c1]");

  /* Without context, there is no group. */
  mac->reset();
  out = macros::process_macros(*mac, line, 0);
  ASSERT_EQ(out, "[] [] [] []");
}

/* In a group, the standard macros used to skip an empty value only when it
 * came first: depending on the order of the members, the same values gave
 * "x", "x," or "x,,". Every empty value is now skipped. */
TEST_F(Macro, PbGroupSkipsEmptyValues) {
  configuration::applier::host hst_aply;
  configuration::Host h1, h2, h3;
  configuration::host_helper h1_hlp(&h1), h2_hlp(&h2), h3_hlp(&h3);
  fill_pb_configuration_host(&h1_hlp, "h1", "", 1);
  fill_pb_configuration_host(&h2_hlp, "h2", "", 2);
  fill_pb_configuration_host(&h3_hlp, "h3", "", 3);
  h2.set_notes("x");
  hst_aply.add_object(h1);
  hst_aply.add_object(h2);
  hst_aply.add_object(h3);

  configuration::applier::hostgroup hg_aply;
  configuration::Hostgroup hg;
  configuration::hostgroup_helper hg_hlp(&hg);
  hg.set_hostgroup_name("hg");
  hg_hlp.hook("members", "h1,h2,h3");
  hg_aply.add_object(hg);

  configuration::error_cnt err;
  _state_hlp->expand(err);
  hst_aply.resolve_object(h1);
  hst_aply.resolve_object(h2);
  hst_aply.resolve_object(h3);
  hg_aply.resolve_object(hg);
  macro_context local_macros;
  macro_context* mac(&local_macros);
  std::string out;
  out = macros::process_macros(*mac, "[$HOSTNOTES:hg:,$] [$HOSTNOTESURL:hg:,$]",
                               0);
  ASSERT_EQ(out, "[x] []");
}

/* $X:sg$ of a custom service macro used to be the servicegroup sg with an
 * empty separator, while it was an error for the standard service macros.
 * It is an error for all of them: a separator is expected. */
TEST_F(Macro, PbServiceHostOnlyIsError) {
  configuration::applier::host hst_aply;
  configuration::applier::service svc_aply;
  configuration::Host h1;
  configuration::host_helper h1_hlp(&h1);
  fill_pb_configuration_host(&h1_hlp, "h1", "", 1);
  hst_aply.add_object(h1);
  configuration::Service s1;
  configuration::service_helper s1_hlp(&s1);
  fill_pb_configuration_service(&s1_hlp, "h1", "s", "", 1);
  s1_hlp.insert_customvariable("_BAR", "bar");
  svc_aply.add_object(s1);

  configuration::applier::servicegroup sg_aply;
  configuration::Servicegroup sg;
  configuration::servicegroup_helper sg_hlp(&sg);
  sg.set_servicegroup_name("sg");
  fill_pair_string_group(sg.mutable_members(), "h1,s");
  sg_aply.add_object(sg);

  configuration::error_cnt err;
  _state_hlp->expand(err);
  hst_aply.resolve_object(h1);
  svc_aply.resolve_object(s1);
  sg_aply.resolve_object(sg);
  macro_context local_macros;
  macro_context* mac(&local_macros);
  std::string out;
  out = macros::process_macros(
      *mac,
      "[$_SERVICEBAR:sg$] [$_SERVICEBAR:sg:,$] [$SERVICEDESC:sg$] "
      "[$SERVICEDESC:sg:,$]",
      0);
  ASSERT_EQ(out, "[] [bar] [] [s]");
}

/* $CONTACTADDRESSn::s$ used to be the address of the contact of the context,
 * while $X::s$ is an error for the other contact macros: the group name is
 * missing. */
TEST_F(Macro, PbContactAddressEmptyName) {
  configuration::applier::contact ct_aply;
  configuration::Contact c1;
  configuration::contact_helper c1_hlp(&c1);
  fill_pb_configuration_contact(&c1_hlp, "c1", true);
  c1.add_address("address-one");
  ct_aply.add_object(c1);
  configuration::error_cnt err;
  _state_hlp->expand(err);
  ct_aply.resolve_object(c1);
  macro_context local_macros;
  macro_context* mac(&local_macros);
  std::string out;
  mac->set_contact(engine::contact::contacts.find("c1")->second.get());
  out = macros::process_macros(*mac,
                               "[$CONTACTADDRESS1::s$] [$CONTACTNAME::s$] "
                               "[$CONTACTADDRESS1::$] [$CONTACTADDRESS1$]",
                               0);
  ASSERT_EQ(out, "[] [] [address-one] [address-one]");
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
  macro_context local_macros;
  macro_context* mac(&local_macros);
  std::string out;
  mac->set_host(engine::host::hosts.find("h1")->second.get());
  out = macros::process_macros(
      *mac,
      "$TOTALHOSTSERVICESWARNING:h2$ $TOTALHOSTSERVICESWARNING$ "
      "$TOTALHOSTSERVICESWARNING:h2$",
      0);
  ASSERT_EQ(out, "2 0 2");
  out = macros::process_macros(
      *mac,
      "$TOTALHOSTSERVICESWARNING$ $TOTALHOSTSERVICESWARNING:h2$ "
      "$TOTALHOSTSERVICESCRITICAL$",
      0);
  ASSERT_EQ(out, "0 2 1");
  mac->reset();

  /* One value per member, in no particular order. */
  out = macros::process_macros(*mac, "$TOTALHOSTSERVICESWARNING:hg:,$", 0);
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
  macro_context local_macros;
  macro_context* mac(&local_macros);
  notification_values& notification = mac->notification.emplace();
  notification.author = "ad`min";
  notification.author_name = "Admin|istrator";
  notification.author_alias = "admin <alias>";
  notification.comment = "it's a \"comment\" & more";
  std::string out;

  /* The notes and URLs of the groups are given as they are. */
  out = macros::process_macros(
      *mac,
      "[$HOSTGROUPNOTES:hg$] [$HOSTGROUPNOTESURL:hg$] "
      "[$HOSTGROUPACTIONURL:hg$] [$SERVICEGROUPNOTES:sg$] "
      "[$SERVICEGROUPNOTESURL:sg$]",
      0);
  ASSERT_EQ(out,
            "[notes & <co>] [http://example.com/hg-notes?a=1&b=2] "
            "[http://example.com/hg-action?a=1&b=2] [notes & <co>] "
            "[http://example.com/sg-notes?a=1&b=2]");

  /* The notification authors and comment are cleaned. */
  out = macros::process_macros(
      *mac,
      "[$NOTIFICATIONAUTHOR$] [$NOTIFICATIONAUTHORNAME$] "
      "[$NOTIFICATIONAUTHORALIAS$] [$NOTIFICATIONCOMMENT$]",
      0);
  ASSERT_EQ(out, "[admin] [Administrator] [admin alias] [its a comment  more]");

  mac->notification.reset();
}
