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
 */
TEST_F(StreamBifurcation, MergeTriggeredByFileCountLimit) {
  tmp_dirs tmp;
  // max_pending_points=1 → rotate on every write; max_files=3 → merge after 3
  stream<lib> s(tmp.metrics, tmp.status, 16,
                /*ignore_update_errors=*/true, make_cfg(tmp, 1, 3));

  const uint64_t id = 4001;
  const uint32_t step = 300;  // 5 min
  // Base timestamp old enough so all writes land in the "old" path.
  const uint64_t base_time = static_cast<uint64_t>(std::time(nullptr)) -
                             static_cast<uint64_t>(step) * 100;

  // The first write also creates the .rrd file via _backend.open().
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
