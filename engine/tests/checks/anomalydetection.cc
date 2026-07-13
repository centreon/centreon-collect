/**
 * Copyright 2020 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/configuration/applier/anomalydetection.hh"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <cstring>

#include "../test_engine.hh"
#include "../timeperiod/utils.hh"
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicedependency.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class AnomalydetectionCheck : public TestEngine {
 public:
  void SetUp() override {
    ::unlink("/tmp/thresholds_status_change.json");
    init_config_state();

    checks_logger->set_level(spdlog::level::trace);
    commands_logger->set_level(spdlog::level::trace);

    error_cnt err;
    configuration::applier::contact ct_aply;
    configuration::contact ctct{new_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(*config);
    ct_aply.resolve_object(ctct, err);

    configuration::host hst{new_configuration_host("test_host", "admin")};
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

    configuration::service svc{
        new_configuration_service("test_host", "test_svc", "admin", 8)};
    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);

    hst_aply.resolve_object(hst, err);
    svc_aply.resolve_object(svc, err);

    configuration::anomalydetection ad{new_configuration_anomalydetection(
        "test_host", "test_ad", "admin", 9, 8,
        "/tmp/thresholds_status_change.json")};
    configuration::applier::anomalydetection ad_aply;
    ad_aply.add_object(ad);

    ad_aply.resolve_object(ad, err);

    host_map const& hm{engine::host::hosts};
    _host = hm.begin()->second;
    _host->set_current_state(engine::host::state_up);
    _host->set_state_type(checkable::hard);
    _host->set_acknowledgement(AckType::NONE);
    _host->set_notify_on(static_cast<uint32_t>(-1));

    service_map const& sm{engine::service::services};
    for (auto& p : sm) {
      std::shared_ptr<engine::service> svc = p.second;
      if (svc->service_id() == 8)
        _svc = svc;
      else
        _ad = std::static_pointer_cast<engine::anomalydetection>(svc);
    }
    _svc->set_current_state(engine::service::state_ok);
    _svc->set_state_type(checkable::hard);
    _svc->set_acknowledgement(AckType::NONE);
    _svc->set_notify_on(static_cast<uint32_t>(-1));
  }

  void TearDown() override {
    _host.reset();
    _svc.reset();
    _ad.reset();
    deinit_config_state();
  }

  void CreateFile(std::string const& filename, std::string const& content) {
    std::ofstream oss(filename);
    oss << content;
    oss.close();
  }

 protected:
  std::shared_ptr<engine::host> _host;
  std::shared_ptr<engine::service> _svc;
  std::shared_ptr<engine::anomalydetection> _ad;
};

// clang-format off

/* The following test comes from this array (inherited from Nagios behaviour):
 *
 * | Time | Check # | State | State type | State change | perf data in range | ad State  | ad state type | ad do check
 * --------------------------------------------------------------------------------------------------------------------
 * | 0    | 1       | OK    | HARD       | No           |  Y                 |  OK       | H             |  N
 * | 1    | 1       | CRTCL | SOFT       | Yes          |  Y                 |  OK       | H             |  N
 * | 2    | 2       | CRTCL | SOFT       | No           |  Y                 |  OK       | H             |  N
 * | 3    | 3       | CRTCL | HARD       | Yes          |  Y                 |  OK       | H             |  N
 * | 4    | 3       | OK    | HARD       | Yes          |  Y                 |  OK       | H             |  N
 * | 5    | 3       | OK    | HARD       | No           |  N                 |  CRTCL    | S             |  Y
 * | 6    | 1       | OK    | HARD       | No           |  N                 |  CRTCL    | S             |  Y
 * | 7    | 1       | OK    | HARD       | No           |  N                 |  CRTCL    | H             |  Y
 * | 8    | 1       | OK    | HARD       | No           |  Y                 |  OK       | H             |  Y
 * | 9    | 1       | OK    | HARD       | No           |  Y                 |  OK       | H             |  N
 * --------------------------------------------------------------------------------------------------------------------
 */

// clang-format on

enum class e_json_version { V1, V2 };

