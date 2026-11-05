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

#include "com/centreon/broker/rrd/retention_manager.hh"

#include <gtest/gtest.h>

#include <fstream>

#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/init.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/rrd/internal.hh"
#include "common/log_v2/log_v2.hh"

namespace fs = std::filesystem;
using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;
using com::centreon::common::log_v2::log_v2;
namespace asio = boost::asio;

namespace {

constexpr uint32_t interval = 60;
constexpr uint32_t rrd_len = 86400;

/**
 * @brief RAII helper: creates unique temporary metrics and status directories
 *        and removes them (recursively) on destruction.
 */
struct tmp_dir {
  fs::path metrics;
  fs::path status;

  tmp_dir() {
    fs::path base =
        fs::temp_directory_path() / fmt::format("rrd_ret_test_{}", ::getpid());
    fs::remove_all(base);
    metrics = base / "metrics";
    status = base / "status";
  }
  ~tmp_dir() {
    std::error_code ec;
    fs::remove_all(metrics.parent_path(), ec);
  }

  fs::path metric_file(uint64_t id, const char* ext = "ret") const {
    return metrics / fmt::format("{}.{}", id, ext);
  }
  fs::path status_file(uint64_t id, const char* ext = "ret") const {
    return status / fmt::format("{}.{}", id, ext);
  }
};

retention_config make_config(const tmp_dir& tmp,
                             uint32_t merge_samples_threshold = 1000,
                             uint32_t orphan_interval = 3600) {
  retention_config cfg;
  cfg.metrics_dir = tmp.metrics;
  cfg.status_dir = tmp.status;
  cfg.merge_samples_threshold = merge_samples_threshold;
  cfg.orphan_interval = orphan_interval;
  return cfg;
}

Metric make_metric(uint64_t metric_id, uint64_t time, double value) {
  Metric m;
  m.set_metric_id(metric_id);
  m.set_time(time);
  m.set_value(value);
  m.set_value_type(Metric_ValueType_GAUGE);
  m.set_rrd_len(rrd_len);
  m.set_interval(interval);
  return m;
}

Status make_status(uint64_t index_id, uint64_t time, uint32_t state) {
  Status s;
  s.set_index_id(index_id);
  s.set_time(time);
  s.set_state(state);
  s.set_rrd_len(rrd_len);
  s.set_interval(interval);
  return s;
}

/// A timestamp aligned on interval, two hours ago.
uint64_t base_time() {
  return (static_cast<uint64_t>(time(nullptr)) / interval) * interval - 7200;
}

std::shared_ptr<spdlog::logger> test_logger() {
  return log_v2::instance().get(log_v2::RRD);
}

using merge_id = std::optional<std::pair<uint64_t, bool>>;

/**
 * @brief Stands for the rrdcached client given to start_merge(), it records
 * the files flushed before a merge.
 */
class flush_recorder : public lib {
 public:
  std::vector<fs::path> flushed;

  flush_recorder() : lib("", 0) {}
  void pre_merge_flush(const fs::path& filename) override {
    flushed.push_back(filename);
  }
};

}  // namespace

/**
 * Merges are posted on _io_context, run by the test with _run_merges(). The
 * end of merge events are read from the multiplexing engine.
 */
