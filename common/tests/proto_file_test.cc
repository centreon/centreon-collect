/**
 * Copyright 2026 Centreon
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
#include <gtest/gtest.h>
#include <unistd.h>

#include <fstream>
#include <iterator>

#include "common/engine_conf/state.pb.h"
#include "common/proto_file/proto_file.hh"

using namespace com::centreon::common;
using com::centreon::engine::configuration::State;

class ProtoFile : public ::testing::Test {
 protected:
  std::filesystem::path _dir;
  std::filesystem::path _file;

 public:
  void SetUp() override {
    _dir = std::filesystem::temp_directory_path() /
           ("ut_proto_file_" + std::to_string(::getpid()));
    std::filesystem::remove_all(_dir);
    std::filesystem::create_directories(_dir);
    _file = _dir / "1.prot";
  }

  void TearDown() override { std::filesystem::remove_all(_dir); }

  /** @brief A State big enough for the compression to matter. */
  static State make_state(const std::string& version) {
    State st;
    st.set_poller_id(1);
    st.set_config_version(version);
    for (int i = 0; i < 100; ++i) {
      auto* h = st.add_hosts();
      h->set_host_id(i + 1);
      h->set_host_name("host_" + std::to_string(i + 1));
      h->set_address("127.0.0.1");
    }
    return st;
  }

  std::string read_raw() const {
    std::ifstream f(_file, std::ios::binary);
    return {std::istreambuf_iterator<char>{f},
            std::istreambuf_iterator<char>{}};
  }

  void write_raw(const std::string& content) const {
    std::ofstream f(_file, std::ios::binary | std::ios::trunc);
    f.write(content.data(), content.size());
  }

  /** @brief Whether something else than the file is left in the directory. */
  bool leftover() const {
    for (const auto& entry : std::filesystem::directory_iterator(_dir))
      if (entry.path() != _file)
        return true;
    return false;
  }
};

/**
 * Scenario: a message written without compression is read back.
 * Given a State
 * When it is written without compression
 * Then the file is the plain serialized message, as the previous versions wrote
 * it
 * And it is read back identical, with no temporary file left.
 */
