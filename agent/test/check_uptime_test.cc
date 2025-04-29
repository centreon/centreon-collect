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

#include "check_uptime.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace std::string_literals;

TEST(native_check_uptime, ok) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "345600", "critical-uptime" : "172800"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status =
      checker.compute((86400 * 5 + 3600 + 60 + 1) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "OK: System uptime is: 5d 1h 1m 1s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 * 5 + 3600 + 60 + 1);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_EQ(perf.critical(), 172800);
  ASSERT_EQ(perf.warning(), 345600);
  ASSERT_EQ(perf.critical_low(), 0);
  ASSERT_EQ(perf.warning_low(), 0);
}

TEST(native_check_uptime, ok_m) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "5760", "critical-uptime" : "2880", "unit": "m"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status =
      checker.compute((86400 * 5 + 3600 + 60 + 1) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "OK: System uptime is: 5d 1h 1m 1s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 * 5 + 3600 + 60 + 1);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_EQ(perf.critical(), 172800);
  ASSERT_EQ(perf.warning(), 345600);
  ASSERT_EQ(perf.critical_low(), 0);
  ASSERT_EQ(perf.warning_low(), 0);
}

TEST(native_check_uptime, ok_h) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "96", "critical-uptime" : "48", "unit": "h"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status =
      checker.compute((86400 * 5 + 3600 + 60 + 1) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "OK: System uptime is: 5d 1h 1m 1s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 * 5 + 3600 + 60 + 1);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_EQ(perf.critical(), 172800);
  ASSERT_EQ(perf.warning(), 345600);
  ASSERT_EQ(perf.critical_low(), 0);
  ASSERT_EQ(perf.warning_low(), 0);
}

TEST(native_check_uptime, ok_d) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "4", "critical-uptime" : "2", "unit": "d"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status =
      checker.compute((86400 * 5 + 3600 + 60 + 1) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "OK: System uptime is: 5d 1h 1m 1s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 * 5 + 3600 + 60 + 1);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_EQ(perf.critical(), 172800);
  ASSERT_EQ(perf.warning(), 345600);
  ASSERT_EQ(perf.critical_low(), 0);
  ASSERT_EQ(perf.warning_low(), 0);
}

TEST(native_check_uptime, ok_w) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "2", "critical-uptime" : "1", "unit": "w"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status =
      checker.compute((86400 * 14 + 3600 + 60 + 1) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "OK: System uptime is: 14d 1h 1m 1s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 * 14 + 3600 + 60 + 1);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_EQ(perf.critical(), 7 * 86400);
  ASSERT_EQ(perf.warning(), 14 * 86400);
  ASSERT_EQ(perf.critical_low(), 0);
  ASSERT_EQ(perf.warning_low(), 0);
}

TEST(native_check_uptime, warning) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "4", "critical-uptime" : "2", "unit": "d"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status =
      checker.compute((86400 * 3 + 3600 + 1) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::warning);
  ASSERT_EQ(output, "WARNING: System uptime is: 3d 1h 0m 1s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 * 3 + 3600 + 1);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_EQ(perf.critical(), 172800);
  ASSERT_EQ(perf.warning(), 345600);
  ASSERT_EQ(perf.critical_low(), 0);
  ASSERT_EQ(perf.warning_low(), 0);
}

TEST(native_check_uptime, warning_bis) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "4", "critical-uptime" : "", "unit": "d"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status =
      checker.compute((86400 * 3 + 3600 + 1) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::warning);
  ASSERT_EQ(output, "WARNING: System uptime is: 3d 1h 0m 1s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 * 3 + 3600 + 1);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_TRUE(std::isnan(perf.critical()));
  ASSERT_EQ(perf.warning(), 345600);
  ASSERT_TRUE(std::isnan(perf.critical_low()));
  ASSERT_EQ(perf.warning_low(), 0);
}

TEST(native_check_uptime, critical) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "4", "critical-uptime" : "2", "unit": "d"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status = checker.compute((86400 + 3600 * 4) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::critical);
  ASSERT_EQ(output, "CRITICAL: System uptime is: 1d 4h 0m 0s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 + 3600 * 4);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_EQ(perf.critical(), 172800);
  ASSERT_EQ(perf.warning(), 345600);
  ASSERT_EQ(perf.critical_low(), 0);
  ASSERT_EQ(perf.warning_low(), 0);
}

TEST(native_check_uptime, critical_bis) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "warning-uptime" : "", "critical-uptime" : "2", "unit": "d"})"_json;

  check_uptime checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;
  e_status status = checker.compute((86400 + 3600 * 4) * 1000, &output, &perf);
  ASSERT_EQ(status, e_status::critical);
  ASSERT_EQ(output, "CRITICAL: System uptime is: 1d 4h 0m 0s");
  ASSERT_EQ(perf.name(), "uptime");
  ASSERT_EQ(perf.unit(), "s");
  ASSERT_EQ(perf.value(), 86400 + 3600 * 4);
  ASSERT_EQ(perf.min(), 0);
  ASSERT_EQ(perf.critical(), 172800);
  ASSERT_TRUE(std::isnan(perf.warning()));
  ASSERT_EQ(perf.critical_low(), 0);
  ASSERT_TRUE(std::isnan(perf.warning_low()));
}