class RetentionManager : public ::testing::Test {
 protected:
  tmp_dir _tmp;
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<retention_manager> _rm;
  std::shared_ptr<multiplexing::muxer> _mux;
  std::shared_ptr<lib> _lib = std::make_shared<lib>("", 0);

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
    _mux = multiplexing::muxer::create("rrd_retention_manager_test",
                                       multiplexing::engine::instance_ptr(),
                                       filter, filter, false);
    multiplexing::engine::instance_ptr()->start();
    _io_context = std::make_shared<asio::io_context>();
    retention_manager::reset();
  }

  void TearDown() override {
    _rm.reset();
    retention_manager::reset();
    _mux->unsubscribe();
    _mux.reset();
    config::applier::deinit();
  }

  std::shared_ptr<retention_manager> _load(const retention_config& cfg) {
    _rm = retention_manager::load(cfg, _io_context, test_logger());
    return _rm;
  }

  /// Run the merges posted on _io_context.
  void _run_merges() {
    _io_context->restart();
    _io_context->run();
  }

  /// Wait for the next end of merge event, nullptr after 10s. As the stream
  /// does, the merge is then cleared so that another one can start.
  std::shared_ptr<io::data> _wait_end_merge() {
    std::shared_ptr<io::data> d;
    time_t limit = time(nullptr) + 10;
    while (!d && time(nullptr) < limit)
      _mux->read(d, time(nullptr) + 1);
    if (d) {
      _mux->ack_events(1);
      if (_rm) {
        if (d->type() == storage::pb_end_metric_merge::static_type())
          _rm->clear_merging(
              std::static_pointer_cast<storage::pb_end_metric_merge>(d)
                  ->obj()
                  .metric_id(),
              false);
        else if (d->type() == storage::pb_end_status_merge::static_type())
          _rm->clear_merging(
              std::static_pointer_cast<storage::pb_end_status_merge>(d)
                  ->obj()
                  .index_id(),
              true);
      }
    }
    return d;
  }
};

TEST_F(RetentionManager, LoadCreatesDirectories) {
  auto rm = _load(make_config(_tmp));
  EXPECT_TRUE(rm->enabled());
  EXPECT_TRUE(fs::is_directory(_tmp.metrics));
  EXPECT_TRUE(fs::is_directory(_tmp.status));
}

TEST_F(RetentionManager, DisabledWhenDirsEmpty) {
  auto rm = _load(retention_config{});
  EXPECT_FALSE(rm->enabled());
  // writes must not crash and do nothing
  rm->write_metric(make_metric(1, 1000, 1.0));
  rm->write_status(make_status(1, 1000, 0));
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));
}

TEST_F(RetentionManager, StatusDisabledWhenStatusDirEmpty) {
  retention_config cfg = make_config(_tmp, 0);
  cfg.status_dir.clear();
  auto rm = _load(cfg);
  EXPECT_TRUE(rm->enabled());

  rm->write_status(make_status(3, 1000, 0));
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));

  rm->write_metric(make_metric(3, 1000, 1.0));
  EXPECT_TRUE(fs::exists(_tmp.metric_file(3)));
}

TEST_F(RetentionManager, WriteCreatesOneFilePerId) {
  auto rm = _load(make_config(_tmp));
  const uint64_t base = base_time();

  for (int i = 0; i < 3; ++i) {
    rm->write_metric(make_metric(42, base + i * interval, i));
    rm->write_status(make_status(42, base + i * interval, 0));
  }
  rm->write_metric(make_metric(43, base, 1.0));

  // metric and status with the same id are in their own directory
  EXPECT_EQ(retention_file(_tmp.metric_file(42)).nb_samples(), 3u);
  EXPECT_FALSE(retention_file(_tmp.metric_file(42)).is_status());
  EXPECT_EQ(retention_file(_tmp.status_file(42)).nb_samples(), 3u);
  EXPECT_TRUE(retention_file(_tmp.status_file(42)).is_status());
  EXPECT_EQ(retention_file(_tmp.metric_file(43)).nb_samples(), 1u);
}

TEST_F(RetentionManager, RemoveMetricAndStatus) {
  auto rm = _load(make_config(_tmp, 0));
  const uint64_t base = base_time();

  rm->write_metric(make_metric(20, base, 5.0));
  rm->write_status(make_status(20, base, 1));
  ASSERT_TRUE(fs::exists(_tmp.metric_file(20)));
  ASSERT_TRUE(fs::exists(_tmp.status_file(20)));

  rm->remove_metric(20);
  EXPECT_FALSE(fs::exists(_tmp.metric_file(20)));
  EXPECT_TRUE(fs::exists(_tmp.status_file(20)));

  rm->remove_status(20);
  EXPECT_FALSE(fs::exists(_tmp.status_file(20)));

  // unknown ids are ignored
  rm->remove_metric(404);
  rm->remove_status(404);

  // nothing left to merge
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));

  // a new point creates a new file
  rm->write_metric(make_metric(20, base + interval, 6.0));
  EXPECT_EQ(retention_file(_tmp.metric_file(20)).nb_samples(), 1u);
}

