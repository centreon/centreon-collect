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

#include "check_event_log.hh"

#include "agent/test/check_event_log_data_test.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::event_log;
using namespace std::string_literals;

extern std::shared_ptr<asio::io_context> g_io_context;

/**
 * @brief Givent an eventlog with no event, we expect an empty output
 *
 */
TEST(check_event_log, empty) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "file" : "System", "empty-state": "${status}, ${count}, Empty or no match for this filter"})"_json;

  check_event_log checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  test_event_container cont(
      "System", "level in ('info', 'error', 'warning')", "level == 'warning'",
      "level == 'error'", std::chrono::days(4), true, spdlog::default_logger());

  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  e_status status = checker.compute(cont, &output, &perfs);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK, 0, Empty or no match for this filter");
  ASSERT_EQ(perfs.size(), 2);
  EXPECT_EQ(perfs.begin()->name(), "critical-count");
  EXPECT_EQ(perfs.begin()->value(), 0);
  EXPECT_EQ(perfs.rbegin()->name(), "warning-count");
  EXPECT_EQ(perfs.rbegin()->value(), 0);
}

/**
 * @brief Given a container with some filters, we expect output with some
 * warning injected events
 *
 */
TEST(check_event_log, warning) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "file" : "System", "warning-status": "level == 'warning' and written > -2s", "verbose": false})"_json;

  check_event_log checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  test_event_container cont("System", "level in ('info', 'error', 'warning')",
                            "level == 'warning' and written > -2s",
                            "level == 'error'", std::chrono::days(4), true,
                            spdlog::default_logger());

  mock_event_data raw_data;
  raw_data.level = 3;  // warning
  raw_data.event_id = 12;
  raw_data.provider = L"my_provider";
  raw_data.set_time_created(1);  // peremption in 1s

  cont.on_event(raw_data, nullptr);

  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  e_status status = checker.compute(cont, &output, &perfs);
  EXPECT_EQ(status, e_status::warning);
  EXPECT_EQ(output, "WARNING: 1  'my_provider 12'");
  ASSERT_EQ(perfs.size(), 2);
  EXPECT_EQ(perfs.begin()->name(), "critical-count");
  EXPECT_EQ(perfs.begin()->value(), 0);
  EXPECT_EQ(perfs.rbegin()->name(), "warning-count");
  EXPECT_EQ(perfs.rbegin()->value(), 1);

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  perfs.clear();
  status = checker.compute(cont, &output, &perfs);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK: Event log seems fine");
  ASSERT_EQ(perfs.size(), 2);
  EXPECT_EQ(perfs.begin()->name(), "critical-count");
  EXPECT_EQ(perfs.begin()->value(), 0);
  EXPECT_EQ(perfs.rbegin()->name(), "warning-count");
  EXPECT_EQ(perfs.rbegin()->value(), 0);
}

/**
 * @brief Given a container with some filters, we expect output with some
 * critical injected events
 *
 */
TEST(check_event_log, critical) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "file" : "System", "critical-status": "level == 'error' and written > -2s", "verbose": false,
      "event-detail-syntax": "'${file} ${source} ${log} ${provider} ${id} ${message} ${status} ${written} ${computer} ${channel} ${keywords} ${level} ${record_id} ${written_str}'"})"_json;

  check_event_log checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  test_event_container cont(
      "System", "level in ('info', 'error', 'warning')", "level == 'warning'",
      "level == 'error' and written > -2s", std::chrono::days(4), true,
      spdlog::default_logger());

  cont.message_content = "my message";

  mock_event_data raw_data;
  raw_data.level = 2;  // error
  raw_data.event_id = 12;
  raw_data.provider = L"my_provider";
  raw_data.record_id = 456;
  raw_data.computer = L"my_computer";
  raw_data.keywords = 0x0030000000000000L;  // audit success audit failure
  raw_data.channel = L"my_channel";
  raw_data.set_time_created(1);  // peremption in 1s

  cont.on_event(raw_data, nullptr);

  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  e_status status = checker.compute(cont, &output, &perfs);
  EXPECT_EQ(status, e_status::critical);
  std::string_view regex =
      "^CRITICAL: 1  'System my_provider my_provider my_provider 12 my message "
      "CRITICAL \\d+ my_computer my_channel audit_success\\|audit_failure 2 "
      "456 \\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}\\.\\d*'$";
  if (!RE2::FullMatch(output, regex)) {
    FAIL() << "output: " << output << " does not match to " << regex;
  }
  ASSERT_EQ(perfs.size(), 2);
  EXPECT_EQ(perfs.begin()->name(), "critical-count");
  EXPECT_EQ(perfs.begin()->value(), 1);
  EXPECT_EQ(perfs.rbegin()->name(), "warning-count");
  EXPECT_EQ(perfs.rbegin()->value(), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  perfs.clear();
  status = checker.compute(cont, &output, &perfs);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK: Event log seems fine");
  ASSERT_EQ(perfs.size(), 2);
  EXPECT_EQ(perfs.begin()->name(), "critical-count");
  EXPECT_EQ(perfs.begin()->value(), 0);
  EXPECT_EQ(perfs.rbegin()->name(), "warning-count");
  EXPECT_EQ(perfs.rbegin()->value(), 0);
}

