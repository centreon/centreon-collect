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

#include <gtest/gtest.h>

#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "com/centreon/broker/influxdb/line_protocol_query.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

TEST(InfluxDBLineProtoQuery, EscapeKey) {
  influxdb::line_protocol_query lpq;

  ASSERT_EQ(lpq.escape_key("The test = valid, I hope"),
            "The\\ test\\ \\=\\ valid\\,\\ I\\ hope");
}

TEST(InfluxDBLineProtoQuery, EscapeMeasurement) {
  influxdb::line_protocol_query lpq;

  ASSERT_EQ(lpq.escape_measurement("The test = valid, I hope"),
            "The\\ test\\ =\\ valid\\,\\ I\\ hope");
}

TEST(InfluxDBLineProtoQuery, EscapeValue) {
  influxdb::line_protocol_query lpq;

  ASSERT_EQ(lpq.escape_value("The \"test\" = valid, I hope"),
            "\"The \\\"test\\\" = valid, I hope\"");
}

TEST(InfluxDBLineProtoQuery, GenerateMetricExcept) {
  influxdb::line_protocol_query lpq1;
  std::vector<influxdb::column> columns;
  std::shared_ptr<persistent_cache> pcache;
  influxdb::macro_cache cache(pcache);
  influxdb::line_protocol_query lpq2(
      "test", columns, influxdb::line_protocol_query::status, cache);
  influxdb::line_protocol_query lpq3(
      "test", columns, influxdb::line_protocol_query::metric, cache);
  storage::pb_metric m1;

  ASSERT_THROW(lpq1.generate_metric(m1), msg_fmt);
  ASSERT_THROW(lpq2.generate_metric(m1), msg_fmt);
  ASSERT_NO_THROW(lpq3.generate_metric(m1));
}

TEST(InfluxDBLineProtoQuery, GenerateMetric) {
  std::vector<influxdb::column> columns;
  std::shared_ptr<persistent_cache> pcache;
  influxdb::macro_cache cache(pcache);
  storage::pb_metric pb_m1, pb_m2, pb_m3;
  Metric &m1 = pb_m1.mut_obj(), &m2 = pb_m2.mut_obj(), &m3 = pb_m3.mut_obj();
  m1.set_host_id(1);
  m1.set_service_id(1);
  m1.set_name("host1");
  m1.set_time(2000llu);
  m1.set_interval(60);
  m1.set_metric_id(42u);
  m1.set_rrd_len(42);
  m1.set_value(42.0);
  m1.set_value_type(Metric::AUTOMATIC);

  m2.set_host_id(1);
  m2.set_service_id(1);
  m2.set_name("host2");
  m2.set_time(4000llu);
  m2.set_interval(120);
  m2.set_metric_id(43u);
  m2.set_rrd_len(42);
  m2.set_value(42.0);
  m2.set_value_type(Metric::AUTOMATIC);

  m3.set_host_id(2);
  m3.set_service_id(3);
  m3.set_name("hotst3");
  m3.set_time(2000llu);
  m3.set_interval(60);
  m3.set_metric_id(42u);
  m3.set_rrd_len(43);
  m3.set_value(43.0);
  m3.set_value_type(Metric::GAUGE);

  columns.push_back(
      influxdb::column{"host1", "42.0", true, influxdb::column::number});
  columns.push_back(
      influxdb::column{"host2", "42.0", false, influxdb::column::number});
  columns.push_back(
      influxdb::column{"host2", "42.0", false, influxdb::column::string});
  columns.push_back(
      influxdb::column{"host3", "43.0", true, influxdb::column::number});

  influxdb::line_protocol_query lpq(
      "test", columns, influxdb::line_protocol_query::metric, cache);

  ASSERT_EQ(lpq.generate_metric(pb_m1),
            "test,host1=42.0,host3=43.0 host2=42.0,host2=\"42.0\" 2000\n");
  ASSERT_EQ(lpq.generate_metric(pb_m2),
            "test,host1=42.0,host3=43.0 host2=42.0,host2=\"42.0\" 4000\n");
  ASSERT_EQ(lpq.generate_metric(pb_m3),
            "test,host1=42.0,host3=43.0 host2=42.0,host2=\"42.0\" 2000\n");
}