TEST_F(RetentionManager, NoMergeBelowThresholds) {
  auto rm = _load(make_config(_tmp, 3));
  const uint64_t base = base_time();

  EXPECT_FALSE(rm->start_merge(_lib, nullptr));
  for (int i = 0; i < 3; ++i)
    rm->write_metric(make_metric(11, base + i * interval, i));
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));
}

/**
 * A file with more than merge_samples_threshold samples is merged into its rrd
 * file, then removed and the end of the merge is published.
 */
TEST_F(RetentionManager, MergeOnSamplesThreshold) {
  auto rm = _load(make_config(_tmp, 3));
  const uint64_t base = base_time();

  for (int i = 0; i < 4; ++i)
    rm->write_metric(make_metric(11, base + i * interval, i));

  merge_id merging = rm->start_merge(_lib, nullptr);
  ASSERT_TRUE(merging);
  EXPECT_EQ(merging->first, 11u);
  EXPECT_FALSE(merging->second);

  _run_merges();

  EXPECT_FALSE(fs::exists(_tmp.metric_file(11)));
  ASSERT_TRUE(fs::exists(_tmp.metric_file(11, "rrd")));
  rrd_existing_data data =
      _lib->fetch_existing(_tmp.metric_file(11, "rrd"), base, base + 180);
  ASSERT_EQ(data.points.size(), 4u);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(data.points[i].first, base + i * interval);
    EXPECT_DOUBLE_EQ(data.points[i].second, i);
  }

  auto end = _wait_end_merge();
  ASSERT_TRUE(end);
  ASSERT_EQ(end->type(), storage::pb_end_metric_merge::static_type());
  EXPECT_EQ(std::static_pointer_cast<storage::pb_end_metric_merge>(end)
                ->obj()
                .metric_id(),
            11u);

  // nothing left to merge
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));
}

/**
 * A COUNTER is merged in two retention files: the last point of the first one
 * is written in the second one, so that the rate of its first point is known.
 */
TEST_F(RetentionManager, CounterMergedTwiceKeepsRate) {
  auto rm = _load(make_config(_tmp, 3));
  const uint64_t base = base_time();
  auto counter = [base](int i) {
    Metric m = make_metric(13, base + i * interval, 600.0 * i);
    m.set_value_type(Metric_ValueType_COUNTER);
    return m;
  };

  for (int i = 0; i < 4; ++i)
    rm->write_metric(counter(i));
  ASSERT_TRUE(rm->start_merge(_lib, nullptr));
  _run_merges();
  ASSERT_TRUE(_wait_end_merge());
  EXPECT_FALSE(fs::exists(_tmp.metric_file(13)));

  // the new file starts with the last point of the merged one
  rm->write_metric(counter(4));
  EXPECT_EQ(retention_file(_tmp.metric_file(13)).nb_samples(), 2u);
  for (int i = 5; i < 8; ++i)
    rm->write_metric(counter(i));
  ASSERT_TRUE(rm->start_merge(_lib, nullptr));
  _run_merges();
  ASSERT_TRUE(_wait_end_merge());

  // 600 every 60s => 10/s, only the first point of the first file is unknown
  rrd_existing_data data = _lib->fetch_existing(
      _tmp.metric_file(13, "rrd"), base, base + 7 * interval);
  ASSERT_EQ(data.points.size(), 7u);
  for (int i = 0; i < 7; ++i) {
    EXPECT_EQ(data.points[i].first, base + (i + 1) * interval);
    EXPECT_DOUBLE_EQ(data.points[i].second, 10.0);
  }
}

