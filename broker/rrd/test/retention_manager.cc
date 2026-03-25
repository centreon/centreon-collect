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

#include "common/log_v2/log_v2.hh"

namespace fs = std::filesystem;
using namespace com::centreon::broker::rrd;
using com::centreon::common::log_v2::log_v2;

namespace {

/**
 * @brief RAII helper: creates unique temporary metrics and status directories
 *        and removes them (recursively) on destruction.
 */
struct tmp_dir {
  fs::path metrics;
  fs::path status;

  tmp_dir() {
    fs::path base = fs::temp_directory_path() /
                    fmt::format("rrd_ret_test_{}", ::getpid());
    metrics = base / "metrics";
    status = base / "status";
    fs::create_directories(metrics);
    fs::create_directories(status);
  }
  ~tmp_dir() {
    std::error_code ec;
    fs::remove_all(metrics.parent_path(), ec);
  }

  // Count files matching a glob-like prefix in the metrics directory.
  size_t count_metric_files(const std::string& prefix) const {
    return _count(metrics, prefix);
  }

  // Count files matching a glob-like prefix in the status directory.
  size_t count_status_files(const std::string& prefix) const {
    return _count(status, prefix);
  }

 private:
  static size_t _count(const fs::path& dir, const std::string& prefix) {
    size_t n = 0;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
      if (e.path().filename().string().rfind(prefix, 0) == 0)
        ++n;
    return n;
  }
};

retention_config make_config(const fs::path& metrics_dir,
                            const fs::path& status_dir,
                            uint32_t max_pending_points = 1024 * 1024,
                            uint32_t max_files = 5,
                            uint32_t orphan_interval = 3600) {
  retention_config cfg;
  cfg.metrics_dir = metrics_dir;
  cfg.status_dir = status_dir;
  cfg.max_pending_points = max_pending_points;
  cfg.max_files = max_files;
  cfg.orphan_interval = orphan_interval;
  return cfg;
}

std::shared_ptr<spdlog::logger> test_logger() {
  return log_v2::instance().get(log_v2::RRD);
}

}  // namespace

// ---------------------------------------------------------------------------
// Basic write / read-back
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, WriteMetricAndReadBack) {
  tmp_dir tmp;
  retention_manager rm(make_config(tmp.metrics, tmp.status), test_logger());
  rm.init();

  rm.write_metric(42, 1000, 3.14, 60);
  rm.write_metric(42, 1060, 2.71, 60);
  rm.write_metric(42, 1120, 1.41, 60);

  auto pts = rm.get_metric_merge_points(42);
  ASSERT_EQ(pts.size(), 3u);
  EXPECT_EQ(pts[0].first, 1000u);
  EXPECT_DOUBLE_EQ(pts[0].second, 3.14);
  EXPECT_EQ(pts[1].first, 1060u);
  EXPECT_DOUBLE_EQ(pts[1].second, 2.71);
  EXPECT_EQ(pts[2].first, 1120u);
  EXPECT_DOUBLE_EQ(pts[2].second, 1.41);
}

TEST(RetentionManagerTest, WriteStatusAndReadBack) {
  tmp_dir tmp;
  retention_manager rm(make_config(tmp.metrics, tmp.status), test_logger());
  rm.init();

  rm.write_status(7, 2000, 0, 300);
  rm.write_status(7, 2300, 1, 300);
  rm.write_status(7, 2600, 2, 300);

  auto pts = rm.get_status_merge_points(7);
  ASSERT_EQ(pts.size(), 3u);
  EXPECT_EQ(pts[0].first, 2000u);
  EXPECT_EQ(pts[0].second, 0u);
  EXPECT_EQ(pts[1].second, 1u);
  EXPECT_EQ(pts[2].second, 2u);
}

// ---------------------------------------------------------------------------
// Empty / unknown ID
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, UnknownMetricReturnsEmpty) {
  tmp_dir tmp;
  retention_manager rm(make_config(tmp.metrics, tmp.status), test_logger());
  rm.init();

  EXPECT_TRUE(rm.get_metric_merge_points(99).empty());
  EXPECT_TRUE(rm.get_status_merge_points(99).empty());
}

// ---------------------------------------------------------------------------
// merge_done clears files
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, MetricMergeDoneClearsFiles) {
  tmp_dir tmp;
  // max_pending_points=1 forces rotation on every write, so files land on disk.
  retention_manager rm(
      make_config(tmp.metrics, tmp.status, /*max_pending_points=*/1), test_logger());
  rm.init();

  rm.write_metric(1, 500, 1.0, 60);
  rm.write_metric(1, 560, 2.0, 60);

  // Files must exist before merge_done.
  EXPECT_GT(tmp.count_metric_files("1"), 0u);

  rm.metric_merge_done(1);

  // Files must be gone after merge_done.
  EXPECT_EQ(tmp.count_metric_files("1"), 0u);

  // Subsequent read returns empty.
  EXPECT_TRUE(rm.get_metric_merge_points(1).empty());
}

