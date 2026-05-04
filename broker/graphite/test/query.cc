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

#include "bbdo/storage/metric_mapping.hh"
#include "broker/core/config/applier/broker_state.hh"
#include "com/centreon/broker/graphite/query.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

class graphiteQuery : public ::testing::Test {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  void SetUp() override {
    _logger = log_v2::instance().get(log_v2::LUA);
    config::applier::state::load<
        com::centreon::broker::config::applier::broker_state>("unittest");
    config::applier::state::instance().initialize_cache(
        log_v2::instance().get(log_v2::CORE));
    config::applier::state::instance().cache().enable_section(
        com::centreon::broker::cache::broker_cache::CACHE_ALL);
  }
  void TearDown() override { config::applier::state::unload(); }
};

TEST_F(graphiteQuery, ComplexMetric) {
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

  m_event.source_id = 3;

  svc->service_description = "svc.1";
  svc->service_id = 1;
  svc->host_id = 1;
  svc->enabled = true;

  host->host_name = "host1";
  host->host_id = 1;
  host->enabled = true;

  instance->mut_obj().set_instance_id(3);
  instance->mut_obj().set_name("poller test");
  instance->mut_obj().set_running(true);

  metric_map->metric_id = 40;
  metric_map->index_id = 41;

  auto& cache = config::applier::state::instance().cache();
  cache.publish(host);
  cache.publish(svc);
  cache.publish(instance);
  cache.publish(metric_map);

  graphite::query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$",
      "a", graphite::query::metric};

  ASSERT_EQ(q.generate_metric(m_event),
            "test_._host1_1_svca1_1_poller_test_3_41__TEST_$ 42 2000\n");
}

TEST_F(graphiteQuery, ComplexStatus) {
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

  graphite::query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$",
      "a", graphite::query::status};

  svc->service_description = "svc1";
  svc->service_id = 1;
  svc->host_id = 1;
  svc->enabled = true;

  host->host_name = "host1";
  host->host_id = 1;
  host->enabled = true;

  instance->mut_obj().set_instance_id(3);
  instance->mut_obj().set_name("poller test");
  instance->mut_obj().set_running(true);

  s_event.source_id = 3;
  s_event.destination_id = 4;
  s_event.broker_id = 1;

  auto& cache = config::applier::state::instance().cache();
  cache.publish(host);
  cache.publish(svc);
  cache.publish(instance);

  ASSERT_EQ(q.generate_status(s_event),
            "test_._host1_1_svc1_1_poller_test_3_3__TEST_$ 2 2000\n");
}

TEST_F(graphiteQuery, ComplexPbMetric) {
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

  m_event.source_id = 3;

  svc->mut_obj().set_description("svc.1");
  svc->mut_obj().set_service_id(1);
  svc->mut_obj().set_host_id(1);
  svc->mut_obj().set_enabled(true);

  host->mut_obj().set_name("host1");
  host->mut_obj().set_host_id(1);
  host->mut_obj().set_enabled(true);

  instance->mut_obj().set_instance_id(3);
  instance->mut_obj().set_name("poller test");
  instance->mut_obj().set_running(true);

  metric_map->metric_id = 40;
  metric_map->index_id = 41;

  auto& cache = config::applier::state::instance().cache();
  cache.publish(host);
  cache.publish(svc);
  cache.publish(instance);
  cache.publish(metric_map);

  graphite::query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$",
      "a", graphite::query::metric};

  ASSERT_EQ(q.generate_metric(m_event),
            "test_._host1_1_svca1_1_poller_test_3_41__TEST_$ 42 2000\n");
}

TEST_F(graphiteQuery, ComplexPbStatus) {
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

  graphite::query q{
      "test . $HOST$ $HOSTID$ $SERVICE$ $SERVICEID$ $INSTANCE$ $INSTANCEID$ "
      "$INDEXID$ $TEST$ TEST $$",
      "a", graphite::query::status};

  svc->mut_obj().set_description("svc1");
  svc->mut_obj().set_service_id(1);
  svc->mut_obj().set_host_id(1);
  svc->mut_obj().set_enabled(true);

  host->mut_obj().set_name("host1");
  host->mut_obj().set_host_id(1);
  host->mut_obj().set_enabled(true);

  instance->mut_obj().set_instance_id(3);
  instance->mut_obj().set_name("poller test");
  instance->mut_obj().set_running(true);

  s_event.source_id = 3;
  s_event.destination_id = 4;
  s_event.broker_id = 1;

  auto& cache = config::applier::state::instance().cache();
  cache.publish(host);
  cache.publish(svc);
  cache.publish(instance);

  ASSERT_EQ(q.generate_status(s_event),
            "test_._host1_1_svc1_1_poller_test_3_3__TEST_$ 2 2000\n");
}

TEST_F(graphiteQuery, Except) {
  storage::pb_status s;
  storage::pb_metric m;

  graphite::query q{"test .", "a", graphite::query::metric};
  graphite::query q2{"test .", "a", graphite::query::status};

  try {
    graphite::query q3{"test . $METRICID$", "a", graphite::query::status};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  try {
    graphite::query q3{"test . $METRIC$", "a", graphite::query::status};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  try {
    graphite::query q3{"test . $METRIC", "a", graphite::query::status};
    ASSERT_TRUE(false);
  } catch (msg_fmt const& ex) {
    ASSERT_TRUE(true);
  }

  m.mut_obj().set_metric_id(3);
  m.mut_obj().set_name("The.full.name.A");

  graphite::query q4{"test . $METRICID$ $METRIC$", "a",
                     graphite::query::metric};

  ASSERT_THROW(q.generate_status(s), msg_fmt);
  ASSERT_THROW(q2.generate_metric(m), msg_fmt);
  ASSERT_EQ(q4.generate_metric(m), "test_._3_TheafullanameaA 0 0\n");

  graphite::query q5{"test . $INSTANCE$", "a", graphite::query::metric};
  ASSERT_EQ(q5.generate_metric(m), "");

  graphite::query q6{"test . $INSTANCE$", "a", graphite::query::status};
  ASSERT_EQ(q6.generate_status(s), "");
}
