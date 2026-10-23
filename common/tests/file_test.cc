/**
 * Copyright 2024-2026 Centreon
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

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <boost/interprocess/exceptions.hpp>

#include "com/centreon/exceptions/msg_fmt.hh"
#include "file.hh"

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

class TestProtoFile : public ::testing::Test {
 protected:
  std::filesystem::path _dir;

  void SetUp() override {
    _dir = std::filesystem::temp_directory_path() / "tests_proto_file";
    std::filesystem::remove_all(_dir);
    std::filesystem::create_directories(_dir);
  }

  void TearDown() override { std::filesystem::remove_all(_dir); }

  size_t _nb_files() const {
    return std::distance(std::filesystem::directory_iterator(_dir),
                         std::filesystem::directory_iterator());
  }
};

TEST_F(TestProtoFile, save_then_load) {
  google::protobuf::Struct to_write;
  auto& fields = *to_write.mutable_fields();
  fields["name"].set_string_value("central");
  fields["id"].set_number_value(42);
  fields["enabled"].set_bool_value(true);

  std::string file_path = (_dir / "data.bin").string();
  ASSERT_NO_THROW(save_proto_to_disk(file_path, to_write));
  ASSERT_EQ(std::filesystem::file_size(file_path), to_write.ByteSizeLong());

  google::protobuf::Struct loaded;
  ASSERT_TRUE(load_proto_from_disk(file_path, loaded));
  ASSERT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(loaded, to_write));
  ASSERT_EQ(loaded.fields().at("name").string_value(), "central");
  ASSERT_EQ(loaded.fields().at("id").number_value(), 42);
  ASSERT_TRUE(loaded.fields().at("enabled").bool_value());
}

TEST_F(TestProtoFile, save_then_load_several_pages) {
  google::protobuf::Struct to_write;
  auto& fields = *to_write.mutable_fields();
  for (int i = 0; i < 10000; ++i)
    fields[fmt::format("key_{}", i)].set_string_value(
        fmt::format("value_{}", i));
  ASSERT_GT(to_write.ByteSizeLong(), 4 * ::getpagesize());

  std::string file_path = (_dir / "big.bin").string();
  ASSERT_NO_THROW(save_proto_to_disk(file_path, to_write));

  google::protobuf::Struct loaded;
  ASSERT_TRUE(load_proto_from_disk(file_path, loaded));
  ASSERT_EQ(loaded.fields_size(), 10000);
  ASSERT_EQ(loaded.fields().at("key_9999").string_value(), "value_9999");
}

TEST_F(TestProtoFile, save_overwrites_and_leaves_no_temporary_file) {
  std::string file_path = (_dir / "data.bin").string();

  google::protobuf::Struct first;
  (*first.mutable_fields())["version"].set_number_value(1);
  (*first.mutable_fields())["padding"].set_string_value(std::string(1000, 'a'));
  ASSERT_NO_THROW(save_proto_to_disk(file_path, first));

  google::protobuf::Struct second;
  (*second.mutable_fields())["version"].set_number_value(2);
  ASSERT_NO_THROW(save_proto_to_disk(file_path, second));

  // the new content is smaller, nothing of the first one must remain
  ASSERT_EQ(std::filesystem::file_size(file_path), second.ByteSizeLong());
  ASSERT_EQ(_nb_files(), 1u);

  google::protobuf::Struct loaded;
  ASSERT_TRUE(load_proto_from_disk(file_path, loaded));
  ASSERT_EQ(loaded.fields_size(), 1);
  ASSERT_EQ(loaded.fields().at("version").number_value(), 2);
}

TEST_F(TestProtoFile, save_empty_message) {
  std::string file_path = (_dir / "empty.bin").string();
  google::protobuf::Struct empty;
  ASSERT_NO_THROW(save_proto_to_disk(file_path, empty));
  ASSERT_TRUE(std::filesystem::exists(file_path));
  ASSERT_EQ(std::filesystem::file_size(file_path), 0u);
  ASSERT_EQ(_nb_files(), 1u);
}

TEST_F(TestProtoFile, save_in_missing_directory) {
  std::string file_path = (_dir / "missing" / "data.bin").string();
  google::protobuf::Struct to_write;
  (*to_write.mutable_fields())["id"].set_number_value(1);
  ASSERT_THROW(save_proto_to_disk(file_path, to_write),
               com::centreon::exceptions::msg_fmt);
  ASSERT_FALSE(std::filesystem::exists(file_path));
}

TEST_F(TestProtoFile, save_rename_failure_removes_temporary_file) {
  // file_path is an existing directory, so rename fails
  std::filesystem::path file_path = _dir / "a_directory";
  std::filesystem::create_directory(file_path);
  google::protobuf::Struct to_write;
  (*to_write.mutable_fields())["id"].set_number_value(1);
  ASSERT_THROW(save_proto_to_disk(file_path.string(), to_write),
               com::centreon::exceptions::msg_fmt);
  ASSERT_TRUE(std::filesystem::is_directory(file_path));
  ASSERT_EQ(_nb_files(), 1u);
}

TEST_F(TestProtoFile, load_missing_file) {
  std::string file_path = (_dir / "missing.bin").string();
  google::protobuf::Struct loaded;
  (*loaded.mutable_fields())["untouched"].set_bool_value(true);
  ASSERT_FALSE(load_proto_from_disk(file_path, loaded));
  ASSERT_EQ(loaded.fields_size(), 1);
  ASSERT_TRUE(loaded.fields().at("untouched").bool_value());
}

TEST_F(TestProtoFile, load_unreadable_file) {
  if (::geteuid() == 0)
    GTEST_SKIP() << "root can read any file";
  std::string file_path = (_dir / "unreadable.bin").string();
  google::protobuf::Struct to_write;
  (*to_write.mutable_fields())["id"].set_number_value(1);
  ASSERT_NO_THROW(save_proto_to_disk(file_path, to_write));
  std::filesystem::permissions(file_path, std::filesystem::perms::none);

  google::protobuf::Struct loaded;
  ASSERT_FALSE(load_proto_from_disk(file_path, loaded));
  ASSERT_EQ(loaded.fields_size(), 0);
}

TEST_F(TestProtoFile, load_empty_file) {
  std::string file_path = (_dir / "empty.bin").string();
  google::protobuf::Struct empty;
  ASSERT_NO_THROW(save_proto_to_disk(file_path, empty));

  google::protobuf::Struct loaded;
  ASSERT_TRUE(load_proto_from_disk(file_path, loaded));
}

TEST_F(TestProtoFile, load_corrupted_file) {
  std::string file_path = (_dir / "corrupted.bin").string();
  {
    std::ofstream out(file_path, std::ios::binary);
    out << std::string(16, '\xff');
  }
  google::protobuf::Struct loaded;
  ASSERT_THROW(load_proto_from_disk(file_path, loaded),
               com::centreon::exceptions::msg_fmt);
}