class AnomalydetectionCheckStatusChange
    : public AnomalydetectionCheck,
      public testing::WithParamInterface<
          std::pair<e_json_version, const char*>> {};

TEST_P(AnomalydetectionCheckStatusChange, StatusChanges) {
  CreateFile("/tmp/thresholds_status_change.json", GetParam().second);
  _ad->init_thresholds();
  _ad->set_status_change(true);

  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_current_attempt(1);
  _svc->set_last_check(50000);

  _ad->set_current_state(engine::service::state_ok);
  _ad->set_last_hard_state(engine::service::state_ok);
  _ad->set_last_hard_state_change(50000);
  _ad->set_last_state_change(50000);
  _ad->set_state_type(checkable::hard);
  _ad->set_current_attempt(1);
  _ad->set_last_check(50000);

  // --- 1 ----
  set_time(50500);
  time_t now = std::time(nullptr);
  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
      "critical| metric=80;25;60",
      now));
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  ASSERT_EQ(_svc->get_plugin_output(), "service critical");
  ASSERT_EQ(_svc->get_perf_data(), "metric=80;25;60");
  int check_options = 0;
  int latency = 0;
  bool time_is_valid;
  time_t preferred_time;
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=80.00");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=80 metric_lower_thresholds=73.31 "
              "metric_upper_thresholds=83.26 metric_fit=78.26 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=80 metric_lower_thresholds=73.31 "
              "metric_upper_thresholds=83.26 metric_fit=78.26 "
              "metric_lower_margin=-4.95 metric_upper_margin=5.00");
  }

  // --- 2 ----
  set_time(51000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
      "critical| metric=80;25;60",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), 50500);
  ASSERT_EQ(_svc->get_current_attempt(), 2);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=80.00");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=80 metric_lower_thresholds=72.62 "
              "metric_upper_thresholds=82.52 metric_fit=77.52 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=80 metric_lower_thresholds=72.62 "
              "metric_upper_thresholds=82.52 metric_fit=77.52 "
              "metric_lower_margin=-4.90 metric_upper_margin=5.00");
  }
  // --- 3 ----
  set_time(51250);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
      "critical| metric=80;25;60",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), 50500);
  ASSERT_EQ(_svc->get_current_attempt(), 3);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=80.00");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=80 metric_lower_thresholds=72.28 "
              "metric_upper_thresholds=82.15 metric_fit=77.15 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=80 metric_lower_thresholds=72.28 "
              "metric_upper_thresholds=82.15 metric_fit=77.15 "
              "metric_lower_margin=-4.88 metric_upper_margin=5.00");
  }
  // --- 4 ----
  set_time(52000);

  now = std::time(nullptr);
  time_t previous = now;
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service "
      "ok| metric=80foo;25;60",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=80.00foo");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=80foo metric_lower_thresholds=71.24foo "
              "metric_upper_thresholds=81.04foo metric_fit=76.04foo "
              "metric_lower_margin=0.00foo metric_upper_margin=0.00foo");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=80foo metric_lower_thresholds=71.24foo "
              "metric_upper_thresholds=81.04foo metric_fit=76.04foo "
              "metric_lower_margin=-4.80foo metric_upper_margin=5.00foo");
  }
  // --- 5 ----
  set_time(52500);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok| "
      "metric=30%;25;60",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), 52000);
  ASSERT_EQ(_svc->get_last_state_change(), 52000);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_ad->get_last_state_change(), now);
  ASSERT_EQ(_ad->get_plugin_output(),
            "NON-OK: Unusual activity, the actual value of metric is 30.00% "
            "which is outside the forecasting range [70.55% : 80.30%]");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=30% metric_lower_thresholds=70.55% "
              "metric_upper_thresholds=80.30% metric_fit=75.30% "
              "metric_lower_margin=0.00% metric_upper_margin=0.00%");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=30% metric_lower_thresholds=70.55% "
              "metric_upper_thresholds=80.30% metric_fit=75.30% "
              "metric_lower_margin=-4.75% metric_upper_margin=5.00%");
  }

  ASSERT_EQ(_ad->get_current_attempt(), 1);

  // --- 6 ----
  set_time(53000);

  previous = now;
  now = std::time(nullptr);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_ad->get_last_state_change(), previous);
  ASSERT_EQ(_ad->get_plugin_output(),
            "NON-OK: Unusual activity, the actual value of metric is 12.00 "
            "which is outside the forecasting range [69.86 : 79.56]");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=12 metric_lower_thresholds=69.86 "
              "metric_upper_thresholds=79.56 metric_fit=74.56 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");

  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=12 metric_lower_thresholds=69.86 "
              "metric_upper_thresholds=79.56 metric_fit=74.56 "
              "metric_lower_margin=-4.70 metric_upper_margin=5.00");
  }
  ASSERT_EQ(_ad->get_current_attempt(), 2);

  // --- 7 ----
  set_time(53500);

  previous = now;
  now = std::time(nullptr);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_ad->get_last_hard_state_change(), now);
  ASSERT_EQ(_ad->get_last_state_change(), 52500);
  ASSERT_EQ(_ad->get_plugin_output(),
            "NON-OK: Unusual activity, the actual value of metric is 12.00 "
            "which is outside the forecasting range [69.17 : 78.82]");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=12 metric_lower_thresholds=69.17 "
              "metric_upper_thresholds=78.82 metric_fit=73.82 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=12 metric_lower_thresholds=69.17 "
              "metric_upper_thresholds=78.82 metric_fit=73.82 "
              "metric_lower_margin=-4.65 metric_upper_margin=5.00");
  }
  ASSERT_EQ(_ad->get_current_attempt(), 3);

  // --- 8 ----
  set_time(54000);
  _ad->get_check_command_ptr()->set_command_line(
      "echo 'output| metric=70%;50;75'");
  previous = now;
  now = std::time(nullptr);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_hard_state_change(), now);
  ASSERT_EQ(_ad->get_last_state_change(), now);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=70.00%");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=70% metric_lower_thresholds=68.48% "
              "metric_upper_thresholds=78.08% metric_fit=73.08% "
              "metric_lower_margin=0.00% metric_upper_margin=0.00%");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=70% metric_lower_thresholds=68.48% "
              "metric_upper_thresholds=78.08% metric_fit=73.08% "
              "metric_lower_margin=-4.60% metric_upper_margin=5.00%");
  }
  ASSERT_EQ(_ad->get_current_attempt(), 1);

  // --- 9 ----
  set_time(54500);

  previous = now;
  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;4;service unknown",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_svc->get_last_hard_state_change(), 52000);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  ASSERT_EQ(_svc->get_plugin_output(), "service unknown");
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_ad->get_last_hard_state_change(), 54000);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_ad->get_plugin_output(),
            "UNKNOWN: Unknown activity, metric did not return any values");
  ASSERT_EQ(_ad->get_current_attempt(), 1);

  ::unlink("/tmp/thresholds_status_change.json");
}

