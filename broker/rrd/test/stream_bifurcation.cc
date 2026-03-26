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
 * @brief Unit tests for the Step-2 write-path bifurcation in stream<lib>.
 *
 * Rule under test (stream.cc write()):
 *   - timestamp >= now - step  →  current data  →  RRD backend (no .prot)
 *   - timestamp <  now - step  →  old / backfill →  retention buffer (.prot)
 *
 * Observable side effects used to verify routing:
 *   - Old data  : a .prot file appears in the retention directory
 *                 (rotation forced by max_pending_points=1).
 *   - Current data : no .prot file; the .rrd file is created/updated by the
 *                    backend.
 */

#include "com/centreon/broker/rrd/stream.hh"

#include <gtest/gtest.h>
#include <unistd.h>

#include <ctime>
#include <filesystem>

#include "bbdo/storage.pb.h"
#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/init.hh"
#include "com/centreon/broker/rrd/internal.hh"

namespace fs = std::filesystem;
using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

// ---------------------------------------------------------------------------
// Test fixture: initialises the broker applier (needed for log_v2).
// ---------------------------------------------------------------------------

class StreamBifurcation : public ::testing::Test {
 public:
  void SetUp() override {
    try {
      config::applier::init<
          com::centreon::broker::config::applier::broker_state>(
          "", 0, "test_broker", 0);
    } catch (const std::exception&) {
    }
  }

  void TearDown() override { config::applier::deinit(); }
};

// ---------------------------------------------------------------------------
// RAII helper: unique temp directories, cleaned up on destruction.
// ---------------------------------------------------------------------------

namespace {

struct tmp_dirs {
  fs::path metrics;
  fs::path status;

  tmp_dirs() {
    fs::path base = fs::temp_directory_path() /
                    fmt::format("rrd_bifurc_test_{}", ::getpid());
    metrics = base / "metrics";
    status = base / "status";
    fs::create_directories(metrics);
    fs::create_directories(status);
  }

  ~tmp_dirs() {
    std::error_code ec;
    fs::remove_all(metrics.parent_path(), ec);
  }

  /**
   * @brief Count .prot files whose name starts with "{id}." in @p dir.
   *
   * Matches "{id}.prot" (shutdown flush) and "{id}.{ts}.prot" (rotated).
   */
  static size_t count_prot(const fs::path& dir, uint64_t id) {
    const std::string prefix = fmt::format("{}.", id);
    size_t n = 0;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
      const std::string name = e.path().filename().string();
      if (name.rfind(prefix, 0) == 0 && absl::EndsWith(name, ".prot"))
        ++n;
    }
    return n;
  }
};

/**
 * @brief Build a retention_config with max_pending_points=1 so a rotation
 *        (and therefore a .prot file) is produced on the very first write.
 */
retention_config make_cfg(const tmp_dirs& tmp,
                          uint32_t max_pending = 1,
                          uint32_t max_files = 5) {
  retention_config cfg;
  cfg.metrics_dir = tmp.metrics;
  cfg.status_dir = tmp.status;
  cfg.max_pending_points = max_pending;
  cfg.max_files = max_files;
  return cfg;
}

}  // namespace

// ---------------------------------------------------------------------------
// Metric bifurcation — old data
// ---------------------------------------------------------------------------

/**
 * A pb_metric whose timestamp is older than (now - step) must be routed to
 * the retention buffer.  With max_pending_points=1 a rotated .prot file
 * appears immediately after the write.
 */
TEST_F(StreamBifurcation, OldMetricGoesToRetention) {
  tmp_dirs tmp;
  stream<lib> s(tmp.metrics, tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(tmp));

  auto e = std::make_shared<storage::pb_metric>();
  Metric& m = e->mut_obj();
  m.set_metric_id(1001);
  m.set_time(static_cast<uint64_t>(std::time(nullptr) - 120));  // 2 min ago
  m.set_interval(60);  // step = 60 s → data is 2 steps old
  m.set_rrd_len(90u * 24u * 3600u);
  m.set_value(3.14);
  m.set_value_type(Metric_ValueType_GAUGE);

  s.write(e);

  EXPECT_GT(tmp_dirs::count_prot(tmp.metrics, 1001), 0u)
      << "old metric should have been buffered to a .prot file";
}

