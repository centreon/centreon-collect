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

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/engine/modules/opentelemetry/host_serv_extractor.hh"

using namespace com::centreon::engine::modules::opentelemetry;
using namespace com::centreon::engine;

TEST(otl_host_serv_extractor_test, empty_request) {
  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();

  otl_data_point::extract_data_points(
      request, [](const otl_data_point& data_pt [[maybe_unused]]) {
        ASSERT_TRUE(false);
      });
}

class otl_host_serv_attributes_extractor_test : public ::testing::Test {
 public:
  const std::string _conf3 =
      "--extractor=attributes "
      "--host_path=resource_metrics.resource.attributes.host "
      "--service_path=resource_metrics.resource.attributes.service";
  const std::string _conf4 =
      "--extractor=attributes "
      "--host_path=resource_metrics.scope_metrics.scope.attributes.host "
      "--service_path=resource_metrics.scope_metrics.scope.attributes.service";
  const std::string _conf5 =
      "--extractor=attributes "
      "--host_path=resource_metrics.scope_metrics.data.data_points.attributes."
      "host "
      "--service_path=resource_metrics.scope_metrics.data.data_points."
      "attributes.service";
  const std::string _conf6 =
      "--extractor=attributes "
      "--host_path=resource_metrics.scope_metrics.data.data_points.attributes."
      "bad_host "
      "--service_path=resource_metrics.scope_metrics.data.data_points."
      "attributes.service";
};

TEST_F(otl_host_serv_attributes_extractor_test, resource_attrib) {
  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();

  auto resources = request->add_resource_metrics();
  auto host = resources->mutable_resource()->mutable_attributes()->Add();
  host->set_key("host");
  host->mutable_value()->set_string_value("my_host");
  auto host2 = resources->mutable_resource()->mutable_attributes()->Add();
  host2->set_key("host");
  host2->mutable_value()->set_string_value("my_host2");
  auto metric = resources->add_scope_metrics()->add_metrics();
  metric->set_name("metric cpu");
  metric->set_description("metric to send");
  metric->set_unit("%");
  auto gauge = metric->mutable_gauge();
  auto point = gauge->add_data_points();
  point->set_time_unix_nano(time(nullptr));

  unsigned data_point_extracted_cpt = 0;

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf3, host_srv_list);
        host_srv_list->register_host_serv("my_host2", "");
        host_srv_list->register_host_serv("my_host2", "my_serv2");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_EQ(to_test.host, "my_host2");
        ASSERT_EQ(to_test.service, "");
        ASSERT_EQ(to_test.metric, "metric cpu");
      });

  ASSERT_EQ(data_point_extracted_cpt, 1);

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf4, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_TRUE(to_test.host.empty());
      });

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf6, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_TRUE(to_test.host.empty());
      });

  auto serv = resources->mutable_resource()->mutable_attributes()->Add();
  serv->set_key("service");
  serv->mutable_value()->set_string_value("my_serv");
  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf3, host_srv_list);
        host_srv_list->register_host_serv("my_host", "my_serv");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_EQ(to_test.host, "my_host");
        ASSERT_EQ(to_test.service, "my_serv");
        ASSERT_EQ(to_test.metric, "metric cpu");
      });

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf3, host_srv_list);
        host_srv_list->register_host_serv("my_host2", "my_serv");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_EQ(to_test.host, "my_host2");
        ASSERT_EQ(to_test.service, "my_serv");
        ASSERT_EQ(to_test.metric, "metric cpu");
      });
  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf3, host_srv_list);
        host_srv_list->register_host_serv("my_host3", "my_serv");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_TRUE(to_test.host.empty());
      });

  auto serv2 = resources->mutable_resource()->mutable_attributes()->Add();
  serv2->set_key("service");
  serv2->mutable_value()->set_string_value("my_serv2");
  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf3, host_srv_list);
        host_srv_list->register_host_serv("my_host2", "");
        host_srv_list->register_host_serv("my_host2", "my_serv2");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_EQ(to_test.host, "my_host2");
        ASSERT_EQ(to_test.service, "my_serv2");
        ASSERT_EQ(to_test.metric, "metric cpu");
      });
}

