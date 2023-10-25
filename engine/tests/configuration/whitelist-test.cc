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
#include <experimental/filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "com/centreon/engine/configuration/whitelist.hh"
#include "com/centreon/engine/log_v2.hh"

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
    _old_log_level = log_v2::config()->level();
    log_v2::config()->set_level(spdlog::level::trace);
  }

  static void TearDownTestSuite() {
    log_v2::config()->set_level(_old_log_level);
  }
};

spdlog::level::level_enum whitelist_test::_old_log_level;

TEST_F(whitelist_test, no_file) {
  ::remove("/tmp/toto");
  whitelist_file file("/tmp/toto");

  ASSERT_THROW(file.parse(), boost::exception);
}

TEST_F(whitelist_test, no_regular_file) {
  whitelist_file file("/tmp");
  ASSERT_THROW(file.parse(), boost::exception);

  whitelist_file file2("/dev/null");
  ASSERT_THROW(file2.parse(), boost::exception);
}

TEST_F(whitelist_test, bad_file) {
  create_file("/tmp/toto", R"(whitelist:
  wildcard:
  -)");

  whitelist_file file("/tmp/toto");
  ASSERT_THROW(file.parse(), boost::exception);
}

TEST_F(whitelist_test, wildcards) {
  create_file("/tmp/toto", R"(whitelist:
  wildcard:
    - /usr/lib/centreon/plugins/centreon_*
    -  /usr/lib/centreon/plugins/check_centreon_bam
    -  /tmp/var/lib/centreon-engine/toto* * *
    - /usr/lib/centreon/plugins/centreon_linux_snmp.pl*
)");

  whitelist_file file("/tmp/toto");
  ASSERT_NO_THROW(file.parse());

  std::vector<std::string> expected{
      "/usr/lib/centreon/plugins/centreon_*",
      "/usr/lib/centreon/plugins/check_centreon_bam",
      "/tmp/var/lib/centreon-engine/toto* * *",
      "/usr/lib/centreon/plugins/centreon_linux_snmp.pl*"};
  ASSERT_EQ(file.get_wildcards(), expected);
  ASSERT_TRUE(file.test("/tmp/var/lib/centreon-engine/totozea 1 1.0.0.0"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_rrgersgesrg0"));
  ASSERT_TRUE(file.test(
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

  whitelist_file file("/tmp/toto");
  ASSERT_NO_THROW(file.parse());

  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/check_centreon_bam"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_12345bam"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(file.test("a/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(file.test("/usr/lib/centreon/plugins/centreon_bam"));
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

  whitelist_file file("/tmp/toto");
  ASSERT_NO_THROW(file.parse());
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/check_centreon_bam"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_12345bam"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(file.test("a/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(file.test("/usr/lib/centreon/plugins/centreon_bam"));
  ASSERT_TRUE(file.test(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti"));
  ASSERT_FALSE(file.test(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti15449"));
  ASSERT_TRUE(file.test(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftata"));
  ASSERT_TRUE(
      file.test("/usr/lib/centreon/plugins/"
                "centreon_totozuiefizenfuieznfizeftata561798189"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_tototata"));
  ASSERT_FALSE(file.test("/usr/lib/centreon/plugins/centreon_tototato"));
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

  whitelist_file file("/tmp/toto");
  ASSERT_NO_THROW(file.parse());
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/check_centreon_bam"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_12345bam"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(file.test("a/usr/lib/centreon/plugins/centreon_12345"));
  ASSERT_FALSE(file.test("/usr/lib/centreon/plugins/centreon_bam"));
  ASSERT_TRUE(file.test(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti"));
  ASSERT_FALSE(file.test(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftiti15449"));
  ASSERT_TRUE(file.test(
      "/usr/lib/centreon/plugins/centreon_totozuiefizenfuieznfizeftata"));
  ASSERT_TRUE(
      file.test("/usr/lib/centreon/plugins/"
                "centreon_totozuiefizenfuieznfizeftata561798189"));
  ASSERT_TRUE(file.test("/usr/lib/centreon/plugins/centreon_tototata"));
  ASSERT_FALSE(file.test("/usr/lib/centreon/plugins/centreon_tototato"));
}

TEST_F(whitelist_test, empty_allow_all) {
  std::error_code err;
  std::experimental::filesystem::remove_all("/tmp/whitelist", err);
  whitelist_directory white_list("/tmp/whitelist");
  mkdir("/tmp/whitelist", S_IRWXU | S_IRGRP | S_IXGRP);

  white_list.refresh();

  ASSERT_TRUE(white_list.test("turlututu"));
}

TEST_F(whitelist_test, no_directory_allow_all) {
  std::error_code err;
  std::experimental::filesystem::remove_all("/tmp/whitelist", err);
  whitelist_directory white_list("/tmp/whitelist");

  white_list.refresh();

  ASSERT_TRUE(white_list.test("turlututu"));
}

TEST_F(whitelist_test, directory) {
  mkdir("/tmp/whitelist", S_IRWXU | S_IRGRP | S_IXGRP);
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

  whitelist_directory white_list("/tmp/whitelist");

  white_list.refresh();

  auto files_to_pointer_list = [&]() {
    std::vector<const whitelist_file*> ret;
    for (const std::unique_ptr<whitelist_file>& f : white_list.get_files()) {
      ret.push_back(f.get());
    }
    return ret;
  };

  ASSERT_TRUE(std::is_sorted(white_list.get_files().begin(),
                             white_list.get_files().end(),
                             [](const std::unique_ptr<whitelist_file>& left,
                                const std::unique_ptr<whitelist_file>& right) {
                               return left->get_path() < right->get_path();
                             }));

  std::vector<const whitelist_file*> cont1 = files_to_pointer_list();

  ASSERT_EQ(cont1.size(), 3);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  // modify time of the second file should go to a reload
  struct utimbuf new_times;
  new_times.actime = time(nullptr);
  new_times.modtime = time(nullptr);
  utime("/tmp/whitelist/b", &new_times);
  white_list.refresh();

  ASSERT_TRUE(white_list.test("/usr/lib/nagios/plugins/check_centreon_bam"));

  ASSERT_TRUE(std::is_sorted(white_list.get_files().begin(),
                             white_list.get_files().end(),
                             [](const std::unique_ptr<whitelist_file>& left,
                                const std::unique_ptr<whitelist_file>& right) {
                               return left->get_path() < right->get_path();
                             }));

  std::vector<const whitelist_file*> cont2 = files_to_pointer_list();
  ASSERT_EQ(cont2.size(), 3);
  ASSERT_EQ(cont1[0], cont2[0]);
  ASSERT_NE(cont1[1], cont2[1]);
  ASSERT_EQ(cont1[2], cont2[2]);

  create_file("/tmp/whitelist/c", R"(whitelist:
  wildcard:
    - /usr/lib/tata/plugins/centreon_toto*titi
    - /usr/lib/tata/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/tata/plugins/centreon_\d{5}.*
    -  /usr/lib/tata/plugins/check_centreon_bam 
)");
  ::remove("/tmp/whitelist/b");
  white_list.refresh();

  ASSERT_FALSE(white_list.test("/usr/lib/nagios/plugins/check_centreon_bam"));
  ASSERT_TRUE(white_list.test("/usr/lib/tata/plugins/check_centreon_bam"));
  ASSERT_TRUE(white_list.test("/usr/lib/tata/plugins/centreon_12345"));
  create_file("/tmp/whitelist/9", R"(whitelist:
  wildcard:
    - /usr/lib/titi/plugins/centreon_toto*titi
    - /usr/lib/titi/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/titi/plugins/centreon_\d{5}.*
    -  /usr/lib/titi/plugins/check_centreon_bam 
)");
  ::remove("/tmp/whitelist/a");
  white_list.refresh();
  ASSERT_EQ(white_list.get_files().size(), 3);
  ASSERT_FALSE(white_list.test("/usr/lib/tutu/plugins/check_centreon_bam"));
  ASSERT_TRUE(white_list.test("/usr/lib/titi/plugins/check_centreon_bam"));
  ASSERT_TRUE(white_list.test("/usr/lib/tata/plugins/check_centreon_bam"));
}