/**
 * @brief Given a container with some filtersand verbose output, we expect
 * output with some critical injected events
 *
 */
TEST(check_event_log, critical_verbose) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "file" : "System", 
        "critical-status": "level == 'error' and written > -2s", 
        "verbose": true, 
        "output-syntax": "${status}: ${count}",
        "unique-index": "${provider}${id}",
        "event-detail-syntax": "'${file} ${source} ${log} ${provider} ${id} ${message} ${status} ${written} ${written_str}'"})"_json;

  check_event_log checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  test_event_container cont(
      "System", "level in ('info', 'error', 'warning')", "level == 'warning'",
      "level == 'error' and written > -60m", std::chrono::days(4), true,
      spdlog::default_logger());

  cont.message_content = "my message";

  mock_event_data raw_data;
  raw_data.level = 2;  // error
  raw_data.event_id = 12;
  raw_data.provider = L"my_provider";
  raw_data.channel = L"my_channel";
  raw_data.set_time_created(2);

  cont.on_event(raw_data, nullptr);
  cont.on_event(raw_data, nullptr);
  raw_data.event_id = 13;  // different event id for unique
  raw_data.set_time_created(1);
  cont.on_event(raw_data, nullptr);

  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  e_status status = checker.compute(cont, &output, &perfs);
  EXPECT_EQ(status, e_status::critical);
  static std::string_view regex =
      "^CRITICAL: 3\n"
      "'System my_provider my_provider my_provider 13 my message "
      "CRITICAL \\d+ \\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}\\.\\d*'\n"
      "'System my_provider my_provider my_provider 12 my message "
      "CRITICAL \\d+ \\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}\\.\\d*'$";

  if (!RE2::FullMatch(output, regex)) {
    FAIL() << "regex:" << regex << "\noutput: " << output;
  }
  ASSERT_EQ(perfs.size(), 2);
  EXPECT_EQ(perfs.begin()->name(), "critical-count");
  EXPECT_EQ(perfs.begin()->value(), 3);
  EXPECT_EQ(perfs.rbegin()->name(), "warning-count");
  EXPECT_EQ(perfs.rbegin()->value(), 0);
}

/**
 * @brief Given a container with some filters that make some critical events
 * become warning after a little delay, we expect output with some critical
 * injected events first and then warning events
 *
 */
TEST(check_event_log, critical_to_warning) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "file" : "System", "critical-status": "level == 'error' and written > -2s", "verbose": false})"_json;

  check_event_log checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  test_event_container cont("System", "level in ('info', 'error', 'warning')",
                            "level == 'warning' or level == 'error'",
                            "level == 'error' and written > -2s",
                            std::chrono::days(4), true,
                            spdlog::default_logger());

  mock_event_data raw_data;
  raw_data.level = 2;  // error
  raw_data.event_id = 12;
  raw_data.provider = L"my_provider";
  raw_data.set_time_created(1);  // peremption in 1s

  cont.on_event(raw_data, nullptr);

  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  e_status status = checker.compute(cont, &output, &perfs);
  EXPECT_EQ(status, e_status::critical);
  EXPECT_EQ(output, "CRITICAL: 1  'my_provider 12'");
  ASSERT_EQ(perfs.size(), 2);
  EXPECT_EQ(perfs.begin()->name(), "critical-count");
  EXPECT_EQ(perfs.begin()->value(), 1);
  EXPECT_EQ(perfs.rbegin()->name(), "warning-count");
  EXPECT_EQ(perfs.rbegin()->value(), 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  perfs.clear();
  status = checker.compute(cont, &output, &perfs);
  EXPECT_EQ(status, e_status::warning);
  EXPECT_EQ(output, "WARNING: 1  'my_provider 12'");
  ASSERT_EQ(perfs.size(), 2);
  EXPECT_EQ(perfs.begin()->name(), "critical-count");
  EXPECT_EQ(perfs.begin()->value(), 0);
  EXPECT_EQ(perfs.rbegin()->name(), "warning-count");
  EXPECT_EQ(perfs.rbegin()->value(), 1);
}
