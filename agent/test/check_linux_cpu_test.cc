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

#include "check_cpu.hh"
#include "com/centreon/common/rapidjson_helper.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;

const char* proc_sample =
    R"(cpu  4360186 24538 1560174 17996659 64169 0 93611 0 0 0
cpu0 1089609 6082 396906 4497394 15269 0 11914 0 0 0
cpu1 1082032 5818 391692 4456828 16624 0 72471 0 0 0
cpu2 1095585 6334 386205 4524762 16543 0 1774 0 0 0
cpu3 1092959 6304 385370 4517673 15732 0 7451 0 0 0
intr 213853764 0 35 0 0 0 0 0 0 0 56927 0 0 134 0 0 0 48 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 29851994 30 0 408 411 0 0 0 0 0 0 0 0 0 0 0 0 0 0 43 26 529900 571944 554845 556829 19615758 7070 8 0 0 0 0 2 15 3220 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
ctxt 529237135
btime 1728880818
processes 274444
procs_running 2
procs_blocked 0
softirq 160085949 64462978 14075755 1523012 4364896 33 0 17578206 28638313 73392 29369364
)";

TEST(proc_stat_file_test, read_sample) {
  constexpr const char* test_file_path = "/tmp/proc_stat_test";

  ::remove(test_file_path);
  std::ofstream f(test_file_path);
  f.write(proc_sample, strlen(proc_sample));

  check_cpu_detail::proc_stat_file to_compare(test_file_path, 4);

  for (const auto& by_cpu : to_compare.get_values()) {
    switch (by_cpu.first) {
      case 0:
        ASSERT_EQ(by_cpu.second.get_total(), 6017174);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_used(),
                         (6017174.0 - 4497394.0) / 6017174);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::user),
                         1089609.0 / 6017174);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::nice),
                         6082.0 / 6017174);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::system),
                         396906.0 / 6017174);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::idle),
                         4497394.0 / 6017174);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::iowait),
                         15269.0 / 6017174);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::irq),
                         0);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::soft_irq),
                         11914.0 / 6017174);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::steal),
                         0);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::guest),
                         0);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::guest_nice),
                         0);
        break;
      case 1:
        ASSERT_EQ(by_cpu.second.get_total(), 6025465);
        break;
      case 2:
        ASSERT_EQ(by_cpu.second.get_total(), 6031203);
        break;
      case 3:
        ASSERT_EQ(by_cpu.second.get_total(), 6025489);
        break;
      case check_cpu_detail::average_cpu_index:
        ASSERT_EQ(by_cpu.second.get_total(), 24099337);
        ASSERT_DOUBLE_EQ(by_cpu.second.get_proportional_value(
                             check_cpu_detail::e_proc_stat_index::system),
                         1560174.0 / 24099337);
        break;
      default:
        FAIL() << "unexpected cpu:" << by_cpu.first;
        break;
    }
  }
}

const char* proc_sample_2 =
    R"(cpu  4574560 24547 1630654 18918908 68531 0 96832 0 0 0
cpu0 1143030 6086 414440 4726292 16461 0 14668 0 0 0
cpu1 1135947 5820 409352 4687911 17696 0 72516 0 0 0
cpu2 1149227 6335 404370 4754742 17697 0 2149 0 0 0
cpu3 1146355 6305 402491 4749962 16675 0 7498 0 0 0
intr 224918652 0 35 0 0 0 0 0 0 0 57636 0 0 134 0 0 0 48 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 30628697 30 0 408 411 0 0 0 0 0 0 0 0 0 0 0 0 0 0 43 26 564911 598184 598096 594403 20270994 8610 8 0 0 0 0 2 15 3220 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
ctxt 558464714
btime 1728880818
processes 289981
procs_running 1
procs_blocked 0
softirq 166407220 66442046 14763247 1577070 4447556 33 0 18081353 30219191 75659 30801065
)";

