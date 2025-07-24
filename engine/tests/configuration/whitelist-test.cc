/*
 * Copyright 2023 Centreon (https://www.centreon.com/)
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
#include <utime.h>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "com/centreon/engine/globals.hh"
#include "common/log_v2/config.hh"

#include "com/centreon/engine/configuration/whitelist.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

static void create_file(const char* path, const char* content) {
  ::remove(path);
  std::ofstream file(path);
  file << content;
  file.close();
}

class whitelist_test : public ::testing::Test {
  static spdlog::level::level_enum _old_log_level;

 public:
  static void SetUpTestSuite() {
    _old_log_level = config_logger->level();
    config_logger->set_level(spdlog::level::trace);
  }

  static void TearDownTestSuite() { config_logger->set_level(_old_log_level); }
};

spdlog::level::level_enum whitelist_test::_old_log_level;

TEST_F(whitelist_test, no_file) {
  ::remove("/tmp/toto");

  whitelist file("/tmp/toto");

  ASSERT_TRUE(file.is_engine_whitelist_empty());
}

TEST_F(whitelist_test, no_regular_file) {
  whitelist file("/tmp");
  ASSERT_TRUE(file.is_engine_whitelist_empty());

  whitelist file2("/dev/null");
  ASSERT_TRUE(file2.is_engine_whitelist_empty());
}

TEST_F(whitelist_test, bad_file) {
  create_file("/tmp/toto", R"(whitelist:
  wildcard:
  -)");

  whitelist file("/tmp/toto");
  ASSERT_TRUE(file.is_engine_whitelist_empty());
}

TEST_F(whitelist_test, wildcards) {
  create_file("/tmp/toto", R"(whitelist:
  wildcard:
    - /usr/lib/centreon/plugins/centreon_*
    -  /usr/lib/centreon/plugins/check_centreon_bam
    -  /tmp/var/lib/centreon-engine/toto* * *
    - /usr/lib/centreon/plugins/centreon_linux_snmp.pl*
)");

  whitelist file("/tmp/toto");
  ASSERT_FALSE(file.is_engine_whitelist_empty());

  std::vector<std::string> expected{
      "/usr/lib/centreon/plugins/centreon_*",
      "/usr/lib/centreon/plugins/check_centreon_bam",
      "/tmp/var/lib/centreon-engine/toto* * *",
      "/usr/lib/centreon/plugins/centreon_linux_snmp.pl*"};
  ASSERT_EQ(file.get_wildcards_engine(), expected);
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/tmp/var/lib/centreon-engine/totozea 1 1.0.0.0"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/centreon_rrgersgesrg0"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins//centreon_linux_snmp.pl "
      "--plugin=os::linux::snmp::plugin --mode=load --hostname=localhost "
      "--snmp-version='2c' --snmp-community='public'  --warning='4,3,2' "
      "--critical='6,5,4'"));
}

TEST_F(whitelist_test, regexp) {
  create_file("/tmp/toto", R"(whitelist:
  regex:
    - /usr/lib/centreon/plugins/centreon_\d{5}.*
    -  /usr/lib/centreon/plugins/check_centreon_bam 
)");

  whitelist file("/tmp/toto");
  ASSERT_FALSE(file.is_engine_whitelist_empty());

  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/check_centreon_bam"));
  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_12345bam"));
  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(
      file.is_allowed_by_engine("a/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_bam"));
}

TEST_F(whitelist_test, regexp_and_wildcards) {
  create_file("/tmp/toto", R"(whitelist:
  wildcard:
    - /usr/lib/centreon/plugins/centreon_toto*titi
    - /usr/lib/centreon/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/centreon/plugins/centreon_\d{5}.*
    -  /usr/lib/centreon/plugins/check_centreon_bam 
)");

  whitelist file("/tmp/toto");
  ASSERT_FALSE(file.is_engine_whitelist_empty());
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/check_centreon_bam"));
  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_12345bam"));
  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(
      file.is_allowed_by_engine("a/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_bam"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti"));
  ASSERT_FALSE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti15449"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftata"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/"
      "centreon_totozuiefizenfuieznfizeftata561798189"));
  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_tototata"));
  ASSERT_FALSE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_tototato"));
}

TEST_F(whitelist_test, regexp_and_wildcards_json) {
  create_file("/tmp/toto", R"({"whitelist": {
  "wildcard": [
    "/usr/lib/centreon/plugins/centreon_toto*titi",
    "/usr/lib/centreon/plugins/centreon_toto*tata*"
  ],
  "regex": [
    "/usr/lib/centreon/plugins/centreon_\\d{5}.*",
    "/usr/lib/centreon/plugins/check_centreon_bam"
  ]
}})");

  whitelist file("/tmp/toto");
  ASSERT_FALSE(file.is_engine_whitelist_empty());
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/check_centreon_bam"));
  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_12345bam"));
  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(
      file.is_allowed_by_engine("a/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_bam"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti"));
  ASSERT_FALSE(file.is_allowed_by_engine(

      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti15449"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftata"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib/centreon/plugins/"
      "centreon_totozuiefizenfuieznfizeftata561798189"));
  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_tototata"));
  ASSERT_FALSE(
      file.is_allowed_by_engine("/usr/lib/centreon/plugins/centreon_tototato"));
}

static const char* tmp_whitelist = "/tmp/whitelist";

TEST_F(whitelist_test, empty_engine_allow_all) {
  std::error_code err;
  std::filesystem::remove_all(tmp_whitelist, err);
  whitelist white_list(&tmp_whitelist, &tmp_whitelist + 1);
  mkdir("/tmp/whitelist", S_IRWXU | S_IRGRP | S_IXGRP);

  ASSERT_TRUE(white_list.is_allowed_by_engine("turlututu"));
}

TEST_F(whitelist_test, no_directory_allow_all) {
  std::error_code err;
  std::filesystem::remove_all(tmp_whitelist, err);
  whitelist white_list(&tmp_whitelist, &tmp_whitelist + 1);

  ASSERT_TRUE(white_list.is_allowed_by_engine("turlututu"));
}

TEST_F(whitelist_test, directory) {
  mkdir(tmp_whitelist, S_IRWXU | S_IRGRP | S_IXGRP);
  create_file("/tmp/whitelist/d", R"(whitelist:
  wildcard:
    - /usr/lib/centreon/plugins/centreon_toto*titi
    - /usr/lib/centreon/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/centreon/plugins/centreon_\d{5}.*
    -  /usr/lib/centreon/plugins/check_centreon_bam 
)");

  create_file("/tmp/whitelist/b", R"(whitelist:
  wildcard:
    - /usr/lib/nagios/plugins/centreon_toto*titi
    - /usr/lib/nagios/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/nagios/plugins/centreon_\d{5}.*
    -  /usr/lib/nagios/plugins/check_centreon_bam 
)");

  create_file("/tmp/whitelist/a", R"(whitelist:
  wildcard:
    - /usr/lib/tutu/plugins/centreon_toto*titi
    - /usr/lib/tutu/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/tutu/plugins/centreon_\d{5}.*
    -  /usr/lib/tutu/plugins/check_centreon_bam 
)");

  whitelist white_list(&tmp_whitelist, &tmp_whitelist + 1);

  ASSERT_TRUE(white_list.is_allowed_by_engine(
      "/usr/lib/nagios/plugins/check_centreon_bam"));

  create_file("/tmp/whitelist/c", R"(whitelist:
  wildcard:
    - /usr/lib/tata/plugins/centreon_toto*titi
    - /usr/lib/tata/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/tata/plugins/centreon_\d{5}.*
    -  /usr/lib/tata/plugins/check_centreon_bam 
)");
  ::remove("/tmp/whitelist/b");
  white_list = whitelist(&tmp_whitelist, &tmp_whitelist + 1);

  ASSERT_FALSE(white_list.is_allowed_by_engine(
      "/usr/lib/nagios/plugins/check_centreon_bam"));
  ASSERT_TRUE(white_list.is_allowed_by_engine(
      "/usr/lib/tata/plugins/check_centreon_bam"));
  ASSERT_TRUE(
      white_list.is_allowed_by_engine("/usr/lib/tata/plugins/centreon_12345"));
  create_file("/tmp/whitelist/9", R"(whitelist:
  wildcard:
    - /usr/lib/titi/plugins/centreon_toto*titi
    - /usr/lib/titi/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/titi/plugins/centreon_\d{5}.*
    -  /usr/lib/titi/plugins/check_centreon_bam 
)");
  ::remove("/tmp/whitelist/a");
  white_list = whitelist(&tmp_whitelist, &tmp_whitelist + 1);
  ASSERT_FALSE(white_list.is_allowed_by_engine(
      "/usr/lib/tutu/plugins/check_centreon_bam"));
  ASSERT_TRUE(white_list.is_allowed_by_engine(
      "/usr/lib/titi/plugins/check_centreon_bam"));
  ASSERT_TRUE(white_list.is_allowed_by_engine(
      "/usr/lib/tata/plugins/check_centreon_bam"));
}

/* * Test with wildcards and regexp in cma-whitelist
 * - default ruleset
 * - host specific ruleset
 */