TEST(RetentionManagerTest, StatusMergeDoneClearsFiles) {
  tmp_dir tmp;
  retention_manager rm(make_config(tmp.metrics, tmp.status), test_logger());
  rm.init();

  rm.write_status(3, 100, 0, 60);
  rm.status_merge_done(3);

  EXPECT_EQ(tmp.count_status_files("3"), 0u);
  EXPECT_TRUE(rm.get_status_merge_points(3).empty());
}

// ---------------------------------------------------------------------------
// File rotation
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, RotationCreatesRotatedFile) {
  tmp_dir tmp;
  // max_pending_points=1 so rotation happens after the first record.
  retention_manager rm(
      make_config(tmp.metrics, tmp.status, /*max_pending_points=*/1), test_logger());
  rm.init();

  rm.write_metric(5, 1000, 1.0, 60);
  // After rotation there should be a rotated file (5.<ts>.prot).
  rm.write_metric(5, 1060, 2.0, 60);

  // At least one rotated file should exist.
  size_t rotated = 0;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(tmp.metrics, ec)) {
    const std::string name = e.path().filename().string();
    // Rotated files have the pattern 5.<digits>.prot
    if (name.rfind("5.", 0) == 0 && name != "5.prot")
      ++rotated;
  }
  EXPECT_GE(rotated, 1u);
}

TEST(RetentionManagerTest, RotationPreservesAllPoints) {
  tmp_dir tmp;
  retention_manager rm(
      make_config(tmp.metrics, tmp.status, /*max_pending_points=*/1), test_logger());
  rm.init();

  const int N = 10;
  for (int i = 0; i < N; ++i)
    rm.write_metric(8, static_cast<uint64_t>(1000 + i * 60),
                    static_cast<double>(i), 60);

  auto pts = rm.get_metric_merge_points(8);
  ASSERT_EQ(pts.size(), static_cast<size_t>(N));
  for (int i = 0; i < N; ++i) {
    EXPECT_EQ(pts[i].first, static_cast<uint64_t>(1000 + i * 60));
    EXPECT_DOUBLE_EQ(pts[i].second, static_cast<double>(i));
  }
}

// ---------------------------------------------------------------------------
// Merge trigger on file-count limit
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, MergeTriggeredOnFileCountLimit) {
  tmp_dir tmp;
  // max_pending_points=1 so every write rotates; max_files=3
  retention_manager rm(make_config(tmp.metrics, tmp.status, 1, 3),
                       test_logger());
  rm.init();

  bool triggered = false;
  for (int i = 0; i < 20 && !triggered; ++i)
    triggered = rm.write_metric(11, static_cast<uint64_t>(1000 + i * 60),
                                static_cast<double>(i), 60);

  EXPECT_TRUE(triggered);
}

// ---------------------------------------------------------------------------
// remove_metric / remove_status
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, RemoveMetricDeletesFiles) {
  tmp_dir tmp;
  retention_manager rm(
      make_config(tmp.metrics, tmp.status, /*max_pending_points=*/1), test_logger());
  rm.init();

  rm.write_metric(20, 1000, 5.0, 60);
  EXPECT_GT(tmp.count_metric_files("20"), 0u);

  rm.remove_metric(20);
  EXPECT_EQ(tmp.count_metric_files("20"), 0u);
}

TEST(RetentionManagerTest, RemoveStatusDeletesFiles) {
  tmp_dir tmp;
  retention_manager rm(
      make_config(tmp.metrics, tmp.status, /*max_pending_points=*/1), test_logger());
  rm.init();

  rm.write_status(21, 1000, 1, 300);
  EXPECT_GT(tmp.count_status_files("21"), 0u);

  rm.remove_status(21);
  EXPECT_EQ(tmp.count_status_files("21"), 0u);
}