TEST_P(AnomalydetectionCheckStatusChange, StatusChangesWithType) {
  CreateFile("/tmp/thresholds_status_change.json", GetParam().second);
  _ad->init_thresholds();
  _ad->set_status_change(true);

  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_current_attempt(1);
  _svc->set_last_check(50000);

  _ad->set_current_state(engine::service::state_ok);
  _ad->set_last_hard_state(engine::service::state_ok);
  _ad->set_last_hard_state_change(50000);
  _ad->set_last_state_change(50000);
  _ad->set_state_type(checkable::hard);
  _ad->set_current_attempt(1);
  _ad->set_last_check(50000);

  // --- 1 ----
  set_time(50500);
  time_t now = std::time(nullptr);
  std::string cmd(fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
      "critical| 'g[metric]'=80;25;60",
      now));
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  ASSERT_EQ(_svc->get_plugin_output(), "service critical");
  ASSERT_EQ(_svc->get_perf_data(), "'g[metric]'=80;25;60");
  int check_options = 0;
  int latency = 0;
  bool time_is_valid;
  time_t preferred_time;
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=80.00");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=80 metric_lower_thresholds=73.31 "
              "metric_upper_thresholds=83.26 metric_fit=78.26 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=80 metric_lower_thresholds=73.31 "
              "metric_upper_thresholds=83.26 metric_fit=78.26 "
              "metric_lower_margin=-4.95 metric_upper_margin=5.00");
  }

  // --- 2 ----
  set_time(51000);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
      "critical| 'g[metric]'=80;25;60",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), 50500);
  ASSERT_EQ(_svc->get_current_attempt(), 2);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=80.00");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=80 metric_lower_thresholds=72.62 "
              "metric_upper_thresholds=82.52 metric_fit=77.52 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=80 metric_lower_thresholds=72.62 "
              "metric_upper_thresholds=82.52 metric_fit=77.52 "
              "metric_lower_margin=-4.90 metric_upper_margin=5.00");
  }
  // --- 3 ----
  set_time(51250);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service "
      "critical| 'g[metric]'=80;25;60",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), 50500);
  ASSERT_EQ(_svc->get_current_attempt(), 3);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=80.00");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=80 metric_lower_thresholds=72.28 "
              "metric_upper_thresholds=82.15 metric_fit=77.15 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=80 metric_lower_thresholds=72.28 "
              "metric_upper_thresholds=82.15 metric_fit=77.15 "
              "metric_lower_margin=-4.88 metric_upper_margin=5.00");
  }
  // --- 4 ----
  set_time(52000);

  now = std::time(nullptr);
  time_t previous = now;
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service "
      "ok| 'g[metric]'=80foo;25;60",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=80.00foo");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=80foo metric_lower_thresholds=71.24foo "
              "metric_upper_thresholds=81.04foo metric_fit=76.04foo "
              "metric_lower_margin=0.00foo metric_upper_margin=0.00foo");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=80foo metric_lower_thresholds=71.24foo "
              "metric_upper_thresholds=81.04foo metric_fit=76.04foo "
              "metric_lower_margin=-4.80foo metric_upper_margin=5.00foo");
  }
  // --- 5 ----
  set_time(52500);

  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;0;service ok| "
      "'g[metric]'=30%;25;60",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::hard);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_svc->get_last_hard_state_change(), 52000);
  ASSERT_EQ(_svc->get_last_state_change(), 52000);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_ad->get_last_state_change(), now);
  ASSERT_EQ(_ad->get_plugin_output(),
            "NON-OK: Unusual activity, the actual value of metric is 30.00% "
            "which is outside the forecasting range [70.55% : 80.30%]");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=30% metric_lower_thresholds=70.55% "
              "metric_upper_thresholds=80.30% metric_fit=75.30% "
              "metric_lower_margin=0.00% metric_upper_margin=0.00%");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "'g[metric]'=30% metric_lower_thresholds=70.55% "
              "metric_upper_thresholds=80.30% metric_fit=75.30% "
              "metric_lower_margin=-4.75% metric_upper_margin=5.00%");
  }

  ASSERT_EQ(_ad->get_current_attempt(), 1);

  // --- 6 ----
  set_time(53000);

  previous = now;
  now = std::time(nullptr);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_ad->get_last_state_change(), previous);
  ASSERT_EQ(_ad->get_plugin_output(),
            "NON-OK: Unusual activity, the actual value of metric is 12.00 "
            "which is outside the forecasting range [69.86 : 79.56]");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=12 metric_lower_thresholds=69.86 "
              "metric_upper_thresholds=79.56 metric_fit=74.56 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");

  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=12 metric_lower_thresholds=69.86 "
              "metric_upper_thresholds=79.56 metric_fit=74.56 "
              "metric_lower_margin=-4.70 metric_upper_margin=5.00");
  }
  ASSERT_EQ(_ad->get_current_attempt(), 2);

  // --- 7 ----
  set_time(53500);

  previous = now;
  now = std::time(nullptr);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_ad->get_last_hard_state_change(), now);
  ASSERT_EQ(_ad->get_last_state_change(), 52500);
  ASSERT_EQ(_ad->get_plugin_output(),
            "NON-OK: Unusual activity, the actual value of metric is 12.00 "
            "which is outside the forecasting range [69.17 : 78.82]");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=12 metric_lower_thresholds=69.17 "
              "metric_upper_thresholds=78.82 metric_fit=73.82 "
              "metric_lower_margin=0.00 metric_upper_margin=0.00");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=12 metric_lower_thresholds=69.17 "
              "metric_upper_thresholds=78.82 metric_fit=73.82 "
              "metric_lower_margin=-4.65 metric_upper_margin=5.00");
  }
  ASSERT_EQ(_ad->get_current_attempt(), 3);

  // --- 8 ----
  set_time(54000);
  _ad->get_check_command_ptr()->set_command_line(
      "echo 'output| metric=70%;50;75'");
  previous = now;
  now = std::time(nullptr);
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::hard);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_ok);
  ASSERT_EQ(_ad->get_last_hard_state_change(), now);
  ASSERT_EQ(_ad->get_last_state_change(), now);
  ASSERT_EQ(_ad->get_plugin_output(), "OK: Regular activity, metric=70.00%");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=70% metric_lower_thresholds=68.48% "
              "metric_upper_thresholds=78.08% metric_fit=73.08% "
              "metric_lower_margin=0.00% metric_upper_margin=0.00%");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "metric=70% metric_lower_thresholds=68.48% "
              "metric_upper_thresholds=78.08% metric_fit=73.08% "
              "metric_lower_margin=-4.60% metric_upper_margin=5.00%");
  }
  ASSERT_EQ(_ad->get_current_attempt(), 1);

  // --- 9 ----
  set_time(54500);

  previous = now;
  now = std::time(nullptr);
  cmd = fmt::format(
      "[{}] PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;4;service unknown",
      now);
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_svc->get_last_hard_state_change(), 52000);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  ASSERT_EQ(_svc->get_plugin_output(), "service unknown");
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_ad->get_last_hard_state_change(), 54000);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_ad->get_plugin_output(),
            "UNKNOWN: Unknown activity, metric did not return any values");
  ASSERT_EQ(_ad->get_current_attempt(), 1);

  ::unlink("/tmp/thresholds_status_change.json");
}