// ---------------------------------------------------------------------------
// Metric bifurcation — current data
// ---------------------------------------------------------------------------

/**
 * A pb_metric with a current timestamp must be routed directly to the RRD
 * backend.  No .prot file should be created.
 */
TEST_F(StreamBifurcation, CurrentMetricGoesToBackend) {
  tmp_dirs tmp;
  stream<lib> s(tmp.metrics, tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(tmp));

  auto e = std::make_shared<storage::pb_metric>();
  Metric& m = e->mut_obj();
  m.set_metric_id(1002);
  m.set_time(static_cast<uint64_t>(std::time(nullptr)));  // now
  m.set_interval(60);
  m.set_rrd_len(90u * 24u * 3600u);
  m.set_value(2.71);
  m.set_value_type(Metric_ValueType_GAUGE);

  s.write(e);

  EXPECT_EQ(tmp_dirs::count_prot(tmp.metrics, 1002), 0u)
      << "current metric must not be buffered (.prot absent)";
  EXPECT_TRUE(fs::exists(tmp.metrics / "1002.rrd"))
      << "current metric must create/update the .rrd file";
}

// ---------------------------------------------------------------------------
// Status bifurcation — old data
// ---------------------------------------------------------------------------

TEST_F(StreamBifurcation, OldStatusGoesToRetention) {
  tmp_dirs tmp;
  stream<lib> s(tmp.metrics, tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(tmp));

  auto e = std::make_shared<storage::pb_status>();
  Status& st = e->mut_obj();
  st.set_index_id(2001);
  st.set_time(static_cast<uint64_t>(std::time(nullptr) - 120));
  st.set_interval(60);
  st.set_rrd_len(90u * 24u * 3600u);
  st.set_state(0);

  s.write(e);

  EXPECT_GT(tmp_dirs::count_prot(tmp.status, 2001), 0u)
      << "old status should have been buffered to a .prot file";
}

// ---------------------------------------------------------------------------
// Status bifurcation — current data
// ---------------------------------------------------------------------------

TEST_F(StreamBifurcation, CurrentStatusGoesToBackend) {
  tmp_dirs tmp;
  stream<lib> s(tmp.metrics, tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(tmp));

  auto e = std::make_shared<storage::pb_status>();
  Status& st = e->mut_obj();
  st.set_index_id(2002);
  st.set_time(static_cast<uint64_t>(std::time(nullptr)));
  st.set_interval(60);
  st.set_rrd_len(90u * 24u * 3600u);
  st.set_state(1);

  s.write(e);

  EXPECT_EQ(tmp_dirs::count_prot(tmp.status, 2002), 0u)
      << "current status must not be buffered (.prot absent)";
  EXPECT_TRUE(fs::exists(tmp.status / "2002.rrd"))
      << "current status must create/update the .rrd file";
}

// ---------------------------------------------------------------------------
// Retention disabled: all data goes to backend regardless of age
// ---------------------------------------------------------------------------

/**
 * When retention is disabled (empty dirs), even old data must be sent to the
 * RRD backend directly.  No .prot file can be created.
 */
TEST_F(StreamBifurcation, RetentionDisabledAllDataToBackend) {
  tmp_dirs tmp;
  // Empty retention_config → retention.enabled() == false → always backend.
  stream<lib> s(tmp.metrics, tmp.status, 16,
                /*ignore_update_errors=*/true, retention_config{});

  auto e = std::make_shared<storage::pb_metric>();
  Metric& m = e->mut_obj();
  m.set_metric_id(3001);
  m.set_time(static_cast<uint64_t>(std::time(nullptr) - 120));  // old
  m.set_interval(60);
  m.set_rrd_len(90u * 24u * 3600u);
  m.set_value(1.0);
  m.set_value_type(Metric_ValueType_GAUGE);

  s.write(e);

  EXPECT_EQ(tmp_dirs::count_prot(tmp.metrics, 3001), 0u)
      << "with retention disabled, no .prot file should be created";
  EXPECT_TRUE(fs::exists(tmp.metrics / "3001.rrd"))
      << "with retention disabled, old data must still reach the backend";
}

