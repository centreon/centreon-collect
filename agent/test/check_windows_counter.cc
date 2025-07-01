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

#include "check.hh"
#include "com/centreon/common/rapidjson_helper.hh"

#include "check_counter.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace std::string_literals;

/*
  Verify that the check_counter constructor works as expected.
  It should parse the JSON arguments and set the appropriate member variables.
*/
TEST(counter_check_windows, constructor) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\LogicalDisk(*)\\% Free Space",
        "verbose": true,
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  ASSERT_TRUE(checker.have_multi_return());
  ASSERT_EQ(checker.counter_name(), "\\LogicalDisk(*)\\% Free Space"s);
  ASSERT_TRUE(checker.use_english());
  ASSERT_FALSE(checker.need_two_samples());

  rapidjson::Document check_args1 =
      R"({"counter": "\\System\\Processes",
    "verbose": true,
    "use_english": true
})"_json;
  check_counter checker1(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args1, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  ASSERT_FALSE(checker1.have_multi_return());
  ASSERT_EQ(checker1.counter_name(), "\\System\\Processes"s);
  ASSERT_TRUE(checker1.use_english());
  ASSERT_FALSE(checker1.need_two_samples());

  rapidjson::Document check_args2 =
      R"({"counter": "\\Thread(*)\\Context Switches/sec",
        "verbose": true,
        "use_english": true
        })"_json;
  check_counter checker2(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args2, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  ASSERT_TRUE(checker2.have_multi_return());
  ASSERT_EQ(checker2.counter_name(), "\\Thread(*)\\Context Switches/sec"s);
  ASSERT_TRUE(checker2.use_english());
  ASSERT_TRUE(checker2.need_two_samples());
}

/*
  Verify that the check_counter class can handle a single return value.
  It should correctly compute the status and output based on the counter value.
*/
TEST(counter_check_windows, single_return) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\System\\Processes",
      "output-syntax": "${status}: {list}",
        "verbose": false,
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  std::list<com::centreon::common::perfdata> perf;

  checker.pdh_snapshot(true);
  ASSERT_EQ(checker.get_size_data(), 1);

  e_status status = checker.compute(&output, &perf);

  ASSERT_EQ(perf.size(), 1);
  ASSERT_EQ(perf.front().name(), "\\System\\Processes");
  ASSERT_NE(output.size(), 0);
  ASSERT_EQ(output.find("OK: \\System\\Processes : "), 0);

  ASSERT_EQ(status, e_status::ok);
}

/*
  Verify that the check_counter class can handle a multiple return value.
  It should correctly compute the status and output based on the counter value.
  also test warning status and warning count
*/
TEST(counter_check_windows, multiple_return) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\LogicalDisk(*)\\% Free Space",
      "output-syntax": "${status}: {problem-list}",
      "filter-process": "any",
        "warning-status": "_total >= 1",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  std::list<com::centreon::common::perfdata> perf;

  checker.pdh_snapshot(true);
  e_status status = checker.compute(&output, &perf);

  ASSERT_NE(output.size(), 0);
  ASSERT_EQ(output.find("WARNING: _total"), 0);
  // the perfdata should contain the warning count and the value should be
  // greater than 0
  auto it = std::prev(perf.end(), 2);
  ASSERT_EQ(it->name(), "warning-count");
  ASSERT_NE(it->value(), 0.0);

  ASSERT_EQ(status, e_status::warning);
}

/*
  Verify that the check_counter class can handle a counter that requires two
  samples. It should correctly compute the status and output based on the
  counter value. also test critical status and critical count
*/
TEST(counter_check_windows, need_two_samples) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "critical-status": "any >= 0",
        "critical-count": "10",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  std::list<com::centreon::common::perfdata> perf;

  checker.pdh_snapshot(true);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  checker.pdh_snapshot(false);
  e_status status = checker.compute(&output, &perf);

  ASSERT_NE(output.size(), 0);
  ASSERT_EQ(output.find("CRITICAL:"), 0);

  ASSERT_GE(perf.size(), 2);
  ASSERT_EQ(perf.back().name(), "critical-count");
  ASSERT_NE(perf.back().value(), 0.0);

  ASSERT_EQ(status, e_status::critical);
}