TEST_F(RetentionManager, MergeStatus) {
  auto rm = _load(make_config(_tmp, 1));
  const uint64_t base = base_time();

  rm->write_status(make_status(9, base, 0));
  rm->write_status(make_status(9, base + interval, 2));

  merge_id merging = rm->start_merge(_lib, nullptr);
  ASSERT_TRUE(merging);
  EXPECT_EQ(merging->first, 9u);
  EXPECT_TRUE(merging->second);

  _run_merges();

  EXPECT_FALSE(fs::exists(_tmp.status_file(9)));
  rrd_existing_data data =
      _lib->fetch_existing(_tmp.status_file(9, "rrd"), base, base + interval);
  ASSERT_EQ(data.points.size(), 2u);
  EXPECT_DOUBLE_EQ(data.points[0].second, 100.0);
  EXPECT_DOUBLE_EQ(data.points[1].second, 0.0);

  auto end = _wait_end_merge();
  ASSERT_TRUE(end);
  ASSERT_EQ(end->type(), storage::pb_end_status_merge::static_type());
  EXPECT_EQ(std::static_pointer_cast<storage::pb_end_status_merge>(end)
                ->obj()
                .index_id(),
            9u);
}

/**
 * Only one merge at a time, the biggest file first. The points received
 * during a merge go to a new file.
 */
TEST_F(RetentionManager, OneMergeAtATimeBiggestFirst) {
  auto rm = _load(make_config(_tmp, 2));
  const uint64_t base = base_time();

  for (int i = 0; i < 3; ++i)
    rm->write_metric(make_metric(1, base + i * interval, i));
  for (int i = 0; i < 5; ++i)
    rm->write_metric(make_metric(2, base + i * interval, i));

  merge_id merging = rm->start_merge(_lib, nullptr);
  ASSERT_TRUE(merging);
  EXPECT_EQ(merging->first, 2u);

  // a merge is running
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));

  _run_merges();
  ASSERT_TRUE(_wait_end_merge());

  merging = rm->start_merge(_lib, nullptr);
  ASSERT_TRUE(merging);
  EXPECT_EQ(merging->first, 1u);
  _run_merges();
  ASSERT_TRUE(_wait_end_merge());

  EXPECT_FALSE(rm->start_merge(_lib, nullptr));
}

/**
 * A file not fed for orphan_interval seconds is merged whatever its size.
 */
TEST_F(RetentionManager, MergeOrphan) {
  auto rm = _load(make_config(_tmp, 1000, 0));
  rm->write_metric(make_metric(5, base_time(), 1.0));

  // last_activity + orphan_interval must be in the past
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  merge_id merging = rm->start_merge(_lib, nullptr);
  ASSERT_TRUE(merging);
  EXPECT_EQ(merging->first, 5u);
  _run_merges();
  EXPECT_FALSE(fs::exists(_tmp.metric_file(5)));
  EXPECT_TRUE(fs::exists(_tmp.metric_file(5, "rrd")));
}

/**
 * An aborted merge leaves the rrd file untouched, but the retention file is
 * removed and the end of the merge is published.
 */
TEST_F(RetentionManager, AbortMerge) {
  auto rm = _load(make_config(_tmp, 1));
  const uint64_t base = base_time();

  rm->write_metric(make_metric(6, base, 1.0));
  rm->write_metric(make_metric(6, base + interval, 2.0));
  ASSERT_TRUE(rm->start_merge(_lib, nullptr));

  // another id or a status with the same id: nothing is aborted
  rm->abort_metric_merge(7);
  rm->abort_status_merge(6);
  rm->abort_metric_merge(6);
  _run_merges();

  EXPECT_FALSE(fs::exists(_tmp.metric_file(6)));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(6, "rrd")));
  EXPECT_TRUE(_wait_end_merge());
}

/**
 * A merge failure doesn't block the next merges and doesn't lose the retention
 * data: the retention file is kept, it is fed by the next points and merged
 * again once merge_retry_delay is elapsed.
 */