using namespace std::string_literals;

TEST(proc_stat_file_test, no_threshold) {
  constexpr const char* test_file_path = "/tmp/proc_stat_test";
  {
    ::remove(test_file_path);
    std::ofstream f(test_file_path);
    f.write(proc_sample, strlen(proc_sample));
  }
  constexpr const char* test_file_path2 = "/tmp/proc_stat_test2";
  {
    ::remove(test_file_path2);
    std::ofstream f(test_file_path2);
    f.write(proc_sample_2, strlen(proc_sample_2));
  }

  check_cpu_detail::proc_stat_file first_measure(test_file_path, 4);

  check_cpu_detail::proc_stat_file second_measure(test_file_path2, 4);

  auto delta = second_measure.subtract(first_measure);

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

  e_status status =
      checker.compute(first_measure, second_measure, &output, &perfs);
  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "OK: CPU(s) average usage is 24.08%");

  ASSERT_EQ(perfs.size(), 5);

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
      ASSERT_NEAR(perf.value(), delta[0].get_proportional_used() * 100, 0.01);
    } else if (perf.name() == "1#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), delta[1].get_proportional_used() * 100, 0.01);
    } else if (perf.name() == "2#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), delta[2].get_proportional_used() * 100, 0.01);
    } else if (perf.name() == "3#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), delta[3].get_proportional_used() * 100, 0.01);
    } else if (perf.name() == "cpu.utilization.percentage") {
      ASSERT_NEAR(
          perf.value(),
          delta[check_cpu_detail::average_cpu_index].get_proportional_used() *
              100,
          0.01);
    } else {
      FAIL() << "unexpected perfdata name:" << perf.name();
    }
  }
}

TEST(proc_stat_file_test, no_threshold_detailed) {
  constexpr const char* test_file_path = "/tmp/proc_stat_test";
  {
    ::remove(test_file_path);
    std::ofstream f(test_file_path);
    f.write(proc_sample, strlen(proc_sample));
  }
  constexpr const char* test_file_path2 = "/tmp/proc_stat_test2";
  {
    ::remove(test_file_path2);
    std::ofstream f(test_file_path2);
    f.write(proc_sample_2, strlen(proc_sample_2));
  }

  check_cpu_detail::proc_stat_file first_measure(test_file_path, 4);

  check_cpu_detail::proc_stat_file second_measure(test_file_path2, 4);

  auto delta = second_measure.subtract(first_measure);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args = R"({"cpu-detailed":"true"})"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  e_status status =
      checker.compute(first_measure, second_measure, &output, &perfs);
  ASSERT_EQ(status, e_status::ok);
  ASSERT_EQ(output, "OK: CPU(s) average usage is 24.08%");

  ASSERT_EQ(perfs.size(), 55);

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

    unsigned cpu_index = check_cpu_detail::average_cpu_index;
    std::string counter_type;
    if (std::isdigit(perf.name()[0])) {
      cpu_index = perf.name()[0] - '0';
      counter_type = perf.name().substr(2, perf.name().find('#') - 2);
    } else {
      counter_type = perf.name().substr(0, perf.name().find('#'));
    }
    const auto& cpu_data = delta[cpu_index];
    if (counter_type == "user") {
      ASSERT_NEAR(perf.value(),
                  (cpu_data.get_proportional_value(
                       check_cpu_detail::e_proc_stat_index::user) *
                   100),
                  0.01);
    } else if (counter_type == "nice") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::nice) *
                      100,
                  0.01);
    } else if (counter_type == "system") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::system) *
                      100,
                  0.01);
    } else if (counter_type == "idle") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::idle) *
                      100,
                  0.01);
    } else if (counter_type == "iowait") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::iowait) *
                      100,
                  0.01);
    } else if (counter_type == "interrupt") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::irq) *
                      100,
                  0.01);
    } else if (counter_type == "softirq") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::soft_irq) *
                      100,
                  0.01);
    } else if (counter_type == "steal") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::steal) *
                      100,
                  0.01);
    } else if (counter_type == "guest") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::guest) *
                      100,
                  0.01);
    } else if (counter_type == "guestnice") {
      ASSERT_NEAR(perf.value(),
                  cpu_data.get_proportional_value(
                      check_cpu_detail::e_proc_stat_index::guest_nice) *
                      100,
                  0.01);
    } else if (counter_type == "used") {
      ASSERT_NEAR(perf.value(), cpu_data.get_proportional_used() * 100, 0.01);
    } else {
      FAIL() << "unexpected perfdata name:" << perf.name();
    }
  }
}

