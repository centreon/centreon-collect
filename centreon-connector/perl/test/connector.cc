/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include <gtest/gtest.h>

#include <fstream>

#include "com/centreon/clib.hh"
#include "com/centreon/connector/log.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/process.hh"

using namespace com::centreon;
using namespace com::centreon::connector;

static std::string perl_connector = BUILD_PATH "/bin/centreon_connector_perl";

static constexpr const char cmd1[] =
    "2\x00"
    "4242\x00"
    "5\x00"
    "123456789\x00";
static constexpr const char cmd2[] = "\x00\x00\x00\x00";
static constexpr const char result[] =
    "3\x00"
    "4242\x00"
    "1\x00"
    "0\x00"
    " \x00"
    "Centreon is wonderful\n"
    "\x00\x00\x00\x00";

static constexpr const char result_warning[] =
    "3\x00"
    "4242\x00"
    "1\x00"
    "1\x00"
    " \x00"
    "Centreon is wonderful\n"
    "\x00\x00\x00\x00";

static constexpr const char result_critical[] =
    "3\x00"
    "4242\x00"
    "1\x00"
    "2\x00"
    " \x00"
    "Centreon is wonderful\n"
    "\x00\x00\x00\x00";

static constexpr std::size_t count = 300;

static constexpr const char cmd3[] = "2\x00";
static constexpr const char cmd4[] =
    "\x00"
    "5\x00"
    "123456789\x00";
static constexpr const char cmd5[] = "\x00\x00\x00\x00";
static constexpr const char result2[] = "Centreon is wonderful\n";
static constexpr const char scripts[] =
    "#!/usr/bin/perl\n\nprint \"Centreon is wonderful\\n\";\nexit 2;\n";

#define NonExistantCMD \
  "2\0"                \
  "4242\0"             \
  "3\0"                \
  "123456789\0"        \
  "/non/existent/script.pl\0\0\0\0"
#define NonExistantRESULT \
  "3\0"                   \
  "4242\0"                \
  "0\0"                   \
  "-1\0"                  \
  " \0"                   \
  " \0\0\0\0"

#define TimeoutKillCMD     \
  "2\0"                    \
  "4242\0"                 \
  "3\0"                    \
  "123456789\0" BUILD_PATH \
  "/../centreon-connector/perl/test/timeout_kill.pl\0\0\0\0"
#define TimeoutKillRESULT \
  "3\0"                   \
  "4242\0"                \
  "1\0"                   \
  "9\0"                   \
  " time out time out\0"  \
  " \0\0\0\0"

#define TimeoutKillTermRESULT \
  "3\0"                       \
  "4242\0"                    \
  "1\0"                       \
  "15\0"                      \
  " time out\0"               \
  " \0\0\0\0"

#define TimeoutTermCMD     \
  "2\0"                    \
  "4242\0"                 \
  "3\0"                    \
  "123456789\0" BUILD_PATH \
  "/../centreon-connector/perl/test/timeout_term.pl\0\0\0\0"

class TestConnector : public testing::Test {
 public:
  void SetUp() override{};
  void TearDown() override{};
  TestConnector() : testing::Test(), p(nullptr, true, true, false) {}

  int wait_for_termination() {
    // Wait for process termination.
    int retval(1);
    if (!p.wait(1000)) {
      p.terminate();
      p.wait();
    } else
      retval = (p.exit_code() != 0);

    return retval;
  }

  void write_cmd(std::string const& cmd) {
    char const* ptr(cmd.c_str());
    unsigned int size(cmd.size());
    while (size > 0) {
      unsigned int rb(p.write(ptr, size));
      size -= rb;
      ptr += rb;
    }
    p.update_ending_process(0);
  }

  std::string read_reply() {
    std::string output;

    while (true) {
      std::string buffer;
      p.read(buffer);
      if (buffer.empty())
        break;
      output.append(buffer);
    }

    return output;
  }

