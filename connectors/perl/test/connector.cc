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
#include <gtest/gtest.h>

#include <fstream>

#include "com/centreon/clib.hh"
#include "com/centreon/connector/log.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/misc/command_line.hh"

using namespace com::centreon::connector;
using namespace com::centreon::exceptions;

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

static std::string perl_connector = BUILD_PATH
    "/connectors/perl/"
    "centreon_connector_perl --debug --log-file=/data/dev/connector.log";

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

class process : public std::enable_shared_from_this<process> {
  shared_io_context _io_context;
  asio::readable_pipe _out, _err;
  asio::writable_pipe _in;

  std::string _cmd_line;
  pid_t _child;

  mutable std::mutex _protect;
  mutable std::condition_variable _wait_for_completion;

 public:
  using pointer = std::shared_ptr<process>;

  process(const std::string& cmd_line, const shared_io_context& io_context);
  ~process() { kill(SIGKILL); }

  void start();
  void close_std_in() { _in.close(); }
  void kill(int signal);
  void write(const std::string& data, const duration& time_out);
  int get_exit_code();
  std::string read_std_out(const duration& time_out);
  std::string read_std_err(const duration& time_out);
};

process::process(const std::string& cmd_line,
                 const shared_io_context& io_context)
    : _io_context(io_context),
      _out(*io_context),
      _err(*io_context),
      _in(*io_context),
      _cmd_line(cmd_line),
      _child(-1) {}

void process::start() {
  // Open pipes.
  int in_pipe[2];
  int err_pipe[2];
  int out_pipe[2];
  if (pipe(in_pipe)) {
    char const* msg(strerror(errno));
    throw msg_fmt("{}", msg);
  } else if (pipe(err_pipe)) {
    char const* msg(strerror(errno));
    close(in_pipe[0]);
    close(in_pipe[1]);
    throw msg_fmt("{}", msg);
  }
  if (pipe(out_pipe)) {
    char const* msg(strerror(errno));
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    throw msg_fmt("{}", msg);
  }

  _io_context->notify_fork(asio::io_context::fork_prepare);
  // Execute Perl file.
  _child = fork();
  if (_child > 0) {  // Parent
    _io_context->notify_fork(asio::io_context::fork_parent);
    log::core()->info("start child pid={}", _child);
    close(in_pipe[0]);
    close(err_pipe[1]);
    close(out_pipe[1]);
    _in.assign(in_pipe[1]);
    _out.assign(out_pipe[0]);
    _err.assign(err_pipe[0]);
  } else if (!_child) {  // Child
    _io_context->notify_fork(asio::io_context::fork_child);
    _io_context->stop();
    // Setup process.
    close(in_pipe[1]);
    close(err_pipe[0]);
    close(out_pipe[0]);
    if (dup2(in_pipe[0], STDIN_FILENO) < 0) {
      char const* msg(strerror(errno));
      std::cerr << "dup2 error: " << msg << std::endl;
      close(in_pipe[0]);
      close(err_pipe[1]);
      close(out_pipe[1]);
      exit(3);
    }
    close(in_pipe[0]);
    if (dup2(err_pipe[1], STDERR_FILENO) < 0) {
      char const* msg(strerror(errno));
      std::cerr << "dup2 error: " << msg << std::endl;
      close(err_pipe[1]);
      close(out_pipe[1]);
      exit(3);
    }
    close(err_pipe[1]);
    if (dup2(out_pipe[1], STDOUT_FILENO) < 0) {
      char const* msg(strerror(errno));
      std::cerr << "dup2 error: " << msg << std::endl;
      close(out_pipe[1]);
      exit(3);
    }
    close(out_pipe[1]);

    com::centreon::misc::command_line cmdline(_cmd_line);
    char* const* args = cmdline.get_argv();

    static char* env[] = {nullptr};

    ::execve(args[0], args, env);

    exit(EXIT_SUCCESS);

  } else if (_child < 0) {  // Error
    char const* msg(strerror(errno));
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    throw msg_fmt("{}", msg);
  }
}

void process::kill(int signal) {
  if (_child > 0) {
    ::kill(_child, signal);
  }
}

void process::write(const std::string& data, const duration& time_out) {
  using err_buff_pair = std::pair<boost::system::error_code, std::string>;
  std::shared_ptr<err_buff_pair> buff(
      std::make_shared<err_buff_pair>(boost::system::error_code(), data));

  asio::async_write(_in,
                    asio::buffer(buff->second.c_str(), buff->second.length()),
                    [me = shared_from_this(), this, buff](
                        const boost::system::error_code& err, size_t) {
                      buff->first = err;
                      std::unique_lock<std::mutex> l(_protect);
                      _wait_for_completion.notify_one();
                    });

  std::unique_lock<std::mutex> l(_protect);
  _wait_for_completion.wait_for(l, time_out);
  if (buff->first) {
    throw msg_fmt("fail to write:{}", buff->first.message());
  }
}

