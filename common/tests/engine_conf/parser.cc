/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/parser.hh"
#include <gtest/gtest.h>

using namespace com::centreon::engine::configuration;

class TestParser : public ::testing::Test {
 public:
  //  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TestParser, hashDirectory_empty) {
  system("mkdir -p /tmp/foo ; rm -rf /tmp/foo/*");
  system("mkdir -p /tmp/bar ; rm -rf /tmp/bar/*");
  std::string hash_foo = parser::hash_directory("/tmp/foo");
  std::string hash_bar = parser::hash_directory("/tmp/bar");
  ASSERT_EQ(hash_foo, hash_bar);
}

TEST_F(TestParser, hashDirectory_simple) {
  system(
      "mkdir -p /tmp/foo ; rm -rf /tmp/foo/* ; mkdir -p /tmp/foo/a ; mkdir -p "
      "/tmp/foo/b ; mkdir -p /tmp/foo/b/a ; touch /tmp/foo/b/a/foobar");
  system(
      "mkdir -p /tmp/bar ; rm -rf /tmp/bar/* ; mkdir -p /tmp/bar/b ; mkdir -p "
      "/tmp/bar/b/a ; touch /tmp/bar/b/a/foobar ; mkdir -p /tmp/bar/a");
  std::string hash_foo = parser::hash_directory("/tmp/foo");
  std::string hash_bar = parser::hash_directory("/tmp/bar");
  ASSERT_EQ(hash_foo, hash_bar);
}

TEST_F(TestParser, hashDirectory_multifiles) {
  system("mkdir -p /tmp/foo ; rm -rf /tmp/foo/*");
  system("mkdir -p /tmp/bar ; rm -rf /tmp/bar/*");
  for (int i = 0; i < 20; i++) {
    system(fmt::format("touch /tmp/foo/file_{}", i).c_str());
  }
  for (int i = 19; i >= 0; i--) {
    system(fmt::format("touch /tmp/bar/file_{}", i).c_str());
  }
  std::string hash_foo = parser::hash_directory("/tmp/foo");
  std::string hash_bar = parser::hash_directory("/tmp/bar");
  ASSERT_EQ(hash_foo,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  ASSERT_EQ(hash_foo, hash_bar);
}

TEST_F(TestParser, hashDirectory_realSituation) {
  system("rm -rf /tmp/tests_foo ; cp -rf tests /tmp/tests_foo");
  std::string hash = parser::hash_directory("tests");
  std::string hash1 = parser::hash_directory("/tmp/tests_foo");
  ASSERT_EQ(hash, hash1);

  // A new line added to a file.
  system("echo test >> /tmp/tests_foo/timeperiods.cfg");
  hash = parser::hash_directory("tests");
  hash1 = parser::hash_directory("/tmp/tests_foo");
  ASSERT_NE(hash, hash1);
}
