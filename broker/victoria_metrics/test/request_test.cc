/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/container/flat_set.hpp>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "com/centreon/broker/namespace.hh"

#include "bbdo/tag.pb.h"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/victoria_metrics/factory.hh"
#include "com/centreon/broker/victoria_metrics/request.hh"
#include "com/centreon/broker/victoria_metrics/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::http_tsdb;
;
using namespace nlohmann;
using log_v3 = com::centreon::common::log_v3::log_v3;

class victoria_request_test : public ::testing::Test {
 protected:
  uint32_t _logger_id;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  static void SetUpTestSuite() {
    file::disk_accessor::load(1000);
    io::protocols::load();
    io::events::load();
    io::events& e(io::events::instance());
    ::remove("/tmp/cache_test.request_test");
    cache::global_cache::load("/tmp/cache_test.request_test");

    // Register events.

    e.register_event(make_type(io::storage, storage::de_pb_metric), "pb_metric",
                     &storage::pb_metric::operations);
    e.register_event(make_type(io::storage, storage::de_pb_status), "pb_status",
                     &storage::pb_status::operations);
  }
  void SetUp() override {
    _logger_id = log_v3::instance().create_logger_or_get_id("victoria_metrics");
    _logger = log_v3::instance().get(_logger_id);
    _logger->set_level(spdlog::level::trace);
  }
};

TEST_F(victoria_request_test, request_body_test) {
  cache::global_cache::instance_ptr()->set_metric_info(
      123, 45, "metric àçxxx", "metric unit", 0.456, 0.987);
  com::centreon::broker::TagInfo host_tags[1];
  host_tags[0].set_id(89);
  cache::global_cache::instance_ptr()->add_tag(89, "tag89",
                                               TagType::HOSTCATEGORY, 5);
  const com::centreon::broker::TagInfo* tag_iter = host_tags;
  cache::global_cache::instance_ptr()->store_host(14, "my host", 1, 2);
  cache::global_cache::instance_ptr()->set_host_tag(14, [&]() -> uint64_t {
    return tag_iter > host_tags ? 0 : (tag_iter++)->id();
  });
  com::centreon::broker::TagInfo tags[2];
  tags[0].set_id(12);
  tags[1].set_id(23);
  cache::global_cache::instance_ptr()->add_tag(12, "tag12",
                                               TagType::SERVICECATEGORY, 5);
  cache::global_cache::instance_ptr()->add_tag(23, "tag23",
                                               TagType::SERVICECATEGORY, 5);
  tag_iter = tags;
  cache::global_cache::instance_ptr()->store_service(14, 78, "my service ", 2,
                                                     3);
  cache::global_cache::instance_ptr()->set_serv_tag(14, 78, [&]() -> uint64_t {
    return tag_iter >= tags + 2 ? 0 : (tag_iter++)->id();
  });
  cache::global_cache::instance_ptr()->set_index_mapping(45, 14, 78);

  http_tsdb::line_protocol_query dummy;
  victoria_metrics::request req(boost::beast::http::verb::post, "localhost",
                                "/", 0, dummy, dummy, "toto");

  Metric metric;
  metric.set_metric_id(123);
  metric.set_value(1.5782);
  metric.set_time(1674715597);
  metric.set_host_id(14);
  metric.set_service_id(78);
  metric.set_name("metric àçxxx");
  req.add_metric(metric);

  Status status;
  status.set_index_id(45);
  status.set_state(1);
  status.set_time(1674715598);
  status.set_host_id(14);
  status.set_service_id(78);
  req.add_status(status);
  ASSERT_EQ(
      "metric,id=123,name=metric\\ "
      "\xC3\xA0\xC3\xA7xxx,host_id=14,serv_id=78,unit=metric\\ unit,"
      "severity_id=3 val=1.5782 1674715597\n"
      "status,id=45,host_id=14,serv_id=78,severity_id=3 val=75 1674715598\n",
      req.body());
}