TEST(proc_stat_file_test, threshold_nodetailed) {
  constexpr const char* test_file_path = "/tmp/proc_stat_test";
  {
    ::remove(test_file_path);
    std::ofstream f(test_file_path);
    f.write(proc_sample, strlen(proc_sample));
  }
  constexpr const char* test_file_path2 = "/tmp/proc_stat_test2";
  {
    ::remove(test_file_path2);
    std::ofstream f(test_file_path2);
    f.write(proc_sample_2, strlen(proc_sample_2));
  }

  check_cpu_detail::proc_stat_file first_measure(test_file_path, 4);

  check_cpu_detail::proc_stat_file second_measure(test_file_path2, 4);

  auto delta = second_measure.subtract(first_measure);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"warning-core" : "24.1", "critical-core" : 24.4, "warning-average" : "10", "critical-average" : "20"})"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  e_status status =
      checker.compute(first_measure, second_measure, &output, &perfs);
  ASSERT_EQ(status, e_status::critical);
  ASSERT_EQ(
      output,
      R"(CRITICAL: CPU'0' Usage: 24.66%, User 17.58%, Nice 0.00%, System 5.77%, Idle 75.34%, IOWait 0.39%, Interrupt 0.00%, Soft Irq 0.91%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% WARNING: CPU'2' Usage: 24.18%, User 17.69%, Nice 0.00%, System 5.99%, Idle 75.82%, IOWait 0.38%, Interrupt 0.00%, Soft Irq 0.12%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% CRITICAL: CPU(s) average Usage: 24.08%, User 17.65%, Nice 0.00%, System 5.80%, Idle 75.92%, IOWait 0.36%, Interrupt 0.00%, Soft Irq 0.27%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00%)");

  ASSERT_EQ(perfs.size(), 5);

  for (const auto& perf : perfs) {
    ASSERT_EQ(perf.critical_low(), 0);
    ASSERT_FALSE(perf.critical_mode());
    ASSERT_EQ(perf.warning_low(), 0);
    if (perf.name() == "cpu.utilization.percentage") {
      ASSERT_NEAR(perf.warning(), 10, 0.01);
      ASSERT_NEAR(perf.critical(), 20, 0.01);
    } else {
      ASSERT_NEAR(perf.warning(), 24.1, 0.01);
      ASSERT_NEAR(perf.critical(), 24.4, 0.01);
    }
    ASSERT_FALSE(perf.warning_mode());
    ASSERT_EQ(perf.min(), 0);
    ASSERT_EQ(perf.max(), 100);
    ASSERT_EQ(perf.unit(), "%");
    ASSERT_EQ(perf.value_type(), com::centreon::common::perfdata::gauge);
    if (perf.name() == "0#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), delta[0].get_proportional_used() * 100, 0.01);
    } else if (perf.name() == "1#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), delta[1].get_proportional_used() * 100, 0.01);
    } else if (perf.name() == "2#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), delta[2].get_proportional_used() * 100, 0.01);
    } else if (perf.name() == "3#core.cpu.utilization.percentage") {
      ASSERT_NEAR(perf.value(), delta[3].get_proportional_used() * 100, 0.01);
    } else if (perf.name() == "cpu.utilization.percentage") {
      ASSERT_NEAR(
          perf.value(),
          delta[check_cpu_detail::average_cpu_index].get_proportional_used() *
              100,
          0.01);
    } else {
      FAIL() << "unexpected perfdata name:" << perf.name();
    }
  }
}

