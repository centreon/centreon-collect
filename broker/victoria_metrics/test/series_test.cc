/**
 * Copyright 2026 Centreon
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
#include <nlohmann/json.hpp>

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "bbdo/storage/remove_graph.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/http_tsdb/factory.hh"
#include "com/centreon/broker/victoria_metrics/factory.hh"
#include "com/centreon/broker/victoria_metrics/request.hh"
#include "com/centreon/broker/victoria_metrics/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;
using com::centreon::exceptions::msg_fmt;
using namespace nlohmann::literals;
using log_v2 = com::centreon::common::log_v2::log_v2;

extern std::shared_ptr<asio::io_context> g_io_context;

namespace {

/**
 * Requests received by the fake server, in the order they were sent, and the
 * answer it gives: (status, body) chosen from the target.
 */
struct fake_server {
  struct received {
    std::string target;
    std::string body;
  };
  static std::mutex protect;
  static std::condition_variable cond;
  static std::vector<received> requests;
  static std::function<std::pair<unsigned, std::string>(const std::string&)>
      answer;

  static void reset() {
    std::lock_guard<std::mutex> l(protect);
    requests.clear();
    answer = [](const std::string&) {
      return std::make_pair(204u, std::string());
    };
  }

  static bool wait_for(size_t count) {
    std::unique_lock<std::mutex> l(protect);
    return cond.wait_for(l, std::chrono::seconds(10),
                         [count] { return requests.size() >= count; });
  }

  static std::vector<received> get() {
    std::lock_guard<std::mutex> l(protect);
    return requests;
  }
};

std::mutex fake_server::protect;
std::condition_variable fake_server::cond;
std::vector<fake_server::received> fake_server::requests;
std::function<std::pair<unsigned, std::string>(const std::string&)>
    fake_server::answer;

class fake_connection : public http::connection_base {
  asio::ip::tcp::socket _not_used;

 public:
  fake_connection(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger,
                  const http::http_config::pointer& conf)
      : connection_base(io_context, logger, conf), _not_used(*io_context) {}

  void shutdown() override { _state = e_not_connected; }

  void connect(http::connect_callback_type&& callback) override {
    _state = e_idle;
    asio::post(*_io_context, [cb = std::move(callback)]() { cb({}, {}); });
  }

  void send(http::request_ptr request,
            http::send_callback_type&& callback) override {
    std::pair<unsigned, std::string> answer;
    {
      std::lock_guard<std::mutex> l(fake_server::protect);
      std::string target(request->target());
      fake_server::requests.push_back({target, request->body()});
      answer = fake_server::answer(target);
    }
    fake_server::cond.notify_all();
    asio::post(*_io_context, [cb = std::move(callback), answer]() {
      auto resp = std::make_shared<http::response_type>();
      resp->result(answer.first);
      resp->body() = answer.second;
      resp->keep_alive(false);
      cb({}, "", resp);
    });
  }

  void _on_accept(http::connect_callback_type&& callback
                  [[maybe_unused]]) override {}
  void answer(const http::response_ptr& response [[maybe_unused]],
              http::answer_callback_type&& callback [[maybe_unused]]) override {
  }
  void receive_request(http::request_callback_type&& callback
                       [[maybe_unused]]) override {}

  asio::ip::tcp::socket& get_socket() override { return _not_used; }
};

std::string selector(const std::string& sel) {
  return "match%5B%5D=" + stream::form_encode(sel);
}

}  // namespace

class victoria_series_test : public ::testing::Test {
 protected:
  std::shared_ptr<http_tsdb::http_tsdb_config> _conf;

  std::shared_ptr<stream> _open(const series_config& series_conf) {
    return stream::load(
        g_io_context, _conf, "",
        [conf = _conf]() {
          return std::make_shared<fake_connection>(
              g_io_context, log_v2::instance().get(log_v2::VICTORIA_METRICS),
              conf);
        },
        series_conf);
  }