TEST_F(whitelist_test, wildcards1) {
  create_file("/tmp/toto", R"(
whitelist:
  regex:
    - /usr/lib(64)?/nagios/plugins/.*
    - /usr/lib(64)?/nagios/plugins/.check_.*

cma-whitelist:
  default:
    regex:
      - /usr/lib(64)?/nagios/plugins/.*
      - /usr/lib(64)?/nagios/plugins/.check_.*
      - /usr/lib/titi/plugins/centreon_toto

  hosts:
    - hostname: Host_1
      regex:
        - /usr/lib(64)?/nagios1/plugins/.*
        - /usr/lib(64)?/nagios/plugins/.check_.*

    - hostname: Host_2
      wildcard:
        - /usr/lib/titi/plugins/centreon_toto*titi
        - /usr/lib/titi/plugins/centreon_toto*tata*
)");

  whitelist file("/tmp/toto");

  ASSERT_FALSE(file.is_engine_whitelist_empty());
  ASSERT_FALSE(file.is_cma_whitelist_empty());

  ASSERT_TRUE(
      file.is_allowed_by_engine("/usr/lib/nagios/plugins/check_centreon_bam"));
  ASSERT_TRUE(file.is_allowed_by_engine(
      "/usr/lib64/nagios/plugins/check_centreon_bam"));

  ASSERT_FALSE(file.is_allowed_by_cma(
      "/usr/lib/nagios0/plugins/check_centreon_bam", "Host_1"));
  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib/nagios1/plugins/check_centreon_bam", "Host_1"));

  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib/titi/plugins/centreon_totozuiefizenfuieznfizeftiti", "Host_2"));
  ASSERT_FALSE(file.is_allowed_by_cma(
      "/usr/lib/titi0/plugins/centreon_totozuiefizenfuieznfizeftiti",
      "Host_2"));
  ASSERT_TRUE(
      file.is_allowed_by_cma("/usr/lib/titi/plugins/centreon_toto", "Host_3"));
  ASSERT_FALSE(file.is_allowed_by_cma(
      "/usr/lib64/nagios0/plugins/check_centreon", "Host_3"));
}

