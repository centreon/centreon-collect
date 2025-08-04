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

#include "com/centreon/broker/influxdb/influxdb.hh"
#include <gtest/gtest.h>
#include "broker/test/test_server.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

class InfluxDB12 : public testing::Test {
 public:
  void SetUp() override {
    _server.init();
    _thread = std::thread(&test_server::run, &_server);

    _server.wait_for_init();
    _logger = log_v2::instance().get(log_v2::INFLUXDB);
  }
  void TearDown() override {
    _server.stop();
    _thread.join();
  }

  test_server _server;
  std::thread _thread;
  std::shared_ptr<spdlog::logger> _logger;
};

TEST_F(InfluxDB12, BadConnection) {
  std::shared_ptr<persistent_cache> cache;
  influxdb::macro_cache mcache{cache};
  std::vector<influxdb::column> mcolumns;
  std::vector<influxdb::column> scolumns;

  ASSERT_THROW(
      influxdb::influxdb idb("centreon", "pass", "localhost", 4243, "centreon",
                             "host_status", scolumns, "host_metrics", mcolumns,
                             mcache, _logger),
      msg_fmt);
}

TEST_F(InfluxDB12, Empty) {
  std::shared_ptr<persistent_cache> cache;
  influxdb::macro_cache mcache{cache};
  std::vector<influxdb::column> mcolumns;
  std::vector<influxdb::column> scolumns;

  influxdb::influxdb idb("centreon", "pass", "localhost", 4242, "centreon",
                         "host_status", scolumns, "host_metrics", mcolumns,
                         mcache, _logger);
  idb.clear();
  ASSERT_NO_THROW(idb.commit());
}

TEST_F(InfluxDB12, Simple) {
  std::shared_ptr<persistent_cache> cache;
  influxdb::macro_cache mcache{cache};
  storage::pb_metric pb_m1, pb_m2, pb_m3;
  Metric &m1 = pb_m1.mut_obj(), &m2 = pb_m2.mut_obj(), &m3 = pb_m3.mut_obj();

  std::vector<influxdb::column> mcolumns;
  mcolumns.push_back(
      influxdb::column{"mhost1", "42.0", true, influxdb::column::number});
  mcolumns.push_back(
      influxdb::column{"mhost2", "42.0", false, influxdb::column::number});
  mcolumns.push_back(
      influxdb::column{"most2", "42.0", false, influxdb::column::string});
  mcolumns.push_back(
      influxdb::column{"most3", "43.0", true, influxdb::column::number});

  std::vector<influxdb::column> scolumns;
  mcolumns.push_back(
      influxdb::column{"shost1", "42.0", true, influxdb::column::number});
  mcolumns.push_back(
      influxdb::column{"shost2", "42.0", false, influxdb::column::number});
  mcolumns.push_back(
      influxdb::column{"shost2", "42.0", false, influxdb::column::string});
  mcolumns.push_back(
      influxdb::column{"shost3", "43.0", true, influxdb::column::number});

  influxdb::influxdb idb("centreon", "pass", "localhost", 4242, "centreon",
                         "host_status", scolumns, "host_metrics", mcolumns,
                         mcache, _logger);
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

  idb.write(pb_m1);
  idb.write(pb_m2);
  idb.write(pb_m3);

  ASSERT_NO_THROW(idb.commit());
}

TEST_F(InfluxDB12, BadServerResponse1) {
  std::shared_ptr<persistent_cache> cache;
  influxdb::macro_cache mcache{cache};
  storage::pb_metric pb_m1, pb_m2, pb_m3;
  Metric &m1 = pb_m1.mut_obj(), &m2 = pb_m2.mut_obj(), &m3 = pb_m3.mut_obj();
  std::vector<influxdb::column> mcolumns;
  std::vector<influxdb::column> scolumns;

  influxdb::influxdb idb("centreon", "fail1", "localhost", 4242, "centreon",
                         "host_status", scolumns, "host_metrics", mcolumns,
                         mcache, _logger);

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

  idb.write(pb_m1);
  idb.write(pb_m2);
  idb.write(pb_m3);

  ASSERT_THROW(idb.commit(), msg_fmt);
}

TEST_F(InfluxDB12, BadServerResponse2) {
  std::shared_ptr<persistent_cache> cache;
  influxdb::macro_cache mcache{cache};
  storage::pb_metric pb_m1, pb_m2, pb_m3;
  Metric &m1 = pb_m1.mut_obj(), &m2 = pb_m2.mut_obj(), &m3 = pb_m3.mut_obj();

  std::vector<influxdb::column> mcolumns;
  mcolumns.push_back(
      influxdb::column{"mhost1", "42.0", true, influxdb::column::number});
  mcolumns.push_back(
      influxdb::column{"mhost2", "42.0", false, influxdb::column::number});
  mcolumns.push_back(
      influxdb::column{"most2", "42.0", false, influxdb::column::string});
  mcolumns.push_back(
      influxdb::column{"most3", "43.0", true, influxdb::column::number});

  std::vector<influxdb::column> scolumns;
  mcolumns.push_back(
      influxdb::column{"shost1", "42.0", true, influxdb::column::number});
  mcolumns.push_back(
      influxdb::column{"shost2", "42.0", false, influxdb::column::number});
  mcolumns.push_back(
      influxdb::column{"shost2", "42.0", false, influxdb::column::string});
  mcolumns.push_back(
      influxdb::column{"shost3", "43.0", true, influxdb::column::number});

  influxdb::influxdb idb("centreon", "fail2", "localhost", 4242, "centreon",
                         "host_status", scolumns, "host_metrics", mcolumns,
                         mcache, _logger);

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

  idb.write(pb_m1);
  idb.write(pb_m2);
  idb.write(pb_m3);

  ASSERT_THROW(idb.commit(), msg_fmt);
}
