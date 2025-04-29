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

#include "check_cpu.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace std::string_literals;

TEST(native_check_cpu_windows, construct) {
  M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION info;
  info.IdleTime.QuadPart = 60;
  info.KernelTime.QuadPart = 70;
  info.UserTime.QuadPart = 25;
  info.DpcTime.QuadPart = 1;
  info.InterruptTime.QuadPart = 5;
  check_cpu_detail::kernel_per_cpu_time k(info);
  ASSERT_EQ(k.get_proportional_value(check_cpu_detail::e_proc_stat_index::user),
            0.25);
  ASSERT_EQ(
      k.get_proportional_value(check_cpu_detail::e_proc_stat_index::system),
      0.1);
  ASSERT_EQ(k.get_proportional_value(check_cpu_detail::e_proc_stat_index::idle),
            0.6);
  ASSERT_EQ(
      k.get_proportional_value(check_cpu_detail::e_proc_stat_index::interrupt),
      0.05);
  ASSERT_EQ(k.get_proportional_value(check_cpu_detail::e_proc_stat_index::dpc),
            0.01);
  ASSERT_EQ(k.get_proportional_used(), 0.4);
}

constexpr M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION info[2] = {
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}};

TEST(native_check_cpu_windows, output_no_threshold) {
  check_cpu_detail::kernel_cpu_time_snapshot first(info, info + 2);

  M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION info2[2];
  info2[0].IdleTime.QuadPart = 60;
  info2[0].KernelTime.QuadPart = 70;
  info2[0].UserTime.QuadPart = 25;
  info2[0].DpcTime.QuadPart = 1;
  info2[0].InterruptTime.QuadPart = 5;

  info2[1].IdleTime.QuadPart = 40;
  info2[1].KernelTime.QuadPart = 50;
  info2[1].UserTime.QuadPart = 45;
  info2[1].DpcTime.QuadPart = 0;
  info2[1].InterruptTime.QuadPart = 5;

  check_cpu_detail::kernel_cpu_time_snapshot second(info2, info2 + 2);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  rapidjson::Document check_args;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  checker.compute(first, second, &output, &perfs);
  ASSERT_EQ(output, "OK: CPU(s) average usage is 50.00%");

  ASSERT_EQ(perfs.size(), 3);

  for (const auto& perf : perfs) {
    ASSERT_TRUE(std::isnan(perf.critical_low()));
    ASSERT_TRUE(std::isnan(perf.critical()));
    ASSERT_FALSE(perf.critical_mode());
    ASSERT_TRUE(std::isnan(perf.warning_low()));
    ASSERT_TRUE(std::isnan(perf.warning()));
    ASSERT_FALSE(perf.warning_mode());
    ASSERT_EQ(perf.min(), 0);
    ASSERT_EQ(perf.max(), 100);
    ASSERT_EQ(perf.unit(), "%");
    ASSERT_EQ(perf.value_type(), com::centreon::common::perfdata::gauge);
    if (perf.name() == "0#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 40.0, 0.01);
    } else if (perf.name() == "1#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 60.0, 0.01);
    } else if (perf.name() == "cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 50.0, 0.01);
    } else {
      FAIL() << "unexpected perfdata name:" << perf.name();
    }
  }
}

TEST(native_check_cpu_windows, output_no_threshold_detailed) {
  check_cpu_detail::kernel_cpu_time_snapshot first(info, info + 2);

  M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION info2[2];
  info2[0].IdleTime.QuadPart = 60;
  info2[0].KernelTime.QuadPart = 70;
  info2[0].UserTime.QuadPart = 25;
  info2[0].DpcTime.QuadPart = 1;
  info2[0].InterruptTime.QuadPart = 5;

  info2[1].IdleTime.QuadPart = 40;
  info2[1].KernelTime.QuadPart = 50;
  info2[1].UserTime.QuadPart = 45;
  info2[1].DpcTime.QuadPart = 0;
  info2[1].InterruptTime.QuadPart = 5;

  check_cpu_detail::kernel_cpu_time_snapshot second(info2, info2 + 2);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"cpu-detailed":true, "warning-core" : "", "critical-core" : ""})"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  checker.compute(first, second, &output, &perfs);
  ASSERT_EQ(output, "OK: CPU(s) average usage is 50.00%");

  ASSERT_EQ(perfs.size(), 18);

  for (const auto& perf : perfs) {
    ASSERT_TRUE(std::isnan(perf.critical_low()));
    ASSERT_TRUE(std::isnan(perf.critical()));
    ASSERT_FALSE(perf.critical_mode());
    ASSERT_TRUE(std::isnan(perf.warning_low()));
    ASSERT_TRUE(std::isnan(perf.warning()));
    ASSERT_FALSE(perf.warning_mode());
    ASSERT_EQ(perf.min(), 0);
    ASSERT_EQ(perf.max(), 100);
    ASSERT_EQ(perf.unit(), "%");
    ASSERT_EQ(perf.value_type(), com::centreon::common::perfdata::gauge);

    if (perf.name() == "0~user#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 25.0, 0.01);
    } else if (perf.name() == "1~user#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 45.0, 0.01);
    } else if (perf.name() == "user#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 35.0, 0.01);
    } else if (perf.name() == "0~system#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 10.0, 0.01);
    } else if (perf.name() == "1~system#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 10.0, 0.01);
    } else if (perf.name() == "system#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 10.0, 0.01);
    } else if (perf.name() == "0~idle#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 60.0, 0.01);
    } else if (perf.name() == "1~idle#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 40.0, 0.01);
    } else if (perf.name() == "idle#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 50.0, 0.01);
    } else if (perf.name() == "0~interrupt#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 5.0, 0.01);
    } else if (perf.name() == "1~interrupt#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 5.0, 0.01);
    } else if (perf.name() == "interrupt#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 5.0, 0.01);
    } else if (perf.name() ==
               "0~dpc_interrupt#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 1.0, 0.01);
    } else if (perf.name() ==
               "1~dpc_interrupt#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 0.0, 0.01);
    } else if (perf.name() == "dpc_interrupt#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 0.5, 0.01);
    } else if (perf.name() == "0~used#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 40.0, 0.01);
    } else if (perf.name() == "1~used#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 60.0, 0.01);
    } else if (perf.name() == "used#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 50.0, 0.01);
    } else {
      FAIL() << "unexpected perfdata name:" << perf.name();
    }
  }
}