  static void _write_file(char const* filename,
                          char const* content,
                          unsigned int size = 0) {
    // Check size.
    if (!size)
      size = strlen(content);

    // Open file.
    FILE* f(fopen(filename, "w"));
    if (!f)
      throw basic_error() << "could not open file " << filename;

    // Write content.
    while (size > 0) {
      size_t wb(fwrite(content, sizeof(*content), size, f));
      if (ferror(f)) {
        fclose(f);
        throw basic_error() << "error while writing file " << filename;
      }
      size -= wb;
    }

    // Close handle.
    fclose(f);
  }

 protected:
  process p;
};

TEST_F(TestConnector, EofOnStdin) {
  // Process.
  p.exec(perl_connector);
  p.update_ending_process(0);

  int retval = wait_for_termination();

  ASSERT_EQ(retval, 0);
}

TEST_F(TestConnector, ExecuteModuleLoading) {
  // Write Perl script.
  std::string script_path(io::file_stream::temp_path());
  _write_file(script_path.c_str(),
              "#!/usr/bin/perl\n"
              "\n"
              "use Sys::Hostname;\n"
              "use IO::Socket;\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 0;\n");

  // Process.
  p.exec(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  // Remove temporary files.
  remove(script_path.c_str());

  ASSERT_EQ(retval, 0);
  std::string expected(result, result + sizeof(result) - 1);

  ASSERT_EQ(output, expected);
}

TEST_F(TestConnector, ExecuteMultipleScripts) {
  // Write Perl scripts.
  std::string script_paths[10];
  for (auto& script_path : script_paths) {
    script_path = io::file_stream::temp_path();
    _write_file(script_path.c_str(), scripts, sizeof(scripts) - 1);
  }

  // Process.
  p.exec(perl_connector);

  // Generate command string.
  std::string cmd;
  {
    std::ostringstream oss;
    for (unsigned int i = 0; i < count; ++i) {
      oss.write(cmd3, sizeof(cmd3) - 1);
      oss << i + 1;
      oss.write(cmd4, sizeof(cmd4) - 1);
      oss << script_paths[i % (sizeof(script_paths) / sizeof(*script_paths))];
      oss.write(cmd5, sizeof(cmd5) - 1);
    }
    cmd = oss.str();
  }
  write_cmd(cmd);

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  // Remove temporary files.
  for (auto& script_path : script_paths)
    remove(script_path.c_str());

  unsigned int nb_right_output(0);
  for (size_t pos(0); (pos = output.find(result2, pos)) != std::string::npos;
       ++nb_right_output, ++pos)
    ;

  ASSERT_EQ(nb_right_output, count);
  ASSERT_EQ(retval, 0);
}

TEST_F(TestConnector, ExecuteSingleScript) {
  // Write Perl script.
  std::string script_path(io::file_stream::temp_path());
  _write_file(script_path.c_str(),
              "#!/usr/bin/perl\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 0;\n");

  // Process.
  p.exec(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  // Remove temporary files.
  remove(script_path.c_str());

  ASSERT_EQ(retval, 0);
  std::string expected(result, result + sizeof(result) - 1);
  ASSERT_EQ(output, expected);
}

TEST_F(TestConnector, ExecuteSingleWarningScript) {
  // Write Perl script.
  std::string script_path(com::centreon::io::file_stream::temp_path());
  _write_file(script_path.c_str(),
              "#!/usr/bin/perl\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 1;\n");
  log::core()->info("write perl code to {}", script_path);

  // Process.
  p.exec(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  // Remove temporary files.
  remove(script_path.c_str());

  ASSERT_EQ(retval, 0);
  std::string expected(result_warning,
                       result_warning + sizeof(result_warning) - 1);
  ASSERT_EQ(output, expected);
}

TEST_F(TestConnector, ExecuteSingleCriticalScript) {
  // Write Perl script.
  std::string script_path(com::centreon::io::file_stream::temp_path());
  _write_file(script_path.c_str(),
              "#!/usr/bin/perl\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 2;\n");
  log::core()->info("write perl code to {}", script_path);

  // Process.
  p.exec(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  // Remove temporary files.
  remove(script_path.c_str());

  ASSERT_EQ(retval, 0);
  std::string expected(result_critical,
                       result_critical + sizeof(result_critical) - 1);
  ASSERT_EQ(output, expected);
}

TEST_F(TestConnector, ExecuteSingleScriptLogFile) {
  // If the log file exists, we remove it.
  std::ifstream f("/tmp/log_file");
  if (f.good()) {
    f.close();
    remove("/tmp/log_file");
  }

  // Write Perl script.
  std::string script_path(io::file_stream::temp_path());
  _write_file(script_path.c_str(),
              "#!/usr/bin/perl\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 0;\n");

  // Process.
  p.exec(perl_connector + " --log-file /tmp/log_file");

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  // Remove temporary files.
  remove(script_path.c_str());

  ASSERT_EQ(retval, 0);
  ASSERT_EQ(output.size(), sizeof(result) - 1);
  ASSERT_FALSE(memcmp(output.c_str(), result, sizeof(result) - 1));
  std::ifstream file("/tmp/log_file");
  ASSERT_TRUE(file.is_open());
  std::string line((std::istreambuf_iterator<char>(file)),
                   std::istreambuf_iterator<char>());
  ASSERT_NE(
      line.find("[info] Centreon Perl Connector " CENTREON_CONNECTOR_VERSION
                " starting"),
      std::string::npos);
  file.close();
}

TEST_F(TestConnector, ExecuteWithAdditionalCode) {
  // Write Perl script.
  std::string script_path(io::file_stream::temp_path());
  _write_file(
      script_path.c_str(),
      "#!/usr/bin/perl\n"
      "\n"
      "print \"$Centreon::Test::company is $Centreon::Test::attribute\\n\";\n"
      "exit 0;\n");

  // Process.
  p.exec(perl_connector +
         " --code 'package Centreon::Test; our $company=\"Centreon\"; our "
         "$attribute=\"wonderful\";'");

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  // Remove temporary files.
  remove(script_path.c_str());

  ASSERT_EQ(retval, 0);
  ASSERT_EQ(output.size(), (sizeof(result) - 1));
  ASSERT_FALSE(memcmp(output.c_str(), result, sizeof(result) - 1));
}

TEST_F(TestConnector, NonExistantScript) {
  // Process.
  p.exec(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(NonExistantCMD, sizeof(NonExistantCMD) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  ASSERT_EQ(retval, 0);
  ASSERT_NE(output.find("Embedded Perl error: failed to open Perl file"),
            std::string::npos);
  ASSERT_FALSE(memcmp(output.c_str(), NonExistantRESULT,
                      12));  // 12 is the length of beginning of the response
                             // without error message
}

/**
 *  Check that connector properly kills timeouting processes.
 *
 *  @return 0 on success.
 */
TEST_F(TestConnector, TimeoutKill) {
  // Process.
  p.exec(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(TimeoutKillCMD, sizeof(TimeoutKillCMD) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  ASSERT_EQ(retval, 0);
  std::string expected(TimeoutKillRESULT,
                       TimeoutKillRESULT + sizeof(TimeoutKillRESULT) - 1);
  ASSERT_EQ(output, expected);
}

TEST_F(TestConnector, TimeoutTerm) {
  // Process.
  p.exec(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(TimeoutTermCMD, sizeof(TimeoutTermCMD) - 1);
  std::string cmd(oss.str());
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  ASSERT_EQ(retval, 0);
  std::string expected(
      TimeoutKillTermRESULT,
      TimeoutKillTermRESULT + sizeof(TimeoutKillTermRESULT) - 1);
  ASSERT_EQ(output, expected);
}