INSTANTIATE_TEST_SUITE_P(
    AnomalydetectionCheckStatusChange,
    AnomalydetectionCheckStatusChange,
    testing::Values(
        std::make_pair(
            e_json_version::V1,
            "[{\n \"host_id\": \"12\",\n \"service_id\": \"9\",\n "
            "\"metric_name\": "
            "\"metric\",\n \"predict\": [{\n \"timestamp\": 50000,\n "
            "\"upper\": "
            "84,\n \"lower\": 74,\n \"fit\": 79\n }, {\n \"timestamp\": "
            "100000,\n "
            "\"upper\": 10,\n \"lower\": 5,\n \"fit\": 5\n }, {\n "
            "\"timestamp\": "
            "150000,\n \"upper\": 100,\n \"lower\": 93,\n \"fit\": 96.5\n }, "
            "{\n "
            "\"timestamp\": 200000,\n \"upper\": 100,\n \"lower\": 97,\n "
            "\"fit\": "
            "98.5\n }, {\n \"timestamp\": 250000,\n \"upper\": 100,\n "
            "\"lower\": "
            "21,\n \"fit\": 60.5\n }\n]}]"),
        std::make_pair(
            e_json_version::V2,
            "[{\n \"host_id\": \"12\",\n \"service_id\": \"9\",\n "
            "\"metric_name\": "
            "\"metric\",\n \"sensitivity\":1,\n \"predict\": [{\n "
            "\"timestamp\": "
            "50000,\n \"upper_margin\": "
            "5,\n \"lower_margin\": -5,\n \"fit\": 79\n }, {\n \"timestamp\": "
            "100000,\n "
            "\"upper_margin\": 5,\n \"lower_margin\": 0,\n \"fit\": 5\n }, {\n "
            "\"timestamp\": "
            "150000,\n \"upper\": 3.5,\n \"lower\": -3.5,\n \"fit\": 96.5\n }, "
            "{\n "
            "\"timestamp\": 200000,\n \"upper_margin\": 1.5,\n "
            "\"lower_margin\": "
            "-1.5,\n \"fit\": "
            "98.5\n }, {\n \"timestamp\": 250000,\n \"upper_margin\": 39.5,\n "
            "\"lower_margin\": "
            "-39.5,\n \"fit\": 60.5\n }\n]}]")));