 public:
  static void SetUpTestSuite() {
    file::disk_accessor::load(1000);
    ::remove("/tmp/victoria_series_test.rt");
    ::remove("/tmp/victoria_series_test.cnf");
    cache::global_cache::load(g_io_context, "/tmp/victoria_series_test");
    auto obj = cache::global_cache::instance_ptr();
    auto index_mapping = std::make_shared<storage::pb_index_mapping>();
    index_mapping->mut_obj().set_index_id(1045);
    index_mapping->mut_obj().set_host_id(1014);
    index_mapping->mut_obj().set_service_id(1078);
    obj->write(index_mapping);
    for (uint64_t metric_id : {10145, 10146}) {
      auto metric_mapping = std::make_shared<storage::pb_metric_mapping>();
      metric_mapping->mut_obj().set_index_id(1045);
      metric_mapping->mut_obj().set_metric_id(metric_id);
      obj->write(metric_mapping);
    }
  }

  static void TearDownTestSuite() {
    cache::global_cache::unload();
    ::remove("/tmp/victoria_series_test.rt");
    ::remove("/tmp/victoria_series_test.cnf");
  }

  void SetUp() override {
    fake_server::reset();
    http::http_config http_conf(
        asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 80),
        "localhost", false, std::chrono::seconds(10), std::chrono::seconds(10),
        std::chrono::seconds(10), 30, std::chrono::seconds(1), 0,
        std::chrono::seconds(1), 5);
    // no extra column: the lines only carry the labels of the contract
    _conf = std::make_shared<http_tsdb::http_tsdb_config>(
        http_conf, "/write", "user", "pwd", 3,
        std::vector<http_tsdb::column>(), std::vector<http_tsdb::column>());
  }
};

TEST_F(victoria_series_test, write_target_declares_seconds) {
  ASSERT_EQ(stream::write_target_with_precision("/write"),
            "/write?precision=s");
  ASSERT_EQ(stream::write_target_with_precision("/write?"),
            "/write?precision=s");
  ASSERT_EQ(stream::write_target_with_precision("/insert/0/influx/write?db=a"),
            "/insert/0/influx/write?db=a&precision=s");
  ASSERT_EQ(stream::write_target_with_precision("/write?precision=ms"),
            "/write?precision=ms");
  ASSERT_EQ(stream::write_target_with_precision("/write?a=1&precision=ns"),
            "/write?a=1&precision=ns");
}

TEST_F(victoria_series_test, form_encode) {
  ASSERT_EQ(stream::form_encode("metric_val{id=\"12\"}"),
            "metric_val%7Bid%3D%2212%22%7D");
}

TEST_F(victoria_series_test, configured_min_max_columns) {
  auto logger = log_v2::instance().get(log_v2::VICTORIA_METRICS);
  line_protocol_query metric_columns(
      stream::allowed_macros,
      http_tsdb::factory::get_columns(R"([
    {"name" : "min", "is_tag" : "true", "value" : "$MIN$", "type":"number"},
    {"name" : "max", "is_tag" : "true", "value" : "$MAX$", "type":"number"}])"_json),
      http_tsdb::line_protocol_query::data_type::metric, logger);
  line_protocol_query status_columns(
      stream::allowed_macros, {},
      http_tsdb::line_protocol_query::data_type::status, logger);
  victoria_metrics::request req(boost::beast::http::verb::post, "localhost",
                                "/", logger, 0, metric_columns, status_columns);
  Metric metric;
  metric.set_metric_id(10145);
  metric.set_value(2);
  metric.set_time(1700000000);
  metric.set_host_id(1014);
  metric.set_service_id(1078);
  metric.set_name("rta");
  metric.set_unit("ms");
  metric.set_min(0.5);
  metric.set_max(10);
  req.add_metric(metric);
  ASSERT_EQ(req.body(),
            "metric,id=10145,index_id=1045,name=rta,host_id=1014,serv_id=1078,unit=ms,"
            "min=0.5,max=10 val=2 1700000000\n");
}

