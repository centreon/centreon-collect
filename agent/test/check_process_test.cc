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

#include "com/centreon/common/rapidjson_helper.hh"

#include "agent/test/process_data_test.hh"
#include "check_process.hh"
#include "process/process_data.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::process;

extern std::shared_ptr<asio::io_context> g_io_context;

struct test_check_process : public check_process {
  std::unique_ptr<process_container_mock> cont;

  test_check_process(const std::shared_ptr<asio::io_context>& io_context,
                     const std::shared_ptr<spdlog::logger>& logger,
                     time_point first_start_expected,
                     duration check_interval,
                     const std::string& serv,
                     const std::string& cmd_name,
                     const std::string& cmd_line,
                     const rapidjson::Value& args,
                     const engine_to_agent_request_ptr& cnf,
                     check::completion_handler&& handler,
                     const checks_statistics::pointer& stat)
      : check_process(io_context,
                      logger,
                      first_start_expected,
                      check_interval,
                      serv,
                      cmd_name,
                      cmd_line,
                      args,
                      cnf,
                      std::move(handler),
                      stat) {
    cont = std::make_unique<process_container_mock>(std::move(*_processes));
  }
};

/**
 * @brief check_process, given a process filter, we expect the correct status
 * and output
 */
TEST(check_process, output_no_verbose) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "empty-state": "{status}: no process", 
          "ok-syntax": "{status}: All processes are ok must empty: '{problem-list}'",
          "verbose": false, 
          "output-syntax": "${status}: ${problem-list}",
          "process-detail-syntax": "{exe} {filename} {status} {creation} {kernel} {kernel_percent} {user} {user_percent} {time} {time_percent} {virtual} {gdi_handle} {user_handle} {pid}",
          "filter-process": "exe != 'tutu.exe'",
          "exclude-process": "exe == 'toto.exe'",
            "warning-process": "time_percent > 50%",
            "critical-process": "time_percent > 80%",
            "warning-rules": "warn_count >= 2",
            "critical-rules": "crit_count >= 1"
          })"_json;

  using namespace std::string_literals;
  test_check_process checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;

  e_status status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK: no process");
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 0);

  checker.cont->processes.clear();
  checker.cont->add_process(1, "tutu.exe");
  checker.cont->add_process(2, "toto.exe");
  checker.cont->refresh({1, 2}, {});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK: no process");
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 0);

  checker.cont->processes.clear();
  checker.cont->add_process(1, "taratata.exe");
  checker.cont->add_process(2, "turlututu.exe");
  checker.cont->refresh({1, 2}, {});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK: All processes are ok must empty: ''");
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 2);

  // one warning
  checker.cont->processes.clear();
  auto now = std::chrono::file_clock::now();
  auto rounded_now =
      std::chrono::floor<std::chrono::seconds, std::chrono::file_clock>(
          now - std::chrono::seconds(10));
  checker.cont->add_process(
      1, "C:\\temp\\taratata.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(1),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 1,
                                      .user_handle_count = 2},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000});
  checker.cont->refresh({1}, {});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output,
            std::format("OK: All processes are ok must empty: 'taratata.exe "
                        "C:\\temp\\taratata.exe unreadable {:%FT%T} 1s 9 "
                        "6s 59 7s 69 1000 1 2 1 '",
                        rounded_now));
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 1);

  // two warning
  checker.cont->processes.clear();
  checker.cont->add_process(
      1, process_data::started, "C:\\temp\\taratata.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(1),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 1,
                                      .user_handle_count = 2},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000});
  checker.cont->add_process(
      2, "C:\\temp\\turlututu.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(2),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 5,
                                      .user_handle_count = 6},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000000});
  checker.cont->refresh({1, 2}, {2});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::warning);
  EXPECT_EQ(
      output,
      std::format("WARNING: taratata.exe "
                  "C:\\temp\\taratata.exe started {0:%FT%T} 1s 9 "
                  "6s 59 7s 69 1000 1 2 1 "
                  "turlututu.exe C:\\temp\\turlututu.exe hung {0:%FT%T} 2s 19 "
                  "6s 59 8s 79 1000000 5 6 2 ",
                  rounded_now));
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 2);

  // two warning and one critical
  checker.cont->processes.clear();
  checker.cont->add_process(
      1, process_data::started, "C:\\temp\\taratata.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(1),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 1,
                                      .user_handle_count = 2},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000});
  checker.cont->add_process(
      2, "C:\\temp\\turlututu.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(2),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 5,
                                      .user_handle_count = 6},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000000});
  checker.cont->add_process(
      3, "C:\\temp\\turlututu.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(3),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 5,
                                      .user_handle_count = 6},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000000});
  checker.cont->refresh({1, 2, 3}, {2});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::critical);
  EXPECT_EQ(
      output,
      std::format(
          "CRITICAL: "
          "turlututu.exe C:\\temp\\turlututu.exe unreadable {0:%FT%T} 3s 29 "
          "6s 59 9s 89 1000000 5 6 3 "
          "taratata.exe C:\\temp\\taratata.exe started {0:%FT%T} 1s 9 "
          "6s 59 7s 69 1000 1 2 1 "
          "turlututu.exe C:\\temp\\turlututu.exe hung {0:%FT%T} 2s 19 "
          "6s 59 8s 79 1000000 5 6 2 ",
          rounded_now));
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 3);
}

