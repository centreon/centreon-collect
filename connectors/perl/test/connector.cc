/**
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
#include <absl/time/time.h>
#include <gtest/gtest.h>

#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <ostream>
#include <thread>

#include "com/centreon/common/process/process.hh"
#include "com/centreon/connector/log.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon::connector;
using namespace com::centreon::exceptions;

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

static std::string perl_connector = BUILD_PATH
    "/connectors/perl/"
    "centreon_connector_perl --debug --log-file=/tmp/connector.log";

static std::string perl_connector_without_log = BUILD_PATH
    "/connectors/perl/"
    "centreon_connector_perl --debug";

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

#define TimeoutKillCMD \
  "2\0"                \
  "4242\0"             \
  "3\0"                \
  "123456789\0" BUILD_PATH "/../connectors/perl/test/timeout_kill.pl\0\0\0\0"
#define TimeoutKillRESULT \
  "3\0"                   \
  "4242\0"                \
  "1\0"                   \
  "3\0"                   \
  "(Process Timeout)\0"   \
  " \0\0\0\0"

#define TimeoutTermCMD \
  "2\0"                \
  "4242\0"             \
  "3\0"                \
  "123456789\0" BUILD_PATH "/../connectors/perl/test/timeout_term.pl\0\0\0\0"

using shared_io_context = std::shared_ptr<asio::io_context>;
using work_guard =
    boost::asio::executor_work_guard<asio::io_context::executor_type>;

static shared_io_context _io_context(std::make_shared<asio::io_context>());
static std::unique_ptr<work_guard> _work_guard;

class process : public com::centreon::common::process<true> {
  std::string _read_stdout ABSL_GUARDED_BY(_read_stdout_m);
  boost::system::error_code _read_stdout_err ABSL_GUARDED_BY(_read_stdout_m);
  absl::Mutex _read_stdout_m;

  std::optional<int> _exit_code ABSL_GUARDED_BY(_exit_code_m);
  absl::Mutex _exit_code_m;

  void _on_read_stdout(const boost::system::error_code& err,
                       const std::string_view& data);

 public:
  using pointer = std::shared_ptr<process>;
  process(const std::string& cmd_line, const shared_io_context& io_context)
      : com::centreon::common::process<true>(io_context,
                                             log::core(),
                                             cmd_line,
                                             false,
                                             true,
                                             {}) {}

  void start();

  std::shared_ptr<process> shared_from_this() {
    return std::static_pointer_cast<process>(
        com::centreon::common::process<true>::shared_from_this());
  }

  std::string read_stdout(const duration& time_out);

  int get_exit_code();
};

void process::start() {
  com::centreon::common::process<true>::start_process(
      [me = shared_from_this()](const com::centreon::common::process<true>&,
                                int exit_code,
                                com::centreon::common::e_exit_status,
                                const std::string& stdout, const std::string&) {
        absl::MutexLock l(&me->_exit_code_m);
        me->_exit_code = exit_code;
      },
      [me = shared_from_this()](const boost::system::error_code& err,
                                const std::string_view& data) {
        me->_on_read_stdout(err, data);
      },
      [](const boost::system::error_code&, const std::string_view&) {}, {});
}

void process::_on_read_stdout(const boost::system::error_code& err,
                              const std::string_view& data) {
  absl::MutexLock l(&_read_stdout_m);
  _read_stdout_err = err;
  if (err == asio::error::eof) {
    _read_stdout = "eof";
  } else {
    _read_stdout += data;
  }
}

std::string process::read_stdout(const duration& timeout) {
  absl::MutexLock l(&_read_stdout_m);
  if (!_read_stdout.empty()) {
    return std::move(_read_stdout);
  }
  _read_stdout_m.AwaitWithTimeout(
      absl::Condition(
          +[](process* proc) { return !proc->_read_stdout.empty(); }, this),
      absl::Seconds(
          std::chrono::duration_cast<std::chrono::seconds>(timeout).count()));
  return std::move(_read_stdout);
}

int process::get_exit_code() {
  absl::MutexLock l(&_exit_code_m);
  if (_exit_code) {
    return *_exit_code;
  }
  _exit_code_m.Await(absl::Condition(
      +[](process* proc) { return proc->_exit_code.has_value(); }, this));
  return *_exit_code;
}

class TestConnector : public testing::Test {
 public:
  void SetUp() override {};
  void TearDown() override {};
  static void SetUpTestSuite() {
    _work_guard = std::make_unique<work_guard>(_io_context->get_executor());
    std::thread t([]() { _io_context->run(); });
    t.detach();
  }
  static void TearDownTestSuite() { _work_guard.reset(); }

  int wait_for_termination(process& p) { return p.get_exit_code(); }

  void write_cmd(process& p, std::string const& cmd) {
    p.write_to_child_stdin(cmd);
    // p.close_stdin();
  }

  std::string read_reply(process& p) {
    return p.read_stdout(std::chrono::seconds(5));
  }

  static void _write_file(const char* filename,
                          char const* content,
                          unsigned int size = 0) {
    // Check size.
    if (!size)
      size = strlen(content);

    // Open file.
    FILE* f = fopen(filename, "w");
    if (!f)
      throw msg_fmt("could not open file {}", filename);

    // Write content.
    while (size > 0) {
      size_t wb(fwrite(content, sizeof(*content), size, f));
      if (ferror(f)) {
        fclose(f);
        throw msg_fmt("error while writing file {}", filename);
      }
      size -= wb;
    }

    // Close handle.
    fclose(f);
  }
};

TEST_F(TestConnector, EofOnStdin) {
  // Process.
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();
  write_cmd(*p, "");
  p->close_stdin();

  int retval = wait_for_termination(*p);

  ASSERT_EQ(retval, 0);
}

TEST_F(TestConnector, ExecuteModuleLoading) {
  // Write Perl script.
  const char* script_path = com::centreon::io::file_stream::temp_path();
  _write_file(script_path,
              "#!/usr/bin/perl\n"
              "\n"
              "use Sys::Hostname;\n"
              "use IO::Socket;\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 0;\n");
  log::core()->info("write perl code to {}", script_path);

  // Process.
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output{read_reply(*p)};

  p->close_stdin();
  int retval{wait_for_termination(*p)};

  // Remove temporary files.
  remove(script_path);

  ASSERT_EQ(retval, 0);
  std::string expected(result, result + sizeof(result) - 1);

  ASSERT_EQ(output, expected);
}

TEST_F(TestConnector, ExecuteMultipleScripts) {
  // Write Perl scripts.
  std::string script_paths[10];
  for (auto& script_path : script_paths) {
    script_path = com::centreon::io::file_stream::temp_path();
    log::core()->info("write perl code to {}", script_path);
    _write_file(script_path.c_str(), scripts, sizeof(scripts) - 1);
  }

  // Process.
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

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
  write_cmd(*p, cmd);

  // Read reply.

  std::string output, out_read;
  while (true) {
    out_read = read_reply(*p);
    if (out_read.empty()) {
      break;
    }
    output += out_read;
  }

  p->close_stdin();
  int retval{wait_for_termination(*p)};

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
  std::string script_path(com::centreon::io::file_stream::temp_path());
  _write_file(script_path.c_str(),
              "#!/usr/bin/perl\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 0;\n");
  log::core()->info("write perl code to {}", script_path);

  // Process.
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output{read_reply(*p)};

  p->close_stdin();
  int retval{wait_for_termination(*p)};

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
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output{read_reply(*p)};

  p->close_stdin();

  int retval{wait_for_termination(*p)};

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
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output{read_reply(*p)};

  p->close_stdin();

  int retval{wait_for_termination(*p)};

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
  std::string script_path(com::centreon::io::file_stream::temp_path());
  _write_file(script_path.c_str(),
              "#!/usr/bin/perl\n"
              "\n"
              "print \"Centreon is wonderful\\n\";\n"
              "exit 0;\n");
  log::core()->info("write perl code to {}", script_path);

  // Process.
  process::pointer p = std::make_shared<process>(
      perl_connector_without_log + " --log-file /tmp/log_file", _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output{read_reply(*p)};

  p->close_stdin();

  int retval{wait_for_termination(*p)};

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
  const char* script_path(com::centreon::io::file_stream::temp_path());
  _write_file(
      script_path,
      "#!/usr/bin/perl\n"
      "\n"
      "print \"$Centreon::Test::company is $Centreon::Test::attribute\\n\";\n"
      "exit 0;\n");
  log::core()->info("write perl code to {}", script_path);

  // Process.
  process::pointer p = std::make_shared<process>(
      perl_connector +
          " --code 'package Centreon::Test; our $company=\"Centreon\"; our "
          "$attribute=\"wonderful\";'",
      _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output{read_reply(*p)};

  p->close_stdin();

  int retval{wait_for_termination(*p)};

  // Remove temporary files.
  remove(script_path);

  ASSERT_EQ(retval, 0);
  std::string expected(result, result + sizeof(result) - 1);
  ASSERT_EQ(output, expected);
}

TEST_F(TestConnector, NonExistantScript) {
  // Process.
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(NonExistantCMD, sizeof(NonExistantCMD) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output{read_reply(*p)};

  p->close_stdin();

  int retval{wait_for_termination(*p)};

  ASSERT_EQ(retval, 0);
  ASSERT_NE(output.find("failed to open Perl file"), std::string::npos);
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
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(TimeoutKillCMD, sizeof(TimeoutKillCMD) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output(p->read_stdout(std::chrono::seconds(25)));

  std::this_thread::sleep_for(std::chrono::seconds(15));

  p->close_stdin();

  int retval{wait_for_termination(*p)};

  ASSERT_EQ(retval, 0);
  std::string expected(TimeoutKillRESULT,
                       TimeoutKillRESULT + sizeof(TimeoutKillRESULT) - 1);
  ASSERT_EQ(output, expected);
}

TEST_F(TestConnector, TimeoutTerm) {
  // Process.
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(TimeoutTermCMD, sizeof(TimeoutTermCMD) - 1);
  std::string cmd(oss.str());
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output(p->read_stdout(std::chrono::seconds(5)));

  p->close_stdin();

  int retval{wait_for_termination(*p)};

  ASSERT_EQ(retval, 0);
  std::string expected(TimeoutKillRESULT,
                       TimeoutKillRESULT + sizeof(TimeoutKillRESULT) - 1);
  ASSERT_EQ(output, expected);
}