TEST_F(victoria_series_test, live_points_use_seconds_target) {
  auto s = _open(series_config());
  auto m = std::make_shared<storage::pb_metric>();
  m->mut_obj().set_metric_id(10145);
  m->mut_obj().set_host_id(1014);
  m->mut_obj().set_service_id(1078);
  m->mut_obj().set_name("rta");
  m->mut_obj().set_unit("ms");
  m->mut_obj().set_value(1.5);
  m->mut_obj().set_time(1700000000);
  s->write(m);
  s->flush();
  ASSERT_TRUE(fake_server::wait_for(1));
  auto reqs = fake_server::get();
  ASSERT_EQ(reqs[0].target, "/write?precision=s");
  ASSERT_EQ(reqs[0].body,
            "metric,id=10145,index_id=1045,name=rta,host_id=1014,serv_id=1078,unit=ms "
            "val=1.5 1700000000\n");
  s->stop();
}

TEST_F(victoria_series_test, rebuild_deletes_then_replays) {
  auto s = _open(series_config());
  // a live point teaches the output the name and unit of metric 10145
  auto live = std::make_shared<storage::pb_metric>();
  live->mut_obj().set_metric_id(10145);
  live->mut_obj().set_host_id(1014);
  live->mut_obj().set_service_id(1078);
  live->mut_obj().set_name("rta");
  live->mut_obj().set_unit("ms");
  live->mut_obj().set_value(9);
  live->mut_obj().set_time(1700000600);
  ASSERT_EQ(s->write(live), 0);

  auto start = std::make_shared<storage::pb_rebuild_message>();
  start->mut_obj().set_state(RebuildMessage_State_START);
  (*start->mut_obj().mutable_metric_to_index_id())[10145] = 1045;
  // the live point is acknowledged once sent, with the start message
  ASSERT_EQ(s->write(start), 2);

  auto data = std::make_shared<storage::pb_rebuild_message>();
  data->mut_obj().set_state(RebuildMessage_State_DATA);
  auto& serie = (*data->mut_obj().mutable_timeserie())[10145];
  for (int i = 0; i < 4; ++i) {
    Point* pt = serie.add_pts();
    pt->set_ctime(1700000000 + i * 60);
    pt->set_value(i);
    pt->set_status(i);  // 3 is UNKNOWN: no status point
  }
  ASSERT_EQ(s->write(data), 1);

  auto end = std::make_shared<storage::pb_rebuild_message>();
  end->mut_obj().set_state(RebuildMessage_State_END);
  (*end->mut_obj().mutable_metric_to_index_id())[10145] = 1045;
  ASSERT_EQ(s->write(end), 1);

  auto reqs = fake_server::get();
  ASSERT_EQ(reqs.size(), 5u);
  ASSERT_EQ(reqs[0].target, "/write?precision=s");
  ASSERT_EQ(reqs[1].target, "/api/v1/admin/tsdb/delete_series");
  ASSERT_EQ(reqs[1].body, selector("metric_val{id=\"10145\"}") + "&" +
                              selector("status_val{id=\"1045\"}"));
  // batches of 3 lines (queries_per_transaction), points in time order
  const std::string labels =
      "metric,id=10145,index_id=1045,name=rta,host_id=1014,serv_id=1078,unit=ms";
  ASSERT_EQ(reqs[2].target, "/write?precision=s");
  ASSERT_EQ(reqs[2].body, labels + " val=0 1700000000\n" + labels +
                              " val=1 1700000060\n" + labels +
                              " val=2 1700000120\n");
  ASSERT_EQ(reqs[3].body, labels +
                              " val=3 1700000180\n"
                              "status,id=1045,host_id=1014,serv_id=1078 val=100 "
                              "1700000000\n"
                              "status,id=1045,host_id=1014,serv_id=1078 val=75 "
                              "1700000060\n");
  ASSERT_EQ(reqs[4].body,
            "status,id=1045,host_id=1014,serv_id=1078 val=0 1700000120\n");
  s->stop();
}