std::string process::read_std_out(const duration& time_out) {
  using recv_data =
      std::tuple<boost::system::error_code, size_t, std::array<char, 4096>>;
  std::shared_ptr<recv_data> data(std::make_shared<recv_data>());
  _out.async_read_some(
      asio::buffer(std::get<2>(*data)),
      [me = shared_from_this(), this, data](
          const boost::system::error_code& err, size_t nb_recv) {
        std::get<0>(*data) = err;
        std::get<1>(*data) = nb_recv;
        std::unique_lock<std::mutex> l(_protect);
        _wait_for_completion.notify_one();
      });
  std::unique_lock<std::mutex> l(_protect);
  _wait_for_completion.wait_for(l, time_out);
  if (std::get<0>(*data)) {
    if (std::get<0>(*data) == asio::error::eof) {
      return "eof";
    }
    log::core()->error("fail to read from std_out:{}",
                       std::get<0>(*data).message());
    throw msg_fmt("fail to read from std_out:{}", std::get<0>(*data).message());
  }
  return std::string(std::get<2>(*data).data(),
                     std::get<2>(*data).data() + std::get<1>(*data));
}

std::string process::read_std_err(const duration& time_out) {
  using recv_data =
      std::tuple<boost::system::error_code, size_t, std::array<char, 4096>>;
  std::shared_ptr<recv_data> data(std::make_shared<recv_data>());
  _out.async_read_some(
      asio::buffer(std::get<2>(*data)),
      [me = shared_from_this(), this, data](
          const boost::system::error_code& err, size_t nb_recv) {
        std::get<0>(*data) = err;
        std::get<1>(*data) = nb_recv;
        std::unique_lock<std::mutex> l(_protect);
        _wait_for_completion.notify_one();
      });
  std::unique_lock<std::mutex> l(_protect);
  _wait_for_completion.wait_for(l, time_out);
  if (std::get<0>(*data)) {
    if (std::get<0>(*data) == asio::error::eof) {
      return "eof";
    }
    log::core()->error("fail to read from std_err:{}",
                       std::get<0>(*data).message());
    throw msg_fmt("fail to read from std_err:{}", std::get<0>(*data).message());
  }
  return std::string(std::get<2>(*data).data(),
                     std::get<2>(*data).data() + std::get<1>(*data));
}

int process::get_exit_code() {
  if (_child <= 0) {
    log::core()->error("son not started");
    return -1;
  }
  int status;
  waitpid(_child, &status, 0);
  return status;
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
    p.write(cmd, std::chrono::seconds(1));
    p.close_std_in();
  }

  std::string read_reply(process& p) {
    return p.read_std_out(std::chrono::seconds(5));
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

  int retval = wait_for_termination(*p);

  ASSERT_EQ(retval, 0);
}

TEST_F(TestConnector, ExecuteModuleLoading) {
  // Write Perl script.
  std::string script_path(com::centreon::io::file_stream::temp_path());
  _write_file(script_path.c_str(),
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

  int retval{wait_for_termination(*p)};

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
  do {
    out_read = read_reply(*p);
    output += out_read;
  } while (out_read != "eof");

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
      perl_connector + " --log-file /tmp/log_file", _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(cmd1, sizeof(cmd1) - 1);
  oss << script_path;
  oss.write(cmd2, sizeof(cmd2) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output{read_reply(*p)};

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
  std::string script_path(com::centreon::io::file_stream::temp_path());
  _write_file(
      script_path.c_str(),
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

  int retval{wait_for_termination(*p)};

  // Remove temporary files.
  remove(script_path.c_str());

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

  int retval{wait_for_termination(*p)};

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
  process::pointer p = std::make_shared<process>(perl_connector, _io_context);
  p->start();

  // Write command.
  std::ostringstream oss;
  oss.write(TimeoutKillCMD, sizeof(TimeoutKillCMD) - 1);
  write_cmd(*p, oss.str());

  // Read reply.
  std::string output(p->read_std_out(std::chrono::seconds(25)));

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
  std::string output(p->read_std_out(std::chrono::seconds(5)));

  int retval{wait_for_termination(*p)};

  ASSERT_EQ(retval, 0);
  std::string expected(
      TimeoutKillTermRESULT,
      TimeoutKillTermRESULT + sizeof(TimeoutKillTermRESULT) - 1);
  ASSERT_EQ(output, expected);
}