/*
  Verify that the check_counter class can handle complex rules for both
  warning and critical statuses. It should correctly compute the status and
  output based on the counter value.
*/
// Test if the warning status is correctly computed, and the output is well
// formatted keywords tested: {status}, {problem-list}, {label}, {value}
TEST(counter_check_windows, complex_rules) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "counter-filter":"any",
        "output-syntax": "${status}: {problem-list}",
        "detail-syntax": "{label} : {value}",
        "warning-status": "_total >= 10",
        "warning-count": "0",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  absl::flat_hash_map<std::string, double> data = {
      {"_total", 150.0},  {"svchost", 24.0}, {"explorer", 18.0},
      {"chrome", 42.0},   {"firefox", 36.0}, {"notepad", 5.0},
      {"winlogon", 12.0}, {"lsass", 16.0},   {"services", 28.0},
      {"csrss", 10.0}};

  checker.set_counter_data(data);

  std::string output;
  std::list<com::centreon::common::perfdata> perf;
  checker.compute(&output, &perf);

  ASSERT_EQ(output, "WARNING: _total : 150.00");
  ASSERT_EQ(perf.size(), 12);
}

// test if the critical status is set
TEST(counter_check_windows, complex_rules_2) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "counter-filter":"any",
        "output-syntax": "${status}: {problem-list}",
        "detail-syntax": "{label} : {value}",
        "warning-status": "_total >= 10",
        "critical-status": "_total >= 100",
        "warning-count": "0",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  absl::flat_hash_map<std::string, double> data = {
      {"_total", 150.0},  {"svchost", 24.0}, {"explorer", 18.0},
      {"chrome", 42.0},   {"firefox", 36.0}, {"notepad", 5.0},
      {"winlogon", 12.0}, {"lsass", 16.0},   {"services", 28.0},
      {"csrss", 10.0}};

  checker.set_counter_data(data);

  std::string output;
  std::list<com::centreon::common::perfdata> perf;
  checker.compute(&output, &perf);

  ASSERT_EQ(output, "CRITICAL: _total : 150.00");
  ASSERT_EQ(perf.size(), 12);
}
// test the keywords {crit-list} and {warn-list} in the output-syntax
// the label _total should be only in the crit-list and not in the warn-list
TEST(counter_check_windows, complex_rules_3) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "counter-filter":"any",
        "output-syntax": "${status}: {crit-list} --- {warn-list}",
        "detail-syntax": "{label} : {value}",
        "warning-status": "_total >= 10",
        "critical-status": "_total >= 100",
        "warning-count": "0",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  absl::flat_hash_map<std::string, double> data = {
      {"_total", 150.0},  {"svchost", 24.0}, {"explorer", 18.0},
      {"chrome", 42.0},   {"firefox", 36.0}, {"notepad", 5.0},
      {"winlogon", 12.0}, {"lsass", 16.0},   {"services", 28.0},
      {"csrss", 10.0}};

  checker.set_counter_data(data);

  std::string output;
  std::list<com::centreon::common::perfdata> perf;
  checker.compute(&output, &perf);
  ASSERT_EQ(output, "CRITICAL: _total : 150.00 --- ");
  ASSERT_EQ(perf.size(), 12);
}

// test the keywords {ok_list}
TEST(counter_check_windows, complex_rules_4) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "counter-filter":"any",
        "output-syntax": "${status}: {ok-list}",
        "detail-syntax": "{label} : {value} unit",
        "warning-status": "_total >= 10",
        "critical-status": "_total >= 100",
        "warning-count": "0",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  absl::flat_hash_map<std::string, double> data = {
      {"_total", 150.0},  {"svchost", 24.0}, {"explorer", 18.0},
      {"chrome", 42.0},   {"firefox", 36.0}, {"notepad", 5.0},
      {"winlogon", 12.0}, {"lsass", 16.0},   {"services", 28.0},
      {"csrss", 10.0}};

  checker.set_counter_data(data);

  std::string output;
  std::list<com::centreon::common::perfdata> perf;
  checker.compute(&output, &perf);
  ASSERT_EQ(
      output,
      "CRITICAL: chrome : 42.00 unit,csrss : 10.00 unit,explorer : 18.00 "
      "unit,firefox : 36.00 unit,lsass : 16.00 unit,notepad : 5.00 "
      "unit,services : 28.00 unit,svchost : 24.00 unit,winlogon : 12.00 unit");
  ASSERT_EQ(perf.size(), 12);
}

