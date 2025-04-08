/**
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include "com/centreon/common/rapidjson_helper.hh"

#include "com/centreon/engine/modules/opentelemetry/grpc_config.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::common::literals;

TEST(otl_grpc_config, nor_host_nor_port_json) {
  ASSERT_THROW(grpc_config t(R"(
{   "toto":5
})"_json),
               std::exception);
}

TEST(otl_grpc_config, no_port_json) {
  ASSERT_THROW(grpc_config t(R"(
{   "host":"127.0.0.1"
})"_json),
               std::exception);
}

TEST(otl_grpc_config, no_host_json) {
  ASSERT_THROW(grpc_config t(R"(
{   "port":5678
})"_json),
               std::exception);
}

TEST(otl_grpc_config, bad_port_json) {
  ASSERT_THROW(grpc_config t(R"(
{   
    "host":"127.0.0.1",
    "port":1000
})"_json),
               std::exception);
}

TEST(otl_grpc_config, bad_port_json2) {
  ASSERT_THROW(grpc_config t(R"(
{   
    "host":"127.0.0.1",
    "port":"2500"
})"_json),
               std::exception);
}

TEST(otl_grpc_config, bad_port_json3) {
  ASSERT_THROW(grpc_config t(R"(
{   
    "host":"127.0.0.1",
    "port":250000
})"_json),
               std::exception);
}

TEST(otl_grpc_config, good_host_port) {
  grpc_config c(R"(
{   
    "host":"127.0.0.1",
    "port":2500
})"_json);
  ASSERT_EQ(c.get_hostport(), "127.0.0.1:2500");
  ASSERT_FALSE(c.is_compressed());
  ASSERT_FALSE(c.is_crypted());
  ASSERT_TRUE(c.get_cert().empty());
  ASSERT_TRUE(c.get_key().empty());
  ASSERT_TRUE(c.get_ca().empty());
  ASSERT_TRUE(c.get_ca_name().empty());
  ASSERT_EQ(c.get_second_keepalive_interval(), 30);
}

TEST(otl_grpc_config, good_host_port2) {
  grpc_config c(R"(
{   
    "host":"127.0.0.1",
    "port":2500,
    "encryption":true,
    "compression": true,
    "ca_name":"toto"
})"_json);
  ASSERT_EQ(c.get_hostport(), "127.0.0.1:2500");
  ASSERT_TRUE(c.is_compressed());
  ASSERT_TRUE(c.is_crypted());
  ASSERT_TRUE(c.get_cert().empty());
  ASSERT_TRUE(c.get_key().empty());
  ASSERT_EQ(c.get_ca_name(), "toto");
  ASSERT_TRUE(c.get_ca().empty());
  ASSERT_EQ(c.get_second_keepalive_interval(), 30);
}

// test if we can set trusted tokens
TEST(otl_grpc_config, tokens) {
  grpc_config c(R"(
{   
    "host":"127.0.0.1",
    "port":2500,
    "encryption":true,
    "compression": true,
    "ca_name":"toto",
    "trusted_tokens":["toto","titi"]
})"_json);
  ASSERT_EQ(c.get_hostport(), "127.0.0.1:2500");
  ASSERT_TRUE(c.is_compressed());
  ASSERT_TRUE(c.is_crypted());
  ASSERT_TRUE(c.get_cert().empty());
  ASSERT_TRUE(c.get_key().empty());
  ASSERT_EQ(c.get_ca_name(), "toto");
  ASSERT_TRUE(c.get_ca().empty());
  ASSERT_EQ(c.get_second_keepalive_interval(), 30);
  ASSERT_EQ(c.get_trusted_tokens()->size(), 2);
  ASSERT_TRUE(c.get_trusted_tokens()->contains("toto"));
  ASSERT_TRUE(c.get_trusted_tokens()->contains("titi"));
}

//  test if we can compare trusted tokens
TEST(otl_grpc_config, tokencompare) {
  grpc_config c(R"(
{   
    "host":"127.0.0.1",
    "port":2500,
    "encryption":true,
    "compression": true,
    "ca_name":"toto"
})"_json);
  grpc_config c_same(R"(
  {   
      "host":"127.0.0.1",
      "port":2500,
      "encryption":true,
      "compression": true,
      "ca_name":"toto"
  })"_json);
  grpc_config c2(R"(
  {   
      "host":"127.0.0.1",
      "port":2500,
      "encryption":true,
      "compression": true,
      "ca_name":"toto",
      "trusted_tokens":["toto","titi"]
  })"_json);
  grpc_config c2_same(R"(
  {   
      "host":"127.0.0.1",
      "port":2500,
      "encryption":true,
      "compression": true,
      "ca_name":"toto",
      "trusted_tokens":["toto","titi"]
  })"_json);
  grpc_config c2_minos(R"(
    {   
        "host":"127.0.0.1",
        "port":2500,
        "encryption":true,
        "compression": true,
        "ca_name":"toto",
        "trusted_tokens":["toto"]
    })"_json);
  grpc_config c2_plus(R"(
      {   
          "host":"127.0.0.1",
          "port":2500,
          "encryption":true,
          "compression": true,
          "ca_name":"toto",
          "trusted_tokens":["toto","titi","tata"]
      })"_json);

  ASSERT_EQ(c.compare(c_same), 0);
  ASSERT_EQ(c.compare(c2), 1);
  ASSERT_EQ(c2.compare(c), 1);
  ASSERT_EQ(c2.compare(c2_same), 0);
  ASSERT_EQ(c2.compare(c2_minos), 1);
  ASSERT_EQ(c2.compare(c2_plus), 1);
}