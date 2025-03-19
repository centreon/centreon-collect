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

#include "com/centreon/broker/graphite/query.hh"
#include <gtest/gtest.h>
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

TEST(graphiteQuery, ComplexMetric) {
  std::shared_ptr<persistent_cache> pcache;
  graphite::macro_cache cache(pcache);
  storage::pb_metric m_event;
  Metric& m = m_event.mut_obj();
  m.set_host_id(1);
  m.set_service_id(1);
  m.set_name("host1");
  m.set_time(2000llu);
  m.set_interval(60);
  m.set_metric_id(40u);
  m.set_rrd_len(42);
  m.set_value(42.0);
  m.set_value_type(Metric::AUTOMATIC);
  std::shared_ptr<neb::host> host{std::make_shared<neb::host>()};
  std::shared_ptr<neb::service> svc{std::make_shared<neb::service>()};
  std::shared_ptr<neb::pb_instance> instance{
      std::make_shared<neb::pb_instance>()};
  std::shared_ptr<storage::metric_mapping> metric_map{
      std::make_shared<storage::metric_mapping>()};
  auto index_map{std::make_shared<storage::index_mapping>()};

  m_event.source_id = 3;

  svc->service_description = "svc.1";
  svc->service_id = 1;
  svc->host_id = 1;

  host->host_name = "host1";
  host->host_id = 1;

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

  graphite::query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$",
      "a", graphite::query::metric, cache};

  ASSERT_EQ(q.generate_metric(m_event),
            "test_._host1_1_svca1_1_poller_test_3_41__TEST_$ 42 2000\n");
}

TEST(graphiteQuery, ComplexStatus) {
  std::shared_ptr<persistent_cache> pcache;
  graphite::macro_cache cache(pcache);
  storage::pb_status s_event;
  Status& s = s_event.mut_obj();
  s.set_time(2000llu);
  s.set_index_id(3);
  s.set_interval(60);
  s.set_rrd_len(9);
  s.set_state(2);
  s.set_host_id(1);
  s.set_service_id(1);

  std::shared_ptr<neb::host> host{std::make_shared<neb::host>()};
  std::shared_ptr<neb::service> svc{std::make_shared<neb::service>()};
  std::shared_ptr<neb::pb_instance> instance{
      std::make_shared<neb::pb_instance>()};
  std::shared_ptr<storage::index_mapping> index_map{
      std::make_shared<storage::index_mapping>()};

  graphite::query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$",
      "a", graphite::query::status, cache};

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

  s_event.source_id = 3;
  s_event.destination_id = 4;
  s_event.broker_id = 1;

  cache.write(host);
  cache.write(svc);
  cache.write(instance);
  cache.write(index_map);

  ASSERT_EQ(q.generate_status(s_event),
            "test_._host1_1_svc1_1_poller_test_3_3__TEST_$ 2 2000\n");
}

TEST(graphiteQuery, ComplexPbMetric) {
  std::shared_ptr<persistent_cache> pcache;
  graphite::macro_cache cache(pcache);
  storage::pb_metric m_event;
  Metric& m = m_event.mut_obj();
  m.set_host_id(1);
  m.set_service_id(1);
  m.set_name("host1");
  m.set_time(2000llu);
  m.set_interval(60);
  m.set_metric_id(40u);
  m.set_rrd_len(42);
  m.set_value(42.0);
  m.set_value_type(Metric::AUTOMATIC);
  auto host{std::make_shared<neb::pb_host>()};
  auto svc{std::make_shared<neb::pb_service>()};
  std::shared_ptr<neb::pb_instance> instance{
      std::make_shared<neb::pb_instance>()};
  auto metric_map{std::make_shared<storage::metric_mapping>()};
  auto index_map{std::make_shared<storage::index_mapping>()};

  m_event.source_id = 3;

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

  graphite::query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$",
      "a", graphite::query::metric, cache};

  ASSERT_EQ(q.generate_metric(m_event),
            "test_._host1_1_svca1_1_poller_test_3_41__TEST_$ 42 2000\n");
}

TEST(graphiteQuery, ComplexPbStatus) {
  std::shared_ptr<persistent_cache> pcache;
  graphite::macro_cache cache(pcache);
  storage::pb_status s_event;
  Status& s = s_event.mut_obj();
  s.set_time(2000llu);
  s.set_index_id(3);
  s.set_interval(60);
  s.set_rrd_len(9);
  s.set_state(2);
  s.set_host_id(1);
  s.set_service_id(1);

  auto host{std::make_shared<neb::pb_host>()};
  auto svc{std::make_shared<neb::pb_service>()};
  std::shared_ptr<neb::pb_instance> instance{
      std::make_shared<neb::pb_instance>()};
  auto index_map{std::make_shared<storage::index_mapping>()};

  graphite::query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$",
      "a", graphite::query::status, cache};

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

  s_event.source_id = 3;
  s_event.destination_id = 4;
  s_event.broker_id = 1;

  cache.write(host);
  cache.write(svc);
  cache.write(instance);
  cache.write(index_map);

  ASSERT_EQ(q.generate_status(s_event),
            "test_._host1_1_svc1_1_poller_test_3_3__TEST_$ 2 2000\n");
}

TEST(graphiteQuery, Except) {
  std::shared_ptr<persistent_cache> pcache;
  graphite::macro_cache cache(pcache);
  storage::pb_status s;
  storage::pb_metric m;

  graphite::query q{"test .", "a", graphite::query::metric, cache};
  graphite::query q2{"test .", "a", graphite::query::status, cache};

  try {
    graphite::query q3{"test . $METRICID$", "a", graphite::query::status,
                       cache};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  try {
    graphite::query q3{"test . $METRIC$", "a", graphite::query::status, cache};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  try {
    graphite::query q3{"test . $METRIC", "a", graphite::query::status, cache};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  m.mut_obj().set_metric_id(3);
  m.mut_obj().set_name("The.full.name.A");

  graphite::query q4{"test . $METRICID$ $METRIC$", "a", graphite::query::metric,
                     cache};

  ASSERT_THROW(q.generate_status(s), msg_fmt);
  ASSERT_THROW(q2.generate_metric(m), msg_fmt);
  ASSERT_EQ(q4.generate_metric(m), "test_._3_TheafullanameaA 0 0\n");

  graphite::query q5{"test . $INSTANCE$", "a", graphite::query::metric, cache};
  ASSERT_EQ(q5.generate_metric(m), "");

  graphite::query q6{"test . $INSTANCE$", "a", graphite::query::status, cache};
  ASSERT_EQ(q6.generate_status(s), "");
}