TEST(proc_stat_file_test, threshold_nodetailed2) {
  constexpr const char* test_file_path = "/tmp/proc_stat_test";
  {
    ::remove(test_file_path);
    std::ofstream f(test_file_path);
    f.write(proc_sample, strlen(proc_sample));
  }
  constexpr const char* test_file_path2 = "/tmp/proc_stat_test2";
  {
    ::remove(test_file_path2);
    std::ofstream f(test_file_path2);
    f.write(proc_sample_2, strlen(proc_sample_2));
  }

  check_cpu_detail::proc_stat_file first_measure(test_file_path, 4);

  check_cpu_detail::proc_stat_file second_measure(test_file_path2, 4);

  auto delta = second_measure.subtract(first_measure);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"warning-core-iowait" : "0.36", "critical-core-iowait" : "0.39", "warning-average-iowait" : "0.3", "critical-average-iowait" : "0.4"})"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  e_status status =
      checker.compute(first_measure, second_measure, &output, &perfs);
  ASSERT_EQ(status, e_status::critical);
  ASSERT_EQ(
      output,
      R"(CRITICAL: CPU'0' Usage: 24.66%, User 17.58%, Nice 0.00%, System 5.77%, Idle 75.34%, IOWait 0.39%, Interrupt 0.00%, Soft Irq 0.91%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% WARNING: CPU'2' Usage: 24.18%, User 17.69%, Nice 0.00%, System 5.99%, Idle 75.82%, IOWait 0.38%, Interrupt 0.00%, Soft Irq 0.12%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% WARNING: CPU(s) average Usage: 24.08%, User 17.65%, Nice 0.00%, System 5.80%, Idle 75.92%, IOWait 0.36%, Interrupt 0.00%, Soft Irq 0.27%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00%)");

  ASSERT_EQ(perfs.size(), 5);

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
  }
}

TEST(proc_stat_file_test, threshold_detailed) {
  constexpr const char* test_file_path = "/tmp/proc_stat_test";
  {
    ::remove(test_file_path);
    std::ofstream f(test_file_path);
    f.write(proc_sample, strlen(proc_sample));
  }
  constexpr const char* test_file_path2 = "/tmp/proc_stat_test2";
  {
    ::remove(test_file_path2);
    std::ofstream f(test_file_path2);
    f.write(proc_sample_2, strlen(proc_sample_2));
  }

  check_cpu_detail::proc_stat_file first_measure(test_file_path, 4);

  check_cpu_detail::proc_stat_file second_measure(test_file_path2, 4);

  auto delta = second_measure.subtract(first_measure);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"cpu-detailed":true, "warning-core" : "24.1", "critical-core" : "24.4", "warning-average" : "10", "critical-average" : "20"})"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  e_status status =
      checker.compute(first_measure, second_measure, &output, &perfs);
  ASSERT_EQ(status, e_status::critical);
  ASSERT_EQ(
      output,
      R"(CRITICAL: CPU'0' Usage: 24.66%, User 17.58%, Nice 0.00%, System 5.77%, Idle 75.34%, IOWait 0.39%, Interrupt 0.00%, Soft Irq 0.91%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% WARNING: CPU'2' Usage: 24.18%, User 17.69%, Nice 0.00%, System 5.99%, Idle 75.82%, IOWait 0.38%, Interrupt 0.00%, Soft Irq 0.12%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% CRITICAL: CPU(s) average Usage: 24.08%, User 17.65%, Nice 0.00%, System 5.80%, Idle 75.92%, IOWait 0.36%, Interrupt 0.00%, Soft Irq 0.27%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00%)");

  ASSERT_EQ(perfs.size(), 55);

  for (const auto& perf : perfs) {
    ASSERT_FALSE(perf.critical_mode());
    if (perf.name().find("used#core.cpu.utilization.percentage") !=
            std::string::npos ||
        perf.name().find("used#cpu.utilization.percentage") !=
            std::string::npos) {
      ASSERT_EQ(perf.critical_low(), 0);
      ASSERT_EQ(perf.warning_low(), 0);
      if (!std::isdigit(perf.name()[0])) {
        ASSERT_NEAR(perf.warning(), 10, 0.01);
        ASSERT_NEAR(perf.critical(), 20, 0.01);
      } else {
        ASSERT_NEAR(perf.warning(), 24.1, 0.01);
        ASSERT_NEAR(perf.critical(), 24.4, 0.01);
      }
    } else {
      ASSERT_TRUE(std::isnan(perf.warning()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.critical_low()));
    }
    ASSERT_FALSE(perf.warning_mode());
    ASSERT_EQ(perf.min(), 0);
    ASSERT_EQ(perf.max(), 100);
    ASSERT_EQ(perf.unit(), "%");
    ASSERT_EQ(perf.value_type(), com::centreon::common::perfdata::gauge);
  }
}

