/**
 * Copyright 2025 Centreon
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

#include "check_sched.hh"

//  Include the task header file.

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace std::string_literals;
using namespace com::centreon::common::literals;
using namespace std::chrono;

tasksched_data g_data1, g_data2, g_data3;

class check_windows_sched : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    auto make_date_info = [](std::chrono::system_clock::time_point when) {
      std::time_t t = std::chrono::system_clock::to_time_t(when);
      std::tm tm_buf{};
      localtime_s(&tm_buf, &t);
      std::ostringstream oss;
      oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
      return DateInfo{when, oss.str()};
    };

    auto now = system_clock::now();

    g_data1.name = "t1";
    g_data1.folder = R"(\Tasks\One\)";
    g_data1.enabled = true;
    g_data1.last_run = make_date_info(now);
    g_data1.next_run = make_date_info(now + seconds(30));
    g_data1.exit_code = 0;
    g_data1.state = TASK_STATE_READY;
    g_data1.author = "me";
    g_data1.description = "dummy";
    g_data1.number_missed_runs = 0;

    g_data2.name = "t2";
    g_data2.folder = R"(\Tasks\Two\)";
    g_data2.enabled = false;
    g_data2.last_run = make_date_info(now - seconds(60));
    g_data2.next_run = make_date_info(now + seconds(60));
    g_data2.exit_code = 0;
    g_data2.state = TASK_STATE_RUNNING;
    g_data2.author = "me2";
    g_data2.description = "dummy2";
    g_data2.number_missed_runs = 1;

    g_data3.name = "t3";
    g_data3.folder = R"(\Tasks\Three\)";
    g_data3.enabled = true;
    g_data3.last_run = make_date_info(now - seconds(120));
    g_data3.next_run = make_date_info(now + seconds(120));
    g_data3.exit_code = 1;
    g_data3.state = TASK_STATE_READY;
    g_data3.author = "me3";
    g_data3.description = "dummy3";
    g_data3.number_missed_runs = 10;
  }
};
/*
  Test default check_sched,

*/
/**
 * @brief Default check_sched constructor and compute test.
 *
 * This test validates the default behavior of the check_sched component
 * the default configuration.
 * output-syntax : "{status}: {problem_list}"
 * task-detail-syntax : "${path}: ${exit_code}"
 * filter-tasks : "enabled == 1"
 * warning-status : "exit_code != 0x0"
 * critical-status : "exit_code < 0x0"
 * warning-count : 1
 * critical-count : 1
 * verbose : false
 */
