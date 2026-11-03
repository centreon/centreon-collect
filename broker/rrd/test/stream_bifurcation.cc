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

/**
 * @file stream_bifurcation.cc
 * @brief Unit tests of the routing of metrics and statuses in stream<lib>
 * between the rrd files and the retention.
 *
 * Rule under test (stream.cc write()):
 *   - time > last update of the rrd file  →  rrd file
 *   - time <= last update of the rrd file  →  retention file {id}.ret, merged
 *     later into the rrd file.
 */

#include "com/centreon/broker/rrd/stream.hh"

#include <gtest/gtest.h>
#include <unistd.h>

#include <filesystem>

#include "bbdo/storage.pb.h"
#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/init.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/rrd/internal.hh"
#include "com/centreon/broker/rrd/retention_file.hh"

namespace fs = std::filesystem;
using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

namespace {

constexpr uint32_t interval = 60;
constexpr uint32_t rrd_len = 86400;

/**
 * @brief RAII helper: unique temp directories, cleaned up on destruction.
 */
struct tmp_dirs {
  fs::path metrics;
  fs::path status;

  tmp_dirs() {
    fs::path base = fs::temp_directory_path() /
                    fmt::format("rrd_bifurc_test_{}", ::getpid());
    fs::remove_all(base);
    metrics = base / "metrics";
    status = base / "status";
    fs::create_directories(metrics);
    fs::create_directories(status);
  }

  ~tmp_dirs() {
    std::error_code ec;
    fs::remove_all(metrics.parent_path(), ec);
  }

  fs::path metric_file(uint64_t id, const char* ext = "rrd") const {
    return metrics / fmt::format("{}.{}", id, ext);
  }
  fs::path status_file(uint64_t id, const char* ext = "rrd") const {
    return status / fmt::format("{}.{}", id, ext);
  }
};

retention_config make_cfg(const tmp_dirs& tmp,
                          uint32_t merge_samples_threshold = 1000) {
  retention_config cfg;
  cfg.metrics_dir = tmp.metrics;
  cfg.status_dir = tmp.status;
  cfg.merge_samples_threshold = merge_samples_threshold;
  return cfg;
}

std::shared_ptr<storage::pb_metric> make_metric(uint64_t id,
                                                uint64_t time,
                                                double value) {
  auto e = std::make_shared<storage::pb_metric>();
  Metric& m = e->mut_obj();
  m.set_metric_id(id);
  m.set_time(time);
  m.set_interval(interval);
  m.set_rrd_len(rrd_len);
  m.set_value(value);
  m.set_value_type(Metric_ValueType_GAUGE);
  return e;
}

std::shared_ptr<storage::pb_status> make_status(uint64_t id,
                                                uint64_t time,
                                                uint32_t state) {
  auto e = std::make_shared<storage::pb_status>();
  Status& st = e->mut_obj();
  st.set_index_id(id);
  st.set_time(time);
  st.set_interval(interval);
  st.set_rrd_len(rrd_len);
  st.set_state(state);
  return e;
}

/**
 * @brief Build a START or END RebuildMessage for a single metric/index pair.
 */
std::shared_ptr<storage::pb_rebuild_message> make_rebuild_boundary(
    RebuildMessage_State state,
    uint64_t metric_id,
    uint64_t index_id) {
  auto msg = std::make_shared<storage::pb_rebuild_message>();
  msg->mut_obj().set_state(state);
  (*msg->mut_obj().mutable_metric_to_index_id())[metric_id] = index_id;
  return msg;
}

/**
 * @brief Build a DATA RebuildMessage with @p n sequential GAUGE points
 *        starting at @p base_time, spaced by interval.
 */
std::shared_ptr<storage::pb_rebuild_message> make_rebuild_data(
    uint64_t metric_id,
    uint64_t base_time,
    int n) {
  auto msg = std::make_shared<storage::pb_rebuild_message>();
  msg->mut_obj().set_state(RebuildMessage_State_DATA);
  auto& ts = (*msg->mut_obj().mutable_timeserie())[metric_id];
  ts.set_data_source_type(0);  // GAUGE
  ts.set_check_interval(interval);
  ts.set_rrd_retention(rrd_len);
  for (int i = 0; i < n; ++i) {
    auto* pt = ts.add_pts();
    pt->set_ctime(static_cast<int64_t>(base_time + i * interval));
    pt->set_value(static_cast<double>(i));
    pt->set_status(0);
  }
  return msg;
}

/// A timestamp aligned on interval, two hours ago.
uint64_t base_time() {
  return (static_cast<uint64_t>(time(nullptr)) / interval) * interval - 7200;
}

/// Number of samples of a retention file, 0 if it doesn't exist.
size_t nb_samples(const fs::path& ret_path) {
  if (!fs::exists(ret_path))
    return 0;
  return retention_file(ret_path).nb_samples();
}

/// Known values of an rrd file between from and to, by timestamp.
std::map<uint64_t, double> fetch(const fs::path& rrd_path,
                                 uint64_t from,
                                 uint64_t to) {
  lib l("", 0);
  rrd_existing_data data = l.fetch_existing(rrd_path, from, to);
  return {data.points.begin(), data.points.end()};
}

}  // namespace

