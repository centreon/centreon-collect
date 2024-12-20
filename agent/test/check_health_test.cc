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
#include <cmath>
#include "check.hh"
#include "com/centreon/common/rapidjson_helper.hh"

#include "check_health.hh"
#include "config.hh"
#include "version.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace std::string_literals;
using namespace com::centreon::common::literals;
using namespace std::chrono_literals;

TEST(check_health_test, no_threshold_no_reverse) {
  config::load(false);

  rapidjson::Document check_args =
      R"({ "warning-interval" : "", "critical-interval" : ""})"_json;

  auto stats = std::make_shared<checks_statistics>();

  stats->add_interval_stat("command1", 10s);
  stats->add_duration_stat("command1", 20s);
  stats->add_interval_stat("command2", 15s);
  stats->add_duration_stat("command2", 25s);

  check_health checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      stats);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status ret = checker.compute(&output, &perfs);
  EXPECT_EQ(ret, e_status::ok);
  EXPECT_EQ(output, "OK: Version: " CENTREON_AGENT_VERSION
                    " - Connection mode: Agent initiated - Current "
                    "configuration: 2 checks - Average runtime: 22s");
  EXPECT_EQ(perfs.size(), 2);
  for (const auto& perf : perfs) {
    EXPECT_EQ(perf.unit(), "s");
    EXPECT_TRUE(std::isnan(perf.warning_low()));
    EXPECT_TRUE(std::isnan(perf.warning()));
    EXPECT_TRUE(std::isnan(perf.critical_low()));
    EXPECT_TRUE(std::isnan(perf.critical()));
    if (perf.name() == "runtime") {
      EXPECT_EQ(perf.value(), 25);
    } else if (perf.name() == "interval") {
      EXPECT_EQ(perf.value(), 15);
    } else {
      FAIL() << "Unexpected perfdata name: " << perf.name();
    }
  }
}

TEST(check_health_test, no_threshold_reverse) {
  config::load(true);

  rapidjson::Document check_args =
      R"({ "warning-interval" : "", "critical-interval" : ""})"_json;

  auto stats = std::make_shared<checks_statistics>();

  stats->add_interval_stat("command1", 10s);
  stats->add_duration_stat("command1", 20s);
  stats->add_interval_stat("command2", 15s);
  stats->add_duration_stat("command2", 25s);

  check_health checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      stats);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status ret = checker.compute(&output, &perfs);
  EXPECT_EQ(ret, e_status::ok);
  EXPECT_EQ(output, "OK: Version: " CENTREON_AGENT_VERSION
                    " - Connection mode: Poller initiated - Current "
                    "configuration: 2 checks - Average runtime: 22s");
  EXPECT_EQ(perfs.size(), 2);
  for (const auto& perf : perfs) {
    EXPECT_EQ(perf.unit(), "s");
    EXPECT_TRUE(std::isnan(perf.warning_low()));
    EXPECT_TRUE(std::isnan(perf.warning()));
    EXPECT_TRUE(std::isnan(perf.critical_low()));
    EXPECT_TRUE(std::isnan(perf.critical()));
    if (perf.name() == "runtime") {
      EXPECT_EQ(perf.value(), 25);
    } else if (perf.name() == "interval") {
      EXPECT_EQ(perf.value(), 15);
    } else {
      FAIL() << "Unexpected perfdata name: " << perf.name();
    }
  }
}

TEST(check_health_test, threshold_1) {
  config::load(true);

  rapidjson::Document check_args =
      R"({ "warning-interval" : "9", "critical-interval" : "14"})"_json;

  auto stats = std::make_shared<checks_statistics>();

  stats->add_interval_stat("command1", 10s);
  stats->add_duration_stat("command1", 20s);
  stats->add_interval_stat("command2", 15s);
  stats->add_duration_stat("command2", 25s);

  check_health checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      stats);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status ret = checker.compute(&output, &perfs);
  EXPECT_EQ(ret, e_status::critical);
  EXPECT_EQ(output,
            "CRITICAL: command2 runtime:25s interval:15s - WARNING: command1 "
            "runtime:20s interval:10s - Version: " CENTREON_AGENT_VERSION
            " - Connection mode: Poller initiated - Current configuration: 2 "
            "checks - Average runtime: 22s");
  EXPECT_EQ(perfs.size(), 2);
  for (const auto& perf : perfs) {
    EXPECT_EQ(perf.unit(), "s");
    if (perf.name() == "runtime") {
      EXPECT_TRUE(std::isnan(perf.warning_low()));
      EXPECT_TRUE(std::isnan(perf.warning()));
      EXPECT_TRUE(std::isnan(perf.critical_low()));
      EXPECT_TRUE(std::isnan(perf.critical()));
      EXPECT_EQ(perf.value(), 25);
    } else if (perf.name() == "interval") {
      EXPECT_EQ(perf.value(), 15);
      EXPECT_EQ(perf.warning_low(), 0);
      EXPECT_EQ(perf.warning(), 9);
      EXPECT_EQ(perf.critical_low(), 0);
      EXPECT_EQ(perf.critical(), 14);
    } else {
      FAIL() << "Unexpected perfdata name: " << perf.name();
    }
  }
}

