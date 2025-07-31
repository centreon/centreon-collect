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

#include <absl/container/btree_set.h>
#include <absl/synchronization/mutex.h>

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

#include <rapidjson/document.h>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

namespace multi_index = boost::multi_index;

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/configuration/applier/connector.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"

#include "com/centreon/agent/streaming_client.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_fmt.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_server.hh"

#include "../test_engine.hh"
#include "helper.hh"

using namespace com::centreon::engine;
using namespace com::centreon::agent;
// using namespace com::centreon::engine::configuration;
// using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::modules::opentelemetry;
using namespace ::opentelemetry::proto::collector::metrics::v1;

class agent_to_engine_test : public TestEngine {
 protected:
  std::shared_ptr<otl_server> _server;

  // agent code is mono-thread so it runs on his own io_context run by only one
  // thread
  std::shared_ptr<asio::io_context> _agent_io_context;

  asio::executor_work_guard<asio::io_context::executor_type> _worker;
  std::thread _agent_io_ctx_thread;

  centreon_agent::agent_stat::pointer _stats =
      std::make_shared<centreon_agent::agent_stat>(_agent_io_context);

 public:
  agent_to_engine_test()
      : _agent_io_context(std::make_shared<asio::io_context>()),
        _worker{asio::make_work_guard(*_agent_io_context)},
        _agent_io_ctx_thread([this] { _agent_io_context->run(); }) {}

  ~agent_to_engine_test() {
    _agent_io_context->stop();
    _agent_io_ctx_thread.join();
  }

  void SetUp() override {
    spdlog::default_logger()->set_level(spdlog::level::trace);
    ::fmt::formatter< ::opentelemetry::proto::collector::metrics::v1::
                          ExportMetricsServiceRequest>::json_grpc_format = true;
    timeperiod::timeperiods.clear();
    contact::contacts.clear();
    host::hosts.clear();
    host::hosts_by_id.clear();
    service::services.clear();
    service::services_by_id.clear();

    init_config_state();

    configuration::applier::connector conn_aply;
    configuration::Connector cnn;
    configuration::connector_helper cnn_hlp(&cnn);
    cnn.set_connector_name("agent");
    cnn.set_connector_line(
        "opentelemetry "
        "--processor=nagios_telegraf --extractor=attributes "
        "--host_path=resource_metrics.scope_metrics.data.data_points."
        "attributes.host "
        "--service_path=resource_metrics.scope_metrics.data.data_points."
        "attributes.service");

    conn_aply.add_object(cnn);
    configuration::error_cnt err;

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_config);
    ct_aply.resolve_object(ctct, err);

    configuration::Host hst =
        new_pb_configuration_host("test_host", "admin", 1, "agent", 1);
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);

    configuration::Service svc = new_pb_configuration_service(
        "test_host", "test_svc", "admin", 1, "agent", 2);
    svc.set_check_interval(1);
    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);

    configuration::Service svc2 = new_pb_configuration_service(
        "test_host", "test_svc_2", "admin", 2, "agent", 3);
    svc2.set_check_interval(1);
    svc_aply.add_object(svc2);

    configuration::Service svc_no_otel = new_pb_configuration_service(
        "test_host", "test_svc_no_otel", "admin", 3, "", 4);
    svc_no_otel.set_check_interval(1);
    svc_aply.add_object(svc_no_otel);

    hst_aply.resolve_object(hst, err);
    svc_aply.resolve_object(svc, err);
    svc_aply.resolve_object(svc2, err);
    svc_aply.resolve_object(svc_no_otel, err);
  }

  void TearDown() override {
    if (_server) {
      _server->shutdown(std::chrono::seconds(15));
      _server.reset();
    }
    deinit_config_state();
  }

  template <class metric_handler_type>
  void start_server(const grpc_config::pointer& listen_endpoint,
                    const centreon_agent::agent_config::pointer& agent_conf,
                    const metric_handler_type& handler) {
    _server = otl_server::load(_agent_io_context, listen_endpoint, agent_conf,
                               handler, spdlog::default_logger(), _stats);
  }
};