/**
 * Merges run on the thread pool, their end events are read from the
 * multiplexing engine and given back to the stream by _end_merge(), as the
 * multiplexing engine does.
 */
class StreamBifurcation : public ::testing::Test {
 protected:
  tmp_dirs _tmp;
  std::shared_ptr<multiplexing::muxer> _mux;

 public:
  void SetUp() override {
    try {
      config::applier::init<
          com::centreon::broker::config::applier::broker_state>(
          "", 0, "test_broker", 0);
    } catch (const std::exception&) {
    }
    config::applier::state::instance().initialize_cache();
    multiplexing::muxer_filter filter{
        storage::pb_end_metric_merge::static_type(),
        storage::pb_end_status_merge::static_type()};
    _mux = multiplexing::muxer::create("rrd_stream_bifurcation_test",
                                       multiplexing::engine::instance_ptr(),
                                       filter, filter, false);
    multiplexing::engine::instance_ptr()->start();
  }

  void TearDown() override {
    _mux->unsubscribe();
    _mux.reset();
    config::applier::deinit();
  }

  /// Wait for the end of the running merge and give it to the stream.
  void _end_merge(stream<lib>& s) {
    std::shared_ptr<io::data> d;
    time_t limit = time(nullptr) + 10;
    while (!d && time(nullptr) < limit)
      _mux->read(d, time(nullptr) + 1);
    ASSERT_TRUE(d) << "no end of merge received";
    _mux->ack_events(1);
    s.write(d);
  }
};

/**
 * The first point of a metric creates its rrd file.
 */
TEST_F(StreamBifurcation, FirstMetricGoesToRrd) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp));
  const uint64_t now = time(nullptr);

  s.write(make_metric(1001, now, 3.14));

  EXPECT_TRUE(fs::exists(_tmp.metric_file(1001)));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(1001, "ret")));
  EXPECT_EQ(lib::last_update(_tmp.metric_file(1001)), static_cast<time_t>(now));
}

/**
 * Points newer than the last update go to the rrd file, older ones (or at the
 * same time) to the retention.
 */
TEST_F(StreamBifurcation, OldMetricGoesToRetention) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp));
  const uint64_t base = base_time();

  for (int i = 5; i < 8; ++i)
    s.write(make_metric(1002, base + i * interval, i));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(1002, "ret")));

  s.write(make_metric(1002, base + 7 * interval, 7.5));
  s.write(make_metric(1002, base + 2 * interval, 2));
  s.write(make_metric(1002, base + 6 * interval + 30, 6.5));
  EXPECT_EQ(nb_samples(_tmp.metric_file(1002, "ret")), 3u);

  // new points still go to the rrd file
  s.write(make_metric(1002, base + 8 * interval, 8));
  EXPECT_EQ(nb_samples(_tmp.metric_file(1002, "ret")), 3u);
  EXPECT_EQ(lib::last_update(_tmp.metric_file(1002)),
            static_cast<time_t>(base + 8 * interval));
}

TEST_F(StreamBifurcation, OldStatusGoesToRetention) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp));
  const uint64_t base = base_time();

  s.write(make_status(2001, base + 5 * interval, 0));
  s.write(make_status(2001, base + 6 * interval, 1));
  EXPECT_TRUE(fs::exists(_tmp.status_file(2001)));
  EXPECT_FALSE(fs::exists(_tmp.status_file(2001, "ret")));

  s.write(make_status(2001, base, 2));
  EXPECT_EQ(nb_samples(_tmp.status_file(2001, "ret")), 1u);
  EXPECT_EQ(lib::last_update(_tmp.status_file(2001)),
            static_cast<time_t>(base + 6 * interval));
}