TEST_F(check_windows_sched, default_check_sched) {
  rapidjson::Document json_config = R"({
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  // -------------------------------------------------------
  // case 1 - No task match the warning or critical status
  // task 1 is enabled, exit_code is 0, so it should be OK
  // task 2 should not be checked exit_code = 1
  // task 3 is enabled, exit_code is 0, so it should be OK

  // set data
  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  // Set exit_code to 1 for g_data2
  checker.get_mutable_tasks()[g_data2.name].exit_code = 1;
  // Set exit_code to 0 for g_data3
  checker.get_mutable_tasks()[g_data3.name].exit_code = 0;

  checker.apply_filter();

  e_status status = checker.compute(&output, &perfs);

  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "OK: No problem tasks found");
  ASSERT_EQ(perfs.size(),
            5);  // 2 tasks(t1,t3) + 3 additional perfdata for ok,warning and
                 // critical count

  // -------------------------------------------------------
  // case 2 - No task match the filter-tasks
  // task 1 is disabled
  // task 2 is disabled
  // task 3 is disabled

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  checker.get_mutable_tasks()[g_data1.name].enabled = false;
  checker.get_mutable_tasks()[g_data2.name].enabled = false;
  checker.get_mutable_tasks()[g_data3.name].enabled = false;
  output.clear();
  perfs.clear();

  checker.apply_filter();

  status = checker.compute(&output, &perfs);
  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "Empty or no match for this filter");
  ASSERT_TRUE(perfs.empty());
  // -------------------------------------------------------
  // case 3 - All tasks match the filter-tasks, but t2,t3 has exit_code != 0
  // task 1 is enabled, exit_code is 0, so it should be OK
  // task 2 is enabled, exit_code is 1, so it should be WARNING
  // task 3 is enabled, exit_code is 1, so it should be WARNING

  // set data
  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  // Enable all tasks
  checker.get_mutable_tasks()[g_data1.name].enabled = true;
  checker.get_mutable_tasks()[g_data2.name].enabled = true;
  checker.get_mutable_tasks()[g_data3.name].enabled = true;

  // Set exit_code to 1 for t2,t3
  checker.get_mutable_tasks()[g_data2.name].exit_code = 1;
  checker.get_mutable_tasks()[g_data3.name].exit_code = 1;
  output.clear();
  perfs.clear();

  checker.apply_filter();

  status = checker.compute(&output, &perfs);
  ASSERT_EQ(status, e_status::warning);
  ASSERT_EQ(output, "WARNING: \\Tasks\\Two\\t2: 0x1,\\Tasks\\Three\\t3: 0x1");
  ASSERT_EQ(
      perfs.size(),
      6);  // 3 tasks + 3 additional perfdata for ok,warning/critical count
  // check perfdata
  auto it = std::find_if(perfs.begin(), perfs.end(),
                         [](const com::centreon::common::perfdata& perf) {
                           return perf.name() == "warning_count";
                         });
  ASSERT_NE(it, perfs.end());
  ASSERT_EQ(it->value(), 2.0f);  // t2,t3 are in warning state
  it = std::find_if(perfs.begin(), perfs.end(),
                    [](const com::centreon::common::perfdata& perf) {
                      return perf.name() == "critical_count";
                    });
  ASSERT_NE(it, perfs.end());
  ASSERT_EQ(it->value(), 0.0f);  // no task in critical state

  // -------------------------------------------------------
  // case 4 - All tasks match the filter-tasks, but t2,t3 has exit_code < 0
  // task 1 is enabled, exit_code is 0, so it should be OK
  // task 2 is enabled, exit_code is -2, so it should be CRITICAL
  // task 3 is enabled, exit_code is -2, so it should be CRITICAL
  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  // Enable all tasks
  checker.get_mutable_tasks()[g_data1.name].enabled = true;
  checker.get_mutable_tasks()[g_data2.name].enabled = true;
  checker.get_mutable_tasks()[g_data3.name].enabled = true;

  // Set exit_code to -1 for t2 and -2 for t3
  checker.get_mutable_tasks()[g_data2.name].exit_code = -1;
  checker.get_mutable_tasks()[g_data3.name].exit_code = -2;

  output.clear();
  perfs.clear();

  checker.apply_filter();

  status = checker.compute(&output, &perfs);
  ASSERT_EQ(status, e_status::critical);
  ASSERT_EQ(output,
            "CRITICAL: \\Tasks\\Two\\t2: "
            "0xffffffff,\\Tasks\\Three\\t3: 0xfffffffe");
  ASSERT_EQ(
      perfs.size(),
      6);  // 3 tasks + 3 additional perfdata for ok,warning/critical count

  // check perfdata
  it = std::find_if(perfs.begin(), perfs.end(),
                    [](const com::centreon::common::perfdata& perf) {
                      return perf.name() == "warning_count";
                    });
  ASSERT_NE(it, perfs.end());
  ASSERT_EQ(it->value(), 0.0f);  // no task in warning state
  it = std::find_if(perfs.begin(), perfs.end(),
                    [](const com::centreon::common::perfdata& perf) {
                      return perf.name() == "critical_count";
                    });
  ASSERT_NE(it, perfs.end());
  ASSERT_EQ(it->value(), 2.0f);  // t2,t3 are in critical state
}

