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

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <cstring>

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
#include "com/centreon/engine/configuration/host.hh"
#include "com/centreon/engine/configuration/service.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/serviceescalation.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ServiceDowntimeNotification : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();

    configuration::applier::contact ct_aply;
    configuration::contact ctct{new_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(*config);
    ct_aply.resolve_object(ctct);

    configuration::host hst{new_configuration_host("test_host", "admin")};
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

    configuration::service svc{
        new_configuration_service("test_host", "test_svc", "admin")};
    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);

    hst_aply.resolve_object(hst);
    svc_aply.resolve_object(svc);

    host_map const& hm{engine::host::hosts};
    _host = hm.begin()->second;
    _host->set_current_state(engine::host::state_up);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));
    _host->set_max_attempts(1);

    service_map const& sm{engine::service::services};
    _svc = sm.begin()->second;
    _svc->set_current_state(engine::service::state_ok);
    _svc->set_state_type(checkable::hard);
    _svc->set_acknowledgement(AckType::NONE);
    _svc->set_notify_on(static_cast<uint32_t>(-1));
    _svc->set_max_attempts(1);
  }

  void TearDown() override {
    _svc.reset();
    _host.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;
};

TEST_F(ServiceDowntimeNotification, SVCKO_SVCOK_Notify) {
  int now(50000);
  set_time(now);
  _svc->set_notification_interval(1);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);

  // service down
  testing::internal::CaptureStdout();

  uint64_t id = _svc->get_next_notification_id();

  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service crit",
      now));
  process_external_command(cmd.c_str());
  // as we have _max_attempts=1, we should notify at the first failure
  checks::checker::instance().reap();
  uint64_t first_notif_id = _svc->get_next_notification_id();

  // service up
  set_time(50300);
  now = 50300;
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", now);
  process_external_command(cmd.c_str());
  // as we have _max_attempts=1, we should notify at the first failure
  checks::checker::instance().reap();

  std::string out = testing::internal::GetCapturedStdout();

  std::cout << " out=" << out << std::endl << std::endl;

  ASSERT_EQ(_svc->get_next_notification_id(), id + 2);

  checks::checker::instance().reap();

  ASSERT_EQ(first_notif_id, id + 1);
  size_t step1{out.find("[50000]")};
  ASSERT_NE(step1, std::string::npos);
  size_t step2 = out.find(
      "SERVICE NOTIFICATION: "
      "admin;test_host;test_svc;CRITICAL;cmd;service crit",
      step1 + 1);
  ASSERT_NE(step2, std::string::npos);

  size_t step7 = out.find("[50300]", step2);
  ASSERT_NE(step7, std::string::npos);
  ASSERT_NE(out.find("SERVICE NOTIFICATION: "
                     "admin;test_host;test_svc;RECOVERY (OK);cmd;service ok",
                     step7 + 1),
            std::string::npos);
}

