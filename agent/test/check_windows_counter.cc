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
      "output-syntax": "${status}: ${label} : ${value}",
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

  ASSERT_EQ(perf.size(), 2);
  ASSERT_EQ(perf.front().name(), "warning-count");
  ASSERT_EQ(perf.front().value(), 1.0);
  ASSERT_EQ(perf.back().name(), "critical-count");

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
      R"({"counter": "\\Thread(*)\\Context Switches/sec",
        "critical-status": "any >= 50",
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
  output
*/
TEST(counter_check_windows, complex_rules) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"counter": "\\Process V2(*)\\Thread Count",
        "pref-display-syntax":"any",
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

  std::string output;
  std::list<com::centreon::common::perfdata> perf;

  checker.pdh_snapshot(true);
  auto data_size = checker.get_size_data();

  e_status status = checker.compute(&output, &perf);

  ASSERT_NE(output.size(), 0);
  // the first filter will trigger the warning status and we will have only
  // _total as label_warning
  ASSERT_EQ(output.find("WARNING: _total"), 0);
  // but for the perfdata we will have all the labels (warning and critical
  // count are always added) the resut should be greter than 2
  ASSERT_GE(perf.size(), 2);
  ASSERT_EQ(status, e_status::warning);
}
