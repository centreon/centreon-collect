/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <gtest/gtest.h>

#include "com/centreon/common/rapidjson_helper.hh"

#include "check_service.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace com::centreon::agent::native_check_detail;

using namespace std::string_literals;

class mock_service_enumerator : public service_enumerator {
 public:
  using enum_with_conf = std::pair<ENUM_SERVICE_STATUSA, QUERY_SERVICE_CONFIGA>;

  std::vector<enum_with_conf> data;

  size_t max_enumerate = 512;

  bool _enumerate_services(serv_array& services,
                           DWORD* services_returned) override;

  bool _query_service_config(
      LPCSTR service_name,
      std::unique_ptr<unsigned char[]>& buffer,
      size_t* buffer_size,
      const std::shared_ptr<spdlog::logger>& logger) override;

  static enum_with_conf create_serv(const char* name,
                                    const char* display,
                                    DWORD state,
                                    DWORD start_type) {
    ENUM_SERVICE_STATUSA serv;
    serv.lpServiceName = const_cast<char*>(name);
    serv.lpDisplayName = const_cast<char*>(display);
    serv.ServiceStatus.dwCurrentState = state;
    serv.ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serv.ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    serv.ServiceStatus.dwWin32ExitCode = 0;
    serv.ServiceStatus.dwServiceSpecificExitCode = 0;
    serv.ServiceStatus.dwCheckPoint = 0;
    serv.ServiceStatus.dwWaitHint = 0;

    QUERY_SERVICE_CONFIGA serv_conf;
    serv_conf.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serv_conf.dwStartType = start_type;
    serv_conf.dwErrorControl = SERVICE_ERROR_NORMAL;
    static char dummy_path[] = "C:\\path\\to\\service.exe";
    serv_conf.lpBinaryPathName = dummy_path;
    serv_conf.lpLoadOrderGroup = nullptr;
    serv_conf.dwTagId = 0;
    serv_conf.lpDependencies = nullptr;
    serv_conf.lpServiceStartName = nullptr;
    serv_conf.lpDisplayName = const_cast<char*>(display);

    return {serv, serv_conf};
  }
};

bool mock_service_enumerator::_enumerate_services(serv_array& services,
                                                  DWORD* services_returned) {
  size_t to_return = std::min(max_enumerate, data.size() - _resume_handle);
  to_return = std::min(to_return, service_array_size);
  *services_returned = to_return;
  for (unsigned i = 0; i < to_return; ++i) {
    services[i] = data[i].first;
  }
  _resume_handle += to_return;
  return true;
}

bool mock_service_enumerator::_query_service_config(
    LPCSTR service_name,
    std::unique_ptr<unsigned char[]>& buffer,
    size_t* buffer_size,
    const std::shared_ptr<spdlog::logger>& logger) {
  for (const auto& service : data) {
    if (strcmp(service_name, service.first.lpServiceName) == 0) {
      memcpy(buffer.get(), &service.second, sizeof(QUERY_SERVICE_CONFIGA));
      return true;
    }
  }
  return false;
}

constexpr std::array<std::string_view, 7> expected_metrics = {
    "services.stopped.count",    "services.starting.count",
    "services.stopping.count",   "services.running.count",
    "services.continuing.count", "services.pausing.count",
    "services.paused.count"};

