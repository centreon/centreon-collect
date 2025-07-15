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
#include <re2/re2.h>
#include <chrono>

#include "check.hh"
#include "check_files.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;
using namespace std::string_literals;

extern std::shared_ptr<asio::io_context> g_io_context;

class check_files_test : public ::testing::Test {
 protected:
 public:
  static inline std::filesystem::path root_;
  static void SetUpTestCase() {
    namespace fs = std::filesystem;

    auto now = std::chrono::high_resolution_clock::now();

    root_ = fs::temp_directory_path() / "check_files_fixture";
    std::cout << "Using root path: " << root_ << std::endl;
    fs::remove_all(root_);
    fs::create_directories(root_);

    constexpr std::string_view dirs[] = {"dirA", "dirB", "dirC", "dirD",
                                         "dirE"};
    constexpr std::string_view top[] = {"dirN1", "dirN2"};
    constexpr std::string_view mid[] = {"level1", "level2"};
    constexpr std::string_view leaf[] = {"deep1", "deep2"};
    constexpr std::string_view exts[] = {".txt",  ".log", ".cpp",
                                         ".json", ".cfg", ".bin"};

    std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> nLines(1, 400);
    std::uniform_int_distribution<int> nWords(3, 12);

    int id = 0;

    auto emit_file = [&](const fs::path& dir) {
      for (auto e : exts) {
        fs::path p = dir / ("file" + std::to_string(id++) + std::string(e));
        std::ofstream out(p, std::ios::binary);

        int lines = nLines(rng), words = nWords(rng);
        for (int l = 0; l < lines; ++l) {
          for (int w = 0; w < words; ++w)
            out << "w" << w << ' ';
          out << '\n';
        }
        if (e == ".bin") {
          std::vector<char> pad(256 + id);
          out.write(pad.data(), pad.size());
        }
      }
    };

    /* A) flat dirs */
    for (auto d : dirs) {
      fs::create_directories(root_ / d);
      emit_file(root_ / d);
    }

    /* B) nested dirs */
    for (auto t : top)
      for (auto m : mid)
        for (auto l : leaf) {
          fs::path dir = root_ / t / m / l;
          fs::create_directories(dir);
          emit_file(dir);
        }

    std::cout << "time to create files: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::high_resolution_clock::now() - now)
                     .count()
              << " ms" << std::endl;
  }

  static void TearDownTestCase() {
    std::filesystem::remove_all(root_);
    check_files::thread_kill();
  }
};

// Test the default behavior of the check_files class
// It should check files in the specified path and return an OK status
TEST_F(check_files_test, default_behavior) {
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

  re2::RE2 ok_regex(R"(OK: All \d+ files are ok)");
  ASSERT_TRUE(RE2::FullMatch(output, ok_regex))
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

  // Should only list files > 1k (output should not mention "Empty" unless none
  // found)
  ASSERT_EQ(output.find("Empty"), std::string::npos);
}

// Test the check_files class with a warning status condition
TEST_F(check_files_test, warning_status) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({
        "path": "C:\\Windows",
        "max-depth": 0,
        "pattern": "*.*",
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
        "max-depth": 0,
        "pattern": "*.*",
        "files-detail-syntax": "${filename}: ${size}",
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

  checker->start_check(std::chrono::seconds(120));

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

  checker->start_check(std::chrono::seconds(120));

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
        "path": "C:\\Windows",
        "max-depth": 0,
        "pattern": "*.dll",
        "filter-files": "size > 1k",
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

  checker->start_check(std::chrono::seconds(120));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));
  // check regex for output  OK: Ok:22|Nok:0|total:22  warning:0|critical:0
  re2::RE2 ok_regex(
      R"(OK: Ok:\d+\|Nok:\d+\|total:\d+  warning:\d+\|critical:\d+)");
  ASSERT_TRUE(RE2::PartialMatch(output, ok_regex))
      << "Output format does not match expected pattern: " << output;
}

// Helper function for checking if a string matches a glob pattern
void ExpectMatches(const std::string& glob,
                   std::list<std::string_view> should_match,
                   std::list<std::string_view> should_fail) {
  std::string re_str = glob_to_regex(glob);
  re2::RE2 re(re_str);

  for (auto s : should_match)
    EXPECT_TRUE(RE2::FullMatch(std::string{s}, re))
        << "glob \"" << glob << "\"  regex \"" << re_str
        << "\"  SHOULD match  \"" << s << '"';

  for (auto s : should_fail)
    EXPECT_FALSE(RE2::FullMatch(std::string{s}, re))
        << "glob \"" << glob << "\"  regex \"" << re_str
        << "\"  SHOULD NOT match  \"" << s << '"';
}

