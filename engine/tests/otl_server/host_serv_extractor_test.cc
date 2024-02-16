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

#include "com/centreon/engine/modules/otl_server/host_serv_extractor.hh"

using namespace com::centreon::engine::modules::otl_server;

TEST(otl_host_serv_extractor_test, empty_request) {
  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();

  data_point::extract_data_points(
      request, [](const data_point& data_pt) { ASSERT_TRUE(false); });
}

class otl_host_serv_attributes_extractor_test : public ::testing::Test {
 public:
  const std::string _conf1;

  const std::string _conf3 =
      "attributes --host_attribute=resource --service_attribute=resource";
  const std::string _conf4 =
      "attributes --host_attribute=scope --service_attribute=scope";
  const std::string _conf5 =
      "attributes --host_attribute=data_point --service_attribute=data_point";
  const std::string _conf6 =
      "attributes --host_attribute=data_point --host_tag=bad_host "
      "--service_attribute=data_point";
};

TEST_F(otl_host_serv_attributes_extractor_test, resource_attrib) {
  metric_request_ptr request =
      std::make_shared<::opentelemetry::proto::collector::metrics::v1::
                           ExportMetricsServiceRequest>();

  auto resources = request->add_resource_metrics();
  auto host = resources->mutable_resource()->mutable_attributes()->Add();
  host->set_key("host");
  host->mutable_value()->set_string_value("my_host");
  auto metric = resources->add_scope_metrics()->add_metrics();
  metric->set_name("metric cpu");
  metric->set_description("metric to send");
  metric->set_unit("%");
  auto gauge = metric->mutable_gauge();
  auto point = gauge->add_data_points();
  point->set_time_unix_nano(time(nullptr));

  unsigned data_point_extracted_cpt = 0;

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf3)->extract_host_serv_metric(data_pt);
    ASSERT_EQ(to_test.host, "my_host");
    ASSERT_EQ(to_test.service, "");
    ASSERT_EQ(to_test.metric, "metric cpu");
  });

  ASSERT_EQ(data_point_extracted_cpt, 1);

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf1)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf4)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf6)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  auto serv = resources->mutable_resource()->mutable_attributes()->Add();
  serv->set_key("service");
  serv->mutable_value()->set_string_value("my_serv");
  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf3)->extract_host_serv_metric(data_pt);
    ASSERT_EQ(to_test.host, "my_host");
    ASSERT_EQ(to_test.service, "my_serv");
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
  auto metric = scope->add_metrics();
  metric->set_name("metric cpu");
  metric->set_description("metric to send");
  metric->set_unit("%");
  auto gauge = metric->mutable_gauge();
  auto point = gauge->add_data_points();
  point->set_time_unix_nano(time(nullptr));

  unsigned data_point_extracted_cpt = 0;

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf4)->extract_host_serv_metric(data_pt);
    ASSERT_EQ(to_test.host, "my_host");
    ASSERT_EQ(to_test.service, "");
    ASSERT_EQ(to_test.metric, "metric cpu");
  });

  ASSERT_EQ(data_point_extracted_cpt, 1);

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf1)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf3)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf5)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  auto serv = scope->mutable_scope()->mutable_attributes()->Add();
  serv->set_key("service");
  serv->mutable_value()->set_string_value("my_serv");
  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf4)->extract_host_serv_metric(data_pt);
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

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf1)->extract_host_serv_metric(data_pt);
    ASSERT_EQ(to_test.host, "my_host");
    ASSERT_EQ(to_test.service, "");
    ASSERT_EQ(to_test.metric, "metric cpu");
  });

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf5)->extract_host_serv_metric(data_pt);
    ASSERT_EQ(to_test.host, "my_host");
    ASSERT_EQ(to_test.service, "");
    ASSERT_EQ(to_test.metric, "metric cpu");
  });

  ASSERT_EQ(data_point_extracted_cpt, 2);

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf3)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf4)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf6)->extract_host_serv_metric(data_pt);
    ASSERT_TRUE(to_test.host.empty());
  });

  auto serv = point->mutable_attributes()->Add();
  serv->set_key("service");
  serv->mutable_value()->set_string_value("my_serv");
  data_point::extract_data_points(request, [this, &data_point_extracted_cpt](
                                               const data_point& data_pt) {
    ++data_point_extracted_cpt;
    host_serv_metric to_test =
        host_serv_extractor::create(_conf1)->extract_host_serv_metric(data_pt);
    ASSERT_EQ(to_test.host, "my_host");
    ASSERT_EQ(to_test.service, "my_serv");
    ASSERT_EQ(to_test.metric, "metric cpu");
  });
}