/* * Test with wildcards and regexp in cma-whitelist
 * - default ruleset
 * - host specific ruleset
 */
TEST_F(whitelist_test, cma_regexp_and_wildcards_json) {
  create_file("/tmp/toto", R"({"cma-whitelist": {"default": {
  "wildcard": [
    "/usr/lib/centreon/plugins/centreon_toto*titi",
    "/usr/lib/centreon/plugins/centreon_toto*tata*"
  ],
  "regex": [
    "/usr/lib/centreon/plugins/centreon_\\d{5}.*",
    "/usr/lib/centreon/plugins/check_centreon_bam"
  ]
  },"hosts": [
  {
    "hostname": "host_1",
    "wildcard": [
      "/usr/lib1/centreon/plugins/centreon_toto*titi"
    ]
  },
  {
    "hostname": "host_2",
    "wildcard": [
      "/usr/lib2/centreon/plugins/centreon_toto*titi"
    ]
  }]}})");

  whitelist file("/tmp/toto");
  ASSERT_FALSE(file.is_cma_whitelist_empty());
  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib/centreon/plugins/check_centreon_bam", ""));
  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib/centreon/plugins/centreon_12345bam", ""));
  ASSERT_TRUE(
      file.is_allowed_by_cma("/usr/lib/centreon/plugins/centreon_12345", ""));
  ASSERT_FALSE(
      file.is_allowed_by_cma("a/usr/lib/centreon/plugins/centreon_12345", ""));
  ASSERT_FALSE(
      file.is_allowed_by_cma("/usr/lib/centreon/plugins/centreon_bam", ""));
  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti", ""));
  ASSERT_FALSE(file.is_allowed_by_cma(

      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti15449",
      ""));
  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftata", ""));
  ASSERT_TRUE(
      file.is_allowed_by_cma("/usr/lib/centreon/plugins/"
                             "centreon_totozuiefizenfuieznfizeftata561798189",
                             ""));
  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib/centreon/plugins/centreon_tototata", ""));
  ASSERT_FALSE(file.is_allowed_by_cma(
      "/usr/lib/centreon/plugins/centreon_tototato", ""));

  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib1/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti",
      "host_1"));
  ASSERT_FALSE(file.is_allowed_by_cma(
      "/usr/lib1/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti15449",
      "host_1"));
  ASSERT_TRUE(file.is_allowed_by_cma(
      "/usr/lib2/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti",
      "host_2"));
  ASSERT_FALSE(file.is_allowed_by_cma(
      "/usr/lib1/centreon/plugins/centreon_tototiti", "host_2"));
}