TEST(InfluxDBLineProtoQuery, ComplexMetric) {
  std::vector<influxdb::column> columns;
  std::shared_ptr<persistent_cache> pcache;
  influxdb::macro_cache cache(pcache);
  storage::pb_metric m;
  Metric& m_obj = m.mut_obj();
  m_obj.set_host_id(1);
  m_obj.set_service_id(1);
  m_obj.set_name("host1");
  m_obj.set_time(2000);
  m_obj.set_interval(60);
  m_obj.set_metric_id(40);
  m_obj.set_rrd_len(42);
  m_obj.set_value(42.0);
  m_obj.set_value_type(Metric::AUTOMATIC);
  std::shared_ptr<neb::host> host{std::make_shared<neb::host>()};
  std::shared_ptr<neb::service> svc{std::make_shared<neb::service>()};
  std::shared_ptr<neb::pb_instance> instance{
      std::make_shared<neb::pb_instance>()};
  std::shared_ptr<storage::metric_mapping> metric_map{
      std::make_shared<storage::metric_mapping>()};
  auto index_map{std::make_shared<storage::pb_index_mapping>()};

  columns.push_back(
      influxdb::column{"host1", "42.0", true, influxdb::column::number});
  columns.push_back(
      influxdb::column{"host2", "42.0", false, influxdb::column::number});
  columns.push_back(
      influxdb::column{"host2", "42.0", false, influxdb::column::string});
  columns.push_back(
      influxdb::column{"host3", "43.0", true, influxdb::column::number});

  m.source_id = 3;

  svc->service_description = "svc.1";
  svc->service_id = 1;
  svc->host_id = 1;

  host->host_name = "host1";
  host->host_id = 1;

  instance->mut_obj().set_instance_id(3);
  instance->mut_obj().set_name("poller test");

  metric_map->metric_id = 40;
  metric_map->index_id = 41;

  index_map->mut_obj().set_index_id(41);
  index_map->mut_obj().set_host_id(1);
  index_map->mut_obj().set_service_id(1);

  cache.write(host);
  cache.write(svc);
  cache.write(instance);
  cache.write(metric_map);
  cache.write(index_map);

  influxdb::line_protocol_query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$ $VALUE$",
      columns, influxdb::line_protocol_query::metric, cache};

  ASSERT_EQ(
      q.generate_metric(m),
      "test\\ .\\ host1\\ 1\\ svc.1\\ 1\\ poller\\ test\\ 3\\ 41\\ \\ TEST\\ $"
      "\\ 42,host1=42.0,host3=43.0 host2=42.0,host2=\"42.0\" 2000\n");
}