TEST_F(ProtoFile, PlainRoundTrip) {
  const State st = make_state("v1");
  std::error_code ec;
  write_proto_file(_file, st, false, ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_EQ(read_raw(), st.SerializeAsString());
  EXPECT_FALSE(leftover());

  State read;
  read_proto_file(_file, read, ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_EQ(read.SerializeAsString(), st.SerializeAsString());
}

/**
 * Scenario: a message written with compression is read back.
 * Given a State
 * When it is written with compression
 * Then the file starts with the header: magic, version 1, zstd
 * And it is smaller than the plain message
 * And it is read back identical, by the template form too.
 */
TEST_F(ProtoFile, CompressedRoundTrip) {
  const State st = make_state("v1");
  std::error_code ec;
  write_proto_file(_file, st, true, ec);
  ASSERT_FALSE(ec) << ec.message();
  const std::string raw = read_raw();
  ASSERT_GE(raw.size(), 6u);
  EXPECT_EQ(raw.substr(0, 4), std::string("\0CBC", 4));
  EXPECT_EQ(raw[4], 1);
  EXPECT_EQ(raw[5], 1);
  EXPECT_LT(raw.size(), st.ByteSizeLong());
  EXPECT_FALSE(leftover());

  const State read = read_proto_file<State>(_file, ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_EQ(read.SerializeAsString(), st.SerializeAsString());
}

/**
 * Scenario: a file is replaced whole.
 * Given a file holding v1
 * When v2 is written over it, compressed, then v3 plain
 * Then each read gives the last version written.
 */
TEST_F(ProtoFile, ReplacesTheFile) {
  std::error_code ec;
  write_proto_file(_file, make_state("v1"), false, ec);
  ASSERT_FALSE(ec);
  write_proto_file(_file, make_state("v2"), true, ec);
  ASSERT_FALSE(ec);
  EXPECT_EQ(read_proto_file<State>(_file, ec).config_version(), "v2");
  write_proto_file(_file, make_state("v3"), false, ec);
  ASSERT_FALSE(ec);
  EXPECT_EQ(read_proto_file<State>(_file, ec).config_version(), "v3");
  EXPECT_FALSE(leftover());
}

/**
 * Scenario: a write that fails leaves nothing behind.
 * Given a path in a directory that does not exist
 * When a message is written there
 * Then the error is the system one
 * And nothing is created.
 */
TEST_F(ProtoFile, FailedWriteLeavesNothing) {
  const auto path = _dir / "missing" / "1.prot";
  std::error_code ec;
  write_proto_file(path, make_state("v1"), true, ec);
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
  EXPECT_FALSE(std::filesystem::exists(_dir / "missing"));
  EXPECT_TRUE(std::filesystem::is_empty(_dir));
}

/**
 * Scenario: a missing file is told apart from a bad one.
 * Given no file
 * When it is read
 * Then the error is no_such_file_or_directory.
 */
TEST_F(ProtoFile, MissingFile) {
  std::error_code ec;
  State st;
  read_proto_file(_file, st, ec);
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
}

/**
 * Scenario: what cannot be read is said precisely.
 * Given a compressed file
 * When its header is truncated, its version or algorithm unknown, its content
 * altered or cut, or a plain file holds garbage
 * Then each read fails with the matching error.
 */
TEST_F(ProtoFile, BadContents) {
  std::error_code ec;
  write_proto_file(_file, make_state("v1"), true, ec);
  ASSERT_FALSE(ec);
  const std::string good = read_raw();
  State st;

  write_raw(good.substr(0, 5));
  read_proto_file(_file, st, ec);
  EXPECT_EQ(ec, proto_file_errc::truncated_header);

  std::string wrong = good;
  wrong[4] = 2;
  write_raw(wrong);
  read_proto_file(_file, st, ec);
  EXPECT_EQ(ec, proto_file_errc::unknown_format_version);

  wrong = good;
  wrong[5] = 2;
  write_raw(wrong);
  read_proto_file(_file, st, ec);
  EXPECT_EQ(ec, proto_file_errc::unknown_algorithm);

  /* The zstd checksum rejects an altered content. */
  wrong = good;
  wrong[wrong.size() - 6] ^= 0x55;
  write_raw(wrong);
  read_proto_file(_file, st, ec);
  EXPECT_EQ(ec, proto_file_errc::corrupted_content);

  write_raw(good.substr(0, good.size() / 2));
  read_proto_file(_file, st, ec);
  EXPECT_EQ(ec, proto_file_errc::corrupted_content);

  /* Field number 0 does not exist: no message starts with a null byte. */
  write_raw(std::string("\0\1\2\3\4\5\6\7", 8));
  read_proto_file(_file, st, ec);
  EXPECT_EQ(ec, proto_file_errc::parse_error);
  EXPECT_NE(ec.message().find("parse"), std::string::npos);
}

/**
 * Scenario: a message produced in a stream is written as the message itself
 * would be.
 * Given a State serialized by a function into the stream it is given
 * When it is written without compression, then with compression
 * Then the plain file is the serialized message
 * And both are read back identical, with no temporary file left.
 */
TEST_F(ProtoFile, StreamedRoundTrip) {
  const State st = make_state("v1");
  const auto serialize = [&st](std::ostream& os) {
    return st.SerializeToOstream(&os);
  };
  std::error_code ec;
  write_proto_file_streamed(_file, serialize, false, ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_EQ(read_raw(), st.SerializeAsString());
  EXPECT_EQ(read_proto_file<State>(_file, ec).SerializeAsString(),
            st.SerializeAsString());
  ASSERT_FALSE(ec) << ec.message();

  write_proto_file_streamed(_file, serialize, true, ec);
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_EQ(read_raw().substr(0, 4), std::string("\0CBC", 4));
  EXPECT_EQ(read_proto_file<State>(_file, ec).SerializeAsString(),
            st.SerializeAsString());
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_FALSE(leftover());
}

/**
 * Scenario: a serialization that fails leaves the previous file.
 * Given a file holding v1
 * When a function that cannot serialize writes over it, with or without
 * compression
 * Then the error is serialize_error
 * And the file still holds v1, with no temporary file left.
 */
TEST_F(ProtoFile, StreamedFailureKeepsThePreviousFile) {
  std::error_code ec;
  write_proto_file(_file, make_state("v1"), false, ec);
  ASSERT_FALSE(ec);
  const auto fail = [](std::ostream& os) {
    os << "partial";
    return false;
  };
  for (bool compress : {false, true}) {
    write_proto_file_streamed(_file, fail, compress, ec);
    EXPECT_EQ(ec, proto_file_errc::serialize_error);
    EXPECT_EQ(read_proto_file<State>(_file, ec).config_version(), "v1");
    EXPECT_FALSE(leftover());
  }
}
