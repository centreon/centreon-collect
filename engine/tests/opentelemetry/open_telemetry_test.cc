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
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/engine/commands/otel_connector.hh"
#include "com/centreon/engine/modules/opentelemetry/open_telemetry.hh"

#include "helper.hh"
#include "test_engine.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::engine;

extern const char* telegraf_example;

extern std::shared_ptr<asio::io_context> g_io_context;

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
  configuration::error_cnt err;
  init_config_state();
  pb_config.mutable_contacts()->Clear();
  configuration::applier::contact ct_aply;
  configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
  ct_aply.add_object(ctct);
  ct_aply.expand_objects(pb_config);
  ct_aply.resolve_object(ctct, err);

  configuration::Host hst{new_pb_configuration_host("localhost", "admin")};
  configuration::applier::host hst_aply;
  hst_aply.add_object(hst);

  configuration::Service svc{
      new_pb_configuration_service("localhost", "check_icmp", "admin")};
  configuration::applier::service svc_aply;
  svc_aply.add_object(svc);

  hst_aply.resolve_object(hst, err);
  svc_aply.resolve_object(svc, err);
}

void open_telemetry_test::TearDown() {
  deinit_config_state();
}

TEST_F(open_telemetry_test, data_available) {
  auto instance = open_telemetry::load("/tmp/otel_conf.json", g_io_context,
                                       spdlog::default_logger());

  std::shared_ptr<commands::otel_connector> conn =
      commands::otel_connector::create(
          "otel_conn",
          "--processor=nagios_telegraf --extractor=attributes "
          "--host_path=resource_metrics.scope_metrics.data.data_points."
          "attributes."
          "host "
          "--service_path=resource_metrics.scope_metrics.data.data_points."
          "attributes.service",
          nullptr);
  conn->register_host_serv("localhost", "check_icmp");

  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();
  ::google::protobuf::util::JsonStringToMessage(telegraf_example,
                                                request.get());
  instance->on_metric(request);
  command_manager::instance().execute();

  bool checked = false;
  checks::checker::instance().inspect_reap_partial(
      [&checked](const std::deque<check_result::pointer>& queue) {
        ASSERT_FALSE(queue.empty());
        check_result::pointer res = *queue.rbegin();
        ASSERT_EQ(res->get_start_time().tv_sec, 1707744430);
        ASSERT_EQ(res->get_finish_time().tv_sec, 1707744430);
        ASSERT_TRUE(res->get_exited_ok());
        ASSERT_EQ(res->get_return_code(), 0);
        ASSERT_EQ(
            res->get_output(),
            "OK|pl=0%;0:40;0:80;; rta=0.022ms;0:200;0:500;0; rtmax=0.071ms;;;; "
            "rtmin=0.008ms;;;;");
        checked = true;
      });

  ASSERT_TRUE(checked);
}