TEST_F(check_windows_sched, filter_task1) {
  rapidjson::Document json_config = R"({
  "filter-tasks": "(enabled == 1 && missed_runs == 0) || state == 'running'"
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  auto& tasks = checker.get_mutable_tasks();
  ASSERT_EQ(tasks.size(), 3);

  auto predicate = [&](auto const& pair) {
    return !checker.get_task_filter()->check(pair.second);
  };

  absl::erase_if(tasks, predicate);

  ASSERT_EQ(tasks.size(), 2);
  ASSERT_TRUE(tasks.find(g_data1.name) != tasks.end());
  ASSERT_TRUE(tasks.find(g_data2.name) != tasks.end());
}

TEST_F(check_windows_sched, filter_task2) {
  rapidjson::Document json_config = R"({
  "filter-tasks": "enabled == 1"
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  auto& tasks = checker.get_mutable_tasks();
  ASSERT_EQ(tasks.size(), 3);

  auto predicate = [&](auto const& pair) {
    return !checker.get_task_filter()->check(pair.second);
  };

  absl::erase_if(tasks, predicate);

  ASSERT_EQ(tasks.size(), 2);
  ASSERT_TRUE(tasks.find(g_data1.name) != tasks.end());
  ASSERT_TRUE(tasks.find(g_data3.name) != tasks.end());
}

TEST_F(check_windows_sched, filter_task3) {
  rapidjson::Document json_config = R"t({
  "filter-tasks": "name in ('t1','t3')"
  })t"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  auto& tasks = checker.get_mutable_tasks();
  EXPECT_EQ(tasks.size(), 3);

  auto predicate = [&](auto const& pair) {
    return !checker.get_task_filter()->check(pair.second);
  };

  absl::erase_if(tasks, predicate);

  EXPECT_EQ(tasks.size(), 2);
  EXPECT_TRUE(tasks.find(g_data1.name) != tasks.end());
  EXPECT_TRUE(tasks.find(g_data3.name) != tasks.end());
}

TEST_F(check_windows_sched, warning_filter) {
  rapidjson::Document json_config = R"({
  "filter-tasks": "",
  "warning-status": "exit_code != 0 || missed_runs > 0"
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  auto& tasks = checker.get_mutable_tasks();
  ASSERT_EQ(tasks.size(), 3);

  std::vector<std::string> result;
  for (const auto& [name, data] : tasks) {
    if (checker.get_warning_rules_filter()->check(data)) {
      result.push_back(name);
    }
  }

  ASSERT_EQ(result.size(), 2);
  ASSERT_TRUE(std::find(result.begin(), result.end(), g_data2.name) !=
              result.end());
  ASSERT_TRUE(std::find(result.begin(), result.end(), g_data3.name) !=
              result.end());
}

TEST_F(check_windows_sched, exclude_tasks) {
  rapidjson::Document json_config = R"({
  "filter-tasks": "enabled == 1",
  "exclude-tasks": "t1"
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  auto& tasks = checker.get_mutable_tasks();
  ASSERT_EQ(tasks.size(), 3);

  auto predicate = [&](auto const& task_data) {
    if (checker.get_exclude_tasks().contains(task_data.first)) {
      return true;  // Exclude this task
    }
    if (!checker.get_task_filter()->check(task_data.second))
      return true;  // Exclude this task if it does not match the filter

    return false;
  };

  absl::erase_if(tasks, predicate);

  ASSERT_EQ(tasks.size(), 1);
  ASSERT_TRUE(tasks.find(g_data3.name) != tasks.end());
}

TEST_F(check_windows_sched, output_format) {
  rapidjson::Document json_config = R"({
  "output-syntax": "${status}: Ok:{ok_count}|Nok:{problem_count}|total:{total}  warning:{warn_count}|critical:{crit_count}",
  "task-detail-syntax": "${folder}${name}: ${exit_code}",
  "warning-status": "exit_code != 0",
  "critical-status": "exit_code < 0"
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  checker.get_mutable_tasks()[g_data2.name].enabled = true;
  checker.get_mutable_tasks()[g_data2.name].exit_code = 1;
  checker.get_mutable_tasks()[g_data3.name].exit_code =
      -2147160572;  // 0x8004EE04

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = checker.compute(&output, &perfs);
  ASSERT_EQ(output, "CRITICAL: Ok:1|Nok:2|total:3  warning:1|critical:1");
}