TEST_F(victoria_series_test, rebuild_reads_unknown_labels_before_delete) {
  fake_server::answer = [](const std::string& target) {
    if (target == "/api/v1/series")
      return std::make_pair(
          200u, std::string(R"({"status":"success","data":[{"__name__":)"
                            R"("metric_val","id":"10146","index_id":"1045",)"
                            R"("name":"pl","unit":"%"}]})"));
    return std::make_pair(204u, std::string());
  };
  auto s = _open(series_config());
  auto start = std::make_shared<storage::pb_rebuild_message>();
  start->mut_obj().set_state(RebuildMessage_State_START);
  (*start->mut_obj().mutable_metric_to_index_id())[10146] = 1045;
  ASSERT_EQ(s->write(start), 1);
  auto data = std::make_shared<storage::pb_rebuild_message>();
  data->mut_obj().set_state(RebuildMessage_State_DATA);
  Point* pt = (*data->mut_obj().mutable_timeserie())[10146].add_pts();
  pt->set_ctime(1700000000);
  pt->set_value(0.5);
  pt->set_status(3);
  ASSERT_EQ(s->write(data), 1);

  auto reqs = fake_server::get();
  ASSERT_EQ(reqs.size(), 3u);
  ASSERT_EQ(reqs[0].target, "/api/v1/series");
  ASSERT_EQ(reqs[0].body, selector("metric_val{id=\"10146\"}") + "&start=1");
  ASSERT_EQ(reqs[1].target, "/api/v1/admin/tsdb/delete_series");
  ASSERT_EQ(reqs[2].body,
            "metric,id=10146,index_id=1045,name=pl,host_id=1014,serv_id=1078,unit=% "
            "val=0.5 1700000000\n");
  s->stop();
}

TEST_F(victoria_series_test, remove_graphs) {
  series_config conf;
  conf.delete_target = "/delete/0/prometheus/api/v1/admin/tsdb/delete_series";
  auto s = _open(conf);
  auto rm = std::make_shared<storage::pb_remove_graph_message>();
  rm->mut_obj().add_metric_ids(10145);
  rm->mut_obj().add_index_ids(1045);
  ASSERT_EQ(s->write(rm), 1);
  auto legacy = std::make_shared<storage::remove_graph>(10146, false);
  ASSERT_EQ(s->write(legacy), 1);

  auto reqs = fake_server::get();
  ASSERT_EQ(reqs.size(), 2u);
  ASSERT_EQ(reqs[0].target, conf.delete_target);
  ASSERT_EQ(reqs[0].body, selector("metric_val{id=\"10145\"}") + "&" +
                              selector("metric_val{index_id=\"1045\"}") + "&" +
                              selector("status_val{id=\"1045\"}"));
  ASSERT_EQ(reqs[1].body, selector("metric_val{id=\"10146\"}"));
  s->stop();
}

TEST_F(victoria_series_test, failed_delete_is_not_acknowledged) {
  fake_server::answer = [](const std::string& target) {
    if (target == "/api/v1/admin/tsdb/delete_series")
      return std::make_pair(503u, std::string("unavailable"));
    return std::make_pair(204u, std::string());
  };
  auto s = _open(series_config());
  auto rm = std::make_shared<storage::pb_remove_graph_message>();
  rm->mut_obj().add_metric_ids(10145);
  // the failover catches it, waits and replays the message
  ASSERT_THROW(s->write(rm), msg_fmt);
  s->stop();
}

TEST_F(victoria_series_test, server_error_keeps_points) {
  std::atomic_uint calls{0};
  fake_server::answer = [&calls](const std::string&) {
    return calls++ ? std::make_pair(204u, std::string())
                   : std::make_pair(500u, std::string("busy"));
  };
  auto s = _open(series_config());
  auto m = std::make_shared<storage::pb_metric>();
  m->mut_obj().set_metric_id(10145);
  m->mut_obj().set_value(1);
  m->mut_obj().set_time(1700000000);
  s->write(m);
  ASSERT_EQ(s->flush(), 0);
  ASSERT_TRUE(fake_server::wait_for(1));
  // wait for the 500 to be handled, then the point is sent again
  int acknowledged = 0;
  for (int i = 0; i < 100 && !acknowledged; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    acknowledged = s->flush();
  }
  ASSERT_EQ(acknowledged, 1);
  auto reqs = fake_server::get();
  ASSERT_EQ(reqs.size(), 2u);
  ASSERT_EQ(reqs[0].body, reqs[1].body);
  s->stop();
}
