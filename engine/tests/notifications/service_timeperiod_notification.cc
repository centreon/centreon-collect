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

#include <cstring>

#include <com/centreon/engine/configuration/applier/timeperiod.hh>
#include <com/centreon/engine/macros.hh>
#include <com/centreon/engine/macros/grab_host.hh>
#include <com/centreon/engine/macros/process.hh>
#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicedependency.hh"
#include "com/centreon/engine/configuration/applier/serviceescalation.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/serviceescalation.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "gtest/gtest.h"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ServiceTimePeriodNotification : public TestEngine {
 public:
  void SetUp() override {
    error_cnt err;
    init_config_state();

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    configuration::Contact ctct1{
        new_pb_configuration_contact("admin1", false, "c,r")};
    ct_aply.add_object(ctct1);
    ct_aply.expand_objects(pb_config);
    ct_aply.resolve_object(ctct, err);
    ct_aply.resolve_object(ctct1, err);

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
    _host = hm.begin()->second;
    _host->set_current_state(engine::host::state_up);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));

    service_map const& sm{engine::service::services};
    _svc = sm.begin()->second;
    _svc->set_current_state(engine::service::state_ok);
    _svc->set_state_type(checkable::hard);
    _svc->set_acknowledgement(AckType::NONE);
    _svc->set_notify_on(static_cast<uint32_t>(-1));

    _tp = _creator.new_timeperiod();
    for (int i(0); i < 7; ++i)
      _creator.new_timerange(0, 0, 24, 0, i);
    _now = strtotimet("2016-11-24 08:00:00");
    set_time(_now);
  }

  void TearDown() override {
    _svc.reset();
    _host.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;
  com::centreon::engine::timeperiod* _tp;
  timeperiod_creator _creator;
  time_t _now;
};