TEST_F(victoria_request_test, request_body_test_default_victoria_extra_column) {
  cache::global_cache::instance_ptr()->set_metric_info(
      123, 45, "metric name", "metric unit", 0.456, 0.987);
  com::centreon::broker::TagInfo host_tags[2];
  host_tags[0].set_id(89);
  host_tags[1].set_id(189);
  cache::global_cache::instance_ptr()->add_tag(89, "tag89",
                                               TagType::HOSTCATEGORY, 5);
  cache::global_cache::instance_ptr()->add_tag(189, "tag189",
                                               TagType::HOSTGROUP, 5);
  const com::centreon::broker::TagInfo* tag_iter = host_tags;
  cache::global_cache::instance_ptr()->store_host(14, "my host", 1, 2);
  cache::global_cache::instance_ptr()->set_host_tag(14, [&]() -> uint64_t {
    return tag_iter > host_tags + 1 ? 0 : (tag_iter++)->id();
  });
  com::centreon::broker::TagInfo tags[4];
  tags[0].set_id(12);
  tags[1].set_id(23);
  tags[2].set_id(112);
  tags[3].set_id(123);
  cache::global_cache::instance_ptr()->add_tag(12, "tag12",
                                               TagType::SERVICECATEGORY, 5);
  cache::global_cache::instance_ptr()->add_tag(23, "tag23",
                                               TagType::SERVICECATEGORY, 5);
  cache::global_cache::instance_ptr()->add_tag(112, "tag112",
                                               TagType::SERVICEGROUP, 5);
  cache::global_cache::instance_ptr()->add_tag(123, "tag123",
                                               TagType::SERVICEGROUP, 5);
  tag_iter = tags;
  cache::global_cache::instance_ptr()->store_service(14, 78, "my service/tutu ",
                                                     2, 3);
  cache::global_cache::instance_ptr()->set_serv_tag(14, 78, [&]() -> uint64_t {
    return tag_iter >= tags + 4 ? 0 : (tag_iter++)->id();
  });
  cache::global_cache::instance_ptr()->set_index_mapping(45, 14, 78);
  cache::global_cache::instance_ptr()->add_host_group(89, 14);
  cache::global_cache::instance_ptr()->add_host_group(88, 14);
  cache::global_cache::instance_ptr()->add_service_group(1278, 14, 78);
  cache::global_cache::instance_ptr()->add_service_group(1279, 14, 78);

  http_tsdb::line_protocol_query metric_columns(
      victoria_metrics::stream::allowed_macros,
      http_tsdb::factory::get_columns(
          victoria_metrics::factory::default_extra_metric_column),
      http_tsdb::line_protocol_query::data_type::status,
      _logger);

  http_tsdb::line_protocol_query status_columns(
      victoria_metrics::stream::allowed_macros,
      http_tsdb::factory::get_columns(
          victoria_metrics::factory::default_extra_status_column),
      http_tsdb::line_protocol_query::data_type::status,
      _logger);

  victoria_metrics::request req(boost::beast::http::verb::post, "localhost",
                                "/", 0, metric_columns, status_columns, "toto");

  Metric metric;
  metric.set_metric_id(123);
  metric.set_value(1.5782);
  metric.set_time(1674715597);
  metric.set_host_id(14);
  metric.set_service_id(78);
  metric.set_name("metric name");
  req.add_metric(metric);

  Status status;
  status.set_index_id(45);
  status.set_state(1);
  status.set_time(1674715598);
  status.set_host_id(14);
  status.set_service_id(78);
  req.add_status(status);
  ASSERT_EQ(
      "metric,id=123,name=metric\\ name,host_id=14,serv_id=78,unit=metric\\ "
      "unit,severity_id=3,"
      "host=my\\ host,serv=my\\ service/tutu\\ ,min=0.456,max=0.987,"
      "host_grp=88\\,89,serv_grp=1278\\,1279,host_tag_cat=tag89,host_tag_grp="
      "tag189,serv_tag_cat=tag12\\,tag23,serv_tag_grp=tag112\\,tag123 "
      "val=1.5782 1674715597\n"
      "status,id=45,host_id=14,serv_id=78,severity_id=3,"
      "host=my\\ host,serv=my\\ service/tutu\\ ,"
      "host_grp=88\\,89,serv_grp=1278\\,1279,host_tag_cat=tag89,host_tag_grp="
      "tag189,serv_tag_cat=tag12\\,tag23,serv_tag_grp=tag112\\,tag123 val=75 "
      "1674715598\n",
      req.body());
}

