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

#include "broker/core/config/applier/broker_state.hh"
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
using log_v2 = com::centreon::common::log_v2::log_v2;

class victoria_request_test : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  static void SetUpTestSuite() {
    file::disk_accessor::load(1000);
    io::protocols::load();
    io::events::load();
    io::events& e(io::events::instance());
    config::applier::state::load<config::applier::broker_state>("unittest");
    config::applier::state::instance().initialize_cache(
        log_v2::instance().get(log_v2::CORE));
    config::applier::state::instance().cache().enable_section(
        cache::broker_cache::CACHE_ALL);

    // Register events.

    e.register_event(make_type(io::storage, storage::de_pb_metric), "pb_metric",
                     &storage::pb_metric::operations);
    e.register_event(make_type(io::storage, storage::de_pb_status), "pb_status",
                     &storage::pb_status::operations);
  }

  static void TearDownTestSuite() { config::applier::state::unload(); }
  void SetUp() override {
    _logger = log_v2::instance().get(log_v2::VICTORIA_METRICS);
    _logger->set_level(spdlog::level::debug);
  }
};

TEST_F(victoria_request_test, request_body_test) {
  auto& bc = config::applier::state::instance().cache();

  auto h = std::make_shared<neb::pb_host>();
  h->mut_obj().set_host_id(14);
  h->mut_obj().set_name("my host");
  h->mut_obj().set_enabled(true);
  bc.update_host(h);

  auto s = std::make_shared<neb::pb_service>();
  s->mut_obj().set_host_id(14);
  s->mut_obj().set_service_id(78);
  s->mut_obj().set_description("my service ");
  s->mut_obj().set_enabled(true);
  s->mut_obj().set_severity_id(42);
  bc.update_service(s);
  bc.set_db_id_for_severity(42, 0, 3);

  auto mm = std::make_shared<storage::pb_metric_mapping>();
  mm->mut_obj().set_metric_id(123);
  mm->mut_obj().set_index_id(45);
  mm->mut_obj().set_uom("metric unit");
  bc.update_metric_mapping(mm);

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
  {
    auto& bc = config::applier::state::instance().cache();

    auto h = std::make_shared<neb::pb_host>();
    h->mut_obj().set_host_id(14);
    h->mut_obj().set_name("my host");
    h->mut_obj().set_enabled(true);
    for (auto [id, type] : std::initializer_list<std::pair<uint64_t, TagType>>{
             {89, TagType::HOSTCATEGORY}, {189, TagType::HOSTGROUP}}) {
      auto* t = h->mut_obj().add_tags();
      t->set_id(id);
      t->set_type(type);
    }
    bc.update_host(h);

    auto s = std::make_shared<neb::pb_service>();
    s->mut_obj().set_host_id(14);
    s->mut_obj().set_service_id(78);
    s->mut_obj().set_description("my service/tutu ");
    s->mut_obj().set_enabled(true);
    s->mut_obj().set_severity_id(42);
    for (auto [id, type] : std::initializer_list<std::pair<uint64_t, TagType>>{
             {12, TagType::SERVICECATEGORY},
             {23, TagType::SERVICECATEGORY},
             {112, TagType::SERVICEGROUP},
             {123, TagType::SERVICEGROUP}}) {
      auto* t = s->mut_obj().add_tags();
      t->set_id(id);
      t->set_type(type);
    }
    bc.update_service(s);
    bc.set_db_id_for_severity(42, 0, 3);

    for (auto [id, name, type] :
         std::initializer_list<std::tuple<uint64_t, const char*, TagType>>{
             {89, "tag89", TagType::HOSTCATEGORY},
             {189, "tag189", TagType::HOSTGROUP},
             {12, "tag12", TagType::SERVICECATEGORY},
             {23, "tag23", TagType::SERVICECATEGORY},
             {112, "tag112", TagType::SERVICEGROUP},
             {123, "tag123", TagType::SERVICEGROUP}}) {
      auto tag = std::make_shared<neb::pb_tag>();
      tag->mut_obj().set_id(id);
      tag->mut_obj().set_name(name);
      tag->mut_obj().set_type(type);
      tag->mut_obj().set_action(Tag_Action_ADD);
      bc.update_tag(tag);
    }

    for (auto [hg_id, poller_id] :
         std::initializer_list<std::pair<uint64_t, uint64_t>>{{89, 1},
                                                              {88, 2}}) {
      auto hgm = std::make_shared<neb::pb_host_group_member>();
      hgm->mut_obj().set_hostgroup_id(hg_id);
      hgm->mut_obj().set_name(std::to_string(hg_id));
      hgm->mut_obj().set_host_id(14);
      hgm->mut_obj().set_poller_id(poller_id);
      hgm->mut_obj().set_enabled(true);
      bc.update_hostgroup_member(hgm);
    }
    for (auto [sg_id, poller_id] :
         std::initializer_list<std::pair<uint64_t, uint64_t>>{{1278, 4},
                                                              {1279, 5}}) {
      auto sgm = std::make_shared<neb::pb_service_group_member>();
      sgm->mut_obj().set_servicegroup_id(sg_id);
      sgm->mut_obj().set_name(std::to_string(sg_id));
      sgm->mut_obj().set_host_id(14);
      sgm->mut_obj().set_service_id(78);
      sgm->mut_obj().set_poller_id(poller_id);
      sgm->mut_obj().set_enabled(true);
      bc.update_servicegroup_member(sgm);
    }
    auto mm = std::make_shared<storage::pb_metric_mapping>();
    mm->mut_obj().set_metric_id(123);
    mm->mut_obj().set_index_id(45);
    mm->mut_obj().set_min(0.456);
    mm->mut_obj().set_max(0.987);
    mm->mut_obj().set_uom("metric unit");
    bc.update_metric_mapping(mm);
  }

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
  {
    auto& bc = config::applier::state::instance().cache();

    auto h = std::make_shared<neb::pb_host>();
    h->mut_obj().set_host_id(14);
    h->mut_obj().set_name("my host");
    h->mut_obj().set_enabled(true);
    for (auto [id, type] : std::initializer_list<std::pair<uint64_t, TagType>>{
             {89, TagType::HOSTCATEGORY}, {189, TagType::HOSTGROUP}}) {
      auto* t = h->mut_obj().add_tags();
      t->set_id(id);
      t->set_type(type);
    }
    bc.update_host(h);

    auto s = std::make_shared<neb::pb_service>();
    s->mut_obj().set_host_id(14);
    s->mut_obj().set_service_id(78);
    s->mut_obj().set_description("my service/tutu ");
    s->mut_obj().set_enabled(true);
    s->mut_obj().set_severity_id(42);
    for (auto [id, type] : std::initializer_list<std::pair<uint64_t, TagType>>{
             {12, TagType::SERVICECATEGORY},
             {23, TagType::SERVICECATEGORY},
             {112, TagType::SERVICEGROUP},
             {123, TagType::SERVICEGROUP}}) {
      auto* t = s->mut_obj().add_tags();
      t->set_id(id);
      t->set_type(type);
    }
    bc.update_service(s);
    bc.set_db_id_for_severity(42, 0, 3);

    for (auto [id, name, type] :
         std::initializer_list<std::tuple<uint64_t, const char*, TagType>>{
             {89, "tag89", TagType::HOSTCATEGORY},
             {189, "tag189", TagType::HOSTGROUP},
             {12, "tag12", TagType::SERVICECATEGORY},
             {23, "tag23", TagType::SERVICECATEGORY},
             {112, "tag112", TagType::SERVICEGROUP},
             {123, "tag123", TagType::SERVICEGROUP}}) {
      auto tag = std::make_shared<neb::pb_tag>();
      tag->mut_obj().set_id(id);
      tag->mut_obj().set_name(name);
      tag->mut_obj().set_type(type);
      tag->mut_obj().set_action(Tag_Action_ADD);
      bc.update_tag(tag);
    }

    for (auto [hg_id, poller_id] :
         std::initializer_list<std::pair<uint64_t, uint64_t>>{{89, 1},
                                                              {88, 2}}) {
      auto hgm = std::make_shared<neb::pb_host_group_member>();
      hgm->mut_obj().set_hostgroup_id(hg_id);
      hgm->mut_obj().set_name(std::to_string(hg_id));
      hgm->mut_obj().set_host_id(14);
      hgm->mut_obj().set_poller_id(poller_id);
      hgm->mut_obj().set_enabled(true);
      bc.update_hostgroup_member(hgm);
    }
    for (auto [sg_id, poller_id] :
         std::initializer_list<std::pair<uint64_t, uint64_t>>{{1278, 5},
                                                              {1279, 6}}) {
      auto sgm = std::make_shared<neb::pb_service_group_member>();
      sgm->mut_obj().set_servicegroup_id(sg_id);
      sgm->mut_obj().set_name(std::to_string(sg_id));
      sgm->mut_obj().set_host_id(14);
      sgm->mut_obj().set_service_id(78);
      sgm->mut_obj().set_poller_id(poller_id);
      sgm->mut_obj().set_enabled(true);
      bc.update_servicegroup_member(sgm);
    }
    auto mm = std::make_shared<storage::pb_metric_mapping>();
    mm->mut_obj().set_metric_id(123);
    mm->mut_obj().set_index_id(45);
    mm->mut_obj().set_uom("metric unit");
    bc.update_metric_mapping(mm);
  }

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