class AnomalydetectionCheckMetricWithQuotes
    : public AnomalydetectionCheck,
      public testing::WithParamInterface<
          std::pair<e_json_version, const char*>> {};

TEST_P(AnomalydetectionCheckMetricWithQuotes, MetricWithQuotes) {
  CreateFile("/tmp/thresholds_status_change.json", GetParam().second);

  _ad->init_thresholds();
  _ad->set_status_change(true);

  set_time(50000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_current_attempt(1);
  _svc->set_last_check(50000);

  _ad->set_current_state(engine::service::state_ok);
  _ad->set_last_hard_state(engine::service::state_ok);
  _ad->set_last_hard_state_change(50000);
  _ad->set_state_type(checkable::hard);
  _ad->set_current_attempt(1);
  _ad->set_last_check(50000);

  set_time(50500);
  std::ostringstream oss;
  std::time_t now{std::time(nullptr)};
  oss << '[' << now << ']'
      << " PROCESS_SERVICE_CHECK_RESULT;test_host;test_svc;2;service critical| "
         "'metric'=90MT;25;60;0;100";
  std::string cmd{oss.str()};
  process_external_command(cmd.c_str());
  checks::checker::instance().reap();
  ASSERT_EQ(_svc->get_state_type(), checkable::soft);
  ASSERT_EQ(_svc->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_svc->get_last_state_change(), now);
  ASSERT_EQ(_svc->get_current_attempt(), 1);
  ASSERT_EQ(_svc->get_plugin_output(), "service critical");
  ASSERT_EQ(_svc->get_perf_data(), "'metric'=90MT;25;60;0;100");
  int check_options = 0;
  int latency = 0;
  bool time_is_valid;
  time_t preferred_time;
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_critical);
  ASSERT_EQ(_ad->get_last_state_change(), now);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(),
            "NON-OK: Unusual activity, the actual value of metric is 90.00MT "
            "which is outside the forecasting range [73.31MT : 83.26MT]");
  if (GetParam().first == e_json_version::V1) {
    ASSERT_EQ(_ad->get_perf_data(),
              "'metric'=90MT;;;0;100 metric_lower_thresholds=73.31MT;;;0;100 "
              "metric_upper_thresholds=83.26MT;;;0;100 "
              "metric_fit=78.26MT;;;0;100 metric_lower_margin=0.00MT;;;0;100 "
              "metric_upper_margin=0.00MT;;;0;100");
  } else {
    ASSERT_EQ(_ad->get_perf_data(),
              "'metric'=90MT;;;0;100 metric_lower_thresholds=73.31MT;;;0;100 "
              "metric_upper_thresholds=83.26MT;;;0;100 "
              "metric_fit=78.26MT;;;0;100 metric_lower_margin=-4.95MT;;;0;100 "
              "metric_upper_margin=5.00MT;;;0;100");
  }

  ::unlink("/tmp/thresholds_status_change.json");
}