TEST_F(ServiceDowntimeNotification, SVCKO_Dt_CancelDt_SVCOK_Notify) {
  int now(50000);
  set_time(now);
  _svc->set_notification_interval(1);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);

  // service down
  testing::internal::CaptureStdout();

  uint64_t id{_svc->get_next_notification_id()};

  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service crit",
      now));
  process_external_command(cmd.c_str());
  // as we have _max_attempts=1, we should notify at the first failure
  checks::checker::instance().reap();
  uint64_t first_notif_id = _svc->get_next_notification_id();

  // start downtime
  set_time(50100);
  int res1 = _svc->notify(notifier::reason_downtimestart, "", "",
                          notifier::notification_option_none);
  _svc->inc_scheduled_downtime_depth();
  uint64_t second_notif_id = _svc->get_next_notification_id();

  // end downtime
  set_time(50200);
  _svc->set_scheduled_downtime_depth(0);
  int res2 = _svc->notify(notifier::reason_downtimeend, "", "",
                          notifier::notification_option_none);
  uint64_t third_notif_id = _svc->get_next_notification_id();

  // service up
  set_time(50300);
  now = 50300;
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  std::string out = testing::internal::GetCapturedStdout();

  std::cout << " out=" << out << std::endl << std::endl;

  ASSERT_EQ(first_notif_id, id + 1);
  size_t step1{out.find("[50000]")};
  ASSERT_NE(step1, std::string::npos);
  size_t step2 = out.find(
      "SERVICE NOTIFICATION: "
      "admin;test_host;test_svc;CRITICAL;cmd;service crit",
      step1 + 1);
  ASSERT_NE(step2, std::string::npos);

  ASSERT_EQ(res1, OK);
  ASSERT_EQ(second_notif_id, id + 2);
  size_t step3{out.find("[50100]", step2)};
  ASSERT_NE(step3, std::string::npos);
  size_t step4 = out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;DOWNTIMESTART "
      "(CRITICAL);cmd;service crit",
      step3);
  ASSERT_NE(step4, std::string::npos);

  ASSERT_EQ(res2, OK);
  ASSERT_EQ(third_notif_id, id + 3);
  size_t step5{out.find("[50200]", step4)};
  ASSERT_NE(step5, std::string::npos);
  size_t step6 = out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;DOWNTIMEEND "
      "(CRITICAL);cmd;service crit",
      step5);
  ASSERT_NE(step6, std::string::npos);

  ASSERT_EQ(_svc->get_next_notification_id(), id + 4);
  size_t step7 = out.find("[50300]", step6);
  ASSERT_NE(step7, std::string::npos);
  ASSERT_NE(out.find("SERVICE NOTIFICATION: "
                     "admin;test_host;test_svc;RECOVERY (OK);cmd;service ok",
                     step7 + 1),
            std::string::npos);
}

TEST_F(ServiceDowntimeNotification,
       SVCOK_Dt_SVCKO_CancelDt_Notify_SVCOK_Notify) {
  int now(50000);
  set_time(now);
  _svc->set_notification_interval(1);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(now);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);

  testing::internal::CaptureStdout();

  uint64_t id = _svc->get_next_notification_id();

  // start downtime
  set_time(50100);
  int res1 = _svc->notify(notifier::reason_downtimestart, "", "",
                          notifier::notification_option_none);
  _svc->inc_scheduled_downtime_depth();
  uint64_t first_notif_id = _svc->get_next_notification_id();

  // service ko
  now = 50200;
  set_time(50200);
  // downtime active => no notification
  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service crit",
      now));
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  uint64_t second_notif_id = _svc->get_next_notification_id();

  // end dt
  now = 50300;
  set_time(50300);
  _svc->set_scheduled_downtime_depth(0);
  int res2 = _svc->notify(notifier::reason_downtimeend, "", "",
                          notifier::notification_option_none);
  uint64_t third_notif_id = _svc->get_next_notification_id();

  // service up
  set_time(50400);
  now = 50400;
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  uint64_t fourth_notif_id = _svc->get_next_notification_id();

  std::string out = testing::internal::GetCapturedStdout();

  std::cout << " out=" << out << std::endl << std::endl;

  ASSERT_EQ(first_notif_id, id + 1);
  size_t step1{out.find("[50100]")};
  ASSERT_NE(step1, std::string::npos);
  size_t step1_bis = out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;DOWNTIMESTART (OK);cmd;",
      step1);
  ASSERT_NE(step1_bis, std::string::npos);

  ASSERT_EQ(out.find("SERVICE NOTIFICATION: "
                     "admin;test_host;test_svc;CRITICAL;cmd;service crit"),
            std::string::npos);

  size_t step2{out.find("[50300]", step1_bis)};
  ASSERT_NE(step2, std::string::npos);
  size_t step2_bis = out.find(
      "SERVICE NOTIFICATION: admin;test_host;test_svc;DOWNTIMEEND "
      "(CRITICAL);cmd;service crit",
      step2);
  ASSERT_NE(step2_bis, std::string::npos);

  ASSERT_EQ(out.find("SERVICE NOTIFICATION: "
                     "admin;test_host;test_svc;RECOVERY (OK);cmd;service ok",
                     step2_bis + 1),
            std::string::npos);
}
