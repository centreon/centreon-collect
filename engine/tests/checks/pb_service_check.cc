/*
 * Copyright 2020-2022 Centreon (https://www.centreon.com/)
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

#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicedependency.hh"
#include "com/centreon/engine/configuration/applier/serviceescalation.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/serviceescalation.hh"
#include "com/centreon/engine/timezone_manager.hh"
#include "common/engine_conf/message_helper.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

extern configuration::State pb_config;

class PbServiceCheck : public TestEngine {
 public:
  void SetUp() override {
    init_config_state();

    pb_config.clear_contacts();
    configuration::applier::contact ct_aply;
    configuration::Contact ctct = new_pb_configuration_contact("admin", true);
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_config);
    configuration::error_cnt err;
    ct_aply.resolve_object(ctct, err);

    configuration::Host hst = new_pb_configuration_host("test_host", "admin");
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

    configuration::Service svc =
        new_pb_configuration_service("test_host", "test_svc", "admin");
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

    // This is to not be bothered by host checks during service checks
    pb_config.set_host_check_timeout(10000);
  }

  void TearDown() override {
    _host.reset();
    _svc.reset();
    deinit_config_state();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;
};

/* The following test comes from this array (inherited from Nagios behaviour):
 *
 * | Time | Check # | State | State type | State change |
 * ------------------------------------------------------
 * | 0    | 1       | OK    | HARD       | No           |
 * | 1    | 1       | CRTCL | SOFT       | Yes          |
 * | 2    | 2       | WARN  | SOFT       | Yes          |
 * | 3    | 3       | CRTCL | HARD       | Yes          |
 * | 4    | 3       | WARN  | HARD       | Yes          |
 * | 5    | 3       | WARN  | HARD       | No           |
 * | 6    | 1       | OK    | HARD       | Yes          |
 * | 7    | 1       | OK    | HARD       | No           |
 * | 8    | 1       | UNKNWN| SOFT       | Yes          |
 * | 9    | 2       | OK    | SOFT       | Yes          |
 * | 10   | 1       | OK    | HARD       | No           |
 * ------------------------------------------------------
 */
TEST_F(PbServiceCheck, SimpleCheck) {
  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_current_attempt(1);

  set_time(50500);

  std::time_t now{std::time(nullptr)};
  std::string cmd{fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now)};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(51000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;1;service warning",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_warning);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 2);

  set_time(51500);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  set_time(52000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;1;service warning",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_warning);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  set_time(52500);

  time_t previous = now;
  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;1;service warning",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_warning);
  ASSERT_EQ(_svc->get_last_hard_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  set_time(53000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(53500);

  previous = now;
  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(54000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;4;service unknown",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now - 1000);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(54500);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 2);

  set_time(55000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok", now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
}

/* The following test comes from this array (inherited from Nagios behaviour):
 *
 * | Time | Check # | State | State type | State change |
 * ------------------------------------------------------
 * | 0    | 1       | OK    | HARD       | No           |
 * | 1    | 1       | CRTCL | SOFT       | Yes          |
 * | 2    | 2       | CRTCL | SOFT       | No           |
 * | 3    | 3       | CRTCL | HARD       | No           |
 * ------------------------------------------------------
 */
TEST_F(PbServiceCheck, OkCritical) {
  set_time(55000);

  time_t now = std::time(nullptr);
  std::string cmd{fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok",
      now)};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(55500);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(56000);

  time_t previous = now;
  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 2);

  set_time(56500);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 3);
}

/* The following test comes from this array (inherited from Nagios behaviour):
 *
 * | Time | Check # | State | State type | State change |
 * ------------------------------------------------------
 * | 0    | 2       | OK    | SOFT       | No           |
 * | 1    | 1       | CRTCL | SOFT       | Yes          |
 * | 2    | 2       | CRTCL | SOFT       | No           |
 * | 3    | 3       | CRTCL | HARD       | No           |
 * ------------------------------------------------------
 */
TEST_F(PbServiceCheck, OkSoft_Critical) {
  set_time(55000);

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_state_change(55000);
  _svc->set_current_attempt(2);
  _svc->set_state_type(checkable::soft);
  _svc->set_accept_passive_checks(true);

  set_time(55500);

  time_t now = std::time(nullptr);
  std::string cmd{fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now)};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(56000);

  time_t previous = now;
  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 2);

  set_time(56500);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 3);
}