INSTANTIATE_TEST_SUITE_P(
    AnomalydetectionCheckMetricWithQuotes,
    AnomalydetectionCheckMetricWithQuotes,
    testing::Values(
        std::make_pair(
            e_json_version::V1,
            "[{\n \"host_id\": \"12\",\n \"service_id\": \"9\",\n "
            "\"metric_name\": "
            "\"metric\",\n \"predict\": [{\n \"timestamp\": 50000,\n "
            "\"upper\": "
            "84,\n \"lower\": 74,\n \"fit\": 79\n }, {\n \"timestamp\": "
            "100000,\n "
            "\"upper\": 10,\n \"lower\": 5,\n \"fit\": 5\n }, {\n "
            "\"timestamp\": "
            "150000,\n \"upper\": 100,\n \"lower\": 93,\n \"fit\": 96.5\n }, "
            "{\n "
            "\"timestamp\": 200000,\n \"upper\": 100,\n \"lower\": 97,\n "
            "\"fit\": "
            "98.5\n }, {\n \"timestamp\": 250000,\n \"upper\": 100,\n "
            "\"lower\": "
            "21,\n \"fit\": 60.5\n }\n]}]"),
        std::make_pair(
            e_json_version::V2,
            "[{\n \"host_id\": \"12\",\n \"service_id\": \"9\",\n "
            "\"metric_name\": "
            "\"metric\",\n \"sensitivity\":1,\n \"predict\": [{\n "
            "\"timestamp\": "
            "50000,\n \"upper_margin\": "
            "5,\n \"lower_margin\": -5,\n \"fit\": 79\n }, {\n \"timestamp\": "
            "100000,\n "
            "\"upper_margin\": 5,\n \"lower_margin\": 0,\n \"fit\": 5\n }, {\n "
            "\"timestamp\": "
            "150000,\n \"upper_margin\": 3.5,\n \"lower_margin\": -3.5,\n "
            "\"fit\": "
            "96.5\n }, {\n "
            "\"timestamp\": 200000,\n \"upper_margin\": 1.5,\n "
            "\"lower_margin\": "
            "-1.5,\n \"fit\": "
            "98.5\n }, {\n \"timestamp\": 250000,\n \"upper_margin\": 39.5,\n "
            "\"lower_margin\": "
            "-39.5,\n \"fit\": 60.5\n }\n]}]")));

