/**
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

#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/neb/internal.hh"
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
using log_v2 = com::centreon::common::log_v2::log_v2;

extern std::shared_ptr<asio::io_context> g_io_context;

class victoria_request_test : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  static void SetUpTestSuite() {
    file::disk_accessor::load(1000);
    io::protocols::load();
    io::events::load();
    io::events& e(io::events::instance());
    ::remove("/tmp/cache_test.request_test");
    cache::global_cache::load(g_io_context, "/tmp/cache_test.request_test");

    // Register events.

    e.register_event(make_type(io::storage, storage::de_pb_metric), "pb_metric",
                     &storage::pb_metric::operations);
    e.register_event(make_type(io::storage, storage::de_pb_status), "pb_status",
                     &storage::pb_status::operations);
  }
  void SetUp() override {
    _logger = log_v2::instance().get(log_v2::VICTORIA_METRICS);
    _logger->set_level(spdlog::level::debug);
    log_v2::instance().get(log_v2::CORE)->set_level(spdlog::level::trace);
  }
};

TEST_F(victoria_request_test, request_body_test) {
  auto obj = cache::global_cache::instance_ptr();
  auto tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(89);
  tg->mut_obj().set_name("tag89");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::HOSTCATEGORY);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(12);
  tg->mut_obj().set_name("tag12");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICECATEGORY);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(23);
  tg->mut_obj().set_name("tag23");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICECATEGORY);
  obj->write(tg);

  auto hst = std::make_shared<neb::pb_host>();
  hst->mut_obj().set_host_id(14);
  hst->mut_obj().set_name("my host");
  auto hst_tag_info = hst->mut_obj().add_tags();
  hst_tag_info->set_id(89);
  hst_tag_info->set_type(TagType::HOSTCATEGORY);
  obj->write(hst);
  auto host_custom_var = std::make_shared<neb::pb_custom_variable>();
  host_custom_var->mut_obj().set_host_id(14);
  host_custom_var->mut_obj().set_name("CRITICALITY_LEVEL");
  host_custom_var->mut_obj().set_value("1");
  obj->write(host_custom_var);

  auto srv = std::make_shared<neb::pb_service>();
  srv->mut_obj().set_host_id(14);
  srv->mut_obj().set_service_id(78);
  srv->mut_obj().set_description("my service ");
  auto srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(12);
  srv_tag_info->set_type(TagType::SERVICECATEGORY);
  obj->write(srv);
  srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(23);
  srv_tag_info->set_type(TagType::SERVICECATEGORY);
  obj->write(srv);

  auto serv_custom_var = std::make_shared<neb::pb_custom_variable>();
  serv_custom_var->mut_obj().set_host_id(14);
  serv_custom_var->mut_obj().set_service_id(78);
  serv_custom_var->mut_obj().set_name("CRITICALITY_LEVEL");
  serv_custom_var->mut_obj().set_value("3");
  obj->write(serv_custom_var);

  auto index_mapp = std::make_shared<storage::pb_index_mapping>();
  index_mapp->mut_obj().set_index_id(45);
  index_mapp->mut_obj().set_host_id(14);
  index_mapp->mut_obj().set_service_id(78);
  obj->write(index_mapp);

  auto metric_index = std::make_shared<storage::pb_metric_mapping>();
  metric_index->mut_obj().set_index_id(45);
  metric_index->mut_obj().set_metric_id(145);
  obj->write(metric_index);

  http_tsdb::line_protocol_query dummy;
  victoria_metrics::request req(boost::beast::http::verb::post, "localhost",
                                "/", _logger, 0, dummy, dummy, "toto");

  Metric metric;
  metric.set_metric_id(123);
  metric.set_value(1.5782);
  metric.set_time(1674715597);
  metric.set_host_id(14);
  metric.set_service_id(78);
  metric.set_name("metric àçxxx");
  metric.set_unit("metric unit");
  metric.set_min(0.456);
  metric.set_max(0.987);
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
  auto obj = cache::global_cache::instance_ptr();
  auto tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(89);
  tg->mut_obj().set_name("tag89");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::HOSTCATEGORY);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(189);
  tg->mut_obj().set_name("tag189");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::HOSTGROUP);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(12);
  tg->mut_obj().set_name("tag12");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICECATEGORY);
  obj->write(tg);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(23);
  tg->mut_obj().set_name("tag23");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICECATEGORY);
  obj->write(tg);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(112);
  tg->mut_obj().set_name("tag112");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICEGROUP);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(123);
  tg->mut_obj().set_name("tag123");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICEGROUP);
  obj->write(tg);

  auto hst = std::make_shared<neb::pb_host>();
  hst->mut_obj().set_host_id(14);
  hst->mut_obj().set_name("my host");
  hst->mut_obj().set_enabled(true);
  auto hst_tag_info = hst->mut_obj().add_tags();
  hst_tag_info->set_id(89);
  hst_tag_info->set_type(TagType::HOSTCATEGORY);
  hst_tag_info = hst->mut_obj().add_tags();
  hst_tag_info->set_id(189);
  hst_tag_info->set_type(TagType::HOSTGROUP);
  obj->write(hst);
  auto host_custom_var = std::make_shared<neb::pb_custom_variable>();
  host_custom_var->mut_obj().set_host_id(14);
  host_custom_var->mut_obj().set_name("CRITICALITY_LEVEL");
  host_custom_var->mut_obj().set_value("2");
  obj->write(host_custom_var);

  auto srv = std::make_shared<neb::pb_service>();
  srv->mut_obj().set_host_id(14);
  srv->mut_obj().set_service_id(78);
  srv->mut_obj().set_description("my service/tutu ");
  srv->mut_obj().set_enabled(true);
  auto srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(12);
  srv_tag_info->set_type(TagType::SERVICECATEGORY);
  obj->write(srv);
  srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(23);
  srv_tag_info->set_type(TagType::SERVICECATEGORY);
  obj->write(srv);
  srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(112);
  srv_tag_info->set_type(TagType::SERVICEGROUP);
  obj->write(srv);
  srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(123);
  srv_tag_info->set_type(TagType::SERVICEGROUP);
  obj->write(srv);
  auto serv_custom_var = std::make_shared<neb::pb_custom_variable>();
  serv_custom_var->mut_obj().set_host_id(14);
  serv_custom_var->mut_obj().set_service_id(78);
  serv_custom_var->mut_obj().set_name("CRITICALITY_LEVEL");
  serv_custom_var->mut_obj().set_value("3");
  obj->write(serv_custom_var);

  auto index_mapp = std::make_shared<storage::pb_index_mapping>();
  index_mapp->mut_obj().set_index_id(45);
  index_mapp->mut_obj().set_host_id(14);
  index_mapp->mut_obj().set_service_id(78);
  obj->write(index_mapp);

  auto metric_index = std::make_shared<storage::pb_metric_mapping>();
  metric_index->mut_obj().set_index_id(45);
  metric_index->mut_obj().set_metric_id(145);
  obj->write(metric_index);

  auto hg = std::make_shared<neb::pb_host_group>();
  hg->mut_obj().set_hostgroup_id(88);
  hg->mut_obj().set_name("host_ group 88");
  hg->mut_obj().set_enabled(true);
  obj->write(hg);
  hg = std::make_shared<neb::pb_host_group>();
  hg->mut_obj().set_hostgroup_id(89);
  hg->mut_obj().set_name("host_ group 89");
  hg->mut_obj().set_enabled(true);
  obj->write(hg);

  auto hgm = std::make_shared<neb::pb_host_group_member>();
  hgm->mut_obj().set_hostgroup_id(88);
  hgm->mut_obj().set_host_id(14);
  hgm->mut_obj().set_enabled(true);
  obj->write(hgm);
  hgm = std::make_shared<neb::pb_host_group_member>();
  hgm->mut_obj().set_hostgroup_id(89);
  hgm->mut_obj().set_host_id(14);
  hgm->mut_obj().set_enabled(true);
  obj->write(hgm);

  auto sgm = std::make_shared<neb::pb_service_group_member>();
  sgm->mut_obj().set_servicegroup_id(1278);
  sgm->mut_obj().set_host_id(14);
  sgm->mut_obj().set_service_id(78);
  sgm->mut_obj().set_enabled(true);
  obj->write(sgm);

  sgm = std::make_shared<neb::pb_service_group_member>();
  sgm->mut_obj().set_servicegroup_id(1279);
  sgm->mut_obj().set_host_id(14);
  sgm->mut_obj().set_service_id(78);
  sgm->mut_obj().set_enabled(true);
  obj->write(sgm);

  http_tsdb::line_protocol_query metric_columns(
      victoria_metrics::stream::allowed_macros,
      http_tsdb::factory::get_columns(
          victoria_metrics::factory::default_extra_metric_column),
      http_tsdb::line_protocol_query::data_type::status, _logger);

  http_tsdb::line_protocol_query status_columns(
      victoria_metrics::stream::allowed_macros,
      http_tsdb::factory::get_columns(
          victoria_metrics::factory::default_extra_status_column),
      http_tsdb::line_protocol_query::data_type::status, _logger);

  victoria_metrics::request req(boost::beast::http::verb::post, "localhost",
                                "/", _logger, 0, metric_columns, status_columns,
                                "toto");

  Metric metric;
  metric.set_metric_id(123);
  metric.set_value(1.5782);
  metric.set_time(1674715597);
  metric.set_host_id(14);
  metric.set_service_id(78);
  metric.set_name("metric name");
  metric.set_unit("metric unit");
  metric.set_min(0.456);
  metric.set_max(0.987);
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
  auto obj = cache::global_cache::instance_ptr();
  auto tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(89);
  tg->mut_obj().set_name("tag89");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::HOSTCATEGORY);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(189);
  tg->mut_obj().set_name("tag189");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::HOSTGROUP);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(12);
  tg->mut_obj().set_name("tag12");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICECATEGORY);
  obj->write(tg);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(23);
  tg->mut_obj().set_name("tag23");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICECATEGORY);
  obj->write(tg);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(112);
  tg->mut_obj().set_name("tag112");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICEGROUP);
  obj->write(tg);
  tg = std::make_shared<neb::pb_tag>();
  tg->mut_obj().set_id(123);
  tg->mut_obj().set_name("tag123");
  tg->mut_obj().set_action(Tag_Action::Tag_Action_ADD);
  tg->mut_obj().set_type(TagType::SERVICEGROUP);
  obj->write(tg);

  auto hst = std::make_shared<neb::pb_host>();
  hst->mut_obj().set_host_id(14);
  hst->mut_obj().set_name("my host");
  hst->mut_obj().set_enabled(true);
  auto hst_tag_info = hst->mut_obj().add_tags();
  hst_tag_info->set_id(89);
  hst_tag_info->set_type(TagType::HOSTCATEGORY);
  hst_tag_info = hst->mut_obj().add_tags();
  hst_tag_info->set_id(189);
  hst_tag_info->set_type(TagType::HOSTGROUP);
  obj->write(hst);
  auto host_custom_var = std::make_shared<neb::pb_custom_variable>();
  host_custom_var->mut_obj().set_host_id(14);
  host_custom_var->mut_obj().set_name("CRITICALITY_LEVEL");
  host_custom_var->mut_obj().set_value("2");
  obj->write(host_custom_var);

  auto srv = std::make_shared<neb::pb_service>();
  srv->mut_obj().set_host_id(14);
  srv->mut_obj().set_service_id(78);
  srv->mut_obj().set_description("my service/tutu ");
  srv->mut_obj().set_enabled(true);
  auto srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(12);
  srv_tag_info->set_type(TagType::SERVICECATEGORY);
  obj->write(srv);
  srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(23);
  srv_tag_info->set_type(TagType::SERVICECATEGORY);
  obj->write(srv);
  srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(112);
  srv_tag_info->set_type(TagType::SERVICEGROUP);
  obj->write(srv);
  srv_tag_info = srv->mut_obj().add_tags();
  srv_tag_info->set_id(123);
  srv_tag_info->set_type(TagType::SERVICEGROUP);
  obj->write(srv);
  auto serv_custom_var = std::make_shared<neb::pb_custom_variable>();
  serv_custom_var->mut_obj().set_host_id(14);
  serv_custom_var->mut_obj().set_service_id(78);
  serv_custom_var->mut_obj().set_name("CRITICALITY_LEVEL");
  serv_custom_var->mut_obj().set_value("3");
  obj->write(serv_custom_var);

  auto index_mapp = std::make_shared<storage::pb_index_mapping>();
  index_mapp->mut_obj().set_index_id(45);
  index_mapp->mut_obj().set_host_id(14);
  index_mapp->mut_obj().set_service_id(78);
  obj->write(index_mapp);

  auto metric_index = std::make_shared<storage::pb_metric_mapping>();
  metric_index->mut_obj().set_index_id(45);
  metric_index->mut_obj().set_metric_id(145);
  obj->write(metric_index);

  auto hg = std::make_shared<neb::pb_host_group>();
  hg->mut_obj().set_hostgroup_id(88);
  hg->mut_obj().set_name("host_ group 88");
  hg->mut_obj().set_enabled(true);
  obj->write(hg);
  hg = std::make_shared<neb::pb_host_group>();
  hg->mut_obj().set_hostgroup_id(89);
  hg->mut_obj().set_name("host_ group 89");
  hg->mut_obj().set_enabled(true);
  obj->write(hg);

  auto hgm = std::make_shared<neb::pb_host_group_member>();
  hgm->mut_obj().set_hostgroup_id(88);
  hgm->mut_obj().set_host_id(14);
  hgm->mut_obj().set_enabled(true);
  obj->write(hgm);
  hgm = std::make_shared<neb::pb_host_group_member>();
  hgm->mut_obj().set_hostgroup_id(89);
  hgm->mut_obj().set_host_id(14);
  hgm->mut_obj().set_enabled(true);
  obj->write(hgm);

  auto sgm = std::make_shared<neb::pb_service_group_member>();
  sgm->mut_obj().set_servicegroup_id(1278);
  sgm->mut_obj().set_host_id(14);
  sgm->mut_obj().set_service_id(78);
  sgm->mut_obj().set_enabled(true);
  obj->write(sgm);

  sgm = std::make_shared<neb::pb_service_group_member>();
  sgm->mut_obj().set_servicegroup_id(1279);
  sgm->mut_obj().set_host_id(14);
  sgm->mut_obj().set_service_id(78);
  sgm->mut_obj().set_enabled(true);
  obj->write(sgm);

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
      http_tsdb::line_protocol_query::data_type::status, _logger);

  http_tsdb::line_protocol_query status_columns(
      victoria_metrics::stream::allowed_macros,
      http_tsdb::factory::get_columns(column),
      http_tsdb::line_protocol_query::data_type::status, _logger);

  victoria_metrics::request req(boost::beast::http::verb::post, "localhost",
                                "/", _logger, 0, metric_columns, status_columns,
                                "toto");

  Metric metric;
  metric.set_metric_id(123);
  metric.set_value(1.5782);
  metric.set_time(1674715597);
  metric.set_host_id(14);
  metric.set_service_id(78);
  metric.set_name("metric name");
  metric.set_unit("metric unit");
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
