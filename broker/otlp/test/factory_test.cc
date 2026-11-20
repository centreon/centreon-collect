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

#include "com/centreon/broker/otlp/factory.hh"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>

#include "com/centreon/broker/config/endpoint.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;
using com::centreon::exceptions::msg_fmt;

namespace {

/**
 * @brief OTLP output configuration with a default endpoint.
 *
 * @param params extra parameters; endpoint is added if absent
 * @return the configuration
 */
config::endpoint make_cfg(
    const absl::btree_map<std::string, std::string>& params = {}) {
  config::endpoint cfg(config::endpoint::io_type::output);
  cfg.name = "otlp-export";
  cfg.type = "otlp";
  cfg.params = params;
  cfg.params.emplace("endpoint", "collector:4317");
  return cfg;
}

/**
 * @brief Write a file, replacing its content.
 *
 * @param path file path
 * @param content file content
 */
void write_file(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::trunc);
  f << content;
}

}  // namespace

TEST(otlp_factory, matches_its_own_type_only) {
  factory f;
  config::endpoint cfg = make_cfg();
  EXPECT_TRUE(f.has_endpoint(cfg, nullptr));

  cfg.type = "OTLP";
  EXPECT_TRUE(f.has_endpoint(cfg, nullptr)) << "type match is case-insensitive";

  cfg.type = "victoria_metrics";
  EXPECT_FALSE(f.has_endpoint(cfg, nullptr));
}

/* the connector keeps the CMA host information in the endpoint cache */
TEST(otlp_factory, cache_is_always_enabled) {
  factory f;
  config::endpoint cfg = make_cfg();
  cfg.cache_enabled = false;
  f.set_default_values(cfg);
  EXPECT_TRUE(cfg.cache_enabled);
}

TEST(otlp_factory, endpoint_is_mandatory) {
  config::endpoint cfg(config::endpoint::io_type::output);
  cfg.name = "otlp-export";
  cfg.type = "otlp";
  EXPECT_THROW(factory::parse_config(cfg), msg_fmt);
}

TEST(otlp_factory, annotation_streams_default_to_on) {
  auto conf = factory::parse_config(make_cfg());
  EXPECT_TRUE(conf->send_thresholds);
  EXPECT_TRUE(conf->send_status);
  EXPECT_TRUE(conf->send_min_max);
}

TEST(otlp_factory, annotation_streams_can_be_turned_off) {
  auto conf = factory::parse_config(make_cfg({{"send_thresholds", "false"},
                                              {"send_status", "false"},
                                              {"send_min_max", "false"}}));
  EXPECT_FALSE(conf->send_thresholds);
  EXPECT_FALSE(conf->send_status);
  EXPECT_FALSE(conf->send_min_max);
}

TEST(otlp_factory, batching_defaults) {
  auto conf = factory::parse_config(make_cfg());
  EXPECT_EQ(conf->max_datapoints_per_batch, 5000u);
  EXPECT_EQ(conf->max_send_interval, 10u);
  EXPECT_EQ(conf->max_inflight_requests, 4u);
  EXPECT_EQ(conf->export_timeout, 30u);
}

TEST(otlp_factory, grpc_target_is_taken_from_endpoint) {
  auto conf = factory::parse_config(make_cfg());
  ASSERT_TRUE(conf->grpc);
  EXPECT_EQ(conf->grpc->get_hostport(), "collector:4317");
  EXPECT_FALSE(conf->grpc->is_crypted());
}

/* The TLS parameters are file paths: grpc_config receives the PEM content. */
TEST(otlp_factory, tls_files_are_read_into_grpc_config) {
  const std::string dir = ::testing::TempDir();
  const std::string ca = dir + "otlp_factory_ca.pem";
  const std::string cert = dir + "otlp_factory_broker.crt";
  const std::string key = dir + "otlp_factory_broker.key";
  write_file(ca, "ca content");
  write_file(cert, "cert content");
  write_file(key, "key content");

  auto conf = factory::parse_config(make_cfg({{"encryption", "true"},
                                              {"ca_certificate", ca},
                                              {"certificate", cert},
                                              {"private_key", key}}));
  ASSERT_TRUE(conf->grpc);
  EXPECT_TRUE(conf->grpc->is_crypted());
  EXPECT_EQ(conf->grpc->get_ca(), "ca content");
  EXPECT_EQ(conf->grpc->get_cert(), "cert content");
  EXPECT_EQ(conf->grpc->get_key(), "key content");

  std::remove(ca.c_str());
  std::remove(cert.c_str());
  std::remove(key.c_str());
}

TEST(otlp_factory, unreadable_tls_file_is_rejected) {
  for (const char* param : {"ca_certificate", "certificate", "private_key"}) {
    try {
      factory::parse_config(make_cfg(
          {{"encryption", "true"}, {param, "/nonexistent/otlp_factory.pem"}}));
      ADD_FAILURE() << param << ": a missing file must be rejected";
    } catch (const msg_fmt& e) {
      EXPECT_NE(std::string(e.what()).find(param), std::string::npos)
          << e.what();
      EXPECT_NE(std::string(e.what()).find("otlp-export"), std::string::npos)
          << e.what();
    }
  }
}

/* Without encryption the TLS files are neither needed nor read. */
TEST(otlp_factory, tls_files_are_ignored_without_encryption) {
  auto conf = factory::parse_config(
      make_cfg({{"ca_certificate", "/nonexistent/otlp_factory.pem"}}));
  ASSERT_TRUE(conf->grpc);
  EXPECT_FALSE(conf->grpc->is_crypted());
  EXPECT_TRUE(conf->grpc->get_ca().empty());
}

TEST(otlp_factory, rejects_non_numeric_values) {
  EXPECT_THROW(factory::parse_config(make_cfg({{"max_send_interval", "soon"}})),
               msg_fmt);
}

/* A zero batch size would never trigger a send. */
TEST(otlp_factory, rejects_degenerate_limits) {
  EXPECT_THROW(
      factory::parse_config(make_cfg({{"max_datapoints_per_batch", "0"}})),
      msg_fmt);
  EXPECT_THROW(
      factory::parse_config(make_cfg({{"max_inflight_requests", "0"}})),
      msg_fmt);
}

TEST(otlp_factory, host_ip_exclude_link_local) {
  auto conf = factory::parse_config(make_cfg());
  EXPECT_FALSE(conf->host_ip_exclude_link_local);

  conf =
      factory::parse_config(make_cfg({{"host_ip_exclude_link_local", "true"}}));
  EXPECT_TRUE(conf->host_ip_exclude_link_local);
}
