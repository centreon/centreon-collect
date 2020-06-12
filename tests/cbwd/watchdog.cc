/*
** Copyright 2019 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <gtest/gtest.h>

#include <fstream>

#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/misc/string.hh"

using namespace com::centreon::broker;

static std::list<int32_t> get_pid(std::string const& name) {
  std::string r = misc::exec("ps ax");
  std::list<std::string> spl = misc::string::split(r, '\n');
  std::list<int32_t> pids;
  for (auto const& l : spl) {
    if (l.find(name) == std::string::npos ||
        l.find("grep") != std::string::npos ||
        l.find("defunc") != std::string::npos)
      continue;
    else
      pids.push_back(atoi(l.c_str()));
  }
  return pids;
}

static void create_conf(std::string const& filename,
                        std::string const& content) {
  std::ofstream oss(filename);
  oss << content;
}

static void remove_file(std::string const& filename) {
  std::remove(filename.c_str());
}

TEST(WatchdogTest, Help) {
  std::string result = com::centreon::broker::misc::exec("bin/cbwd -h");
  ASSERT_EQ("USAGE: cbwd configuration_file\n", result);
}

TEST(WatchdogTest, NoConfig) {
  std::string result = com::centreon::broker::misc::exec("bin/cbwd");
  ASSERT_EQ("USAGE: cbwd configuration_file\n", result);
}

TEST(WatchdogTest, NotExistingConfig) {
  std::string result = com::centreon::broker::misc::exec("bin/cbwd foo");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file 'foo':"
      " Config parser: Cannot read file 'foo': No such file or directory\n",
      result);
}

TEST(WatchdogTest, BadConfig) {
  std::string result = com::centreon::broker::misc::exec(
      "bin/cbwd " CENTREON_BROKER_WD_TEST "bad-config.json");
  char const* str =
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'" CENTREON_BROKER_WD_TEST
      "bad-config.json': reload field not provided for cbd instance\n";
  ASSERT_EQ(std::string(str), result);
}

//test CbdArray//
TEST(WatchdogTest, CbdArray) {
  std::string const& content{
        "{\n"
        "  \"centreonBroker\": {\n"
        "   \"cbd\": \"var\"\n"
        "   }\n"
        "}\n"};
    create_conf("/tmp/cbd-array-conf.json", content);
    char const* arg[]{"bin/cbwd", "/tmp/cbd-array-conf.json", nullptr};
    char const* str = "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'cbd-array-config.json': Config parser: Cannot read file 'cbd-array-config.json': "
      "No such file or directory\n";
    std::string result = com::centreon::broker::misc::exec(
      "bin/cbwd cbd-array-config.json"
    );
    ASSERT_EQ(std::string(str), result);
}

TEST(WatchdogTest, SimpleConfig) {
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-broker-master\",\n"
      "        \"configuration_file\": \"Master\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
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
  char const* arg[]{"bin/cbwd", "/tmp/simple-conf.json", nullptr};
  com::centreon::broker::misc::exec_process(arg, false);

  std::list<int32_t> pids = get_pid("tester");
  int32_t count = 100;
  while (count > 0 && pids.size() < 2u) {
    usleep(100);
    pids = get_pid("tester");
    count--;
  }

  // There are 3 elements, but the last one is empty
  ASSERT_EQ(pids.size(), 2u);

  // We send a term signal to one child
  int32_t pid = pids.front();
  kill(pid, SIGTERM);

  // We wait for the next event
  sleep(5);
  pids = get_pid("tester");
  ASSERT_EQ(pids.size(), 2u);

  // We send a term signal to cbwd
  // the space after cbwd is important to avoid confusion with cbwd_ut
  pids = get_pid("bin/cbwd ");
  ASSERT_EQ(pids.size(), 1u);
  pid = pids.front();
  kill(pid, SIGTERM);
  sleep(2);

  // No tester anymore.
  pids = get_pid("tester");
  ASSERT_TRUE(pids.empty());

  // the space after cbwd is important to avoid confusion with cbwd_ut
  pids = get_pid("bin/cbwd ");
  ASSERT_TRUE(pids.empty());
}

TEST(WatchdogTest, SimpleConfigUpdated) {
  std::string const& content1{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester-echo\",\n"
      "        \"name\": \"central-broker-master\",\n"
      "        \"configuration_file\": \"Master\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester-echo\",\n"
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
      "tester-echo\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"configuration_file\": \"Slave\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester-echo\",\n"
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
  char const* arg[]{"bin/cbwd", "/tmp/simple-conf.json", nullptr};

  com::centreon::broker::misc::exec_process(arg, false);

  int32_t count = 100;
  std::list<int32_t> lst = get_pid("tester-echo");
  while (lst.size() < 2 && count > 0) {
    usleep(100);
    lst = get_pid("tester-echo");
    count--;
  }
  ASSERT_EQ(lst.size(), 2u);

  // We change the configuration
  create_conf("/tmp/simple-conf.json", content2);
  // We send a sighup signal to cbwd
  lst = get_pid("bin/cbwd ");
  ASSERT_FALSE(lst.empty());
  int pid = lst.front();
  kill(pid, SIGHUP);

  int timeout = 20;
  do {
    sleep(1);
    // Testers are here again but with different pid.
    lst = get_pid("tester-echo");
    // There are 3 elements, but the last one is empty
  } while (lst.size() < 2u && --timeout > 0);
  ASSERT_GT(timeout, 0);
  ASSERT_EQ(lst.size(), 2u);

  // We send a term signal to cbwd
  kill(pid, SIGTERM);
  timeout = 10;
  do {
    sleep(1);
    lst = get_pid("bin/cbwd ");
  } while (!lst.empty());
  ASSERT_GT(timeout, 0);
}
