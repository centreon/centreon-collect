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
#include <regex>

#include "check.hh"
#include "check_files.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;
using namespace std::string_literals;

extern std::shared_ptr<asio::io_context> g_io_context;

class check_files_test : public ::testing::Test {
 protected:
 public:
  static void SetUpTestCase() {}
  static void TearDownTestCase() { check_files::thread_kill(); }
};

// Test the default behavior of the check_files class
// It should check files in the specified path and return an OK status
TEST_F(check_files_test, default) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({
        "path": "C:\\Windows"
        })"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  bool complete = false;

  auto is_complete = [&]() { return complete; };

  auto checker = std::make_shared<check_files>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), "serv"s, "cmd_name"s, "cmd_line"s, check_args,
      nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        complete = true;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(20));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  std::regex ok_regex(R"(OK: All \d+ files are ok)");
  ASSERT_TRUE(std::regex_search(output, ok_regex))
      << "Output format does not match expected pattern: " << output;
}

// Test the check_files class with a specific pattern and filter
TEST_F(check_files_test, test_filter) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({
        "path": "C:\\Windows",
        "max-depth": 0,
        "pattern": "*.*",
        "filter-files": "size > 1k"
        })"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  bool complete = false;

  auto is_complete = [&]() { return complete; };

  auto checker = std::make_shared<check_files>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), "serv"s, "cmd_name"s, "cmd_line"s, check_args,
      nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        complete = true;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(20));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  // Should only list files > 10k (output should not mention "Empty" unless none
  // found)
  ASSERT_EQ(output.find("Empty"), std::string::npos);
}

// Test the check_files class with a warning status condition
TEST_F(check_files_test, warning_status) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({
        "path": "C:\\Windows",
        "max-depth": 1,
        "pattern": "*.txt",
        "files-detail-syntax": "${filename}: ${size}",
        "warning-status": "size > 1k",
        "verbose": false
})"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  bool complete = false;

  auto is_complete = [&]() { return complete; };

  auto checker = std::make_shared<check_files>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), "serv"s, "cmd_name"s, "cmd_line"s, check_args,
      nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        complete = true;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(20));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_NE(output.find("WARNING:"), std::string::npos);
}

// Test the check_files class with a critical status condition
TEST_F(check_files_test, critical_status) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({
        "path": "C:\\Windows",
        "max-depth": 1,
        "pattern": "*.txt",
        "files-detail-syntax": "${filename}: ${size} {line_count}",
        "critical-status": "size > 1k",
        "verbose": false
})"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  bool complete = false;

  auto is_complete = [&]() { return complete; };

  auto checker = std::make_shared<check_files>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), "serv"s, "cmd_name"s, "cmd_line"s, check_args,
      nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        complete = true;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(20));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_NE(output.find("CRITICAL:"), std::string::npos);
}

// Test the check_files class with a version detail syntax
TEST_F(check_files_test, version) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({
        "path": "C:\\Windows",
        "max-depth": 1,
        "pattern": "*.exe",
        "filter-files": "filename == 'cmd.exe'",
        "ok-syntax": "${status}: {list}",
        "files-detail-syntax": "${filename}: ${version}",
        "verbose": false
})"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  bool complete = false;

  auto is_complete = [&]() { return complete; };

  auto checker = std::make_shared<check_files>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), "serv"s, "cmd_name"s, "cmd_line"s, check_args,
      nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        complete = true;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(20));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));
  ASSERT_NE(output.find("OK: cmd.exe: "), std::string::npos)
      << "Output does not contain expected version information: " << output;
}

// Test the check_files class with a specific file type (DLL)
TEST_F(check_files_test, dll) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({
        "path": "C:\\Windows\\System32",
        "max-depth": 1,
        "pattern": "*.dll",
        "filter-files": "size > 10m",
        "verbose": true
})"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  bool complete = false;

  auto is_complete = [&]() { return complete; };

  auto checker = std::make_shared<check_files>(
      g_io_context, spdlog::default_logger(), std::chrono::system_clock::now(),
      std::chrono::seconds(1), "serv"s, "cmd_name"s, "cmd_line"s, check_args,
      nullptr,
      [&]([[maybe_unused]] const std::shared_ptr<check>& caller,
          [[maybe_unused]] int status,
          [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
              perfdata,
          [[maybe_unused]] const std::list<std::string>& outputs) {
        absl::MutexLock lck(&wait_m);
        complete = true;
        output = outputs.front();
      },
      std::make_shared<checks_statistics>());

  checker->start_check(std::chrono::seconds(20));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));
  // check regex for output  OK: Ok:22|Nok:0|total:22  warning:0|critical:0
  std::regex ok_regex(
      R"(OK: Ok:\d+\|Nok:\d+\|total:\d+  warning:\d+\|critical:\d+)");
  ASSERT_TRUE(std::regex_search(output, ok_regex))
      << "Output format does not match expected pattern: " << output;
}