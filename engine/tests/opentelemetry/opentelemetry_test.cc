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

#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/synchronization/notification.h>
#include <google/protobuf/util/json_util.h>
#include <gtest/gtest.h>

#include <boost/algorithm/string.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <rapidjson/document.h>

#include "com/centreon/common/http/http_server.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "common/engine_legacy_conf/host.hh"
#include "common/engine_legacy_conf/service.hh"

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/engine/modules/opentelemetry/opentelemetry.hh"

#include "helper.hh"
#include "test_engine.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::engine;

extern const char* telegraf_example;

extern std::shared_ptr<asio::io_context> g_io_context;

class open_telemetry
    : public com::centreon::engine::modules::opentelemetry::open_telemetry {
 protected:
  void _create_otl_server(const grpc_config::pointer& server_conf) override {}

 public:
  open_telemetry(const std::string_view config_file_path,
                 const std::shared_ptr<asio::io_context>& io_context,
                 const std::shared_ptr<spdlog::logger>& logger)
      : com::centreon::engine::modules::opentelemetry::open_telemetry(
            config_file_path,
            io_context,
            logger) {}

  void on_metric(const metric_request_ptr& metric) { _on_metric(metric); }
  void shutdown() { _shutdown(); }
  static std::shared_ptr<open_telemetry> load(
      const std::string_view& config_path,
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger) {
    std::shared_ptr<open_telemetry> ret =
        std::make_shared<open_telemetry>(config_path, io_context, logger);
    ret->_reload();
    ret->_start_second_timer();
    return ret;
  }
};

class open_telemetry_test : public TestEngine {
 public:
  commands::otel::host_serv_list::pointer _host_serv_list;

  open_telemetry_test();
  static void SetUpTestSuite();
  void SetUp() override;
  void TearDown() override;
};

open_telemetry_test::open_telemetry_test()
    : _host_serv_list(std::make_shared<commands::otel::host_serv_list>()) {
  _host_serv_list->register_host_serv("localhost", "check_icmp");
}

void open_telemetry_test::SetUpTestSuite() {
  std::ofstream conf_file("/tmp/otel_conf.json");
  conf_file << R"({
    "otel_server": {
      "host": "127.0.0.1",
      "port": 4317
    }
}
)";
  conf_file.close();
  // spdlog::default_logger()->set_level(spdlog::level::trace);
}

void open_telemetry_test::SetUp() {
  init_config_state();
  config->contacts().clear();
  configuration::applier::contact ct_aply;
  configuration::contact ctct{new_configuration_contact("admin", true)};
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(*config);
  ct_aply.resolve_object(ctct);

  configuration::host hst{new_configuration_host("localhost", "admin")};
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::service svc{
      new_configuration_service("localhost", "check_icmp", "admin")};
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst);
  svc_aply.resolve_object(svc);
  data_point_fifo::update_fifo_limit(std::numeric_limits<time_t>::max(), 10);
}

void open_telemetry_test::TearDown() {
  deinit_config_state();
}

TEST_F(open_telemetry_test, data_available) {
  auto instance = ::open_telemetry::load("/tmp/otel_conf.json", g_io_context,
                                         spdlog::default_logger());

  instance->create_extractor(
      "--extractor=attributes "
      "--host_path=resource_metrics.scope_metrics.data.data_points.attributes."
      "host "
      "--service_path=resource_metrics.scope_metrics.data.data_points."
      "attributes.service",
      _host_serv_list);

  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();
  ::google::protobuf::util::JsonStringToMessage(telegraf_example,
                                                request.get());
  instance->on_metric(request);
  // data are now available
  commands::result res;
  nagios_macros macros;
  macros.host_ptr = host::hosts.begin()->second.get();
  macros.service_ptr = service::services.begin()->second.get();
  ASSERT_TRUE(instance->check("nagios_telegraf",
                              instance->create_check_result_builder_config(
                                  "--processor=nagios_telegraf"),
                              1, macros, 1, res,
                              [](const commands::result&) {}));
  ASSERT_EQ(res.command_id, 1);
  ASSERT_EQ(res.start_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.end_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.exit_code, 0);
  ASSERT_EQ(res.exit_status, com::centreon::process::normal);
  ASSERT_EQ(res.output,
            "OK|pl=0%;0:40;0:80;; rta=0.022ms;0:200;0:500;0; rtmax=0.071ms;;;; "
            "rtmin=0.008ms;;;;");
}

TEST_F(open_telemetry_test, timeout) {
  auto instance = ::open_telemetry::load("/tmp/otel_conf.json", g_io_context,
                                         spdlog::default_logger());

  instance->create_extractor(
      "--extractor=attributes "
      "--host_path=resource_metrics.scope_metrics.data.data_points.attributes."
      "host "
      "--service_path=resource_metrics.scope_metrics.data.data_points."
      "attributes.service",
      _host_serv_list);

  commands::result res;
  res.exit_status = com::centreon::process::normal;
  nagios_macros macros;
  macros.host_ptr = host::hosts.begin()->second.get();
  macros.service_ptr = service::services.begin()->second.get();
  std::condition_variable cv;
  std::mutex cv_m;
  ASSERT_FALSE(instance->check("nagios_telegraf",
                               instance->create_check_result_builder_config(
                                   "--processor=nagios_telegraf"),
                               1, macros, 1, res,
                               [&res, &cv](const commands::result& async_res) {
                                 res = async_res;
                                 cv.notify_one();
                               }));

  std::unique_lock l(cv_m);
  ASSERT_EQ(cv.wait_for(l, std::chrono::seconds(3)),
            std::cv_status::no_timeout);
  ASSERT_EQ(res.exit_status, com::centreon::process::timeout);
}

TEST_F(open_telemetry_test, wait_for_data) {
  auto instance = ::open_telemetry::load("/tmp/otel_conf.json", g_io_context,
                                         spdlog::default_logger());

  static const std::string otl_conf =
      "--processor=nagios_telegraf "
      "--extractor=attributes "
      "--host_path=resource_metrics.scope_metrics.data.data_points.attributes."
      "host "
      "--service_path=resource_metrics.scope_metrics.data.data_points."
      "attributes.service";

  instance->create_extractor(otl_conf, _host_serv_list);

  commands::result res;
  res.exit_status = com::centreon::process::normal;
  nagios_macros macros;
  macros.host_ptr = host::hosts.begin()->second.get();
  macros.service_ptr = service::services.begin()->second.get();
  std::mutex cv_m;
  std::condition_variable cv;
  bool data_available = instance->check(
      "nagios_telegraf", instance->create_check_result_builder_config(otl_conf),
      1, macros, 1, res, [&res, &cv](const commands::result& async_res) {
        res = async_res;
        cv.notify_one();
      });
  ASSERT_FALSE(data_available);

  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();
  ::google::protobuf::util::JsonStringToMessage(telegraf_example,
                                                request.get());
  std::thread t([instance, request]() { instance->on_metric(request); });

  std::unique_lock l(cv_m);
  ASSERT_EQ(cv.wait_for(l, std::chrono::seconds(1)),
            std::cv_status::no_timeout);
  ASSERT_EQ(res.command_id, 1);
  ASSERT_EQ(res.start_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.end_time.to_useconds(), 1707744430000000);
  ASSERT_EQ(res.exit_code, 0);
  ASSERT_EQ(res.exit_status, com::centreon::process::normal);
  ASSERT_EQ(res.output,
            "OK|pl=0%;0:40;0:80;; rta=0.022ms;0:200;0:500;0; rtmax=0.071ms;;;; "
            "rtmin=0.008ms;;;;");
  t.join();
}