TEST(native_check_cpu_windows, output_threshold) {
  check_cpu_detail::kernel_cpu_time_snapshot first(info, info + 2);

  M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION info2[2];
  info2[0].IdleTime.QuadPart = 60;
  info2[0].KernelTime.QuadPart = 70;
  info2[0].UserTime.QuadPart = 25;
  info2[0].DpcTime.QuadPart = 1;
  info2[0].InterruptTime.QuadPart = 5;

  info2[1].IdleTime.QuadPart = 40;
  info2[1].KernelTime.QuadPart = 50;
  info2[1].UserTime.QuadPart = 45;
  info2[1].DpcTime.QuadPart = 0;
  info2[1].InterruptTime.QuadPart = 5;

  check_cpu_detail::kernel_cpu_time_snapshot second(info2, info2 + 2);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"cpu-detailed":"", "warning-core" : "39", "critical-core" : "59", "warning-average" : "49", "critical-average" : "60"})"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  checker.compute(first, second, &output, &perfs);
  ASSERT_EQ(
      output,
      "WARNING: CPU'0' Usage: 40.00%, User 25.00%, System 10.00%, Idle 60.00%, "
      "Interrupt 5.00%, Dpc Interrupt 1.00% CRITICAL: CPU'1' Usage: 60.00%, "
      "User 45.00%, System 10.00%, Idle 40.00%, Interrupt 5.00%, Dpc Interrupt "
      "0.00% WARNING: CPU(s) average Usage: 50.00%, User 35.00%, System "
      "10.00%, Idle 50.00%, Interrupt 5.00%, Dpc Interrupt 0.50%");

  ASSERT_EQ(perfs.size(), 3);

  for (const auto& perf : perfs) {
    if (perf.name() == "cpu.utilization.percentage") {
      ASSERT_NEAR(perf.warning(), 49.0, 0.01);
      ASSERT_NEAR(perf.critical(), 60.0, 0.01);
    } else {
      ASSERT_NEAR(perf.warning(), 39.0, 0.01);
      ASSERT_NEAR(perf.critical(), 59.0, 0.01);
    }
    ASSERT_EQ(perf.warning_low(), 0);
    ASSERT_EQ(perf.critical_low(), 0);
    ASSERT_FALSE(perf.critical_mode());
    ASSERT_FALSE(perf.warning_mode());
    ASSERT_EQ(perf.min(), 0);
    ASSERT_EQ(perf.max(), 100);
    ASSERT_EQ(perf.unit(), "%");
    ASSERT_EQ(perf.value_type(), com::centreon::common::perfdata::gauge);
    if (perf.name() == "0#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 40.0, 0.01);
    } else if (perf.name() == "1#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 60.0, 0.01);
    } else if (perf.name() == "cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 50.0, 0.01);
    } else {
      FAIL() << "unexpected perfdata name:" << perf.name();
    }
  }
}