bool compare_to_expected_host_metric(
    const opentelemetry::proto::metrics::v1::ResourceMetrics& metric) {
  bool host_found = false, serv_found = false;
  for (const auto& attrib : metric.resource().attributes()) {
    if (attrib.key() == "host.name") {
      if (attrib.value().string_value() != "test_host") {
        return false;
      }
      host_found = true;
    }
    if (attrib.key() == "service.name") {
      if (!attrib.value().string_value().empty()) {
        return false;
      }
      serv_found = true;
    }
  }
  if (!host_found || !serv_found) {
    return false;
  }
  const auto& scope_metric = metric.scope_metrics();
  if (scope_metric.size() != 1)
    return false;
  const auto& metrics = scope_metric.begin()->metrics();
  if (metrics.empty())
    return false;
  const auto& status_metric = *metrics.begin();
  if (status_metric.name() != "status")
    return false;
  if (!status_metric.has_gauge())
    return false;
  if (status_metric.gauge().data_points().empty())
    return false;
  return status_metric.gauge().data_points().begin()->as_int() == 0;
}

bool test_exemplars(
    const google::protobuf::RepeatedPtrField<
        ::opentelemetry::proto::metrics::v1::Exemplar>& examplars,
    const std::map<std::string, double>& expected) {
  std::set<std::string> matches;

  for (const auto& ex : examplars) {
    if (ex.filtered_attributes().empty())
      continue;
    auto search = expected.find(ex.filtered_attributes().begin()->key());
    if (search == expected.end())
      return false;

    if (search->second != ex.as_double())
      return false;
    matches.insert(search->first);
  }
  return matches.size() == expected.size();
}

bool compare_to_expected_serv_metric(
    const opentelemetry::proto::metrics::v1::ResourceMetrics& metric,
    const std::string_view& serv_name) {
  bool host_found = false, serv_found = false;
  for (const auto& attrib : metric.resource().attributes()) {
    if (attrib.key() == "host.name") {
      if (attrib.value().string_value() != "test_host") {
        return false;
      }
      host_found = true;
    }
    if (attrib.key() == "service.name") {
      if (attrib.value().string_value() != serv_name) {
        return false;
      }
      serv_found = true;
    }
  }
  if (!host_found || !serv_found) {
    return false;
  }
  const auto& scope_metric = metric.scope_metrics();
  if (scope_metric.size() != 1)
    return false;
  const auto& metrics = scope_metric.begin()->metrics();
  if (metrics.empty())
    return false;

  for (const auto& met : metrics) {
    if (!met.has_gauge())
      return false;
    if (met.name() == "metric") {
      if (met.gauge().data_points().empty())
        return false;
      if (met.gauge().data_points().begin()->as_double() != 12)
        return false;
      if (!test_exemplars(met.gauge().data_points().begin()->exemplars(),
                          {{"crit_gt", 75.0},
                           {"crit_lt", 0.0},
                           {"warn_gt", 50.0},
                           {"warn_lt", 0.0}}))
        return false;
    } else if (met.name() == "metric2") {
      if (met.gauge().data_points().empty())
        return false;
      if (met.gauge().data_points().begin()->as_double() != 30)
        return false;
      if (!test_exemplars(met.gauge().data_points().begin()->exemplars(),
                          {{"crit_gt", 80.0},
                           {"crit_lt", 75.0},
                           {"warn_gt", 75.0},
                           {"warn_lt", 50.0},
                           {"min", 0.0},
                           {"max", 100.0}}))
        return false;

    } else if (met.name() == "status") {
      if (met.gauge().data_points().begin()->as_int() != 0)
        return false;
    } else
      return false;
  }

  return true;
}