TEST_F(RetentionManager, MergeErrorKeepsRetentionFile) {
  retention_config cfg = make_config(_tmp, 1);
  cfg.merge_retry_delay = 2;
  auto rm = _load(cfg);
  const uint64_t base = base_time();

  rm->write_metric(make_metric(8, base, 1.0));
  rm->write_metric(make_metric(8, base + interval, 2.0));
  // the rrd file can't be read by librrd
  {
    std::ofstream out(_tmp.metric_file(8, "rrd"));
    out << "not an rrd file";
  }
  ASSERT_TRUE(rm->start_merge(_lib, nullptr));
  _run_merges();
  EXPECT_TRUE(_wait_end_merge());
  ASSERT_TRUE(fs::exists(_tmp.metric_file(8)));
  EXPECT_EQ(retention_file(_tmp.metric_file(8)).nb_samples(), 2u);
  EXPECT_FALSE(fs::exists(_tmp.metric_file(8, "rrd.to_merge")));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(8, "rrd.tmp")));

  // the failed file is not merged again before merge_retry_delay
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));

  // another file can be merged, the biggest one first
  for (int i = 0; i < 3; ++i)
    rm->write_metric(make_metric(9, base + i * interval, i));
  merge_id merging = rm->start_merge(_lib, nullptr);
  ASSERT_TRUE(merging);
  EXPECT_EQ(merging->first, 9u);
  _run_merges();
  ASSERT_TRUE(_wait_end_merge());
  EXPECT_FALSE(fs::exists(_tmp.metric_file(9)));

  // the kept file is still fed
  rm->write_metric(make_metric(8, base + 2 * interval, 3.0));
  EXPECT_EQ(retention_file(_tmp.metric_file(8)).nb_samples(), 3u);

  // once the rrd file is fixed and merge_retry_delay elapsed, the retention
  // file is merged
  fs::remove(_tmp.metric_file(8, "rrd"));
  std::this_thread::sleep_for(std::chrono::seconds(3));
  merging = rm->start_merge(_lib, nullptr);
  ASSERT_TRUE(merging);
  EXPECT_EQ(merging->first, 8u);
  _run_merges();
  ASSERT_TRUE(_wait_end_merge());
  EXPECT_FALSE(fs::exists(_tmp.metric_file(8)));
  rrd_existing_data data = _lib->fetch_existing(_tmp.metric_file(8, "rrd"),
                                                base, base + 2 * interval);
  ASSERT_EQ(data.points.size(), 3u);
  for (int i = 0; i < 3; ++i)
    EXPECT_DOUBLE_EQ(data.points[i].second, 1.0 + i);
}

/**
 * With the default merge_retry_delay (120s), a failed merge is not retried
 * right after the failure.
 */
TEST_F(RetentionManager, MergeErrorDefaultRetryDelay) {
  auto rm = _load(make_config(_tmp, 1));
  const uint64_t base = base_time();

  rm->write_metric(make_metric(8, base, 1.0));
  rm->write_metric(make_metric(8, base + interval, 2.0));
  {
    std::ofstream out(_tmp.metric_file(8, "rrd"));
    out << "not an rrd file";
  }
  ASSERT_TRUE(rm->start_merge(_lib, nullptr));
  _run_merges();
  EXPECT_TRUE(_wait_end_merge());
  ASSERT_TRUE(fs::exists(_tmp.metric_file(8)));

  fs::remove(_tmp.metric_file(8, "rrd"));
  std::this_thread::sleep_for(std::chrono::seconds(2));
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));
}

/**
 * With rrdcached, the rrd file is flushed before its merge is posted, so that
 * the merge reads all the real-time data.
 */