// ---------------------------------------------------------------------------
// Merge triggered by rotated-file count limit
// ---------------------------------------------------------------------------

/**
 * After enough old-data writes to fill max_files rotated files, the stream
 * should trigger _do_metric_merge() which replays the buffered points into the
 * .rrd file and then clears the retention buffer.  After the merge all .prot
 * files for that metric must be gone.
 *
 * Pre-condition: a current-data point arrives first so that the .rrd file
 * already exists when the quota-based merge fires.  This reflects the realistic
 * reconnection scenario where live data and backfill data arrive concurrently.
 */
TEST_F(StreamBifurcation, MergeTriggeredByFileCountLimit) {
  tmp_dirs tmp;
  // max_pending_points=1 → rotate on every write; max_files=3 → merge after 3
  stream<lib> s(tmp.metrics, tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(tmp, 1, 3));

  const uint64_t id = 4001;
  const uint32_t step = 300;  // 5 min
  const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  // Base timestamp old enough so backfill writes land in the "old" path.
  const uint64_t base_time = now - static_cast<uint64_t>(step) * 100;

  // Write one current-data point first to create the .rrd file.
  {
    auto e = std::make_shared<storage::pb_metric>();
    Metric& m = e->mut_obj();
    m.set_metric_id(id);
    m.set_time(now);
    m.set_interval(step);
    m.set_rrd_len(90u * 24u * 3600u);
    m.set_value(0.0);
    m.set_value_type(Metric_ValueType_GAUGE);
    s.write(e);
  }
  ASSERT_TRUE(fs::exists(tmp.metrics / fmt::format("{}.rrd", id)))
      << "current-data write must create the .rrd file";

  // Now send old (backfill) data until the quota-based merge fires.
  bool merge_triggered = false;
  for (int i = 0; i <= 10 && !merge_triggered; ++i) {
    auto e = std::make_shared<storage::pb_metric>();
    Metric& m = e->mut_obj();
    m.set_metric_id(id);
    m.set_time(base_time + static_cast<uint64_t>(i) * step);
    m.set_interval(step);
    m.set_rrd_len(90u * 24u * 3600u);
    m.set_value(static_cast<double>(i));
    m.set_value_type(Metric_ValueType_GAUGE);
    s.write(e);
    // After the merge the retention .prot files disappear.
    if (i > 0)
      merge_triggered = (tmp_dirs::count_prot(tmp.metrics, id) == 0);
  }

  EXPECT_TRUE(merge_triggered)
      << "merge should have fired and cleared all .prot files";
  EXPECT_TRUE(fs::exists(tmp.metrics / fmt::format("{}.rrd", id)))
      << ".rrd file must exist after merge";
}

// ---------------------------------------------------------------------------
// check_metric_junction() — direct unit tests on retention_manager
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Build a retention_manager backed by real temporary directories.
 *
 * @param max_pending  Point-count threshold before rotation (default: large to
 *                     avoid interfering with junction tests).
 */
std::unique_ptr<retention_manager> make_manager(const tmp_dirs& tmp,
                                                uint32_t max_pending = 1024) {
  retention_config cfg;
  cfg.metrics_dir = tmp.metrics;
  cfg.status_dir = tmp.status;
  cfg.max_pending_points = max_pending;
  cfg.max_files = 100;
  auto mgr =
      std::make_unique<retention_manager>(cfg, spdlog::default_logger());
  mgr->init();
  return mgr;
}

}  // namespace