/* The following test comes from this array (inherited from Nagios behaviour):
 *
 * | Time | Check # | State | State type | State change |
 * ------------------------------------------------------
 * | 0    | 1       | OK    | HARD       | No           |
 * | 1    | 2       | OK    | HARD       | No           |
 * | 2    | 3       | WARN  | HARD       | Yes          |
 * | 3    | 4       | CRTCL | HARD       | Yes          |
 * | 4    | 5       | CRTCL | HARD       | Yes          |
 * | 5    | 6       | CRTCL | HARD       | Yes          |
 * | 6    | 7       | CRTCL | HARD       | No           |
 * | 7    | 8       | CRTCL | HARD       | No           |
 * ------------------------------------------------------
 */
TEST_F(PbServiceCheck, OkCriticalStalking) {
  set_time(55000);

  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_state_change(55000);
  _svc->set_current_attempt(2);
  _svc->set_state_type(checkable::soft);
  _svc->set_accept_passive_checks(true);
  _svc->set_stalk_on(static_cast<uint32_t>(-1));

  set_time(55500);
  testing::internal::CaptureStdout();
  time_t now = std::time(nullptr);
  std::string cmd{fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;RAID array "
      "optimal",
      now)};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(56000);
  time_t previous = now;

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;RAID array "
      "optimal",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 1);

  set_time(56500);
  for (int i = 0; i < 3; i++) {
    // When i == 0, the state_critical is soft => no notification
    // When i == 1, the state_critical is soft => no notification
    // When i == 2, the state_critical is hard down => notification
    now = std::time(nullptr);
    cmd = fmt::format(
        "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;1;RAID array "
        "degraded (1 drive bad, 1 hot spare rebuilding)",
        now);
    process_external_command(cmd.c_str());
    checks::checker::instance().reap();
  }
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_warning);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  set_time(57000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;RAID array "
      "degraded (2 drives bad, 1 host spare online, 1 hot spare rebuilding)",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  set_time(57500);
  previous = now;

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;RAID array "
      "degraded (3 drives bad, 2 hot spares online)",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_hard_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  set_time(58000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;RAID array "
      "failed",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_hard_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  set_time(58500);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;RAID array "
      "failed",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_hard_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  set_time(59000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;RAID array "
      "failed",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();

  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_hard_state_change(), previous);
  ASSERT_EQ(_svc->get_current_attempt(), 3);

  std::string out{testing::internal::GetCapturedStdout()};
  std::cout << out << std::endl;
  ASSERT_NE(
      out.find(
          "SERVICE ALERT: test_host;test_svc;OK;HARD;1;RAID array optimal"),
      std::string::npos);
  ASSERT_NE(out.find("SERVICE ALERT: test_host;test_svc;WARNING;HARD;3;RAID "
                     "array degraded (1 drive bad, 1 hot spare rebuilding)"),
            std::string::npos);
  ASSERT_NE(out.find("SERVICE ALERT: test_host;test_svc;CRITICAL;HARD;3;RAID "
                     "array degraded (2 drives bad, 1 host spare online, 1 hot "
                     "spare rebuilding)"),
            std::string::npos);
  ASSERT_NE(out.find("SERVICE ALERT: test_host;test_svc;CRITICAL;HARD;3;RAID "
                     "array degraded (3 drives bad, 2 hot spares online"),
            std::string::npos);
  ASSERT_NE(out.find("SERVICE ALERT: test_host;test_svc;CRITICAL;HARD;3;RAID "
                     "array failed"),
            std::string::npos);
}

TEST_F(PbServiceCheck, CheckRemoveCheck) {
  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_current_attempt(1);

  set_time(50500);
  std::time_t now{std::time(nullptr)};
  std::string cmd{fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical",
      now)};
  process_external_command(cmd.c_str());

  /* We simulate a reload that destroyed the service */
  engine::service::services.clear();
  engine::service::services_by_id.clear();
  _svc.reset();

  checks::checker::instance().reap();
}

TEST_F(PbServiceCheck, CheckUpdateMultilineOutput) {
  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_current_attempt(1);

  set_time(50500);
  std::time_t now{std::time(nullptr)};
  std::string cmd{fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
      "critical\\nline2\\nline3\\nline4\\nline5|res;2;5;5\\n",
      now)};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_plugin_output(), "service critical");
  ASSERT_EQ(_svc->get_long_plugin_output(), "line2\\nline3\\nline4\\nline5");
  ASSERT_EQ(_svc->get_perf_data(), "res;2;5;5");
}
