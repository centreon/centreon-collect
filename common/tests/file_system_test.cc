/**
 * Copyright 2024 Centreon
 * Licensed under the Apache License, Version 2.0(the "License");
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

#include <gtest/gtest.h>

#include "file_system.hh"

using namespace com::centreon::common;

TEST(TestParser, hashDirectory_empty) {
  system("mkdir -p /tmp/foo ; rm -rf /tmp/foo/*");
  system("mkdir -p /tmp/bar ; rm -rf /tmp/bar/*");
  std::error_code ec1, ec2;
  std::string hash_foo = hash_directory("/tmp/foo", ec1);
  std::string hash_bar = hash_directory("/tmp/bar", ec2);
  ASSERT_FALSE(ec1);
  ASSERT_FALSE(ec2);
  ASSERT_EQ(hash_foo, hash_bar);
}

TEST(TestParser, hashDirectory_simple) {
  system(
      "mkdir -p /tmp/foo ; rm -rf /tmp/foo/* ; mkdir -p /tmp/foo/a ; mkdir -p "
      "/tmp/foo/b ; mkdir -p /tmp/foo/b/a ; touch /tmp/foo/b/a/foobar");
  system(
      "mkdir -p /tmp/bar ; rm -rf /tmp/bar/* ; mkdir -p /tmp/bar/b ; mkdir -p "
      "/tmp/bar/b/a ; touch /tmp/bar/b/a/foobar ; mkdir -p /tmp/bar/a");
  std::error_code ec1, ec2;
  std::string hash_foo = hash_directory("/tmp/foo", ec1);
  std::string hash_bar = hash_directory("/tmp/bar", ec2);
  ASSERT_FALSE(ec1);
  ASSERT_FALSE(ec2);
  ASSERT_EQ(hash_foo, hash_bar);
}

TEST(TestParser, hashDirectory_multifiles) {
  system("mkdir -p /tmp/foo ; rm -rf /tmp/foo/*");
  system("mkdir -p /tmp/bar ; rm -rf /tmp/bar/*");
  for (int i = 0; i < 20; i++) {
    system(fmt::format("touch /tmp/foo/file_{}", i).c_str());
  }
  for (int i = 19; i >= 0; i--) {
    system(fmt::format("touch /tmp/bar/file_{}", i).c_str());
  }
  std::error_code ec1, ec2;
  std::string hash_foo = hash_directory("/tmp/foo", ec1);
  std::string hash_bar = hash_directory("/tmp/bar", ec2);
  ASSERT_FALSE(ec1);
  ASSERT_FALSE(ec2);
  ASSERT_EQ(hash_foo,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  ASSERT_EQ(hash_foo, hash_bar);
}

TEST(TestParser, hashDirectory_realSituation) {
  system("rm -rf /tmp/tests_foo ; cp -rf tests /tmp/tests_foo");
  std::error_code ec1, ec2;
  std::string hash = hash_directory("tests", ec1);
  std::string hash1 = hash_directory("/tmp/tests_foo", ec2);
  ASSERT_FALSE(ec1);
  ASSERT_FALSE(ec2);
  ASSERT_EQ(hash, hash1);

  // A new line added to a file.
  system("echo test >> /tmp/tests_foo/timeperiods.cfg");
  hash = hash_directory("tests", ec1);
  hash1 = hash_directory("/tmp/tests_foo", ec2);
  ASSERT_FALSE(ec1);
  ASSERT_FALSE(ec2);
  ASSERT_NE(hash, hash1);
}

TEST(TestParser, hashDirectory_error) {
  std::error_code ec;
  std::string hash = hash_directory("/tmp/doesnotexist", ec);
  ASSERT_TRUE(ec);
  ASSERT_EQ(hash, "");
}

TEST(TestParser, with_file_error) {
  std::error_code ec;
  system("echo test > /tmp/my_file");
  std::string hash = hash_directory("/tmp/my_file", ec);
  ASSERT_TRUE(ec);
  ASSERT_EQ(hash, "");
}

TEST(DirContent, empty_dir) {
  system("rm -rf /tmp/test_dir_content && mkdir -p /tmp/test_dir_content");
  auto result = dir_content("/tmp/test_dir_content", false);
  ASSERT_TRUE(result.empty());
}

TEST(DirContent, empty_dir_recursive) {
  system("rm -rf /tmp/test_dir_content && mkdir -p /tmp/test_dir_content");
  auto result = dir_content("/tmp/test_dir_content", true);
  ASSERT_TRUE(result.empty());
}

TEST(DirContent, nonRecursive_returns_top_level_files_only) {
  system(
      "rm -rf /tmp/test_dir_content && mkdir -p /tmp/test_dir_content && "
      "touch /tmp/test_dir_content/a.txt && "
      "touch /tmp/test_dir_content/b.txt && "
      "mkdir -p /tmp/test_dir_content/sub && "
      "touch /tmp/test_dir_content/sub/nested.txt");
  auto result = dir_content("/tmp/test_dir_content", false);
  result.sort();
  ASSERT_EQ(result.size(), 2u);
  auto it = result.begin();
  ASSERT_EQ(*it, "/tmp/test_dir_content/a.txt");
  ++it;
  ASSERT_EQ(*it, "/tmp/test_dir_content/b.txt");
}

TEST(DirContent, recursive_includes_subdir_files) {
  system(
      "rm -rf /tmp/test_dir_content && mkdir -p /tmp/test_dir_content && "
      "touch /tmp/test_dir_content/top.txt && "
      "mkdir -p /tmp/test_dir_content/sub && "
      "touch /tmp/test_dir_content/sub/nested.txt");
  auto result = dir_content("/tmp/test_dir_content", true);
  result.sort();
  ASSERT_EQ(result.size(), 2u);
  auto it = result.begin();
  ASSERT_EQ(*it, "/tmp/test_dir_content/sub/nested.txt");
  ++it;
  ASSERT_EQ(*it, "/tmp/test_dir_content/top.txt");
}

TEST(DirContent, recursive_deep_nesting) {
  system(
      "rm -rf /tmp/test_dir_content && mkdir -p /tmp/test_dir_content && "
      "touch /tmp/test_dir_content/root.txt && "
      "mkdir -p /tmp/test_dir_content/a/b/c && "
      "touch /tmp/test_dir_content/a/b/c/deep.txt && "
      "touch /tmp/test_dir_content/a/mid.txt");
  auto result = dir_content("/tmp/test_dir_content", true);
  result.sort();
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  ASSERT_EQ(*it, "/tmp/test_dir_content/a/b/c/deep.txt");
  ++it;
  ASSERT_EQ(*it, "/tmp/test_dir_content/a/mid.txt");
  ++it;
  ASSERT_EQ(*it, "/tmp/test_dir_content/root.txt");
}

TEST(DirContent, nonexistent_dir_throws) {
  system("rm -rf /tmp/test_dir_content_noexist");
  ASSERT_THROW(dir_content("/tmp/test_dir_content_noexist", false),
               std::exception);
}