// ---------------------------------------------------------------------------
// init() at restart: existing files are recovered
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, InitRecoversExistingFiles) {
  tmp_dir tmp;

  // First instance: write some points, then "crash" (destroy without merge).
  {
    retention_manager rm(make_config(tmp.metrics, tmp.status), test_logger());
    rm.init();
    rm.write_metric(30, 5000, 9.9, 60);
    rm.write_metric(30, 5060, 8.8, 60);
    // Destroyed without calling merge_done — files stay on disk.
  }

  // Files should still be there.
  ASSERT_GT(tmp.count_metric_files("30"), 0u);

  // Second instance: init() scans and finds the existing files.
  {
    retention_manager rm(make_config(tmp.metrics, tmp.status), test_logger());
    rm.init();

    auto pts = rm.get_metric_merge_points(30);
    ASSERT_EQ(pts.size(), 2u);
    EXPECT_EQ(pts[0].first, 5000u);
    EXPECT_DOUBLE_EQ(pts[0].second, 9.9);
    EXPECT_EQ(pts[1].first, 5060u);
    EXPECT_DOUBLE_EQ(pts[1].second, 8.8);
  }
}

TEST(RetentionManagerTest, InitRecoversRotatedAndCurrentFiles) {
  tmp_dir tmp;

  {
    // Use max_pending_points=1 to force rotation.
    retention_manager rm(make_config(tmp.metrics, tmp.status, 1), test_logger());
    rm.init();
    for (int i = 0; i < 5; ++i)
      rm.write_metric(31, static_cast<uint64_t>(1000 + i * 60),
                      static_cast<double>(i), 60);
    // Destroyed without merge.
  }

  {
    retention_manager rm(make_config(tmp.metrics, tmp.status, 1), test_logger());
    rm.init();
    auto pts = rm.get_metric_merge_points(31);
    ASSERT_EQ(pts.size(), 5u);
    for (int i = 0; i < 5; ++i)
      EXPECT_DOUBLE_EQ(pts[i].second, static_cast<double>(i));
  }
}

// ---------------------------------------------------------------------------
// cleanup_orphans
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, CleanupOrphansRemovesInactiveMetrics) {
  tmp_dir tmp;
  // max_pending_points=1 forces rotation so the file lands on disk; orphan_interval=10s
  retention_manager rm(make_config(tmp.metrics, tmp.status, 1, 5, 10),
                       test_logger());
  rm.init();

  rm.write_metric(50, 1000, 1.0, 60);
  EXPECT_GT(tmp.count_metric_files("50"), 0u);

  // Simulate the metric being inactive: call cleanup with now = last_activity + 11
  // We don't have direct access to last_activity_time, so we use a large now.
  uint64_t far_future = static_cast<uint64_t>(std::time(nullptr)) + 3600;
  rm.cleanup_orphans(far_future);

  EXPECT_EQ(tmp.count_metric_files("50"), 0u);
  EXPECT_TRUE(rm.get_metric_merge_points(50).empty());
}

TEST(RetentionManagerTest, CleanupOrphansKeepsRecentMetrics) {
  tmp_dir tmp;
  // max_pending_points=1 forces rotation so the file lands on disk; orphan_interval=3600s
  retention_manager rm(make_config(tmp.metrics, tmp.status, 1, 5, 3600),
                       test_logger());
  rm.init();

  rm.write_metric(51, 2000, 7.7, 60);

  // cleanup_orphans with now = current time (metric was just written)
  rm.cleanup_orphans(static_cast<uint64_t>(std::time(nullptr)));

  // File should still be there and data intact.
  EXPECT_GT(tmp.count_metric_files("51"), 0u);
  EXPECT_EQ(rm.get_metric_merge_points(51).size(), 1u);
}

// ---------------------------------------------------------------------------
// Multiple metrics are independent
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, MultipleMetricsAreIndependent) {
  tmp_dir tmp;
  retention_manager rm(make_config(tmp.metrics, tmp.status), test_logger());
  rm.init();

  rm.write_metric(100, 1000, 1.0, 60);
  rm.write_metric(101, 2000, 2.0, 60);
  rm.write_metric(102, 3000, 3.0, 60);

  rm.metric_merge_done(101);  // clear only metric 101

  auto pts100 = rm.get_metric_merge_points(100);
  auto pts101 = rm.get_metric_merge_points(101);
  auto pts102 = rm.get_metric_merge_points(102);

  EXPECT_EQ(pts100.size(), 1u);
  EXPECT_TRUE(pts101.empty());
  EXPECT_EQ(pts102.size(), 1u);
}

// ---------------------------------------------------------------------------
// disabled (empty dir) — no crash, no files created
// ---------------------------------------------------------------------------

TEST(RetentionManagerTest, DisabledWhenDirEmpty) {
  retention_config cfg;  // dir is empty → disabled
  retention_manager rm(cfg, test_logger());

  EXPECT_FALSE(rm.enabled());
  // init() and writes must not crash
  rm.init();
  EXPECT_FALSE(rm.write_metric(1, 1000, 1.0, 60));
  EXPECT_TRUE(rm.get_metric_merge_points(1).empty());
}