/**
 * @brief check_process, given a process filter, we expect the correct status
 * and output
 */
TEST(check_process, output_verbose) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({ "empty-state": "{status}: no process", 
          "ok-syntax": "{status}: All processes are ok",
          "verbose": true, 
          "output-syntax": "${status}",
          "process-detail-syntax": "{exe} {filename} {status} {creation} {kernel} {kernel_percent} {user} {user_percent} {time} {time_percent} {virtual} {gdi_handle} {user_handle} {pid}",
          "filter-process": "exe != 'tutu.exe'",
          "exclude-process": "exe == 'toto.exe'",
            "warning-process": "time_percent > 50%",
            "critical-process": "time_percent > 80%",
            "warning-rules": "warn_count >= 2",
            "critical-rules": "crit_count >= 1"
          })"_json;

  using namespace std::string_literals;
  test_check_process checker(
      g_io_context, spdlog::default_logger(), {}, {}, "serv"s, "cmd_name"s,
      "cmd_line"s, check_args, nullptr,
      []([[maybe_unused]] const std::shared_ptr<check>& caller,
         [[maybe_unused]] int status,
         [[maybe_unused]] const std::list<com::centreon::common::perfdata>&
             perfdata,
         [[maybe_unused]] const std::list<std::string>& outputs) {},
      std::make_shared<checks_statistics>());

  std::string output;
  com::centreon::common::perfdata perf;

  e_status status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK: no process");
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 0);

  checker.cont->processes.clear();
  checker.cont->add_process(1, "tutu.exe");
  checker.cont->add_process(2, "toto.exe");
  checker.cont->refresh({1, 2}, {});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK: no process");
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 0);

  checker.cont->processes.clear();
  checker.cont->add_process(1, "taratata.exe");
  checker.cont->add_process(2, "turlututu.exe");
  checker.cont->refresh({1, 2}, {});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output, "OK: All processes are ok");
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 2);

  // one warning
  checker.cont->processes.clear();
  auto now = std::chrono::file_clock::now();
  auto rounded_now =
      std::chrono::floor<std::chrono::seconds, std::chrono::file_clock>(
          now - std::chrono::seconds(10));
  checker.cont->add_process(
      1, "C:\\temp\\taratata.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(1),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 1,
                                      .user_handle_count = 2},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000});
  checker.cont->refresh({1}, {});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::ok);
  EXPECT_EQ(output,
            std::format("OK: All processes are ok\ntaratata.exe "
                        "C:\\temp\\taratata.exe unreadable {:%FT%T} 1s 9 "
                        "6s 59 7s 69 1000 1 2 1",
                        rounded_now));
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 1);

  // two warning
  checker.cont->processes.clear();
  checker.cont->add_process(
      1, process_data::started, "C:\\temp\\taratata.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(1),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 1,
                                      .user_handle_count = 2},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000});
  checker.cont->add_process(
      2, "C:\\temp\\turlututu.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(2),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 5,
                                      .user_handle_count = 6},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000000});
  checker.cont->refresh({1, 2}, {2});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::warning);
  EXPECT_EQ(
      output,
      std::format("WARNING\ntaratata.exe "
                  "C:\\temp\\taratata.exe started {0:%FT%T} 1s 9 "
                  "6s 59 7s 69 1000 1 2 1\n"
                  "turlututu.exe C:\\temp\\turlututu.exe hung {0:%FT%T} 2s 19 "
                  "6s 59 8s 79 1000000 5 6 2",
                  rounded_now));
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 2);

  // two warning and one critical
  checker.cont->processes.clear();
  checker.cont->add_process(
      1, process_data::started, "C:\\temp\\taratata.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(1),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 1,
                                      .user_handle_count = 2},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000});
  checker.cont->add_process(
      2, "C:\\temp\\turlututu.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(2),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 5,
                                      .user_handle_count = 6},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000000});
  checker.cont->add_process(
      3, "C:\\temp\\turlututu.exe",
      mock_process_data::times{.creation_time = now - std::chrono::seconds(10),
                               .kernel_time = std::chrono::seconds(3),
                               .user_time = std::chrono::seconds(6)},
      mock_process_data::handle_count{.gdi_handle_count = 5,
                                      .user_handle_count = 6},
      PROCESS_MEMORY_COUNTERS_EX{.PrivateUsage = 1000000});
  checker.cont->refresh({1, 2, 3}, {2});
  status = checker.compute(*checker.cont, &output, &perf);
  EXPECT_EQ(status, e_status::critical);
  EXPECT_EQ(
      output,
      std::format(
          "CRITICAL\n"
          "turlututu.exe C:\\temp\\turlututu.exe unreadable {0:%FT%T} 3s 29 "
          "6s 59 9s 89 1000000 5 6 3\n"
          "taratata.exe C:\\temp\\taratata.exe started {0:%FT%T} 1s 9 "
          "6s 59 7s 69 1000 1 2 1\n"
          "turlututu.exe C:\\temp\\turlututu.exe hung {0:%FT%T} 2s 19 "
          "6s 59 8s 79 1000000 5 6 2",
          rounded_now));
  EXPECT_EQ(perf.name(), "process.count");
  EXPECT_EQ(perf.value(), 3);
}