/** No retention state for the requested id → always false. */
TEST(RetentionManagerJunction, NoStateReturnsFalse) {
  tmp_dirs tmp;
  auto mgr = make_manager(tmp);
  EXPECT_FALSE(mgr->check_metric_junction(9001, 1000));
}

/** earliest_current_time == 0 means "unknown" → always false. */
TEST(RetentionManagerJunction, EctZeroReturnsFalse) {
  tmp_dirs tmp;
  auto mgr = make_manager(tmp);
  mgr->write_metric(9002, 1000, 1.0, 60);
  EXPECT_FALSE(mgr->check_metric_junction(9002, 0));
}

/** last_retention_time + step < ect → not yet at junction. */
TEST(RetentionManagerJunction, NotReached) {
  tmp_dirs tmp;
  auto mgr = make_manager(tmp);
  // last=1000, step=60 → 1060 < 1200
  mgr->write_metric(9003, 1000, 1.0, 60);
  EXPECT_FALSE(mgr->check_metric_junction(9003, 1200));
}

/** last_retention_time + step == ect → exactly at junction boundary. */
TEST(RetentionManagerJunction, AtBoundary) {
  tmp_dirs tmp;
  auto mgr = make_manager(tmp);
  // last=1000, step=60 → 1060 >= 1060
  mgr->write_metric(9004, 1000, 1.0, 60);
  EXPECT_TRUE(mgr->check_metric_junction(9004, 1060));
}

/** last_retention_time + step > ect → past junction. */
TEST(RetentionManagerJunction, PastBoundary) {
  tmp_dirs tmp;
  auto mgr = make_manager(tmp);
  // last=1000, step=60 → 1060 >= 1000
  mgr->write_metric(9005, 1000, 1.0, 60);
  EXPECT_TRUE(mgr->check_metric_junction(9005, 1000));
}

// ---------------------------------------------------------------------------
// Stream junction — old-data write path
// ---------------------------------------------------------------------------

/**
 * Scenario: a current-data point arrives first (sets ect), then old data is
 * replayed.  When the last old point satisfies  t + step >= ect  the junction
 * is detected from the old-data write path and a merge is triggered.
 *
 * Timestamps (step = 60 s, now = std::time(nullptr)):
 *   current point : now - 20  (clearly current: >= now - 60)
 *   old point 1   : now - 90  (old; (now-90)+60 = now-30 < now-20 → no junction)
 *   old point 2   : now - 65  (old; (now-65)+60 = now-5  >= now-20 → junction!)
 */
TEST_F(StreamBifurcation, OldDataPathTriggersJunctionMerge) {
  tmp_dirs tmp;
  const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  const uint32_t step = 60;
  const uint64_t id = 7001;
  // max_pending=1 → each write rotates to a .prot file (makes buffer visible)
  // max_files=100 → quota-based merge won't interfere
  stream<lib> s(tmp.metrics, tmp.status, 16, /*ignore_update_errors=*/true,
                make_cfg(tmp, /*max_pending=*/1, /*max_files=*/100));

  auto make_m = [&](uint64_t t) {
    auto e = std::make_shared<storage::pb_metric>();
    Metric& m = e->mut_obj();
    m.set_metric_id(id);
    m.set_time(t);
    m.set_interval(step);
    m.set_rrd_len(90u * 24u * 3600u);
    m.set_value(1.0);
    m.set_value_type(Metric_ValueType_GAUGE);
    return e;
  };

  // 1. Current data → creates .rrd, sets ect = now-20.
  s.write(make_m(now - 20));
  ASSERT_TRUE(fs::exists(tmp.metrics / fmt::format("{}.rrd", id)));

  // 2. Old data: (now-90)+60 = now-30 < now-20 → no junction yet.
  s.write(make_m(now - 90));
  EXPECT_GT(tmp_dirs::count_prot(tmp.metrics, id), 0u)
      << "point buffered, junction not yet reached";

  // 3. Old data: (now-65)+60 = now-5 >= now-20 → junction → merge.
  s.write(make_m(now - 65));
  EXPECT_EQ(tmp_dirs::count_prot(tmp.metrics, id), 0u)
      << "junction merge must clear all .prot files";
  EXPECT_TRUE(fs::exists(tmp.metrics / fmt::format("{}.rrd", id)));
}