TEST_F(otl_host_serv_attributes_extractor_test, scope_attrib) {
  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();

  auto resources = request->add_resource_metrics();
  auto scope = resources->add_scope_metrics();
  auto host = scope->mutable_scope()->mutable_attributes()->Add();
  host->set_key("host");
  host->mutable_value()->set_string_value("my_host");
  auto burk_host = scope->mutable_scope()->mutable_attributes()->Add();
  burk_host->set_key("host");
  burk_host->mutable_value()->set_string_value("bad_host");
  auto metric = scope->add_metrics();
  metric->set_name("metric cpu");
  metric->set_description("metric to send");
  metric->set_unit("%");
  auto gauge = metric->mutable_gauge();
  auto point = gauge->add_data_points();
  point->set_time_unix_nano(time(nullptr));

  unsigned data_point_extracted_cpt = 0;

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf4, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);

        ASSERT_EQ(to_test.host, "my_host");
        ASSERT_EQ(to_test.service, "");
        ASSERT_EQ(to_test.metric, "metric cpu");
      });

  ASSERT_EQ(data_point_extracted_cpt, 1);

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf3, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_TRUE(to_test.host.empty());
      });

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf5, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_TRUE(to_test.host.empty());
      });

  auto serv = scope->mutable_scope()->mutable_attributes()->Add();
  serv->set_key("service");
  serv->mutable_value()->set_string_value("my_serv");
  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf4, host_srv_list);
        host_srv_list->register_host_serv("my_host", "my_serv");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_EQ(to_test.host, "my_host");
        ASSERT_EQ(to_test.service, "my_serv");
        ASSERT_EQ(to_test.metric, "metric cpu");
      });
}

TEST_F(otl_host_serv_attributes_extractor_test, data_point_attrib) {
  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();

  auto resources = request->add_resource_metrics();
  auto scope = resources->add_scope_metrics();
  auto metric = scope->add_metrics();
  metric->set_name("metric cpu");
  metric->set_description("metric to send");
  metric->set_unit("%");
  auto gauge = metric->mutable_gauge();
  auto point = gauge->add_data_points();
  point->set_time_unix_nano(time(nullptr));
  auto host = point->mutable_attributes()->Add();
  host->set_key("host");
  host->mutable_value()->set_string_value("my_host");

  unsigned data_point_extracted_cpt = 0;

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf5, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_EQ(to_test.host, "my_host");
        ASSERT_EQ(to_test.service, "");
        ASSERT_EQ(to_test.metric, "metric cpu");
      });

  ASSERT_EQ(data_point_extracted_cpt, 1);

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf3, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_TRUE(to_test.host.empty());
      });

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf4, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_TRUE(to_test.host.empty());
      });

  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf6, host_srv_list);
        host_srv_list->register_host_serv("my_host", "");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_TRUE(to_test.host.empty());
      });

  auto serv = point->mutable_attributes()->Add();
  serv->set_key("service");
  serv->mutable_value()->set_string_value("my_serv");
  otl_data_point::extract_data_points(
      request,
      [this, &data_point_extracted_cpt](const otl_data_point& data_pt) {
        ++data_point_extracted_cpt;
        commands::otel::host_serv_list::pointer host_srv_list =
            std::make_shared<commands::otel::host_serv_list>();
        auto extractor = host_serv_extractor::create(_conf5, host_srv_list);
        host_srv_list->register_host_serv("my_host", "my_serv");
        host_serv_metric to_test = extractor->extract_host_serv_metric(data_pt);
        ASSERT_EQ(to_test.host, "my_host");
        ASSERT_EQ(to_test.service, "my_serv");
        ASSERT_EQ(to_test.metric, "metric cpu");
      });
}