TEST(check_service, service_no_threshold_all_running) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service1", "desc serv1",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service2", "desc serv2",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service3", "desc serv3",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
  };

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = "{ }"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::ok);

  EXPECT_EQ(output, "OK: all services are running");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.running.count") {
      EXPECT_EQ(perf.value(), 3.0);
    } else {
      EXPECT_EQ(perf.value(), 0.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_no_threshold_one_by_state) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_start_pending", "desc service_start_pending",
          SERVICE_START_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_stop_pending", "desc service_stop_pending",
          SERVICE_STOP_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_running",
                                           "desc service_running",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", " desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_paused",
                                           "desc service_paused",
                                           SERVICE_PAUSED, SERVICE_AUTO_START),
  };

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = "{ }"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::ok);

  EXPECT_EQ(output,
            "OK: services: 1 stopped, 1 starting, 1 stopping, 1 running, "
            "1 continuing, 1 pausing, 1 paused");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    EXPECT_EQ(perf.value(), 1.0);
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_filter_exclude_all_service) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_start_pending", "desc service_start_pending ",
          SERVICE_START_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_stop_pending", "desc service_stop_pending",
          SERVICE_STOP_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_running",
                                           "desc service_running",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_paused",
                                           "desc service_paused",
                                           SERVICE_PAUSED, SERVICE_AUTO_START)};

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({ "exclude-name": ".*"  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::critical);

  EXPECT_EQ(output, "CRITICAL: no service found");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    EXPECT_EQ(perf.value(), 0.0);
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_filter_allow_some_service) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_start_pending", "desc service_start_pending",
          SERVICE_START_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_stop_pending", "desc service_stop_pending",
          SERVICE_STOP_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_running",
                                           "desc service_running",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_paused",
                                           "desc service_paused",
                                           SERVICE_PAUSED, SERVICE_AUTO_START)};

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({ "filter-name": "service_s.*"  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::ok);

  EXPECT_EQ(output, "OK: services: 1 stopped, 1 starting, 1 stopping");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.stopped.count" ||
        perf.name() == "services.starting.count" ||
        perf.name() == "services.stopping.count") {
      EXPECT_EQ(perf.value(), 1.0);
    } else {
      EXPECT_EQ(perf.value(), 0.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_filter_exclude_some_service) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_start_pending", "desc service_start_pending",
          SERVICE_START_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_stop_pending", "desc service_stop_pending",
          SERVICE_STOP_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_running",
                                           "desc service_running",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_paused",
                                           "desc service_paused",
                                           SERVICE_PAUSED, SERVICE_AUTO_START)};

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({ "exclude-name": "service_s.*"  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::ok);

  EXPECT_EQ(output,
            "OK: services: 1 running, 1 continuing, 1 pausing, 1 paused");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.stopped.count" ||
        perf.name() == "services.starting.count" ||
        perf.name() == "services.stopping.count") {
      EXPECT_EQ(perf.value(), 0.0);
    } else {
      EXPECT_EQ(perf.value(), 1.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_filter_allow_some_service_warning_running) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_start_pending", "desc service_start_pending",
          SERVICE_START_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_stop_pending", "desc service_stop_pending",
          SERVICE_STOP_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_running",
                                           "desc service_running",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_paused",
                                           "desc service_paused",
                                           SERVICE_PAUSED, SERVICE_AUTO_START)};

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "filter-name": "service_s.*", "warning-total-running": "5", "critical-total-running": ""  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::warning);

  EXPECT_EQ(output, "WARNING: services: 1 stopped, 1 starting, 1 stopping");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.stopped.count" ||
        perf.name() == "services.starting.count" ||
        perf.name() == "services.stopping.count") {
      EXPECT_EQ(perf.value(), 1.0);
    } else {
      EXPECT_EQ(perf.value(), 0.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_filter_allow_some_service_warning_stopped) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_start_pending", "desc service_start_pending",
          SERVICE_START_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_stopped2",
                                           "desc service_stopped2",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_running",
                                           "desc service_running",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_paused",
                                           "desc service_paused",
                                           SERVICE_PAUSED, SERVICE_AUTO_START),
  };

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "filter-name": "service_s.*", "warning-total-stopped": 1  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::warning);

  EXPECT_EQ(output, "WARNING: services: 2 stopped, 1 starting");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.stopped.count") {
      EXPECT_EQ(perf.value(), 2.0);
    } else if (perf.name() == "services.starting.count") {
      EXPECT_EQ(perf.value(), 1.0);
    } else {
      EXPECT_EQ(perf.value(), 0.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_filter_allow_some_service_critical_state) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_start_pending", "desc service_start_pending",
          SERVICE_START_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_stopping", "desc service_stopping", SERVICE_STOP_PENDING,
          SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_running",
                                           "desc service_running",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_paused",
                                           "desc service_paused",
                                           SERVICE_PAUSED, SERVICE_AUTO_START)};

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "filter-name": "service_s.*", "critical-state": "stop.*"  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::critical);

  EXPECT_EQ(output,
            "CRITICAL: services: 1 stopped, 1 starting, 1 stopping "
            "CRITICAL: service_stopped is stopped CRITICAL: service_stopping "
            "is stopping");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.stopped.count" ||
        perf.name() == "services.starting.count" ||
        perf.name() == "services.stopping.count") {
      EXPECT_EQ(perf.value(), 1.0);
    } else {
      EXPECT_EQ(perf.value(), 0.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_filter_start_auto_true) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_start_pending",
                                           "desc service_start_pending",
                                           SERVICE_START_PENDING, 0),
      mock_service_enumerator::create_serv(
          "service_stopping", "desc service_stopping", SERVICE_STOP_PENDING,
          SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_running", "desc service_running", SERVICE_RUNNING, 0),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_paused", "desc service_paused", SERVICE_PAUSED, 0)};

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({ "start-auto": true  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::ok);

  EXPECT_EQ(output,
            "OK: services: 1 stopped, 1 stopping, 1 continuing, 1 pausing");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.stopped.count" ||
        perf.name() == "services.continuing.count" ||
        perf.name() == "services.pausing.count" ||
        perf.name() == "services.stopping.count") {
      EXPECT_EQ(perf.value(), 1.0);
    } else {
      EXPECT_EQ(perf.value(), 0.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service, service_filter_start_auto_false) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_start_pending",
                                           "desc service_start_pending",
                                           SERVICE_START_PENDING, 0),
      mock_service_enumerator::create_serv(
          "service_stopping", "desc service_stopping", SERVICE_STOP_PENDING,
          SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_running", "desc service_running", SERVICE_RUNNING, 0),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_paused", "desc service_paused", SERVICE_PAUSED, 0)};

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({ "start-auto": false  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::ok);

  EXPECT_EQ(output, "OK: services: 1 starting, 1 running, 1 paused");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.starting.count" ||
        perf.name() == "services.running.count" ||
        perf.name() == "services.paused.count") {
      EXPECT_EQ(perf.value(), 1.0);
    } else {
      EXPECT_EQ(perf.value(), 0.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}

TEST(check_service,
     service_filter_allow_some_service_filtered_by_display_warning_running) {
  mock_service_enumerator::enum_with_conf data[] = {
      mock_service_enumerator::create_serv("service_stopped",
                                           "desc service_stopped",
                                           SERVICE_STOPPED, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_start_pending", "desc service_start_pending",
          SERVICE_START_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_stop_pending", "desc service_stop_pending",
          SERVICE_STOP_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_running",
                                           "desc service_running",
                                           SERVICE_RUNNING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_continue_pending", "desc service_continue_pending",
          SERVICE_CONTINUE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv(
          "service_pause_pending", "desc service_pause_pending",
          SERVICE_PAUSE_PENDING, SERVICE_AUTO_START),
      mock_service_enumerator::create_serv("service_paused",
                                           "desc service_paused",
                                           SERVICE_PAUSED, SERVICE_AUTO_START)};

  mock_service_enumerator mock;
  mock.data = {std::begin(data), std::end(data)};

  check_service::_enumerator_constructor = [&mock]() {
    return std::make_unique<mock_service_enumerator>(mock);
  };

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "filter-display": "desc service_s.*", "warning-total-running": "5"  })"_json;

  check_service test_check(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      [](const std::shared_ptr<check>& caller, int status,
         const std::list<com::centreon::common::perfdata>& perfdata,
         const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto snap = test_check.measure();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = test_check.compute(*snap, &output, &perfs);

  EXPECT_EQ(status, e_status::warning);

  EXPECT_EQ(output, "WARNING: services: 1 stopped, 1 starting, 1 stopping");

  EXPECT_EQ(perfs.size(), 7);

  for (const com::centreon::common::perfdata& perf : perfs) {
    EXPECT_NE(std::find(expected_metrics.begin(), expected_metrics.end(),
                        perf.name()),
              expected_metrics.end());
    if (perf.name() == "services.stopped.count" ||
        perf.name() == "services.starting.count" ||
        perf.name() == "services.stopping.count") {
      EXPECT_EQ(perf.value(), 1.0);
    } else {
      EXPECT_EQ(perf.value(), 0.0);
    }
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), snap->get_metric(e_service_metric::total));
  }
}
