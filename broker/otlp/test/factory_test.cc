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

#include "com/centreon/broker/config/endpoint.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;
using com::centreon::exceptions::msg_fmt;

namespace {

config::endpoint make_cfg(
    const std::map<std::string, std::string>& params = {}) {
  config::endpoint cfg(config::endpoint::io_type::output);
  cfg.name = "otlp-export";
  cfg.type = "otlp";
  cfg.params = params;
  cfg.params.emplace("endpoint", "collector:4317");
  return cfg;
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

TEST(otlp_factory, endpoint_is_mandatory) {
  config::endpoint cfg(config::endpoint::io_type::output);
  cfg.name = "otlp-export";
  cfg.type = "otlp";
  EXPECT_THROW(factory::parse_config(cfg), msg_fmt);
}

/* The study requires thresholds, status and bounds to be exported unless the
 * operator turns them off. */
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

TEST(otlp_factory, tls_material_is_forwarded_to_grpc_config) {
  auto conf = factory::parse_config(
      make_cfg({{"encryption", "true"},
                {"ca_certificate", "/etc/ssl/ca.pem"},
                {"certificate", "/etc/ssl/broker.crt"},
                {"private_key", "/etc/ssl/broker.key"}}));
  ASSERT_TRUE(conf->grpc);
  EXPECT_TRUE(conf->grpc->is_crypted());
}

TEST(otlp_factory, rejects_non_numeric_values) {
  EXPECT_THROW(
      factory::parse_config(make_cfg({{"max_send_interval", "soon"}})),
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