// ---------------------------------------------------------------------------
// Stream junction — current-data write path
// ---------------------------------------------------------------------------

/**
 * Scenario: old data is buffered first (no current data → ect unknown).
 * When the first current-data point arrives, check_metric_junction() is
 * called immediately and fires the merge.
 *
 * Timestamps:
 *   old point 1   : now - 150  (old)
 *   old point 2   : now - 65   (old; last_retention_time = now-65)
 *   current point : now - 20   (current; ect = now-20;
 *                               (now-65)+60 = now-5 >= now-20 → junction!)
 */
TEST_F(StreamBifurcation, CurrentDataPathTriggersJunctionMerge) {
  tmp_dirs tmp;
  const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  const uint32_t step = 60;
  const uint64_t id = 8001;
  stream<lib> s(tmp.metrics, tmp.status, 16, /*ignore_update_errors=*/true,
                make_cfg(tmp, /*max_pending=*/1, /*max_files=*/100));

  auto make_m = [&](uint64_t t) {
    auto e = std::make_shared<storage::pb_metric>();
    Metric& m = e->mut_obj();
    m.set_metric_id(id);
    m.set_time(t);
    m.set_interval(step);
    m.set_rrd_len(90u * 24u * 3600u);
    m.set_value(1.0);
    m.set_value_type(Metric_ValueType_GAUGE);
    return e;
  };

  // 1-2. Old data only — no ect, no junction.
  s.write(make_m(now - 150));
  s.write(make_m(now - 65));  // last_retention_time = now-65
  ASSERT_GT(tmp_dirs::count_prot(tmp.metrics, id), 0u)
      << "old data buffered; no junction without current data";

  // 3. First current point → creates .rrd, sets ect = now-20,
  //    check_metric_junction fires → merge.
  s.write(make_m(now - 20));
  EXPECT_EQ(tmp_dirs::count_prot(tmp.metrics, id), 0u)
      << "junction merge must fire on first current-data arrival";
  EXPECT_TRUE(fs::exists(tmp.metrics / fmt::format("{}.rrd", id)));
}

// ---------------------------------------------------------------------------
// No junction when no current data has been seen
// ---------------------------------------------------------------------------

/**
 * Old data is buffered but ect is never set → junction condition cannot be
 * evaluated → no junction merge.
 */
TEST_F(StreamBifurcation, NoJunctionWithoutCurrentData) {
  tmp_dirs tmp;
  const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
  const uint32_t step = 60;
  const uint64_t id = 8002;
  // max_pending=1 makes .prot files visible; max_files=100 blocks quota merge.
  stream<lib> s(tmp.metrics, tmp.status, 16, /*ignore_update_errors=*/true,
                make_cfg(tmp, /*max_pending=*/1, /*max_files=*/100));

  auto make_m = [&](uint64_t t) {
    auto e = std::make_shared<storage::pb_metric>();
    Metric& m = e->mut_obj();
    m.set_metric_id(id);
    m.set_time(t);
    m.set_interval(step);
    m.set_rrd_len(90u * 24u * 3600u);
    m.set_value(1.0);
    m.set_value_type(Metric_ValueType_GAUGE);
    return e;
  };

  // All old data — even the point closest to the current boundary cannot
  // trigger a junction merge because ect is unknown.
  s.write(make_m(now - 150));
  s.write(make_m(now - 90));
  s.write(make_m(now - 65));

  EXPECT_GT(tmp_dirs::count_prot(tmp.metrics, id), 0u)
      << "without current data no junction merge must occur";
  EXPECT_FALSE(fs::exists(tmp.metrics / fmt::format("{}.rrd", id)))
      << "no .rrd file without a merge";
}
