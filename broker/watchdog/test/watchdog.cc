/**
 * Copyright 2019-2022 Centreon
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

#include <absl/algorithm/container.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <gtest/gtest.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <set>

#include "com/centreon/broker/misc/misc.hh"

using namespace com::centreon::broker;

void create_conf(std::string const& filename, std::string const& content) {
  std::ofstream oss(filename);
  oss << content;
}

void remove_file(std::string const& filename) {
  std::remove(filename.c_str());
}

namespace {
/**
 * @brief Read the state and the parent of a process from /proc.
 *
 * @param pid The process.
 * @param state Filled with the process state ('R', 'S', 'Z'...).
 * @param ppid Filled with the parent pid.
 *
 * @return false if the process does not exist (anymore).
 */
bool read_proc_stat(pid_t pid, char& state, pid_t& ppid) {
  std::ifstream f("/proc/" + std::to_string(pid) + "/stat");
  std::string line;
  if (!std::getline(f, line))
    return false;
  /* The command name is between parentheses and may contain spaces, the
   * fields we want follow the last ')'. */
  size_t pos = line.rfind(')');
  if (pos == std::string::npos || pos + 2 >= line.size())
    return false;
  std::vector<std::string_view> fields =
      absl::StrSplit(std::string_view(line).substr(pos + 2), ' ');
  if (fields.size() < 2 || fields[0].empty() ||
      !absl::SimpleAtoi(fields[1], &ppid))
    return false;
  state = fields[0][0];
  return true;
}

/**
 * @brief Tell if a process exists and is not a zombie.
 */
bool alive(pid_t pid) {
  char state;
  pid_t ppid;
  return read_proc_stat(pid, state, ppid) && state != 'Z';
}

/**
 * @brief The living children of a process. Only the processes started by the
 * cbwd of the current test are seen, not the ones of another test or of a
 * previous run.
 */
std::set<pid_t> living_children(pid_t parent) {
  std::set<pid_t> retval;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator("/proc", ec)) {
    pid_t pid;
    char state;
    pid_t ppid;
    if (absl::SimpleAtoi(entry.path().filename().string(), &pid) &&
        read_proc_stat(pid, state, ppid) && ppid == parent && state != 'Z')
      retval.insert(pid);
  }
  return retval;
}
}  // namespace

/**
 * @brief The cbwd started by a test is always stopped with its children, even
 * when the test fails: a cbwd left behind would restart its testers forever.
 */
class WatchdogTest : public ::testing::Test {
 protected:
  pid_t _cbwd = -1;

  void start_cbwd(const std::string& conf) {
    std::string cbwd("broker/watchdog/cbwd");
    std::string conf_arg(conf);
    char* argv[]{cbwd.data(), conf_arg.data(), nullptr};
    ASSERT_EQ(posix_spawn(&_cbwd, argv[0], nullptr, nullptr, argv, environ), 0);
  }

  /**
   * @brief Wait for cbwd to have count living children.
   *
   * @return The children pids (their number may differ on timeout).
   */
  std::set<pid_t> wait_for_children(size_t count, int timeout) {
    std::set<pid_t> retval = living_children(_cbwd);
    while (retval.size() != count && timeout-- > 0) {
      sleep(1);
      retval = living_children(_cbwd);
    }
    return retval;
  }

  /**
   * @brief Wait for cbwd to exit and reap it.
   *
   * @return true if cbwd exited before the timeout.
   */
  bool wait_for_cbwd_exit(int timeout) {
    for (;;) {
      if (waitpid(_cbwd, nullptr, WNOHANG) == _cbwd) {
        _cbwd = -1;
        return true;
      }
      if (timeout-- <= 0)
        return false;
      sleep(1);
    }
  }

  void TearDown() override {
    if (_cbwd > 0) {
      /* cbwd is frozen first, so that it does not restart the testers we
       * kill. */
      kill(_cbwd, SIGSTOP);
      for (pid_t child : living_children(_cbwd))
        kill(child, SIGKILL);
      kill(_cbwd, SIGKILL);
      waitpid(_cbwd, nullptr, 0);
      _cbwd = -1;
    }
  }
};

TEST_F(WatchdogTest, Help) {
  std::string result =
      com::centreon::broker::misc::exec("broker/watchdog/cbwd -h");
  ASSERT_EQ("USAGE: cbwd configuration_file\n", result);
}