TEST(proc_stat_file_test, threshold_detailed2) {
  constexpr const char* test_file_path = "/tmp/proc_stat_test";
  {
    ::remove(test_file_path);
    std::ofstream f(test_file_path);
    f.write(proc_sample, strlen(proc_sample));
  }
  constexpr const char* test_file_path2 = "/tmp/proc_stat_test2";
  {
    ::remove(test_file_path2);
    std::ofstream f(test_file_path2);
    f.write(proc_sample_2, strlen(proc_sample_2));
  }

  check_cpu_detail::proc_stat_file first_measure(test_file_path, 4);

  check_cpu_detail::proc_stat_file second_measure(test_file_path2, 4);

  auto delta = second_measure.subtract(first_measure);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"cpu-detailed":"true",  "warning-core-iowait" : "0.36", "critical-core-iowait" : "0.39", "warning-average-iowait" : "0.3", "critical-average-iowait" : "0.4"})"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  e_status status =
      checker.compute(first_measure, second_measure, &output, &perfs);
  ASSERT_EQ(status, e_status::critical);
  ASSERT_EQ(
      output,
      R"(CRITICAL: CPU'0' Usage: 24.66%, User 17.58%, Nice 0.00%, System 5.77%, Idle 75.34%, IOWait 0.39%, Interrupt 0.00%, Soft Irq 0.91%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% WARNING: CPU'2' Usage: 24.18%, User 17.69%, Nice 0.00%, System 5.99%, Idle 75.82%, IOWait 0.38%, Interrupt 0.00%, Soft Irq 0.12%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% WARNING: CPU(s) average Usage: 24.08%, User 17.65%, Nice 0.00%, System 5.80%, Idle 75.92%, IOWait 0.36%, Interrupt 0.00%, Soft Irq 0.27%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00%)");

  ASSERT_EQ(perfs.size(), 55);

  for (const auto& perf : perfs) {
    ASSERT_FALSE(perf.critical_mode());
    if (perf.name().find("iowait#core.cpu.utilization.percentage") !=
            std::string::npos ||
        perf.name().find("iowait#cpu.utilization.percentage") !=
            std::string::npos) {
      ASSERT_EQ(perf.critical_low(), 0);
      ASSERT_EQ(perf.warning_low(), 0);
      if (!std::isdigit(perf.name()[0])) {
        ASSERT_NEAR(perf.warning(), 0.3, 0.01);
        ASSERT_NEAR(perf.critical(), 0.4, 0.01);
      } else {
        ASSERT_NEAR(perf.warning(), 0.36, 0.01);
        ASSERT_NEAR(perf.critical(), 0.39, 0.01);
      }
    } else {
      ASSERT_TRUE(std::isnan(perf.warning()));
      ASSERT_TRUE(std::isnan(perf.critical()));
      ASSERT_TRUE(std::isnan(perf.warning_low()));
      ASSERT_TRUE(std::isnan(perf.critical_low()));
    }
    ASSERT_FALSE(perf.warning_mode());
    ASSERT_EQ(perf.min(), 0);
    ASSERT_EQ(perf.max(), 100);
    ASSERT_EQ(perf.unit(), "%");
    ASSERT_EQ(perf.value_type(), com::centreon::common::perfdata::gauge);
  }
}