TEST(InfluxDBLineProtoQuery, ComplexStatus) {
  std::vector<influxdb::column> columns;
  std::shared_ptr<persistent_cache> pcache;
  influxdb::macro_cache cache(pcache);
  storage::pb_status s;
  Status& obj_s = s.mut_obj();
  obj_s.set_time(2000);
  obj_s.set_index_id(3);
  obj_s.set_interval(60);
  obj_s.set_rrd_len(9);
  obj_s.set_state(2);
  obj_s.set_host_id(1);
  obj_s.set_service_id(1);

  std::shared_ptr<neb::host> host{std::make_shared<neb::host>()};
  std::shared_ptr<neb::service> svc{std::make_shared<neb::service>()};
  std::shared_ptr<neb::pb_instance> instance{
      std::make_shared<neb::pb_instance>()};
  std::shared_ptr<storage::index_mapping> index_map{
      std::make_shared<storage::index_mapping>()};

  columns.push_back(
      influxdb::column{"host1", "42.0", true, influxdb::column::number});
  columns.push_back(
      influxdb::column{"host2", "42.0", false, influxdb::column::number});
  columns.push_back(
      influxdb::column{"host2", "42.0", false, influxdb::column::string});
  columns.push_back(
      influxdb::column{"host3", "43.0", true, influxdb::column::number});

  influxdb::line_protocol_query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$ $VALUE$",
      columns, influxdb::line_protocol_query::status, cache};

  svc->service_description = "svc1";
  svc->service_id = 1;
  svc->host_id = 1;

  host->host_name = "host1";
  host->host_id = 1;

  instance->mut_obj().set_instance_id(3);
  instance->mut_obj().set_name("poller test");

  index_map->index_id = 3;
  index_map->host_id = 1;
  index_map->service_id = 1;

  s.source_id = 3;
  s.destination_id = 4;
  s.broker_id = 1;

  cache.write(host);
  cache.write(svc);
  cache.write(instance);
  cache.write(index_map);

  ASSERT_EQ(
      q.generate_status(s),
      "test\\ .\\ host1\\ 1\\ svc1\\ 1\\ poller\\ test\\ 3\\ 3\\ \\ "
      "TEST\\ $\\ 2,host1=42.0,host3=43.0 host2=42.0,host2=\"42.0\" 2000\n");
}

TEST(InfluxDBLineProtoQuery, ComplexPbMetric) {
  std::vector<influxdb::column> columns;
  std::shared_ptr<persistent_cache> pcache;
  influxdb::macro_cache cache(pcache);
  storage::pb_metric m;
  Metric& m_obj = m.mut_obj();
  m_obj.set_host_id(1);
  m_obj.set_service_id(1);
  m_obj.set_name("host1");
  m_obj.set_time(2000);
  m_obj.set_interval(60);
  m_obj.set_metric_id(40);
  m_obj.set_rrd_len(42);
  m_obj.set_value(42.0);
  m_obj.set_value_type(Metric::AUTOMATIC);
  auto host{std::make_shared<neb::pb_host>()};
  auto svc{std::make_shared<neb::pb_service>()};
  auto instance{std::make_shared<neb::pb_instance>()};
  auto metric_map{std::make_shared<storage::metric_mapping>()};
  auto index_map{std::make_shared<storage::index_mapping>()};

  columns.emplace_back("host1", "42.0", true, influxdb::column::number);
  columns.emplace_back("host2", "42.0", false, influxdb::column::number);
  columns.emplace_back("host2", "42.0", false, influxdb::column::string);
  columns.emplace_back("host3", "43.0", true, influxdb::column::number);

  m.source_id = 3;

  svc->mut_obj().set_description("svc.1");
  svc->mut_obj().set_service_id(1);
  svc->mut_obj().set_host_id(1);

  host->mut_obj().set_name("host1");
  host->mut_obj().set_host_id(1);

  instance->mut_obj().set_instance_id(3);
  instance->mut_obj().set_name("poller test");

  metric_map->metric_id = 40;
  metric_map->index_id = 41;

  index_map->index_id = 41;

  cache.write(host);
  cache.write(svc);
  cache.write(instance);
  cache.write(metric_map);
  cache.write(index_map);

  influxdb::line_protocol_query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$ $VALUE$",
      columns, influxdb::line_protocol_query::metric, cache};

  ASSERT_EQ(
      q.generate_metric(m),
      "test\\ .\\ host1\\ 1\\ svc.1\\ 1\\ poller\\ test\\ 3\\ 41\\ \\ TEST\\ $"
      "\\ 42,host1=42.0,host3=43.0 host2=42.0,host2=\"42.0\" 2000\n");
}

