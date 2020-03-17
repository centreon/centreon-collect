/*
 * Copyright 2020 Centreon (https://www.centreon.com/)
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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/process.hh"

using namespace com::centreon;
static std::string perl_connector = BUILD_PATH "/perl/centreon_connector_perl";

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

#define TimeoutKillCMD \
  "2\0"                \
  "4242\0"             \
  "3\0"                \
  "123456789\0" BUILD_PATH "/../perl/test/timeout_kill.pl\0\0\0\0"
#define TimeoutKillRESULT \
  "3\0"                   \
  "4242\0"                \
  "1\0"                   \
  "-1\0"                  \
  " \0"                   \
  " \0\0\0\0"

#define TimeoutTermCMD \
  "2\0"                \
  "4242\0"             \
  "3\0"                \
  "123456789\0" BUILD_PATH "/../perl/test/timeout_term.pl\0\0\0\0"

class TestConnector : public testing::Test {
 public:
  process p;

  void SetUp() override{};
  void TearDown() override{};

  void launch_process(std::string const& process_name) {
    p.enable_stream(process::in, true);
    p.enable_stream(process::out, true);
    p.enable_stream(process::err, false);
    p.exec(process_name.c_str());
  }

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
    p.enable_stream(process::in, false);
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

  void _write_file(char const* filename,
                   char const* content,
                   unsigned int size = 0) {
    // Check size.
    if (!size)
      size = strlen(content);

    // Open file.
    FILE* f(fopen(filename, "w"));
    if (!f)
      throw(basic_error() << "could not open file " << filename);

    // Write content.
    while (size > 0) {
      size_t wb(fwrite(content, sizeof(*content), size, f));
      if (ferror(f)) {
        fclose(f);
        throw(basic_error() << "error while writing file " << filename);
      }
      size -= wb;
    }

    // Close handle.
    fclose(f);
  }
};

TEST_F(TestConnector, EofOnStdin) {
  // Process.
  launch_process(perl_connector);
  p.enable_stream(process::in, false);

  int retval{wait_for_termination()};

  ASSERT_EQ(retval, 0);
}

TEST_F(TestConnector, ExecuteModuleLoading) {
  // Write Perl script.
  std::string script_path(io::file_stream::temp_path());
  _write_file(script_path.c_str(),
              "#!/usr/bin/perl\n"
              "\n"
              "use Error::Simple;\n"
              "use IO::Socket;\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 0;\n");

  // Process.
  launch_process(perl_connector);

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

TEST_F(TestConnector, ExecuteMultipleScripts) {
  // Write Perl scripts.
  std::string script_paths[10];
  for (unsigned int i = 0; i < sizeof(script_paths) / sizeof(*script_paths);
       ++i) {
    script_paths[i] = io::file_stream::temp_path();
    _write_file(script_paths[i].c_str(), scripts, sizeof(scripts) - 1);
  }

  // Process.
  launch_process(perl_connector);

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
  for (unsigned int i = 0; i < sizeof(script_paths) / sizeof(*script_paths);
       ++i)
    remove(script_paths[i].c_str());

  unsigned int nb_right_output(0);
  for (size_t pos(0); (pos = output.find(result2, pos)) != std::string::npos;
       ++nb_right_output, ++pos)
    ;

  ASSERT_TRUE(nb_right_output == count);
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
  launch_process(perl_connector);

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
  launch_process(perl_connector + " --log-file /tmp/log_file");

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
      line.find(
          "[info] Centreon Perl Connector " CENTREON_CONNECTOR_VERSION
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
  launch_process(
      perl_connector +
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
  launch_process(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(NonExistantCMD, sizeof(NonExistantCMD) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  ASSERT_EQ(retval, 0);
  ASSERT_EQ(output.find("could not run"), std::string::npos);
  ASSERT_EQ(output.size(), sizeof(NonExistantRESULT) - 1);
  ASSERT_FALSE(
      memcmp(output.c_str(), NonExistantRESULT, sizeof(NonExistantRESULT) - 1));
}

/**
 *  Check that connector properly kills timeouting processes.
 *
 *  @return 0 on success.
 */
TEST_F(TestConnector, TimeoutKill) {
  // Process.
  launch_process(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(TimeoutKillCMD, sizeof(TimeoutKillCMD) - 1);
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  ASSERT_EQ(retval, 0);
  ASSERT_EQ(output.size(), sizeof(TimeoutKillRESULT) - 1);
  ASSERT_FALSE(
      memcmp(output.c_str(), TimeoutKillRESULT, sizeof(TimeoutKillRESULT) - 1));
}

TEST_F(TestConnector, TimeoutTerm) {
  // Process.
  launch_process(perl_connector);

  // Write command.
  std::ostringstream oss;
  oss.write(TimeoutTermCMD, sizeof(TimeoutTermCMD) - 1);
  std::string cmd(oss.str());
  write_cmd(oss.str());

  // Read reply.
  std::string output{std::move(read_reply())};

  int retval{wait_for_termination()};

  ASSERT_EQ(retval, 0);
  ASSERT_EQ(output.size(), sizeof(TimeoutKillRESULT) - 1);
  ASSERT_FALSE(
      memcmp(output.c_str(), TimeoutKillRESULT, sizeof(TimeoutKillRESULT) - 1));
}