/**
 * The last update of the status rrd file is used for a status, not the one of
 * the metric with the same id.
 */
TEST_F(StreamBifurcation, StatusAndMetricWithSameId) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp));
  const uint64_t base = base_time();

  s.write(make_metric(10, base + 10 * interval, 1.0));
  s.write(make_status(10, base, 0));

  EXPECT_TRUE(fs::exists(_tmp.status_file(10)));
  EXPECT_FALSE(fs::exists(_tmp.status_file(10, "ret")));

  s.write(make_metric(10, base + interval, 1.0));
  EXPECT_EQ(nb_samples(_tmp.metric_file(10, "ret")), 1u);
  EXPECT_FALSE(fs::exists(_tmp.status_file(10, "ret")));
}

/**
 * Without retention, old points are dropped and the rrd file is not modified.
 */
TEST_F(StreamBifurcation, RetentionDisabledOldDataDropped) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, retention_config{});
  const uint64_t base = base_time();

  s.write(make_metric(3001, base + 5 * interval, 1.0));
  s.write(make_metric(3001, base, 2.0));

  EXPECT_FALSE(fs::exists(_tmp.metric_file(3001, "ret")));
  EXPECT_EQ(lib::last_update(_tmp.metric_file(3001)),
            static_cast<time_t>(base + 5 * interval));
}

TEST_F(StreamBifurcation, WriteMetricsDisabled) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp),
                /*write_metrics=*/false, /*write_status=*/true);
  const uint64_t base = base_time();

  s.write(make_metric(3002, base, 1.0));
  s.write(make_status(3002, base, 0));

  EXPECT_FALSE(fs::exists(_tmp.metric_file(3002)));
  EXPECT_TRUE(fs::exists(_tmp.status_file(3002)));
}

TEST_F(StreamBifurcation, WriteStatusDisabled) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp),
                /*write_metrics=*/true, /*write_status=*/false);
  const uint64_t base = base_time();

  s.write(make_metric(3003, base, 1.0));
  s.write(make_status(3003, base, 0));

  EXPECT_TRUE(fs::exists(_tmp.metric_file(3003)));
  EXPECT_FALSE(fs::exists(_tmp.status_file(3003)));
}

/**
 * When the retention file has more than merge_samples_threshold samples, it is
 * merged into the rrd file. Events of this metric received during the merge
 * are postponed and replayed at its end.
 */
TEST_F(StreamBifurcation, MergeWithPostponedEvents) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp, 2));
  const uint64_t base = base_time();

  // real-time data
  for (int i = 10; i <= 15; ++i)
    s.write(make_metric(4001, base + i * interval, 100 + i));
  // old data, the third one starts the merge
  for (int i = 0; i < 3; ++i)
    s.write(make_metric(4001, base + i * interval, i));

  // during the merge, events are postponed
  s.write(make_metric(4001, base + 3 * interval, 3));
  s.write(make_metric(4001, base + 16 * interval, 116));

  _end_merge(s);

  // the old point replayed is in a new retention file
  EXPECT_EQ(nb_samples(_tmp.metric_file(4001, "ret")), 1u);
  EXPECT_EQ(lib::last_update(_tmp.metric_file(4001)),
            static_cast<time_t>(base + 16 * interval));

  auto values = fetch(_tmp.metric_file(4001), base, base + 16 * interval);
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(values.count(base + i * interval)) << "retention " << i;
    EXPECT_DOUBLE_EQ(values[base + i * interval], i);
  }
  // the first real-time value is unknown since the creation of the file
  for (int i = 11; i <= 16; ++i) {
    ASSERT_TRUE(values.count(base + i * interval)) << "real-time " << i;
    EXPECT_DOUBLE_EQ(values[base + i * interval], 100 + i);
  }
}

TEST_F(StreamBifurcation, StatusMerge) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp, 1));
  const uint64_t base = base_time();

  for (int i = 10; i <= 12; ++i)
    s.write(make_status(4002, base + i * interval, 0));
  s.write(make_status(4002, base, 2));
  s.write(make_status(4002, base + interval, 1));

  _end_merge(s);

  EXPECT_FALSE(fs::exists(_tmp.status_file(4002, "ret")));
  auto values = fetch(_tmp.status_file(4002), base, base + 12 * interval);
  EXPECT_DOUBLE_EQ(values[base], 0.0);
  EXPECT_DOUBLE_EQ(values[base + interval], 75.0);
  EXPECT_DOUBLE_EQ(values[base + 12 * interval], 100.0);

  // new events are no longer postponed
  s.write(make_status(4002, base + 13 * interval, 0));
  EXPECT_EQ(lib::last_update(_tmp.status_file(4002)),
            static_cast<time_t>(base + 13 * interval));
}