TEST_F(victoria_request_test, request_body_test_victoria_extra_column) {
  cache::global_cache::instance_ptr()->set_metric_info(
      123, 45, "metric name", "metric unit", 0.456, 0.987);
  com::centreon::broker::TagInfo host_tags[2];
  host_tags[0].set_id(89);
  host_tags[1].set_id(189);
  cache::global_cache::instance_ptr()->add_tag(89, "tag89",
                                               TagType::HOSTCATEGORY, 5);
  cache::global_cache::instance_ptr()->add_tag(189, "tag189",
                                               TagType::HOSTGROUP, 5);
  const com::centreon::broker::TagInfo* tag_iter = host_tags;
  cache::global_cache::instance_ptr()->store_host(14, "my host", 1, 2);
  cache::global_cache::instance_ptr()->set_host_tag(14, [&]() -> uint64_t {
    return tag_iter > host_tags + 1 ? 0 : (tag_iter++)->id();
  });
  com::centreon::broker::TagInfo tags[4];
  tags[0].set_id(12);
  tags[1].set_id(23);
  tags[2].set_id(112);
  tags[3].set_id(123);
  cache::global_cache::instance_ptr()->add_tag(12, "tag12",
                                               TagType::SERVICECATEGORY, 5);
  cache::global_cache::instance_ptr()->add_tag(23, "tag23",
                                               TagType::SERVICECATEGORY, 5);
  cache::global_cache::instance_ptr()->add_tag(112, "tag112",
                                               TagType::SERVICEGROUP, 5);
  cache::global_cache::instance_ptr()->add_tag(123, "tag123",
                                               TagType::SERVICEGROUP, 5);
  tag_iter = tags;
  cache::global_cache::instance_ptr()->store_service(14, 78, "my service/tutu ",
                                                     2, 3);
  cache::global_cache::instance_ptr()->set_serv_tag(14, 78, [&]() -> uint64_t {
    return tag_iter >= tags + 4 ? 0 : (tag_iter++)->id();
  });
  cache::global_cache::instance_ptr()->set_index_mapping(45, 14, 78);
  cache::global_cache::instance_ptr()->add_host_group(89, 14);
  cache::global_cache::instance_ptr()->add_host_group(88, 14);
  cache::global_cache::instance_ptr()->add_service_group(1278, 14, 78);
  cache::global_cache::instance_ptr()->add_service_group(1279, 14, 78);

  json column = R"([
    {"name" : "host", "is_tag" : "true", "value" : "$HOST$", "type":"string"},
    {"name" : "serv", "is_tag" : "true", "value" : "$SERVICE$", "type":"string"},
    {"name" : "host_grp", "is_tag" : "true", "value" : "$HOSTGROUP$", "type":"string"},
    {"name" : "serv_grp", "is_tag" : "true", "value" : "$SERVICE_GROUP$", "type":"string"},
    {"name" : "host_tag_cat_id", "is_tag" : "true", "value" : "$HOST_TAG_CAT_ID$", "type":"string"},
    {"name" : "host_tag_grp_id", "is_tag" : "true", "value" : "$HOST_TAG_GROUP_ID$", "type":"string"},
    {"name" : "serv_tag_cat_id", "is_tag" : "true", "value" : "$SERV_TAG_CAT_ID$", "type":"string"},
    {"name" : "serv_tag_grp_id", "is_tag" : "true", "value" : "$SERV_TAG_GROUP_ID$", "type":"string"},
    {"name" : "host_tag_cat", "is_tag" : "true", "value" : "$HOST_TAG_CAT_NAME$", "type":"string"},
    {"name" : "host_tag_grp", "is_tag" : "true", "value" : "$HOST_TAG_GROUP_NAME$", "type":"string"},
    {"name" : "serv_tag_cat", "is_tag" : "true", "value" : "$SERV_TAG_CAT_NAME$", "type":"string"},
    {"name" : "serv_tag_grp", "is_tag" : "true", "value" : "$SERV_TAG_GROUP_NAME$", "type":"string"}])"_json;

  http_tsdb::line_protocol_query metric_columns(
      victoria_metrics::stream::allowed_macros,
      http_tsdb::factory::get_columns(column),
      http_tsdb::line_protocol_query::data_type::status,
      _logger);

  http_tsdb::line_protocol_query status_columns(
      victoria_metrics::stream::allowed_macros,
      http_tsdb::factory::get_columns(column),
      http_tsdb::line_protocol_query::data_type::status,
      _logger);

  victoria_metrics::request req(boost::beast::http::verb::post, "localhost",
                                "/", 0, metric_columns, status_columns, "toto");

  Metric metric;
  metric.set_metric_id(123);
  metric.set_value(1.5782);
  metric.set_time(1674715597);
  metric.set_host_id(14);
  metric.set_service_id(78);
  metric.set_name("metric name");
  req.add_metric(metric);

  Status status;
  status.set_index_id(45);
  status.set_state(1);
  status.set_time(1674715598);
  status.set_host_id(14);
  status.set_service_id(78);
  req.add_status(status);
  ASSERT_EQ(
      "metric,id=123,name=metric\\ name,host_id=14,serv_id=78,unit=metric\\ "
      "unit,severity_id=3,"
      "host=my\\ host,serv=my\\ service/tutu\\ ,"
      "host_grp=88\\,89,serv_grp=1278\\,1279,"
      "host_tag_cat_id=89,host_tag_grp_id="
      "189,serv_tag_cat_id=12\\,23,serv_tag_grp_id=112\\,123,"
      "host_tag_cat=tag89,host_tag_grp="
      "tag189,serv_tag_cat=tag12\\,tag23,serv_tag_grp=tag112\\,tag123 "
      "val=1.5782 1674715597\n"
      "status,id=45,host_id=14,serv_id=78,severity_id=3,"
      "host=my\\ host,serv=my\\ service/tutu\\ ,"
      "host_grp=88\\,89,serv_grp=1278\\,1279,"
      "host_tag_cat_id=89,host_tag_grp_id="
      "189,serv_tag_cat_id=12\\,23,serv_tag_grp_id=112\\,123,"
      "host_tag_cat=tag89,host_tag_grp="
      "tag189,serv_tag_cat=tag12\\,tag23,serv_tag_grp=tag112\\,tag123 val=75 "
      "1674715598\n",
      req.body());
}
