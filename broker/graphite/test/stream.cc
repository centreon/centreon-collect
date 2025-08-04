/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/graphite/stream.hh"
#include <gtest/gtest.h>
#include <com/centreon/broker/graphite/connector.hh>
#include "broker/test/test_server.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

class graphiteStream : public testing::Test {
 public:
  void SetUp() override {
    _server.init();
    _thread = std::thread(&test_server::run, &_server);

    _server.wait_for_init();
  }
  void TearDown() override {
    _server.stop();
    _thread.join();
  }

  test_server _server;
  std::thread _thread;
};

TEST_F(graphiteStream, BadPort) {
  std::shared_ptr<persistent_cache> cache;

  ASSERT_THROW(graphite::stream st("metric_name", "status_name", "a", "user",
                                   "pass", "localhost", 4243, 3, cache),
               msg_fmt);
}

TEST_F(graphiteStream, Read) {
  std::shared_ptr<persistent_cache> cache;
  std::shared_ptr<io::data> data;

  graphite::stream st("metric_name", "status_name", "a", "user", "pass",
                      "localhost", 4242, 3, cache);
  ASSERT_THROW(st.read(data, -1), msg_fmt);
}

TEST_F(graphiteStream, Write) {
  std::shared_ptr<persistent_cache> cache;
  std::shared_ptr<storage::pb_metric> d1 =
      std::make_shared<storage::pb_metric>();
  std::shared_ptr<storage::pb_metric> d2 =
      std::make_shared<storage::pb_metric>();
  std::shared_ptr<storage::pb_metric> d3 =
      std::make_shared<storage::pb_metric>();
  graphite::stream st("metric_name", "status_name", "a", "user", "pass",
                      "localhost", 4242, 3, cache);

  Metric& m1 = d1->mut_obj();
  Metric& m2 = d2->mut_obj();
  Metric& m3 = d3->mut_obj();

  m1.set_time(2000llu);
  m1.set_interval(60);
  m1.set_metric_id(42u);
  m1.set_name("host1");
  m1.set_rrd_len(42);
  m1.set_value(42.0);
  m1.set_value_type(Metric::AUTOMATIC);
  m1.set_host_id(1u);
  m1.set_service_id(1u);

  m2.set_time(2000llu);
  m2.set_interval(60);
  m2.set_metric_id(42u);
  m2.set_name("host1");
  m2.set_rrd_len(42);
  m2.set_value(42.0);
  m2.set_value_type(Metric::AUTOMATIC);
  m2.set_host_id(1u);
  m2.set_service_id(1u);

  m3.set_time(2000llu);
  m3.set_interval(60);
  m3.set_metric_id(42u);
  m3.set_name("host1");
  m3.set_rrd_len(42);
  m3.set_value(42.0);
  m3.set_value_type(Metric::AUTOMATIC);
  m3.set_host_id(1u);
  m3.set_service_id(1u);

  ASSERT_FALSE(st.write(d1));
  ASSERT_FALSE(st.write(d2));
  ASSERT_TRUE(st.write(d3));
}

TEST_F(graphiteStream, Flush) {
  std::shared_ptr<persistent_cache> cache;
  std::shared_ptr<storage::pb_metric> d1 =
      std::make_shared<storage::pb_metric>();
  std::shared_ptr<storage::pb_metric> d2 =
      std::make_shared<storage::pb_metric>();
  std::shared_ptr<storage::pb_metric> d3 =
      std::make_shared<storage::pb_metric>();
  graphite::stream st("metric_name", "status_name", "a", "user", "pass",
                      "localhost", 4242, 9, cache);

  Metric& m1 = d1->mut_obj();
  Metric& m2 = d2->mut_obj();
  Metric& m3 = d3->mut_obj();

  m1.set_time(2000llu);
  m1.set_interval(60);
  m1.set_metric_id(42u);
  m1.set_name("host1");
  m1.set_rrd_len(42);
  m1.set_value(42.0);
  m1.set_value_type(Metric::AUTOMATIC);
  m1.set_host_id(1u);
  m1.set_service_id(1u);

  m2.set_time(2000llu);
  m2.set_interval(60);
  m2.set_metric_id(42u);
  m2.set_name("host1");
  m2.set_rrd_len(42);
  m2.set_value(42.0);
  m2.set_value_type(Metric::AUTOMATIC);
  m2.set_host_id(1u);
  m2.set_service_id(1u);

  m3.set_time(2000llu);
  m3.set_interval(60);
  m3.set_metric_id(42u);
  m3.set_name("host1");
  m3.set_rrd_len(42);
  m3.set_value(42.0);
  m3.set_value_type(Metric::AUTOMATIC);
  m3.set_host_id(1u);
  m3.set_service_id(1u);

  ASSERT_FALSE(st.write(d1));
  ASSERT_FALSE(st.write(d2));
  ASSERT_FALSE(st.write(d3));

  ASSERT_TRUE(st.flush());
}

TEST_F(graphiteStream, NullData) {
  std::shared_ptr<persistent_cache> cache;
  std::shared_ptr<io::data> data;
  graphite::stream st("metric_name", "status_name", "a", "user", "pass",
                      "localhost", 4242, 9, cache);

  std::shared_ptr<io::data> d1{nullptr};
  ASSERT_FALSE(st.write(d1));
}

TEST_F(graphiteStream, FlushStatusOK) {
  std::shared_ptr<persistent_cache> cache;
  std::shared_ptr<storage::pb_status> d1 =
      std::make_shared<storage::pb_status>();
  std::shared_ptr<storage::pb_status> d2 =
      std::make_shared<storage::pb_status>();
  std::shared_ptr<storage::pb_status> d3 =
      std::make_shared<storage::pb_status>();
  Status s1 = d1->mut_obj();
  Status s2 = d2->mut_obj();
  Status s3 = d3->mut_obj();

  std::shared_ptr<io::data> data;
  graphite::stream st("metric_name", "status_name", "a", "user", "pass",
                      "localhost", 4242, 9, cache);

  d1->source_id = 3;
  d1->destination_id = 4;
  d1->broker_id = 1;
  s1.set_time(2000llu);
  s1.set_interval(60);
  s1.set_index_id(3);
  s1.set_rrd_len(9);
  s1.set_state(2);

  d2->source_id = 3;
  d2->destination_id = 4;
  d2->broker_id = 1;
  s2.set_time(2000llu);
  s2.set_interval(60);
  s2.set_index_id(3);
  s2.set_rrd_len(9);
  s2.set_state(2);

  d3->source_id = 3;
  d3->destination_id = 4;
  d3->broker_id = 1;
  s3.set_time(2000llu);
  s3.set_interval(60);
  s3.set_index_id(3);
  s3.set_rrd_len(9);
  s3.set_state(2);

  ASSERT_FALSE(st.write(d1));
  ASSERT_FALSE(st.write(d2));
  ASSERT_FALSE(st.write(d3));

  ASSERT_TRUE(st.flush());
}

TEST_F(graphiteStream, StatsAndConnector) {
  std::shared_ptr<persistent_cache> cache;
  graphite::connector con;
  con.connect_to("metric_name", "status_name", "a", "user", "pass", "localhost",
                 4242, 3, cache);

  nlohmann::json obj;
  con.open()->statistics(obj);
  ASSERT_TRUE(obj.is_null());
}