TEST_F(AnomalydetectionCheck, BadThresholdsFile) {
  ::unlink("/tmp/thresholds_status_change.json");
  set_time(50000);
  std::time_t now{std::time(nullptr)};
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(50000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_current_attempt(1);
  _svc->set_last_check(50000);
  _svc->set_perf_data("metric=90MT;25;60;0;100");

  _ad->set_current_state(engine::service::state_ok);
  _ad->set_last_hard_state(engine::service::state_ok);
  _ad->set_last_hard_state_change(50000);
  _ad->set_state_type(checkable::hard);
  _ad->set_current_attempt(1);
  _ad->set_last_check(50000);

  int check_options = 0;
  int latency = 0;
  bool time_is_valid;
  time_t preferred_time;
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_ad->get_last_state_change(), now);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(),
            "The thresholds file is not viable for metric metric");
  ASSERT_EQ(_ad->get_perf_data(), "metric=90MT;25;60;0;100");

  set_time(51000);
  now = std::time(nullptr);
  // _ad is not OK so _ad will do the check
  _ad->get_check_command_ptr()->set_command_line(
      "echo 'output| metric=70%;50;75'");

  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_ad->get_last_state_change(), 50000);
  ASSERT_EQ(_ad->get_current_attempt(), 2);
  ASSERT_EQ(_ad->get_plugin_output(),
            "The thresholds file is not viable for metric metric");
  ASSERT_EQ(_ad->get_perf_data(), "metric=70%;50;75");

  ::unlink("/tmp/thresholds_status_change.json");
}

class AnomalydetectionCheckFileTooOld
    : public AnomalydetectionCheck,
      public testing::WithParamInterface<
          std::pair<e_json_version, const char*>> {};