TEST(InfluxDBLineProtoQuery, ComplexPBStatus) {
  std::vector<influxdb::column> columns;
  std::shared_ptr<persistent_cache> pcache;
  influxdb::macro_cache cache(pcache);
  storage::pb_status s;
  Status& obj_s = s.mut_obj();
  obj_s.set_time(2000);
  obj_s.set_index_id(3);
  obj_s.set_interval(60);
  obj_s.set_rrd_len(9);
  obj_s.set_state(2);
  obj_s.set_host_id(1);
  obj_s.set_service_id(1);

  auto host{std::make_shared<neb::pb_host>()};
  auto svc{std::make_shared<neb::pb_service>()};
  auto instance{std::make_shared<neb::pb_instance>()};
  auto index_map{std::make_shared<storage::index_mapping>()};

  columns.emplace_back("host1", "42.0", true, influxdb::column::number);
  columns.emplace_back("host2", "42.0", false, influxdb::column::number);
  columns.emplace_back("host2", "42.0", false, influxdb::column::string);
  columns.emplace_back("host3", "43.0", true, influxdb::column::number);

  influxdb::line_protocol_query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$ $VALUE$",
      columns, influxdb::line_protocol_query::status, cache};

  svc->mut_obj().set_description("svc1");
  svc->mut_obj().set_service_id(1);
  svc->mut_obj().set_host_id(1);

  host->mut_obj().set_name("host1");
  host->mut_obj().set_host_id(1);

  instance->mut_obj().set_instance_id(3);
  instance->mut_obj().set_name("poller test");

  index_map->index_id = 3;
  index_map->host_id = 1;
  index_map->service_id = 1;

  s.source_id = 3;
  s.destination_id = 4;
  s.broker_id = 1;

  cache.write(host);
  cache.write(svc);
  cache.write(instance);
  cache.write(index_map);

  ASSERT_EQ(
      q.generate_status(s),
      "test\\ .\\ host1\\ 1\\ svc1\\ 1\\ poller\\ test\\ 3\\ 3\\ \\ "
      "TEST\\ $\\ 2,host1=42.0,host3=43.0 host2=42.0,host2=\"42.0\" 2000\n");
}

TEST(InfluxDBLineProtoQuery, Except) {
  std::vector<influxdb::column> columns;
  std::shared_ptr<persistent_cache> pcache;
  influxdb::macro_cache cache(pcache);
  storage::pb_metric m;
  storage::pb_status s;

  influxdb::line_protocol_query q{"test .", columns,
                                  influxdb::line_protocol_query::metric, cache};
  influxdb::line_protocol_query q2{
      "test .", columns, influxdb::line_protocol_query::status, cache};

  try {
    influxdb::line_protocol_query q3{"test . $METRICID$", columns,
                                     influxdb::line_protocol_query::status,
                                     cache};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  try {
    influxdb::line_protocol_query q3{"test . $METRIC$", columns,
                                     influxdb::line_protocol_query::status,
                                     cache};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  try {
    influxdb::line_protocol_query q3{"test . $METRIC", columns,
                                     influxdb::line_protocol_query::status,
                                     cache};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  m.mut_obj().set_metric_id(3);
  m.mut_obj().set_name("A");

  influxdb::line_protocol_query q4{"test . $METRICID$ $METRIC$", columns,
                                   influxdb::line_protocol_query::metric,
                                   cache};

  ASSERT_THROW(q.generate_status(s), msg_fmt);
  ASSERT_THROW(q2.generate_metric(m), msg_fmt);
  ASSERT_EQ(q4.generate_metric(m), "test\\ .\\ 3\\ A 0\n");

  influxdb::line_protocol_query q5{"test . $INSTANCE$", columns,
                                   influxdb::line_protocol_query::metric,
                                   cache};
  ASSERT_EQ(q5.generate_metric(m), "");

  influxdb::line_protocol_query q6{"test . $INSTANCE$", columns,
                                   influxdb::line_protocol_query::status,
                                   cache};
  ASSERT_EQ(q6.generate_status(s), "");
}