TEST_F(check_files_test, globs) {
  // * and ? wildcards
  ExpectMatches("*", {"abc", "", "file.txt"}, {});
  ExpectMatches("file?.txt", {"file1.txt", "filea.txt"},
                {"file.txt", "file12.txt"});
  // [] wildcard
  ExpectMatches("data[0-9].csv", {"data0.csv", "data5.csv"},
                {"data.csv", "data12.csv", "dataX.csv"});
  // {} for alternation
  ExpectMatches("*.{txt,log,cpp,TXT}",
                {"readme.TXT", "readme.txt", "error.log", "main.cpp"},
                {"image.png", "filetxt", "main.c"});
  // complex patterns
  ExpectMatches("lib{foo?,bar*}.so",
                {"libfoo1.so", "libbar.so", "libbar_old.so"},
                {"libfoo12.so", "libbaz.so"});

  ExpectMatches("lib{foo[1-2],bar*}.so",
                {"libfoo1.so", "libfoo2.so", "libbar.so", "libbar_old.so"},
                {"libfoo12.so", "libfoo3.so", "libbaz.so"});
  // nested patterns
  ExpectMatches("lib{{foo1,foo2},bar*}.so",
                {"libfoo1.so", "libfoo2.so", "libbar.so", "libbar_old.so"},
                {"libfoo12.so", "libfoo3.so", "libbaz.so"});

  // should throw missing }
  EXPECT_THROW(glob_to_regex("*.{txt,log"), std::runtime_error);

  // No changes
  std::string regex = glob_to_regex("file(+).txt");
  EXPECT_EQ(regex, R"(file\(\+\)\.txt)");
  re2::RE2 re(regex);
  EXPECT_TRUE(RE2::FullMatch("file(+).txt", re));
  EXPECT_FALSE(RE2::FullMatch("file12.txt", re));
}

TEST_F(check_files_test, regex_failures) {
  using namespace com::centreon::common::literals;
  std::string json_str = R"({
        "path": ")" + root_.string() +
                         R"(",
        "max-depth": -1,
        "pattern": "[0-9*.*",
        "verbose": false,
        "files-detail-syntax": "${filename}",
        "ok-syntax": "${status}: {list}"
})";
  // Replace all '\' with '\\' in the path for JSON
  size_t pos = 0;
  while ((pos = json_str.find("\\", pos)) != std::string::npos) {
    json_str.replace(pos, 1, "\\\\");
    pos += 2;
  }
  std::cout << "JSON String: " << json_str << std::endl;
  rapidjson::Document check_args;
  check_args.Parse(json_str.c_str());

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

  checker->start_check(std::chrono::seconds(120));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  // Expect an error due to invalid regex pattern
  re2::RE2 error_regex("Invalid regex pattern:");
  ASSERT_TRUE(RE2::PartialMatch(output, error_regex))
      << "Output should indicate regex failure: " << output;
}

TEST_F(check_files_test, pattern_matching) {
  using namespace com::centreon::common::literals;
  std::string json_str = R"({
        "path": ")" + root_.string() +
                         R"(",
        "max-depth": -1,
        "pattern": "*.{txt,log}",
        "verbose": false,
        "files-detail-syntax": "${filename}",
        "ok-syntax": "${status}: {list}"
})";
  // Replace all '\' with '\\' in the path for JSON
  size_t pos = 0;
  while ((pos = json_str.find("\\", pos)) != std::string::npos) {
    json_str.replace(pos, 1, "\\\\");
    pos += 2;
  }
  std::cout << "JSON String: " << json_str << std::endl;
  rapidjson::Document check_args;
  check_args.Parse(json_str.c_str());

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

  checker->start_check(std::chrono::seconds(120));

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  ASSERT_TRUE(output.starts_with("OK: "))
      << "Unexpected status line: " << output;

  pos = output.find(':');
  ASSERT_NE(pos, std::string::npos);  // we just tested starts_with

  std::string list_str = output.substr(pos + 1);  // after "OK:"
  std::string_view list = list_str;
  list.remove_prefix(list.find_first_not_of(' '));  // trim

  size_t count = 0;
  while (!list.empty()) {
    auto comma = list.find(',');
    std::string_view file =
        list.substr(0, comma == std::string_view::npos ? list.size() : comma);

    ASSERT_TRUE(file.ends_with(".txt") || file.ends_with(".log"))
        << "Found unexpected extension in \"" << file << '"';

    ++count;
    if (comma == std::string_view::npos)
      break;
    list.remove_prefix(comma + 1);
  }

  EXPECT_EQ(count, 26u) << "Fixture should contain 26 *.txt / *.log files";
}

TEST_F(check_files_test, no_dangling_pointer) {
  using namespace com::centreon::common::literals;
  rapidjson::Document check_args =
      R"({
        "path": "C:\\Windows",
        "max-depth": 1,
        "pattern": "*.exe"
        })"_json;

  absl::Mutex wait_m;
  std::list<com::centreon::common::perfdata> perfs;
  std::string output;
  bool complete = false;

  auto is_complete = [&]() { return complete; };

  {
    auto checker = std::make_shared<check_files>(
        g_io_context, spdlog::default_logger(),
        std::chrono::system_clock::now(), std::chrono::seconds(1), "serv"s,
        "cmd_name"s, "cmd_line"s, check_args, nullptr,
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

    checker->start_check(std::chrono::seconds(10));
    checker.reset();  // Reset the checker to ensure it is deleted
  }

  absl::MutexLock lck(&wait_m);
  wait_m.Await(absl::Condition(&is_complete));

  re2::RE2 ok_regex(R"(OK: All \d+ files are ok)");
  ASSERT_TRUE(RE2::FullMatch(output, ok_regex))
      << "Output format does not match expected pattern: " << output;
}