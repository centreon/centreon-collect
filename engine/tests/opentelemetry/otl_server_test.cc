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

#include <absl/synchronization/mutex.h>

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

#include <rapidjson/document.h>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/engine/modules/opentelemetry/otl_server.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace ::opentelemetry::proto::collector::metrics::v1;

extern std::shared_ptr<asio::io_context> g_io_context;

class otl_client {
  std::shared_ptr<::grpc::Channel> _channel;
  std::unique_ptr<MetricsService::Stub> _stub;
  ::grpc::ClientContext _client_context;

 public:
  otl_client(const std::string&,
             const std::shared_ptr<::grpc::ChannelCredentials>& credentials =
                 ::grpc::InsecureChannelCredentials());
  ~otl_client();

  ::grpc::Status expor(const ExportMetricsServiceRequest& request,
                       ExportMetricsServiceResponse* response);
};

otl_client::otl_client(
    const std::string& hostport,
    const std::shared_ptr<::grpc::ChannelCredentials>& credentials) {
  _channel = ::grpc::CreateChannel(hostport, credentials);
  _stub = MetricsService::NewStub(_channel);
}

otl_client::~otl_client() {
  _stub.reset();

  _channel.reset();
}

::grpc::Status otl_client::expor(const ExportMetricsServiceRequest& request,
                                 ExportMetricsServiceResponse* response) {
  return _stub->Export(&_client_context, request, response);
}

class otl_server_test : public ::testing::Test {
  std::shared_ptr<otl_server> _server;

 public:
  void SetUp() override {}

  void TearDown() override {
    if (_server) {
      _server->shutdown(std::chrono::seconds(15));
      _server.reset();
    }
  }

  template <class metric_handler_type>
  void start_server(const grpc_config::pointer& conf,
                    const metric_handler_type& handler) {
    std::shared_ptr<centreon_agent::agent_config> agent_conf =
        std::make_shared<centreon_agent::agent_config>(60, 100, 60, 10);
    _server = otl_server::load(
        g_io_context, conf, agent_conf, handler, spdlog::default_logger(),
        std::make_shared<centreon_agent::agent_stat>(g_io_context));
  }
};

TEST_F(otl_server_test, unsecure_client_server) {
  grpc_config::pointer serv_conf =
      std::make_shared<grpc_config>("127.0.0.1:6789", false);
  metric_request_ptr received;
  auto handler = [&](const metric_request_ptr& request) { received = request; };
  start_server(serv_conf, handler);

  otl_client client("127.0.0.1:6789");

  ExportMetricsServiceRequest request;
  ExportMetricsServiceResponse response;

  auto metrics = request.add_resource_metrics();
  metrics->set_schema_url("hello opentelemetry");
  auto metric = metrics->add_scope_metrics()->add_metrics();
  metric->set_name("metric cpu");
  metric->set_description("metric to send");
  metric->set_unit("%");
  auto gauge = metric->mutable_gauge();
  auto point = gauge->add_data_points();
  point->set_time_unix_nano(time(nullptr));

  ::grpc::Status status = client.expor(request, &response);

  ASSERT_TRUE(status.ok());
}