TEST_F(check_windows_sched, output_format_2) {
  rapidjson::Document json_config = R"({
  "output-syntax": "${status}: {ok_list}",
  "task-detail-syntax": "${folder}${name}({state}): ${exit_code},${next_run},${last_run},${missed_runs},${author},${enabled}",
  "warning-status": "exit_code != 0",
  "critical-status": "exit_code < 0"
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  checker.get_mutable_tasks()[g_data2.name].enabled = true;
  checker.get_mutable_tasks()[g_data2.name].exit_code = 1;
  checker.get_mutable_tasks()[g_data3.name].exit_code =
      -2147160572;  // 0x8004EE04

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = checker.compute(&output, &perfs);
  std::string expected_output = "CRITICAL: \\Tasks\\One\\t1(ready): 0x0,";
  expected_output += g_data1.next_run.formatted + ",";
  expected_output += g_data1.last_run.formatted + ",0,me,True";
  ASSERT_EQ(output, expected_output);
}

TEST_F(check_windows_sched, filter_by_name_and_author) {
  rapidjson::Document json_config = R"({
  "filter-tasks": "name == 't1' || author == 'me3'",
  "verbose": true
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  auto& tasks = checker.get_mutable_tasks();
  ASSERT_EQ(tasks.size(), 3);

  auto predicate = [&](auto const& task_data) {
    if (checker.get_exclude_tasks().contains(task_data.first)) {
      return true;  // Exclude this task
    }
    if (!checker.get_task_filter()->check(task_data.second))
      return true;  // Exclude this task if it does not match the filter

    return false;
  };

  absl::erase_if(tasks, predicate);

  ASSERT_EQ(tasks.size(), 2);
  ASSERT_TRUE(tasks.find(g_data1.name) != tasks.end());
  ASSERT_TRUE(tasks.find(g_data3.name) != tasks.end());
}

// only task t1 have last_run <= 50s
TEST_F(check_windows_sched, filter_by_last_run) {
  rapidjson::Document json_config = R"({
  "filter-tasks": "last_run <= 50s",
  "verbose": true
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  checker.get_mutable_tasks()[g_data1.name].duration_last_run = 50;
  checker.get_mutable_tasks()[g_data2.name].duration_last_run = 60;
  checker.get_mutable_tasks()[g_data3.name].duration_last_run = 120;

  checker.apply_filter();

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = checker.compute(&output, &perfs);
  EXPECT_EQ(status, e_status::ok);
  std::string expected_output =
      "OK: Ok:1|Nok:0|total:1  warning:0|critical:0\n"
      "\\Tasks\\One\\t1: last run: " +
      g_data1.last_run.formatted + " next run " + g_data1.next_run.formatted +
      " (exit code: 0x0)\n";
  ASSERT_EQ(output, expected_output);
}

TEST_F(check_windows_sched, warning_last_run) {
  rapidjson::Document json_config = R"({
  "warning-status": "last_run <= 119s && last_run > 60s"
  })"_json;

  check_sched checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, json_config, nullptr,
      [](const std::shared_ptr<check>&, int,
         const std::list<com::centreon::common::perfdata>&,
         const std::list<std::string>&) {},
      std::make_shared<checks_statistics>());

  checker.get_mutable_tasks()[g_data1.name] = g_data1;
  checker.get_mutable_tasks()[g_data2.name] = g_data2;
  checker.get_mutable_tasks()[g_data3.name] = g_data3;

  checker.get_mutable_tasks()[g_data2.name].enabled = true;

  checker.get_mutable_tasks()[g_data1.name].duration_last_run = 121;
  checker.get_mutable_tasks()[g_data2.name].duration_last_run = 61;
  checker.get_mutable_tasks()[g_data3.name].duration_last_run = 0;

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status status = checker.compute(&output, &perfs);
  ASSERT_EQ(status, e_status::warning);
  std::string expected_output = "WARNING: \\Tasks\\Two\\t2: 0x0";
  ASSERT_EQ(output, expected_output);
}