// test the keywords {warn-count},{crit-count},{ok-count},{problem-count} and
// {total} in the output-syntax
TEST(counter_check_windows, complex_rules_5) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "counter-filter":"any",
        "output-syntax": "${status}: O:{ok-count}/W:{warn-count}/C:{crit-count}/{total} --- {problem-count}/{total}",
        "detail-syntax": "{label}",
        "warning-status": "value >= 20",
        "critical-status": "value >= 100",
        "warning-count": "3",
        "critical-count": "5",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  absl::flat_hash_map<std::string, double> data = {
      {"_total", 150.0},  {"svchost", 24.0}, {"explorer", 18.0},
      {"chrome", 42.0},   {"firefox", 36.0}, {"notepad", 5.0},
      {"winlogon", 12.0}, {"lsass", 16.0},   {"services", 28.0},
      {"csrss", 10.0}};

  checker.set_counter_data(data);

  std::string output;
  std::list<com::centreon::common::perfdata> perf;
  checker.compute(&output, &perf);
  ASSERT_EQ(output, "WARNING: O:5/W:4/C:1/10 --- 5/10");
}

// test the keywords {warn-list} and {crit-list} and also the count for critical
// status
TEST(counter_check_windows, complex_rules_6) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "counter-filter":"any",
        "output-syntax": "${status}: Warn :{warn-list} --- crit : {crit-list}",
        "detail-syntax": "{label}",
        "warning-status": "value >= 20",
        "critical-status": "value >= 42",
        "warning-count": "0",
        "critical-count": "2",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  absl::flat_hash_map<std::string, double> data = {
      {"_total", 150.0},  {"svchost", 24.0}, {"explorer", 18.0},
      {"chrome", 42.0},   {"firefox", 36.0}, {"notepad", 5.0},
      {"winlogon", 12.0}, {"lsass", 16.0},   {"services", 28.0},
      {"csrss", 10.0}};

  checker.set_counter_data(data);

  std::string output;
  std::list<com::centreon::common::perfdata> perf;
  checker.compute(&output, &perf);
  ASSERT_EQ(
      output,
      "CRITICAL: Warn :firefox,services,svchost --- crit : _total,chrome");
  ASSERT_EQ(perf.size(), 12);
}
// test the complex rultes for warning
TEST(counter_check_windows, complex_rules_7) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "counter-filter":"any",
        "output-syntax": "${status}: Warn :{warn-list} --- crit : {crit-list}",
        "detail-syntax": "{label}",
        "warning-status": "chrome >= 25 && firefox >= 15 && svchost >= 20",
        "critical-status": "value >= 100",
        "warning-count": "2",
        "critical-count": "2",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  absl::flat_hash_map<std::string, double> data = {
      {"_total", 150.0},  {"svchost", 24.0}, {"explorer", 18.0},
      {"chrome", 42.0},   {"firefox", 36.0}, {"notepad", 5.0},
      {"winlogon", 12.0}, {"lsass", 16.0},   {"services", 28.0},
      {"csrss", 10.0}};

  checker.set_counter_data(data);

  std::string output;
  std::list<com::centreon::common::perfdata> perf;
  checker.compute(&output, &perf);
  ASSERT_EQ(output, "WARNING: Warn :chrome,firefox,svchost --- crit : _total");
}

// test the complex rules for critical status
TEST(counter_check_windows, complex_rules_8) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "counter-filter":"any",
        "output-syntax": "${status}: Warn :{warn-list} --- crit : {crit-list}",
        "detail-syntax": "{label}",
        "warning-status": "",
        "critical-status": "chrome >= 150 || firefox >= 150 || svchost >= 20",
        "critical-count": "1",
        "use_english": true
    })"_json;

  check_counter checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  absl::flat_hash_map<std::string, double> data = {
      {"_total", 150.0},  {"svchost", 24.0}, {"explorer", 18.0},
      {"chrome", 42.0},   {"firefox", 36.0}, {"notepad", 5.0},
      {"winlogon", 12.0}, {"lsass", 16.0},   {"services", 28.0},
      {"csrss", 10.0}};

  checker.set_counter_data(data);

  std::string output;
  std::list<com::centreon::common::perfdata> perf;
  checker.compute(&output, &perf);
  ASSERT_EQ(output, "CRITICAL: Warn : --- crit : svchost");
}