TEST(check_health_test, threshold_2) {
  config::load(true);

  rapidjson::Document check_args =
      R"({ "warning-interval" : "9", "critical-interval" : "14", "warning-runtime": 19, "critical-runtime":24})"_json;

  auto stats = std::make_shared<checks_statistics>();

  stats->add_interval_stat("command1", 10s);
  stats->add_duration_stat("command1", 20s);
  stats->add_interval_stat("command2", 15s);
  stats->add_duration_stat("command2", 25s);

  check_health checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      stats);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status ret = checker.compute(&output, &perfs);
  EXPECT_EQ(ret, e_status::critical);
  EXPECT_EQ(output,
            "CRITICAL: command2 runtime:25s interval:15s - WARNING: command1 "
            "runtime:20s interval:10s - Version: " CENTREON_AGENT_VERSION
            " - Connection mode: Poller initiated - Current configuration: 2 "
            "checks - Average runtime: 22s");
  EXPECT_EQ(perfs.size(), 2);
  for (const auto& perf : perfs) {
    EXPECT_EQ(perf.unit(), "s");
    if (perf.name() == "runtime") {
      EXPECT_EQ(perf.value(), 25);
      EXPECT_EQ(perf.warning_low(), 0);
      EXPECT_EQ(perf.warning(), 19);
      EXPECT_EQ(perf.critical_low(), 0);
      EXPECT_EQ(perf.critical(), 24);
    } else if (perf.name() == "interval") {
      EXPECT_EQ(perf.value(), 15);
      EXPECT_EQ(perf.warning_low(), 0);
      EXPECT_EQ(perf.warning(), 9);
      EXPECT_EQ(perf.critical_low(), 0);
      EXPECT_EQ(perf.critical(), 14);
    } else {
      FAIL() << "Unexpected perfdata name: " << perf.name();
    }
  }
}

TEST(check_health_test, threshold_3) {
  config::load(true);

  rapidjson::Document check_args =
      R"({ "warning-interval" : "", "critical-interval" : "14", "warning-runtime": 19})"_json;

  auto stats = std::make_shared<checks_statistics>();

  stats->add_interval_stat("command1", 10s);
  stats->add_duration_stat("command1", 20s);
  stats->add_interval_stat("command2", 15s);
  stats->add_duration_stat("command2", 25s);

  check_health checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      stats);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status ret = checker.compute(&output, &perfs);
  EXPECT_EQ(ret, e_status::critical);
  EXPECT_EQ(output,
            "CRITICAL: command2 runtime:25s interval:15s - WARNING: command1 "
            "runtime:20s interval:10s - Version: " CENTREON_AGENT_VERSION
            " - Connection mode: Poller initiated - Current configuration: 2 "
            "checks - Average runtime: 22s");
  EXPECT_EQ(perfs.size(), 2);
  for (const auto& perf : perfs) {
    EXPECT_EQ(perf.unit(), "s");
    if (perf.name() == "runtime") {
      EXPECT_EQ(perf.value(), 25);
      EXPECT_EQ(perf.warning_low(), 0);
      EXPECT_EQ(perf.warning(), 19);
      EXPECT_TRUE(std::isnan(perf.critical_low()));
      EXPECT_TRUE(std::isnan(perf.critical()));
    } else if (perf.name() == "interval") {
      EXPECT_EQ(perf.value(), 15);
      EXPECT_TRUE(std::isnan(perf.warning_low()));
      EXPECT_TRUE(std::isnan(perf.warning()));
      EXPECT_EQ(perf.critical_low(), 0);
      EXPECT_EQ(perf.critical(), 14);
    } else {
      FAIL() << "Unexpected perfdata name: " << perf.name();
    }
  }
}

TEST(check_health_test, threshold_4) {
  config::load(true);

  rapidjson::Document check_args =
      R"({ "warning-interval" : "", "critical-interval" : "16", "warning-runtime": 19})"_json;

  auto stats = std::make_shared<checks_statistics>();

  stats->add_interval_stat("command1", 10s);
  stats->add_duration_stat("command1", 20s);
  stats->add_interval_stat("command2", 15s);
  stats->add_duration_stat("command2", 25s);

  check_health checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      stats);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;
  e_status ret = checker.compute(&output, &perfs);
  EXPECT_EQ(ret, e_status::warning);
  EXPECT_EQ(output,
            "WARNING: command2 runtime:25s interval:15s, command1 runtime:20s "
            "interval:10s - Version: " CENTREON_AGENT_VERSION
            " - Connection mode: Poller initiated - Current configuration: 2 "
            "checks - Average runtime: 22s");
  EXPECT_EQ(perfs.size(), 2);
  for (const auto& perf : perfs) {
    EXPECT_EQ(perf.unit(), "s");
    if (perf.name() == "runtime") {
      EXPECT_EQ(perf.value(), 25);
      EXPECT_EQ(perf.warning_low(), 0);
      EXPECT_EQ(perf.warning(), 19);
      EXPECT_TRUE(std::isnan(perf.critical_low()));
      EXPECT_TRUE(std::isnan(perf.critical()));
    } else if (perf.name() == "interval") {
      EXPECT_EQ(perf.value(), 15);
      EXPECT_TRUE(std::isnan(perf.warning_low()));
      EXPECT_TRUE(std::isnan(perf.warning()));
      EXPECT_EQ(perf.critical_low(), 0);
      EXPECT_EQ(perf.critical(), 16);
    } else {
      FAIL() << "Unexpected perfdata name: " << perf.name();
    }
  }
}
