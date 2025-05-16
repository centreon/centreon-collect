/**
 * Copyright 2024 Centreon
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
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_stat.hh"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_reverse_client.hh"
#include "com/centreon/engine/modules/opentelemetry/centreon_agent/to_agent_connector.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

extern std::shared_ptr<asio::io_context> g_io_context;

struct fake_connector : public to_agent_connector {
  using config_to_fake = absl::btree_map<grpc_config::pointer,
                                         std::shared_ptr<to_agent_connector>,
                                         grpc_config_compare>;

  fake_connector(const grpc_config::pointer& conf,
                 const std::shared_ptr<boost::asio::io_context>& io_context,
                 const centreon_agent::agent_config::pointer& agent_conf,
                 const metric_handler& handler,
                 const std::shared_ptr<spdlog::logger>& logger,
                 const agent_stat::pointer& stats)
      : to_agent_connector(conf,
                           io_context,
                           agent_conf,
                           handler,
                           logger,
                           stats) {}

  void start() override {
    all_fake.emplace(std::static_pointer_cast<grpc_config>(get_conf()),
                     shared_from_this());
  }

  static std::shared_ptr<to_agent_connector> load(
      const grpc_config::pointer& conf,
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const centreon_agent::agent_config::pointer& agent_conf,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger,
      const agent_stat::pointer& stats) {
    std::shared_ptr<to_agent_connector> ret = std::make_shared<fake_connector>(
        conf, io_context, agent_conf, handler, logger, stats);
    ret->start();
    return ret;
  }

  static config_to_fake all_fake;

  void shutdown() override {
    all_fake.erase(std::static_pointer_cast<grpc_config>(get_conf()));
  }
};

fake_connector::config_to_fake fake_connector::all_fake;

class my_agent_reverse_client : public agent_reverse_client {
 public:
  my_agent_reverse_client(
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const metric_handler& handler,
      const std::shared_ptr<spdlog::logger>& logger,
      const agent_stat::pointer& stats)
      : agent_reverse_client(io_context, handler, logger, stats) {}

  agent_reverse_client::config_to_client::iterator
  _create_new_client_connection(
      const grpc_config::pointer& agent_endpoint,
      const agent_config::pointer& agent_conf) override {
    return _agents
        .try_emplace(
            agent_endpoint,
            fake_connector::load(agent_endpoint, _io_context, agent_conf,
                                 _metric_handler, _logger, _agent_stats))
        .first;
  }

  void _shutdown_connection(config_to_client::const_iterator to_delete) {
    to_delete->second->shutdown();
  }
};

TEST(agent_reverse_client, update_config) {
  my_agent_reverse_client to_test(
      g_io_context, [](const metric_request_ptr&) {}, spdlog::default_logger(),
      std::make_shared<agent_stat>(g_io_context));

  ASSERT_TRUE(fake_connector::all_fake.empty());

  auto agent_conf = std::shared_ptr<centreon_agent::agent_config>(
      new centreon_agent::agent_config(
          100, 60, 10, {std::make_shared<grpc_config>("host1:port1", false)}));
  to_test.update(agent_conf);
  ASSERT_EQ(fake_connector::all_fake.size(), 1);
  ASSERT_EQ(fake_connector::all_fake.begin()->first,
            *agent_conf->get_agent_grpc_reverse_conf().begin());
  agent_conf = std::make_shared<centreon_agent::agent_config>(100, 1, 10);
  to_test.update(agent_conf);
  ASSERT_EQ(fake_connector::all_fake.size(), 0);

  agent_conf = std::shared_ptr<centreon_agent::agent_config>(
      new centreon_agent::agent_config(
          100, 60, 10,
          {std::make_shared<grpc_config>("host1:port1", false),
           std::make_shared<grpc_config>("host1:port3", false)}));
  to_test.update(agent_conf);
  ASSERT_EQ(fake_connector::all_fake.size(), 2);
  auto first_conn = fake_connector::all_fake.begin()->second;
  auto second_conn = (++fake_connector::all_fake.begin())->second;
  agent_conf = std::shared_ptr<centreon_agent::agent_config>(
      new centreon_agent::agent_config(
          100, 60, 10,
          {std::make_shared<grpc_config>("host1:port1", false),
           std::make_shared<grpc_config>("host1:port2", false),
           std::make_shared<grpc_config>("host1:port3", false)}));

  to_test.update(agent_conf);
  ASSERT_EQ(fake_connector::all_fake.size(), 3);
  ASSERT_EQ(fake_connector::all_fake.begin()->second, first_conn);
  ASSERT_EQ((++(++fake_connector::all_fake.begin()))->second, second_conn);
  second_conn = (++fake_connector::all_fake.begin())->second;
  auto third_conn = (++(++fake_connector::all_fake.begin()))->second;

  agent_conf = std::shared_ptr<centreon_agent::agent_config>(
      new centreon_agent::agent_config(
          100, 60, 10,
          {std::make_shared<grpc_config>("host1:port1", false),
           std::make_shared<grpc_config>("host1:port3", false)}));
  to_test.update(agent_conf);
  ASSERT_EQ(fake_connector::all_fake.size(), 2);
  ASSERT_EQ(fake_connector::all_fake.begin()->second, first_conn);
  ASSERT_EQ((++fake_connector::all_fake.begin())->second, third_conn);

  agent_conf = std::shared_ptr<centreon_agent::agent_config>(
      new centreon_agent::agent_config(
          100, 60, 10, {std::make_shared<grpc_config>("host1:port3", false)}));
  to_test.update(agent_conf);
  ASSERT_EQ(fake_connector::all_fake.size(), 1);
  ASSERT_EQ(fake_connector::all_fake.begin()->second, third_conn);
}