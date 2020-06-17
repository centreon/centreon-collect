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

// When the help command "-h" is inputted
TEST(WatchdogTest, Help) {
  std::string result = com::centreon::broker::misc::exec("bin/cbwd -h");
  ASSERT_EQ("USAGE: cbwd configuration_file\n", result);
}  // Then an explaination message is returned

// When there is no configuration file
TEST(WatchdogTest, NoConfig) {
  std::string result = com::centreon::broker::misc::exec("bin/cbwd");
  ASSERT_EQ("USAGE: cbwd configuration_file\n", result);
}  // Then an explaination message is returned

// When the configuration file does not exist
TEST(WatchdogTest, NotExistingConfig) {
  std::string result = com::centreon::broker::misc::exec("bin/cbwd foo");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file 'foo':"
      " Config parser: Cannot read file 'foo': No such file or directory\n",
      result);
}  // Then an error message is returned : The file/directory does not exist

// When the configuration file is badly set
TEST(WatchdogTest, BadConfig) {
  std::string result = com::centreon::broker::misc::exec(
      "bin/cbwd " CENTREON_BROKER_WD_TEST "bad-config.json");
  char const* str =
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'" CENTREON_BROKER_WD_TEST
      "bad-config.json': reload field not provided for cbd instance\n";
  ASSERT_EQ(std::string(str), result);
}  // Then an error message is returned

// When the json configuration file is empty
TEST(WatchdogTest, JsonObject) {
  std::string const& content{
      "{\n"
      "}"};
  create_conf("/tmp/json-object-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/json-object-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'/tmp/json-object-conf.json': Config parser: Cannot parse file "
      "'/tmp/json-object-conf.json': it must contain a centreonBroker object\n",
      result);
}  // Then an error message is returned : The file must contain an object

// When the json configuration file does not contain json
TEST(WatchdogTest, JsonNull) {
  std::string const& content{"$&!"};
  create_conf("/tmp/json-null-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/json-null-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'/tmp/json-null-conf.json': Config parser: Cannot parse file "
      "'/tmp/json-null-conf.json': expected value, got '$' (36)\n",
      result);
}  // Then an error message is returned : The file must contain json language

// When the content of the object in the json configuration file is not an array
TEST(WatchdogTest, CbdArray) {
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": \"var\"\n"
      "   }\n"
      "}\n"};
  create_conf("/tmp/cbd-array-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/cbd-array-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'/tmp/cbd-array-conf.json': error in watchdog config syntax 'cbd'"
      " must be an array\n",
      result);
}  // Then an error message is returned : 'cbd' must be an array

// When the object name is not recognized in the json configuration file
TEST(WatchdogTest, Object) {
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"object\": [\n"
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
      "    \"object\": \"/tmp/watchdog.log\"\n"
      "  }\n"
      "}"};
  create_conf("/tmp/object-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/object-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'/tmp/object-conf.json': error in watchdog config 'object' key is "
      "not recognized\n",
      result);
}  // Then an error message is returned : The object is not recognized

// When the instance name is missing in the json configuration file
TEST(WatchdogTest, Empty_Name) {
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"\",\n"
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
  create_conf("/tmp/empty-name-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/empty-name-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'/tmp/empty-name-conf.json': watchdog: missing instance name\n",
      result);
}  // Then an error message is returned : The file must contain an instance name

TEST(WatchdogTest,
     Name) {  // When the name field is not provided in the json document
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
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
  create_conf("/tmp/name-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/name-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file"
      " '/tmp/name-conf.json': name field not provided for cbd instance\n",
      result);
}  // Then an error message is returned : The name field must be provided for
   // cbd instance

TEST(WatchdogTest, Instance_config) {
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-broker-master\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      }\n"
      "    ],\n"
      "    \"log\": \"/tmp/watchdog.log\"\n"
      "  }\n"
      "}"};
  create_conf("/tmp/instance-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/instance-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file"
      " '/tmp/instance-conf.json': instance_config field not provided for cbd "
      "instance\n",
      result);
}  // Then an error message is returned : The instance_config field must be
   // provided for cbd instance

TEST(WatchdogTest, Run) {
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-broker-master\",\n"
      "        \"configuration_file\": \"Master\",\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"configuration_file\": \"Slave\",\n"
      "        \"reload\": true\n"
      "      }\n"
      "    ],\n"
      "    \"log\": \"/tmp/watchdog.log\"\n"
      "  }\n"
      "}"};
  create_conf("/tmp/run-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/run-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'/tmp/run-conf.json': run field not provided for cbd instance\n",
      result);
}  // Then an error message is returned : The run field must be provided for cbd
   // instance

TEST(WatchdogTest, Reload) {
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-broker-master\",\n"
      "        \"configuration_file\": \"Master\",\n"
      "        \"run\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"configuration_file\": \"Slave\",\n"
      "        \"run\": true\n"
      "      }\n"
      "    ],\n"
      "    \"log\": \"/tmp/watchdog.log\"\n"
      "  }\n"
      "}"};
  create_conf("/tmp/reload-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/reload-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'/tmp/reload-conf.json': reload field not provided for cbd instance\n",
      result);
}  // Then an error message is returned : The reload field must be provided for
   // cbd instance

TEST(WatchdogTest, exist) {
  std::string const& content{
      "{\n"
      "  \"centreonBroker\": {\n"
      "   \"cbd\": [\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"configuration_file\": \"Master\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      },\n"
      "      {\n"
      "        \"executable\": \"" CENTREON_BROKER_WD_TEST
      "tester\",\n"
      "        \"name\": \"central-rrd-master\",\n"
      "        \"configuration_file\": \"Master\",\n"
      "        \"run\": true,\n"
      "        \"reload\": true\n"
      "      }\n"
      "    ],\n"
      "    \"log\": \"/tmp/watchdog.log\"\n"
      "  }\n"
      "}"};
  create_conf("/tmp/exist-conf.json", content);
  std::string result =
      com::centreon::broker::misc::exec("bin/cbwd /tmp/exist-conf.json");
  ASSERT_EQ(
      "[cbwd] [error] watchdog: Could not parse the configuration file "
      "'/tmp/exist-conf.json': instance 'central-rrd-master' already exists\n",
      result);
}  // Then an error message is returned : The instance 'central-rrd-master'
   // already exist

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