TEST(proc_stat_file_test, threshold_detailed3) {
  constexpr const char* test_file_path = "/tmp/proc_stat_test";
  {
    ::remove(test_file_path);
    std::ofstream f(test_file_path);
    f.write(proc_sample, strlen(proc_sample));
  }
  constexpr const char* test_file_path2 = "/tmp/proc_stat_test2";
  {
    ::remove(test_file_path2);
    std::ofstream f(test_file_path2);
    f.write(proc_sample_2, strlen(proc_sample_2));
  }

  check_cpu_detail::proc_stat_file first_measure(test_file_path, 4);

  check_cpu_detail::proc_stat_file second_measure(test_file_path2, 4);

  auto delta = second_measure.subtract(first_measure);

  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({"cpu-detailed":"true",  "warning-core-iowait" : "0.36", "critical-core-iowait" : "0.39", "warning-average-iowait" : "", "critical-average-iowait" : ""})"_json;

  check_cpu checker(
      g_io_context, spdlog::default_logger(), {}, {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  e_status status =
      checker.compute(first_measure, second_measure, &output, &perfs);
  EXPECT_EQ(status, e_status::critical);
  EXPECT_EQ(
      output,
      R"(CRITICAL: CPU'0' Usage: 24.66%, User 17.58%, Nice 0.00%, System 5.77%, Idle 75.34%, IOWait 0.39%, Interrupt 0.00%, Soft Irq 0.91%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00% WARNING: CPU'2' Usage: 24.18%, User 17.69%, Nice 0.00%, System 5.99%, Idle 75.82%, IOWait 0.38%, Interrupt 0.00%, Soft Irq 0.12%, Steal 0.00%, Guest 0.00%, Guest Nice 0.00%)");

  EXPECT_EQ(perfs.size(), 55);

  for (const auto& perf : perfs) {
    EXPECT_FALSE(perf.critical_mode());
    if (perf.name().find("iowait#core.cpu.utilization.percentage") !=
            std::string::npos ||
        perf.name().find("iowait#cpu.utilization.percentage") !=
            std::string::npos) {
      if (!std::isdigit(perf.name()[0])) {
        EXPECT_TRUE(std::isnan(perf.critical_low()));
        EXPECT_TRUE(std::isnan(perf.warning_low()));
        EXPECT_TRUE(std::isnan(perf.warning()));
        EXPECT_TRUE(std::isnan(perf.critical()));
      } else {
        EXPECT_EQ(perf.critical_low(), 0);
        EXPECT_EQ(perf.warning_low(), 0);
        EXPECT_NEAR(perf.warning(), 0.36, 0.01);
        EXPECT_NEAR(perf.critical(), 0.39, 0.01);
      }
    } else {
      EXPECT_TRUE(std::isnan(perf.warning()));
      EXPECT_TRUE(std::isnan(perf.critical()));
      EXPECT_TRUE(std::isnan(perf.warning_low()));
      EXPECT_TRUE(std::isnan(perf.critical_low()));
    }
    ASSERT_FALSE(perf.warning_mode());
    EXPECT_EQ(perf.min(), 0);
    EXPECT_EQ(perf.max(), 100);
    EXPECT_EQ(perf.unit(), "%");
    EXPECT_EQ(perf.value_type(), com::centreon::common::perfdata::gauge);
  }
}
