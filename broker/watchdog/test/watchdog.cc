/*
** Copyright 2019-2022 Centreon
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

#include <absl/strings/str_split.h>
#include <gtest/gtest.h>
#include "broker/core/misc/misc.hh"

using namespace com::centreon::broker;

void create_conf(std::string const& filename, std::string const& content) {
  std::ofstream oss(filename);
  oss << content;
}

void remove_file(std::string const& filename) {
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
  ASSERT_TRUE(
      result.find(
          "Could not parse the configuration file 'foo': Config parser: Cannot "
          "read file 'foo': No such file or directory\n") != std::string::npos);
}

TEST(WatchdogTest, BadConfig) {
  std::string result = com::centreon::broker::misc::exec(
      "bin/cbwd " CENTREON_BROKER_WD_TEST "/bad-config.json");
  ASSERT_TRUE(
      result.find(
          "Could not parse the configuration file '" CENTREON_BROKER_WD_TEST
          "/bad-config.json': reload field not provided for cbd instance\n") !=
      std::string::npos);
}

TEST(WatchdogTest, SimpleConfig) {
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
  char const* arg[]{"bin/cbwd", "/tmp/simple-conf.json", nullptr};
  com::centreon::broker::misc::exec_process(arg, false);
  std::string r;
  int32_t time = 0;
  std::list<std::string_view> lst1;
  while (time < 5) {
    r = misc::exec("ps ax | grep tester | grep -v grep | awk '{print $1}'");
    lst1 = absl::StrSplit(r, '\n');
    if (lst1.size() == 3u)
      break;
    time++;
    sleep(1);
  }
  // There are 3 elements, but the last one is empty
  ASSERT_EQ(lst1.size(), 3u);
  ASSERT_TRUE(lst1.back().empty());

  std::list<std::string_view> lst = absl::StrSplit(r, '\n');
  // There are 3 elements, but the last one is empty
  ASSERT_EQ(lst.size(), 3u);
  ASSERT_TRUE(lst.back().empty());

  // We send a term signal to one child
  int pid;
  if (!absl::SimpleAtoi(lst.front(), &pid)) {
    ASSERT_FALSE("First element in lst should be a pid.");
  }
  kill(pid, SIGTERM);

  // We wait for the next event
  time = 0;
  for (;;) {
    r = misc::exec("ps ax | grep tester | grep -v grep | awk '{print $1}'");
    lst = absl::StrSplit(r, '\n');
    // There are still 3 elements, the lost child is resurrected
    if (lst.size() == 3u || time > 10)
      break;
    sleep(1);
    time++;
  }
  ASSERT_EQ(lst.size(), 3u);
  ASSERT_TRUE(lst.back().empty());

  // We send a term signal to cbwd
  r = misc::exec("ps ax | grep bin/cbwd | grep -v grep | awk '{print $1}'");
  pid = std::stol(r);
  kill(pid, SIGTERM);

  time = 0;
  for (;;) {
    // No tester anymore.
    r = misc::exec("ps ax | grep tester | grep -v grep | awk '{print $1}'");
    if (r == "" || time > 10)
      break;
    sleep(1);
    time++;
  }
  ASSERT_EQ(r, "");

  r = misc::exec("ps ax | grep bin/cbwd | grep -v grep | awk '{print $1}'");
  // No cbwd anymore.
  ASSERT_EQ(r, "");
}

TEST(WatchdogTest, SimpleConfigUpdated) {
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
  char const* arg[]{"bin/cbwd", "/tmp/simple-conf.json", nullptr};

  com::centreon::broker::misc::exec_process(arg, false);
  std::list<std::string_view> lst;
  int32_t time = 0;
  std::string r;
  while (lst.size() != 3 && time < 5) {
    r = misc::exec(
        "ps ax | grep tester-echo | grep -v grep | grep -v defunc | awk "
        "'{print "
        "$1}'");
    lst = absl::StrSplit(r, '\n');
    time++;
    sleep(1);
  }
  // There are 3 elements, but the last one is empty

  ASSERT_EQ(lst.size(), 3u);
  ASSERT_TRUE(lst.back().empty());

  // We change the configuration
  create_conf("/tmp/simple-conf.json", content2);
  // We send a sighup signal to cbwd
  r = misc::exec("ps ax | grep bin/cbwd | grep -v grep | awk '{print $1}'");
  int pid = std::stol(r);
  kill(pid, SIGHUP);

  int timeout = 20;
  do {
    sleep(1);
    // Testers are here again but with different pid.
    r = misc::exec("ps ax | grep tester-echo | grep -v grep | grep -v defunc");
    lst = absl::StrSplit(r, '\n');
    // There are 3 elements, but the last one is empty
  } while ((lst.size() < 3u || !lst.back().empty()) && --timeout > 0);
  ASSERT_GT(timeout, 0);
  ASSERT_TRUE(lst.back().empty());

  // We send a term signal to cbwd
  kill(pid, SIGTERM);
  timeout = 10;
  do {
    sleep(1);
    r = misc::exec(
        "ps ax | grep bin/cbwd | grep -v grep | grep -v defunc | awk "
        "'{print $1}'");
  } while (!r.empty());
  ASSERT_GT(timeout, 0);
}