TEST_F(WatchdogTest, NoConfig) {
  std::string result =
      com::centreon::broker::misc::exec("broker/watchdog/cbwd");
  ASSERT_EQ("USAGE: cbwd configuration_file\n", result);
}

TEST_F(WatchdogTest, NotExistingConfig) {
  std::string result =
      com::centreon::broker::misc::exec("broker/watchdog/cbwd foo");
  ASSERT_TRUE(
      result.find(
          "Could not parse the configuration file 'foo': Config parser: Cannot "
          "read file 'foo': No such file or directory\n") != std::string::npos);
}

TEST_F(WatchdogTest, BadConfig) {
  std::string result = com::centreon::broker::misc::exec(
      "broker/watchdog/cbwd " CENTREON_BROKER_WD_TEST "/bad-config.json");
  ASSERT_TRUE(
      result.find(
          "Could not parse the configuration file '" CENTREON_BROKER_WD_TEST
          "/bad-config.json': reload field not provided for cbd instance\n") !=
      std::string::npos);
}

TEST_F(WatchdogTest, SimpleConfig) {
  const std::string& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "/tester\",\n"
      "        \"name\": \"central-broker-master\",\n"
      "        \"configuration_file\": \"Master\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "/tester\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"configuration_file\": \"Slave\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      }\n"
      "    ],\n"
      "    \"log\": \"/tmp/watchdog.log\"\n"
      "  }\n"
      "}"};
  create_conf("/tmp/simple-conf.json", content);
  start_cbwd("/tmp/simple-conf.json");

  std::set<pid_t> testers = wait_for_children(2u, 5);
  ASSERT_EQ(testers.size(), 2u);

  // We send a term signal to one child, cbwd resurrects it.
  pid_t victim = *testers.begin();
  kill(victim, SIGTERM);
  int timeout = 10;
  do {
    sleep(1);
    testers = living_children(_cbwd);
  } while ((testers.size() != 2u || testers.count(victim)) && --timeout > 0);
  ASSERT_EQ(testers.size(), 2u);
  ASSERT_FALSE(testers.count(victim));

  // We send a term signal to cbwd, it stops its testers before exiting.
  kill(_cbwd, SIGTERM);
  ASSERT_TRUE(wait_for_cbwd_exit(10));
  timeout = 10;
  while (absl::c_any_of(testers, alive) && timeout-- > 0)
    sleep(1);
  ASSERT_FALSE(absl::c_any_of(testers, alive));
}

TEST_F(WatchdogTest, SimpleConfigUpdated) {
  std::string const& content1{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "/tester-echo\",\n"
      "        \"name\": \"central-broker-master\",\n"
      "        \"configuration_file\": \"Master\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "/tester-echo\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"configuration_file\": \"Slave\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      }\n"
      "    ],\n"
      "    \"log\": \"/tmp/watchdog.log\"\n"
      "  }\n"
      "}"};
  std::string const& content2{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "/tester-echo\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"configuration_file\": \"Slave\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "/tester-echo\",\n"
      "        \"name\": \"central-broker-master\",\n"
      "        \"configuration_file\": \"God\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      }\n"
      "    ],\n"
      "    \"log\": \"/tmp/watchdog.log\"\n"
      "  }\n"
      "}"};
  create_conf("/tmp/simple-conf.json", content1);
  start_cbwd("/tmp/simple-conf.json");

  std::set<pid_t> testers = wait_for_children(2u, 5);
  ASSERT_EQ(testers.size(), 2u);

  // We change the configuration and send a sighup signal to cbwd.
  create_conf("/tmp/simple-conf.json", content2);
  kill(_cbwd, SIGHUP);

  // Testers are here again but at least one with a different pid.
  std::set<pid_t> new_testers;
  int timeout = 20;
  do {
    sleep(1);
    new_testers = living_children(_cbwd);
  } while ((new_testers.size() != 2u || new_testers == testers) &&
           --timeout > 0);
  ASSERT_EQ(new_testers.size(), 2u);
  ASSERT_NE(new_testers, testers);

  // We send a term signal to cbwd.
  kill(_cbwd, SIGTERM);
  ASSERT_TRUE(wait_for_cbwd_exit(10));
}