TEST_P(AnomalydetectionCheckFileTooOld, FileTooOld) {
  CreateFile("/tmp/thresholds_status_change.json", GetParam().second);
  _ad->init_thresholds();
  _ad->set_status_change(true);

  set_time(300000);
  _svc->set_current_state(engine::service::state_ok);
  _svc->set_last_hard_state(engine::service::state_ok);
  _svc->set_last_hard_state_change(300000);
  _svc->set_state_type(checkable::hard);
  _svc->set_accept_passive_checks(true);
  _svc->set_current_attempt(1);
  _svc->set_last_check(300000);
  _svc->set_perf_data("metric=90MT;25;60;0;100");

  _ad->set_current_state(engine::service::state_ok);
  _ad->set_last_hard_state(engine::service::state_ok);
  _ad->set_last_hard_state_change(300000);
  _ad->set_state_type(checkable::hard);
  _ad->set_current_attempt(1);
  _ad->set_last_check(300000);

  int check_options = 0;
  int latency = 0;
  bool time_is_valid;
  time_t preferred_time;
  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_ad->get_last_state_change(), 300000);
  ASSERT_EQ(_ad->get_current_attempt(), 1);
  ASSERT_EQ(_ad->get_plugin_output(),
            "The thresholds file is too old compared to the check timestamp "
            "300000 for metric metric");
  ASSERT_EQ(_ad->get_perf_data(), "metric=90MT;25;60;0;100");

  set_time(301000);
  // _ad is not OK so _ad will do the check
  _ad->get_check_command_ptr()->set_command_line(
      "echo 'output| metric=70%;50;75'");

  _ad->run_async_check(check_options, latency, true, true, &time_is_valid,
                       &preferred_time);
  checks::checker::instance().wait_completion(
      checks::checker::e_completion_filter::service);
  checks::checker::instance().reap();
  ASSERT_EQ(_ad->get_state_type(), checkable::soft);
  ASSERT_EQ(_ad->get_current_state(), engine::service::state_unknown);
  ASSERT_EQ(_ad->get_last_state_change(), 300000);
  ASSERT_EQ(_ad->get_current_attempt(), 2);
  ASSERT_EQ(_ad->get_plugin_output(),
            "The thresholds file is too old compared to the check timestamp "
            "301000 for metric metric");
  ASSERT_EQ(_ad->get_perf_data(), "metric=70%;50;75");

  ::unlink("/tmp/thresholds_status_change.json");
  // let's time to callback to be called
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

INSTANTIATE_TEST_SUITE_P(
    FileTooOld,
    AnomalydetectionCheckFileTooOld,
    testing::Values(
        std::make_pair(
            e_json_version::V1,
            "[{\n \"host_id\": \"12\",\n \"service_id\": \"9\",\n "
            "\"metric_name\": "
            "\"metric\",\n \"predict\": [{\n \"timestamp\": 50000,\n "
            "\"upper\": "
            "84,\n \"lower\": 74,\n \"fit\": 79\n }, {\n \"timestamp\": "
            "100000,\n "
            "\"upper\": 10,\n \"lower\": 5,\n \"fit\": 51.5\n }, {\n "
            "\"timestamp\": "
            "150000,\n \"upper\": 100,\n \"lower\": 93,\n \"fit\": 96.5\n }, "
            "{\n "
            "\"timestamp\": 200000,\n \"upper\": 100,\n \"lower\": 97,\n "
            "\"fit\": "
            "98.5\n }, {\n \"timestamp\": 250000,\n \"upper\": 100,\n "
            "\"lower\": "
            "21,\n \"fit\": 60.5\n }\n]}]"),
        std::make_pair(
            e_json_version::V2,
            "[{\n \"host_id\": \"12\",\n \"service_id\": \"9\",\n "
            "\"metric_name\": "
            "\"metric\",\n \"sensitivity\":1,\n \"predict\": [{\n "
            "\"timestamp\": "
            "50000,\n \"upper_margin\": "
            "5,\n \"lower_margin\": -5,\n \"fit\": 79\n }, {\n \"timestamp\": "
            "100000,\n "
            "\"upper_margin\": 5,\n \"lower\": 0,\n \"fit\": 5\n }, {\n "
            "\"timestamp\": "
            "150000,\n \"upper_margin\": 3.5,\n \"lower_margin\": -3.5,\n "
            "\"fit\": "
            "96.5\n }, {\n "
            "\"timestamp\": 200000,\n \"upper_margin\": 1.5,\n "
            "\"lower_margin\": "
            "-1.5,\n \"fit\": "
            "98.5\n }, {\n \"timestamp\": 250000,\n \"upper_margin\": 39.5,\n "
            "\"lower_margin\": "
            "-39.5,\n \"fit\": 60.5\n }\n]}]")));