TEST(native_check_cpu_windows, output_threshold_detailed) {
  check_cpu_detail::kernel_cpu_time_snapshot first(info, info + 2);

  M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION info2[2];
  info2[0].IdleTime.QuadPart = 60;
  info2[0].KernelTime.QuadPart = 70;
  info2[0].UserTime.QuadPart = 25;
  info2[0].DpcTime.QuadPart = 1;
  info2[0].InterruptTime.QuadPart = 5;

  info2[1].IdleTime.QuadPart = 40;
  info2[1].KernelTime.QuadPart = 50;
  info2[1].UserTime.QuadPart = 45;
  info2[1].DpcTime.QuadPart = 0;
  info2[1].InterruptTime.QuadPart = 5;

  check_cpu_detail::kernel_cpu_time_snapshot second(info2, info2 + 2);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"cpu-detailed":"true", "warning-core" : 39, "critical-core" : 59, "warning-average" : "49", "critical-average" : "60", "warning-core-user": "30", "critical-core-user": "40", "warning-average-user": "31", "critical-average-user": "41" })"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  checker.compute(first, second, &output, &perfs);
  ASSERT_EQ(
      output,
      "WARNING: CPU'0' Usage: 40.00%, User 25.00%, System 10.00%, Idle 60.00%, "
      "Interrupt 5.00%, Dpc Interrupt 1.00% CRITICAL: CPU'1' Usage: 60.00%, "
      "User 45.00%, System 10.00%, Idle 40.00%, Interrupt 5.00%, Dpc Interrupt "
      "0.00% WARNING: CPU(s) average Usage: 50.00%, User 35.00%, System "
      "10.00%, Idle 50.00%, Interrupt 5.00%, Dpc Interrupt 0.50%");

  ASSERT_EQ(perfs.size(), 18);

  for (const auto& perf : perfs) {
    ASSERT_FALSE(perf.critical_mode());
    ASSERT_FALSE(perf.warning_mode());
    ASSERT_EQ(perf.min(), 0);
    ASSERT_EQ(perf.max(), 100);
    ASSERT_EQ(perf.unit(), "%");
    ASSERT_EQ(perf.value_type(), com::centreon::common::perfdata::gauge);

    if (perf.name() == "0~user#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 25.0, 0.01);
      ASSERT_NEAR(perf.warning(), 30.0, 0.01);
      ASSERT_NEAR(perf.critical(), 40.0, 0.01);
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_EQ(perf.critical_low(), 0);
    } else if (perf.name() == "1~user#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 45.0, 0.01);
      ASSERT_NEAR(perf.warning(), 30.0, 0.01);
      ASSERT_NEAR(perf.critical(), 40.0, 0.01);
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_EQ(perf.critical_low(), 0);
    } else if (perf.name() == "user#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 35.0, 0.01);
      ASSERT_NEAR(perf.warning(), 31.0, 0.01);
      ASSERT_NEAR(perf.critical(), 41.0, 0.01);
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_EQ(perf.critical_low(), 0);
    } else if (perf.name() == "0~system#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 10.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "1~system#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 10.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "system#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 10.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "0~idle#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 60.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "1~idle#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 40.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "idle#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 50.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "0~interrupt#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 5.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "1~interrupt#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 5.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "interrupt#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 5.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() ==
               "0~dpc_interrupt#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 1.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));

    } else if (perf.name() ==
               "1~dpc_interrupt#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 0.0, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "dpc_interrupt#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 0.5, 0.01);
      ASSERT_TRUE(std::isnan(perf.critical_low()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.warning()));
    } else if (perf.name() == "0~used#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 40.0, 0.01);
      ASSERT_NEAR(perf.warning(), 39.0, 0.01);
      ASSERT_NEAR(perf.critical(), 59.0, 0.01);
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_EQ(perf.critical_low(), 0);
    } else if (perf.name() == "1~used#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 60.0, 0.01);
      ASSERT_NEAR(perf.warning(), 39.0, 0.01);
      ASSERT_NEAR(perf.critical(), 59.0, 0.01);
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_EQ(perf.critical_low(), 0);
    } else if (perf.name() == "used#cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), 50.0, 0.01);
      ASSERT_NEAR(perf.warning(), 49.0, 0.01);
      ASSERT_NEAR(perf.critical(), 60.0, 0.01);
      ASSERT_EQ(perf.warning_low(), 0);
      ASSERT_EQ(perf.critical_low(), 0);
    } else {
      FAIL() << "unexpected perfdata name:" << perf.name();
    }
  }
}

TEST(native_check_cpu_windows, compare_kernel_dph) {
  using namespace com::centreon::common::literals;
  rapidjson::Document nt_check_args =
      R"({"use-nt-query-system-information":true })"_json;

  check_cpu nt_checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, nt_check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  rapidjson::Document pdh_check_args =
      R"({"use-nt-query-system-information":"false" })"_json;

  check_cpu pdh_checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, pdh_check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  auto first_nt = nt_checker.get_cpu_time_snapshot(true);
  auto first_pdh = pdh_checker.get_cpu_time_snapshot(true);

  std::this_thread::sleep_for(std::chrono::milliseconds(2500));

  auto second_nt = nt_checker.get_cpu_time_snapshot(false);
  auto second_pdh = pdh_checker.get_cpu_time_snapshot(false);

  auto diff_nt = second_nt->subtract(*first_nt);
  auto diff_pdh = second_pdh->subtract(*first_pdh);

  ASSERT_EQ(diff_nt.size(), diff_pdh.size());
  auto nt_iter = diff_nt.begin();
  auto pdh_iter = diff_pdh.begin();
  auto nt_iter_end = diff_nt.end();
  for (; nt_iter != nt_iter_end; ++nt_iter, ++pdh_iter) {
    ASSERT_NEAR(nt_iter->second.get_proportional_used(),
                pdh_iter->second.get_proportional_used(), 0.1);
    for (size_t j = 0; j < 5; ++j) {
      ASSERT_NEAR(nt_iter->second.get_proportional_value(j),
                  pdh_iter->second.get_proportional_value(j), 0.1);
    }
  }
}