TEST_F(agent_to_engine_test, server_send_conf_to_agent_and_receive_metrics) {
  grpc_config::pointer listen_endpoint =
      std::make_shared<grpc_config>("127.0.0.1:4623", false);

  ::credentials_decrypt.reset();
  absl::Mutex mut;
  std::vector<metric_request_ptr> received;
  std::vector<const opentelemetry::proto::metrics::v1::ResourceMetrics*>
      resource_metrics;

  auto agent_conf = std::make_shared<centreon_agent::agent_config>(1, 10, 5);

  start_server(listen_endpoint, agent_conf,
               [&](const metric_request_ptr& metric) {
                 absl::MutexLock l(&mut);
                 received.push_back(metric);
                 for (const opentelemetry::proto::metrics::v1::ResourceMetrics&
                          res_metric : metric->resource_metrics()) {
                   resource_metrics.push_back(&res_metric);
                 }
               });

  auto agent_client =
      streaming_client::load(_agent_io_context, spdlog::default_logger(),
                             listen_endpoint, "test_host");

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  command_manager::instance().execute();

  auto metric_received = [&]() { return resource_metrics.size() >= 3; };

  mut.LockWhen(absl::Condition(&metric_received));
  mut.Unlock();

  agent_client->shutdown();

  _server->shutdown(std::chrono::seconds(15));

  bool host_metric_found = true;
  bool serv_1_found = false;
  bool serv_2_found = false;

  for (const opentelemetry::proto::metrics::v1::ResourceMetrics* to_compare :
       resource_metrics) {
    if (compare_to_expected_serv_metric(*to_compare, "test_svc")) {
      serv_1_found = true;
    } else if (compare_to_expected_serv_metric(*to_compare, "test_svc_2")) {
      serv_2_found = true;
    } else if (compare_to_expected_host_metric(*to_compare)) {
      host_metric_found = true;
    } else {
      SPDLOG_ERROR("bad resource metric: {}", to_compare->DebugString());
      ASSERT_TRUE(false);
    }
  }
  ASSERT_TRUE(host_metric_found);
  ASSERT_TRUE(serv_1_found);
  ASSERT_TRUE(serv_2_found);
}

extern std::unique_ptr<com::centreon::common::crypto::aes256>
    credentials_decrypt;

TEST_F(
    agent_to_engine_test,
    server_send_no_encrypted_conf_on_no_crypted_connection_to_agent_and_receive_metrics) {
  grpc_config::pointer listen_endpoint =
      std::make_shared<grpc_config>("127.0.0.1:4623", false);

  ::credentials_decrypt =
      std::make_unique<com::centreon::common::crypto::aes256>(
          "SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=", "U2FsdA==");

  struct cred_eraser {
    ~cred_eraser() { ::credentials_decrypt.reset(); }
  };

  pb_config.set_credentials_encryption(true);

  cred_eraser eraser;

  absl::Mutex mut;
  std::vector<metric_request_ptr> received;
  std::vector<const opentelemetry::proto::metrics::v1::ResourceMetrics*>
      resource_metrics;

  auto agent_conf = std::make_shared<centreon_agent::agent_config>(1, 10, 5);

  start_server(listen_endpoint, agent_conf,
               [&](const metric_request_ptr& metric) {
                 absl::MutexLock l(&mut);
                 received.push_back(metric);
                 for (const opentelemetry::proto::metrics::v1::ResourceMetrics&
                          res_metric : metric->resource_metrics()) {
                   resource_metrics.push_back(&res_metric);
                 }
               });

  auto agent_client =
      streaming_client::load(_agent_io_context, spdlog::default_logger(),
                             listen_endpoint, "test_host");

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  command_manager::instance().execute();

  auto metric_received = [&]() { return resource_metrics.size() >= 3; };

  mut.LockWhen(absl::Condition(&metric_received));
  mut.Unlock();

  agent_client->shutdown();

  _server->shutdown(std::chrono::seconds(15));

  bool host_metric_found = true;
  bool serv_1_found = false;
  bool serv_2_found = false;

  for (const opentelemetry::proto::metrics::v1::ResourceMetrics* to_compare :
       resource_metrics) {
    if (compare_to_expected_serv_metric(*to_compare, "test_svc")) {
      serv_1_found = true;
    } else if (compare_to_expected_serv_metric(*to_compare, "test_svc_2")) {
      serv_2_found = true;
    } else if (compare_to_expected_host_metric(*to_compare)) {
      host_metric_found = true;
    } else {
      SPDLOG_ERROR("bad resource metric: {}", to_compare->DebugString());
      ASSERT_TRUE(false);
    }
  }
  ASSERT_TRUE(host_metric_found);
  ASSERT_TRUE(serv_1_found);
  ASSERT_TRUE(serv_2_found);
}