TEST_F(RetentionManager, MergeFlushesCachedClient) {
  auto rm = _load(make_config(_tmp, 1));
  const uint64_t base = base_time();
  flush_recorder cached_client;

  rm->write_metric(make_metric(12, base, 1.0));
  rm->write_metric(make_metric(12, base + interval, 2.0));
  ASSERT_TRUE(rm->start_merge(_lib, &cached_client));
  // flushed by start_merge(), before the merge runs
  EXPECT_EQ(cached_client.flushed,
            std::vector<fs::path>{_tmp.metric_file(12, "rrd")});
  _run_merges();
  ASSERT_TRUE(_wait_end_merge());

  rm->write_status(make_status(12, base, 0));
  rm->write_status(make_status(12, base + interval, 0));
  ASSERT_TRUE(rm->start_merge(_lib, &cached_client));
  ASSERT_EQ(cached_client.flushed.size(), 2u);
  EXPECT_EQ(cached_client.flushed[1], _tmp.status_file(12, "rrd"));
  _run_merges();
  ASSERT_TRUE(_wait_end_merge());

  // no merge, no flush
  EXPECT_FALSE(rm->start_merge(_lib, &cached_client));
  EXPECT_EQ(cached_client.flushed.size(), 2u);
}

/**
 * At restart, the retention files are reloaded with their number of samples,
 * invalid files are ignored or removed.
 */
TEST_F(RetentionManager, LoadReloadsFiles) {
  const uint64_t base = base_time();
  {
    auto rm = _load(make_config(_tmp));
    for (int i = 0; i < 4; ++i) {
      rm->write_metric(make_metric(30, base + i * interval, i));
      rm->write_status(make_status(31, base + i * interval, 0));
    }
  }
  // restart
  retention_manager::reset();
  {
    std::ofstream out(_tmp.metrics / "foo.ret");
    out << "ignored";
  }
  {
    std::ofstream out(_tmp.metric_file(32));
    out << "too short";
  }

  auto rm = _load(make_config(_tmp, 3));
  EXPECT_TRUE(fs::exists(_tmp.metrics / "foo.ret"));
  EXPECT_FALSE(fs::exists(_tmp.metric_file(32)));

  // both reloaded files have more than 3 samples
  std::set<std::pair<uint64_t, bool>> merged;
  for (int i = 0; i < 2; ++i) {
    merge_id merging = rm->start_merge(_lib, nullptr);
    ASSERT_TRUE(merging);
    merged.insert(*merging);
    _run_merges();
    ASSERT_TRUE(_wait_end_merge());
  }
  EXPECT_EQ(merged,
            (std::set<std::pair<uint64_t, bool>>{{30, false}, {31, true}}));
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));
}

TEST_F(RetentionManager, ReloadedFileIsAppended) {
  const uint64_t base = base_time();
  {
    auto rm = _load(make_config(_tmp));
    rm->write_metric(make_metric(34, base, 1.0));
  }
  // restart
  retention_manager::reset();
  auto rm = _load(make_config(_tmp));
  rm->write_metric(make_metric(34, base + interval, 2.0));
  EXPECT_EQ(retention_file(_tmp.metric_file(34)).nb_samples(), 2u);
}

/**
 * load() returns the same retention_manager until reset(): only the merge
 * thresholds of the next configurations are applied, the directories of the
 * first one are kept.
 */
TEST_F(RetentionManager, LoadSharesInstanceUntilReset) {
  const uint64_t base = base_time();
  auto rm = _load(make_config(_tmp, 1000));
  for (int i = 0; i < 3; ++i)
    rm->write_metric(make_metric(40, base + i * interval, i));
  EXPECT_FALSE(rm->start_merge(_lib, nullptr));

  // a stream opened again, with a lower threshold and no status directory
  retention_config cfg = make_config(_tmp, 2);
  cfg.status_dir.clear();
  auto rm2 = _load(cfg);
  EXPECT_EQ(rm2, rm);
  rm2->write_status(make_status(41, base, 0));
  EXPECT_TRUE(fs::exists(_tmp.status_file(41)));
  merge_id merging = rm2->start_merge(_lib, nullptr);
  ASSERT_TRUE(merging);
  EXPECT_EQ(merging->first, 40u);
  _run_merges();
  ASSERT_TRUE(_wait_end_merge());

  retention_manager::reset();
  auto rm3 = _load(make_config(_tmp));
  EXPECT_NE(rm3, rm);
}
