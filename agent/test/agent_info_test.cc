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

#include "agent_info.hh"

using namespace com::centreon::agent;

TEST(agent_info, fill_agent_info_otel_fields) {
  read_os_version();
  AgentInfo info;
  fill_agent_info("my_host", "my_template", &info, spdlog::default_logger());

  EXPECT_EQ(info.host(), "my_host");
  EXPECT_EQ(info.host_template(), "my_template");
#ifdef _WIN32
  EXPECT_EQ(info.os_type(), "windows");
#else
  EXPECT_EQ(info.os_type(), "linux");
#endif
  EXPECT_FALSE(info.arch().empty());

  // ips must be sorted and unique, so that two collects compare equal
  std::vector<std::string> ips(info.ips().begin(), info.ips().end());
  EXPECT_TRUE(std::is_sorted(ips.begin(), ips.end()));
  EXPECT_EQ(std::adjacent_find(ips.begin(), ips.end()), ips.end());
  for (const std::string& ip : ips) {
    EXPECT_NE(ip, "127.0.0.1");
    EXPECT_NE(ip, "::1");
  }

  AgentInfo info2;
  fill_agent_info("my_host", "my_template", &info2, spdlog::default_logger());
  EXPECT_EQ(info.SerializeAsString(), info2.SerializeAsString());
}

#ifndef _WIN32

TEST(agent_info, otel_arch) {
  EXPECT_EQ(otel_arch("x86_64"), "amd64");
  EXPECT_EQ(otel_arch("aarch64"), "arm64");
  EXPECT_EQ(otel_arch("armv7l"), "arm32");
  EXPECT_EQ(otel_arch("i686"), "x86");
  EXPECT_EQ(otel_arch("ppc64le"), "ppc64");
  EXPECT_EQ(otel_arch("ppc"), "ppc32");
  EXPECT_EQ(otel_arch("s390x"), "s390x");
  EXPECT_EQ(otel_arch("riscv64"), "riscv64");
}

class agent_info_machine_id : public ::testing::Test {
 protected:
  std::string _path;

  void SetUp() override {
    _path = testing::TempDir() + "agent_info_machine_id";
  }
  void TearDown() override { std::remove(_path.c_str()); }

  void write(const std::string& content) {
    std::ofstream f(_path, std::ios::trunc);
    f << content;
  }
};

TEST_F(agent_info_machine_id, valid) {
  write("4c4c4544004d3510804bb4c04f4a3432\n");
  EXPECT_EQ(read_machine_id(_path), "4c4c4544004d3510804bb4c04f4a3432");
}

TEST_F(agent_info_machine_id, uninitialized) {
  write("uninitialized\n");
  EXPECT_EQ(read_machine_id(_path), "");
}

TEST_F(agent_info_machine_id, bad_length) {
  write("4c4c4544004d3510804bb4c04f4a34\n");
  EXPECT_EQ(read_machine_id(_path), "");
}

TEST_F(agent_info_machine_id, absent) {
  EXPECT_EQ(read_machine_id(_path + "_absent"), "");
}

#endif