/**
 * The retention files of a previous run are reloaded and merged.
 */
TEST_F(StreamBifurcation, RestartReloadsRetention) {
  const uint64_t base = base_time();
  {
    stream<lib> s(_tmp.metrics, _tmp.status, 16,
                  /*ignore_update_errors=*/true, make_cfg(_tmp));
    for (int i = 10; i <= 12; ++i)
      s.write(make_metric(5001, base + i * interval, 100 + i));
    s.write(make_metric(5001, base, 0));
    s.write(make_metric(5001, base + interval, 1));
  }
  ASSERT_EQ(nb_samples(_tmp.metric_file(5001, "ret")), 2u);

  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp, 1));
  // any write starts the merge
  s.write(make_metric(5001, base + 13 * interval, 113));
  _end_merge(s);

  EXPECT_FALSE(fs::exists(_tmp.metric_file(5001, "ret")));
  auto values = fetch(_tmp.metric_file(5001), base, base + 13 * interval);
  EXPECT_DOUBLE_EQ(values[base], 0.0);
  EXPECT_DOUBLE_EQ(values[base + interval], 1.0);
  EXPECT_DOUBLE_EQ(values[base + 13 * interval], 113.0);
}

/**
 * A rebuild removes the rrd and retention files, events received during the
 * rebuild are written at its end.
 */
TEST_F(StreamBifurcation, RebuildRemovesRetention) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp));
  const uint64_t base = base_time();
  const uint64_t metric_id = 6001;
  const uint64_t index_id = 6002;

  s.write(make_metric(metric_id, base + 10 * interval, 1));
  s.write(make_metric(metric_id, base, 2));
  s.write(make_status(index_id, base + 10 * interval, 0));
  s.write(make_status(index_id, base, 0));
  ASSERT_TRUE(fs::exists(_tmp.metric_file(metric_id, "ret")));
  ASSERT_TRUE(fs::exists(_tmp.status_file(index_id, "ret")));

  s.write(
      make_rebuild_boundary(RebuildMessage_State_START, metric_id, index_id));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(metric_id)));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(metric_id, "ret")));
  EXPECT_FALSE(fs::exists(_tmp.status_file(index_id)));
  EXPECT_FALSE(fs::exists(_tmp.status_file(index_id, "ret")));

  // postponed until the end of the rebuild
  s.write(make_metric(metric_id, base + 20 * interval, 20));
  s.write(make_rebuild_data(metric_id, base, 5));
  // rebuild data never go to the retention
  EXPECT_FALSE(fs::exists(_tmp.metric_file(metric_id, "ret")));

  s.write(make_rebuild_boundary(RebuildMessage_State_END, metric_id, index_id));
  EXPECT_EQ(lib::last_update(_tmp.metric_file(metric_id)),
            static_cast<time_t>(base + 20 * interval));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(metric_id, "ret")));
}

TEST_F(StreamBifurcation, RemoveGraphRemovesRetention) {
  stream<lib> s(_tmp.metrics, _tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(_tmp));
  const uint64_t base = base_time();

  s.write(make_metric(7001, base + 10 * interval, 1));
  s.write(make_metric(7001, base, 2));
  s.write(make_status(7002, base + 10 * interval, 0));
  s.write(make_status(7002, base, 0));

  auto e = std::make_shared<storage::pb_remove_graph_message>();
  e->mut_obj().add_metric_ids(7001);
  e->mut_obj().add_index_ids(7002);
  s.write(e);

  EXPECT_FALSE(fs::exists(_tmp.metric_file(7001)));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(7001, "ret")));
  EXPECT_FALSE(fs::exists(_tmp.status_file(7002)));
  EXPECT_FALSE(fs::exists(_tmp.status_file(7002, "ret")));

  // the metric starts again from scratch
  s.write(make_metric(7001, base, 2));
  EXPECT_TRUE(fs::exists(_tmp.metric_file(7001)));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(7001, "ret")));
}