// Given a service with a notification interval = 2, a
// first_delay_notification = 0, an escalation from 2 to 12 with a contactgroup
// and notification_interval = 4
// When a normal notification is sent 11 times,
// Then contacts from the escalation are notified when notification number
// is in [2,6] and are separated by at less 4*60s.
TEST_F(ServiceTimePeriodNotification, NoTimePeriodOk) {
  error_cnt err;
  init_macros();
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  int now{20000};
  set_time(now);

  configuration::applier::contact ct_aply;
  configuration::Contact ctct{
      new_pb_configuration_contact("test_contact", false)};
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg{
      new_pb_configuration_contactgroup("test_cg", "test_contact")};
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se{new_pb_configuration_serviceescalation(
      "test_host", "test_svc", "test_cg")};
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);

  // uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(15000, 38000);
    list_time.emplace_back(65000, 85000);
    tperiod->days[i] = list_time;
  }

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(1);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(20000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_notification_period_ptr(tperiod.get());

  testing::internal::CaptureStdout();
  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now = 70000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  oss.str("");
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  cmd = oss.str();
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 32000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{out.find("NOW = 70000", step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: test_contact;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_NE(step4, std::string::npos);
}

TEST_F(ServiceTimePeriodNotification, NoTimePeriodKo) {
  error_cnt err;
  init_macros();
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  int now{20000};
  set_time(now);

  configuration::applier::contact ct_aply;
  configuration::Contact ctct{
      new_pb_configuration_contact("test_contact", false)};
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg{
      new_pb_configuration_contactgroup("test_cg", "test_contact")};
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se;
  configuration::serviceescalation_helper se_hlp(&se);
  se.set_first_notification(1);
  se.set_last_notification(1);
  se.set_notification_interval(0);
  se_hlp.hook("escalation_options", "w,u,c,r");
  se_hlp.hook("host_name", "test_host");
  se_hlp.hook("service_description", "test_svc");
  se_hlp.hook("contact_groups", "test_cg");
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);
  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(35000, 85000);
    tperiod->days[i] = list_time;
  }

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(0);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(20000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_notification_period_ptr(tperiod.get());

  testing::internal::CaptureStdout();

  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now = 70000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  oss.str("");
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  cmd = oss.str();
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 35000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{out.find("NOW = 59000", step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: test_contact;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_NE(step4, std::string::npos);

  size_t step5{out.find("NOW = 44000")};
  size_t step6{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step5 + 1)};
  ASSERT_EQ(step6, std::string::npos);
}

TEST_F(ServiceTimePeriodNotification, TimePeriodOut) {
  error_cnt err;
  init_macros();
  std::unique_ptr<engine::timeperiod> tperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  int now{20000};
  set_time(now);

  configuration::applier::contact ct_aply;
  configuration::Contact ctct{
      new_pb_configuration_contact("test_contact", false)};
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg{
      new_pb_configuration_contactgroup("test_cg", "test_contact")};
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se{new_pb_configuration_serviceescalation(
      "test_host", "test_svc", "test_cg")};
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);

  // uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(1000, 15000);
    list_time.emplace_back(80000, 85000);
    tperiod->days[i] = list_time;
  }

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(1);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(20000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_notification_period_ptr(tperiod.get());

  testing::internal::CaptureStdout();
  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now = 70000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  oss.str("");
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  cmd = oss.str();
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 32000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{out.find("NOW = 59000", step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: test_contact;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_EQ(step4, std::string::npos);
}

TEST_F(ServiceTimePeriodNotification, TimePeriodUserOut) {
  error_cnt err;
  init_macros();
  std::unique_ptr<engine::timeperiod> tiperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  int now{20000};
  set_time(now);
  configuration::Timeperiod tperiod;
  configuration::timeperiod_helper tperiod_hlp(&tperiod);
  tperiod.set_timeperiod_name("24x9");
  tperiod.set_alias("24x9");
  tperiod_hlp.hook("monday", "00:00-09:00");
  tperiod_hlp.hook("tuesday", "00:00-09:00");
  tperiod_hlp.hook("wednesday", "00:00-09:00");
  tperiod_hlp.hook("thursday", "00:00-09:00");
  tperiod_hlp.hook("friday", "00:00-09:00");
  tperiod_hlp.hook("saterday", "00:00-09:00");
  tperiod_hlp.hook("sunday", "00:00-09:00");
  configuration::applier::timeperiod aplyr;
  aplyr.add_object(tperiod);

  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test_contact");
  ctct.set_service_notification_period("24x9");
  ctct_hlp.hook("host_notification_commands", "cmd");
  ctct_hlp.hook("service_notification_commands", "cmd");
  ctct_hlp.hook("host_notification_options", "d,r,f,s");
  ctct_hlp.hook("service_notification_options", "a");
  ctct.set_host_notifications_enabled(true);
  ctct.set_service_notifications_enabled(true);

  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg{
      new_pb_configuration_contactgroup("test_cg", "test_contact")};
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se{new_pb_configuration_serviceescalation(
      "test_host", "test_svc", "test_cg")};
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);

  // uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(8000, 85000);
    tiperiod->days[i] = list_time;
  }

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(1);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(20000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_notification_period_ptr(tiperiod.get());

  testing::internal::CaptureStdout();
  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now = 70000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  oss.str("");
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  cmd = oss.str();
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 32000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{out.find("NOW = 59000", step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: test_contact;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_EQ(step4, std::string::npos);
}

TEST_F(ServiceTimePeriodNotification, TimePeriodUserIn) {
  error_cnt err;
  init_macros();
  std::unique_ptr<engine::timeperiod> tiperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  int now{20000};
  set_time(now);
  configuration::Timeperiod tperiod;
  configuration::timeperiod_helper tperiod_hlp(&tperiod);
  tperiod.set_timeperiod_name("24x9");
  tperiod.set_alias("24x9");
  tperiod_hlp.hook("monday", "09:00-20:00");
  tperiod_hlp.hook("tuesday", "09:00-20:00");
  tperiod_hlp.hook("wednesday", "09:00-20:00");
  tperiod_hlp.hook("thursday", "09:00-20:00");
  tperiod_hlp.hook("friday", "09:00-20:00");
  tperiod_hlp.hook("saterday", "09:00-20:00");
  tperiod_hlp.hook("sunday", "09:00-20:00");
  configuration::applier::timeperiod aplyr;
  aplyr.add_object(tperiod);

  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test_contact");
  ctct.set_service_notification_period("24x9");
  ctct_hlp.hook("host_notification_commands", "cmd");
  ctct_hlp.hook("service_notification_commands", "cmd");
  ctct_hlp.hook("host_notification_options", "d,r,f,s");
  ctct_hlp.hook("service_notification_options", "a");
  ctct.set_host_notifications_enabled(true);
  ctct.set_service_notifications_enabled(true);

  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg{
      new_pb_configuration_contactgroup("test_cg", "test_contact")};
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se{new_pb_configuration_serviceescalation(
      "test_host", "test_svc", "test_cg")};
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);

  // uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(8000, 85000);
    tiperiod->days[i] = list_time;
  }

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(1);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(20000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_notification_period_ptr(tiperiod.get());

  testing::internal::CaptureStdout();
  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now = 70000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  oss.str("");
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  cmd = oss.str();
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 32000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{out.find("NOW = 59000", step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: test_contact;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_NE(step4, std::string::npos);
}

TEST_F(ServiceTimePeriodNotification, TimePeriodUserAll) {
  init_macros();
  std::unique_ptr<engine::timeperiod> tiperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  int now{20000};
  set_time(now);
  configuration::Timeperiod tperiod;
  configuration::timeperiod_helper tperiod_hlp(&tperiod);
  tperiod.set_timeperiod_name("24x9");
  tperiod.set_alias("24x9");
  tperiod_hlp.hook("monday", "00:00-24:00");
  tperiod_hlp.hook("tuesday", "00:00-24:00");
  tperiod_hlp.hook("wednesday", "00:00-24:00");
  tperiod_hlp.hook("thursday", "00:00-24:00");
  tperiod_hlp.hook("friday", "00:00-24:00");
  tperiod_hlp.hook("saterday", "00:00-24:00");
  tperiod_hlp.hook("sunday", "00:00-24:00");
  configuration::applier::timeperiod aplyr;
  aplyr.add_object(tperiod);

  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test_contact");
  ctct.set_service_notification_period("24x9");
  ctct_hlp.hook("host_notification_commands", "cmd");
  ctct_hlp.hook("service_notification_commands", "cmd");
  ctct_hlp.hook("host_notification_options", "d,r,f,s");
  ctct_hlp.hook("service_notification_options", "a");
  ctct.set_host_notifications_enabled(true);
  ctct.set_service_notifications_enabled(true);

  error_cnt err;
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg{
      new_pb_configuration_contactgroup("test_cg", "test_contact")};
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se{new_pb_configuration_serviceescalation(
      "test_host", "test_svc", "test_cg")};
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);

  // uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(8000, 85000);
    tiperiod->days[i] = list_time;
  }

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(1);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(20000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_notification_period_ptr(tiperiod.get());

  testing::internal::CaptureStdout();
  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now = 70000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  oss.str("");
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  cmd = oss.str();
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 32000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{out.find("NOW = 59000", step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: test_contact;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_NE(step4, std::string::npos);
}

TEST_F(ServiceTimePeriodNotification, TimePeriodUserNone) {
  init_macros();
  std::unique_ptr<engine::timeperiod> tiperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  int now{20000};
  set_time(now);
  configuration::Timeperiod tperiod;
  configuration::timeperiod_helper tperiod_hlp(&tperiod);
  tperiod.set_timeperiod_name("24x9");
  tperiod.set_alias("24x9");
  configuration::applier::timeperiod aplyr;
  aplyr.add_object(tperiod);

  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test_contact");
  ctct.set_service_notification_period("24x9");
  ctct_hlp.hook("host_notification_commands", "cmd");
  ctct_hlp.hook("service_notification_commands", "cmd");
  ctct_hlp.hook("host_notification_options", "d,r,f,s");
  ctct_hlp.hook("service_notification_options", "a");
  ctct.set_host_notifications_enabled(true);
  ctct.set_service_notifications_enabled(true);

  error_cnt err;
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg{
      new_pb_configuration_contactgroup("test_cg", "test_contact")};
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se{new_pb_configuration_serviceescalation(
      "test_host", "test_svc", "test_cg")};
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);

  // uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(8000, 85000);
    tiperiod->days[i] = list_time;
  }

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(1);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(20000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_notification_period_ptr(tiperiod.get());

  testing::internal::CaptureStdout();
  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now = 70000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  oss.str("");
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  cmd = oss.str();
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 32000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{out.find("NOW = 59000", step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: test_contact;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_EQ(step4, std::string::npos);
}

TEST_F(ServiceTimePeriodNotification, NoTimePeriodUser) {
  init_macros();
  std::unique_ptr<engine::timeperiod> tiperiod{
      new_timeperiod_with_timeranges("tperiod", "alias")};
  int now{20000};
  set_time(now);
  configuration::Timeperiod tperiod;
  configuration::timeperiod_helper tperiod_hlp(&tperiod);
  tperiod.set_timeperiod_name("24x9");
  tperiod.set_alias("24x9");
  configuration::applier::timeperiod aplyr;
  aplyr.add_object(tperiod);

  configuration::applier::contact ct_aply;
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test_contact");
  ctct_hlp.hook("host_notification_commands", "cmd");
  ctct_hlp.hook("service_notification_commands", "cmd");
  ctct_hlp.hook("host_notification_options", "d,r,f,s");
  ctct_hlp.hook("service_notification_options", "a");
  ctct.set_host_notifications_enabled(true);
  ctct.set_service_notifications_enabled(true);

  error_cnt err;
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::applier::contactgroup cg_aply;
  configuration::Contactgroup cg{
      new_pb_configuration_contactgroup("test_cg", "test_contact")};
  cg_aply.add_object(cg);
  cg_aply.expand_objects(pb_config);
  cg_aply.resolve_object(cg, err);

  configuration::applier::serviceescalation se_aply;
  configuration::Serviceescalation se{new_pb_configuration_serviceescalation(
      "test_host", "test_svc", "test_cg")};
  se_aply.add_object(se);
  se_aply.expand_objects(pb_config);
  se_aply.resolve_object(se, err);

  // uint64_t id{_svc->get_next_notification_id()};
  for (int i = 0; i < 7; ++i) {
    timerange_list list_time;
    list_time.emplace_back(8000, 85000);
    tiperiod->days[i] = list_time;
  }

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_notification_interval(1);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(20000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_notification_period_ptr(tiperiod.get());

  testing::internal::CaptureStdout();
  for (int i = 0; i < 12; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now += 3000;
    std::cout << "NOW = " << now << std::endl;
    set_time(now);
    _svc->set_last_state(_svc->get_current_state());
    if (notifier::hard == _svc->get_state_type())
      _svc->set_last_hard_state(_svc->get_current_state());

    std::ostringstream oss;
    std::time_t now{std::time(nullptr)};
    oss << '[' << now << ']'
        << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
           "critical";
    std::string cmd{oss.str()};
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }

  now += 3000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  std::ostringstream oss;
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  now = 70000;
  std::cout << "NOW = " << now << std::endl;
  set_time(now);
  oss.str("");
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok";
  cmd = oss.str();
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  size_t step1{out.find("NOW = 32000")};
  size_t step2{
      out.find("SERVICE NOTIFICATION: "
               "test_contact;test_host;test_svc;CRITICAL;cmd;service critical",
               step1 + 1)};
  size_t step3{out.find("NOW = 59000", step2 + 1)};
  size_t step4{
      out.find("SERVICE NOTIFICATION: test_contact;test_host;test_svc;RECOVERY "
               "(OK);cmd;service ok",
               step3 + 1)};
  ASSERT_NE(step4, std::string::npos);
}
