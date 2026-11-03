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

#include "com/centreon/broker/rrd/retention_file.hh"

#include <gtest/gtest.h>

#include <fstream>

namespace fs = std::filesystem;
using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

namespace {

constexpr uint32_t interval = 60;
constexpr uint32_t rrd_len = 86400;

/**
 * @brief RAII helper: creates a unique temporary directory and removes it
 *        (recursively) on destruction.
 */
struct tmp_dir {
  fs::path path;

  tmp_dir() {
    path = fs::temp_directory_path() /
           fmt::format("rrd_ret_file_test_{}", ::getpid());
    fs::remove_all(path);
    fs::create_directories(path);
  }
  ~tmp_dir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

Metric make_metric(uint64_t metric_id,
                   uint64_t time,
                   double value,
                   Metric_ValueType value_type = Metric_ValueType_GAUGE) {
  Metric m;
  m.set_metric_id(metric_id);
  m.set_time(time);
  m.set_value(value);
  m.set_value_type(value_type);
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

/// Known values of an rrd file between from and to, by timestamp.
std::map<uint64_t, double> fetch(const fs::path& rrd_path,
                                 uint64_t from,
                                 uint64_t to) {
  lib l("", 0);
  rrd_existing_data data = l.fetch_existing(rrd_path, from, to);
  return {data.points.begin(), data.points.end()};
}

/// Create an rrd file as the stream does and write values in it.
void write_rrd(const fs::path& rrd_path,
               const std::vector<std::pair<uint64_t, double>>& values) {
  lib l("", 0);
  l.open(rrd_path, rrd_len, values.front().first - 1, interval);
  for (auto& [ts, value] : values)
    l.update(ts, fmt::format("{:f}", value));
}

std::string file_content(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

class RetentionFile : public ::testing::Test {
 protected:
  tmp_dir _tmp;
};

TEST_F(RetentionFile, CreateMetricFile) {
  retention_file f(_tmp.path, make_metric(12, 1000, 3.5));

  EXPECT_TRUE(fs::exists(_tmp.path / "12.ret"));
  EXPECT_EQ(f.file_path(), _tmp.path / "12.ret");
  EXPECT_EQ(f.id(), 12u);
  EXPECT_FALSE(f.is_status());
  EXPECT_EQ(f.rrd_len(), rrd_len);
  EXPECT_EQ(f.interval(), interval);
  EXPECT_EQ(f.first_ts(), 1000u);
  EXPECT_EQ(f.last_ts(), 1000u);
  EXPECT_EQ(f.nb_samples(), 1u);
}

TEST_F(RetentionFile, CreateStatusFile) {
  retention_file f(_tmp.path, make_status(7, 2000, 2));

  EXPECT_TRUE(fs::exists(_tmp.path / "7.ret"));
  EXPECT_EQ(f.id(), 7u);
  EXPECT_TRUE(f.is_status());
  EXPECT_EQ(f.rrd_len(), rrd_len);
  EXPECT_EQ(f.interval(), interval);
  EXPECT_EQ(f.first_ts(), 2000u);
  EXPECT_EQ(f.last_ts(), 2000u);
  EXPECT_EQ(f.nb_samples(), 1u);
}

TEST_F(RetentionFile, CreateExistingFileThrows) {
  retention_file f(_tmp.path, make_metric(12, 1000, 1.0));
  EXPECT_THROW(retention_file(_tmp.path, make_metric(12, 2000, 2.0)),
               std::exception);
  // the existing file is not modified
  EXPECT_EQ(f.nb_samples(), 1u);
  EXPECT_EQ(retention_file(_tmp.path / "12.ret").nb_samples(), 1u);
}

TEST_F(RetentionFile, AddUpdatesFirstAndLastTs) {
  retention_file f(_tmp.path, make_metric(12, 1000, 1.0));
  time_t activity = f.last_activity();
  EXPECT_LE(time(nullptr) - activity, 1);

  f.add_metric(make_metric(12, 1300, 2.0));
  EXPECT_EQ(f.first_ts(), 1000u);
  EXPECT_EQ(f.last_ts(), 1300u);

  // Late point: only first_ts moves.
  f.add_metric(make_metric(12, 700, 3.0));
  EXPECT_EQ(f.first_ts(), 700u);
  EXPECT_EQ(f.last_ts(), 1300u);

  // Point inside the range: nothing moves.
  f.add_metric(make_metric(12, 1100, 4.0));
  EXPECT_EQ(f.first_ts(), 700u);
  EXPECT_EQ(f.last_ts(), 1300u);

  EXPECT_EQ(f.nb_samples(), 4u);
  EXPECT_GE(f.last_activity(), activity);
}

TEST_F(RetentionFile, AddWrongTypeThrows) {
  retention_file metric_file(_tmp.path, make_metric(12, 1000, 1.0));
  EXPECT_THROW(metric_file.add_status(make_status(12, 1100, 0)),
               std::exception);

  retention_file status_file(_tmp.path, make_status(7, 1000, 0));
  EXPECT_THROW(status_file.add_metric(make_metric(7, 1100, 1.0)),
               std::exception);

  // Nothing has been written.
  EXPECT_EQ(metric_file.nb_samples(), 1u);
  EXPECT_EQ(status_file.nb_samples(), 1u);
}

TEST_F(RetentionFile, ReopenReadsHeaderAndSamplesCount) {
  {
    retention_file f(_tmp.path, make_metric(12, 1000, 1.0));
    f.add_metric(make_metric(12, 1300, 2.0));
    f.add_metric(make_metric(12, 400, 3.0));
  }

  retention_file f(_tmp.path / "12.ret");
  EXPECT_EQ(f.id(), 12u);
  EXPECT_FALSE(f.is_status());
  EXPECT_EQ(f.rrd_len(), rrd_len);
  EXPECT_EQ(f.interval(), interval);
  EXPECT_EQ(f.first_ts(), 400u);
  EXPECT_EQ(f.last_ts(), 1300u);
  EXPECT_EQ(f.nb_samples(), 3u);

  // The reopened file can still be appended.
  f.add_metric(make_metric(12, 1600, 4.0));
  EXPECT_EQ(f.last_ts(), 1600u);
  EXPECT_EQ(f.nb_samples(), 4u);
  EXPECT_EQ(retention_file(_tmp.path / "12.ret").nb_samples(), 4u);
}

TEST_F(RetentionFile, ReopenStatusFile) {
  {
    retention_file f(_tmp.path, make_status(7, 1000, 1));
    f.add_status(make_status(7, 1060, 2));
  }

  retention_file f(_tmp.path / "7.ret");
  EXPECT_TRUE(f.is_status());
  EXPECT_EQ(f.nb_samples(), 2u);
  EXPECT_THROW(f.add_metric(make_metric(7, 1100, 1.0)), std::exception);
}

TEST_F(RetentionFile, OpenMissingFileThrows) {
  EXPECT_THROW(retention_file(_tmp.path / "404.ret"), std::exception);
  EXPECT_FALSE(fs::exists(_tmp.path / "404.ret"));
}

TEST_F(RetentionFile, OpenTruncatedFileThrowsAndRemovesIt) {
  fs::path p = _tmp.path / "13.ret";
  {
    std::ofstream out(p, std::ios::binary);
    out << "too short";
  }
  EXPECT_THROW(retention_file{p}, std::exception);
  EXPECT_FALSE(fs::exists(p));
}

/**
 * The rrd file doesn't exist: it is created from the samples, received in any
 * order. The first sample must not be lost.
 */
TEST_F(RetentionFile, MergeIntoMissingRrd) {
  const uint64_t base = base_time();
  const fs::path rrd_path = _tmp.path / "12.rrd";

  retention_file f(_tmp.path, make_metric(12, base + 3 * interval, 4.5));
  for (int i : {0, 4, 1, 2})
    f.add_metric(make_metric(12, base + i * interval, i * 1.5));

  lib l("", 0);
  f.merge(rrd_path, l);

  ASSERT_TRUE(fs::exists(rrd_path));
  EXPECT_FALSE(fs::exists(_tmp.path / "12.rrd.to_merge"));
  EXPECT_FALSE(fs::exists(_tmp.path / "12.rrd.tmp"));
  // the merge doesn't remove the retention file
  EXPECT_TRUE(fs::exists(f.file_path()));

  EXPECT_EQ(lib::last_update(rrd_path),
            static_cast<time_t>(base + 4 * interval));
  auto values = fetch(rrd_path, base, base + 4 * interval);
  ASSERT_EQ(values.size(), 5u);
  for (int i = 0; i < 5; ++i)
    EXPECT_DOUBLE_EQ(values[base + i * interval], i * 1.5);
}

/**
 * The rrd file contains real-time data more recent than the retention ones:
 * after the merge, it contains both and its last update is unchanged.
 */
TEST_F(RetentionFile, MergeKeepsRrdData) {
  const uint64_t base = base_time();
  const fs::path rrd_path = _tmp.path / "12.rrd";

  // real-time data from base + 10 * interval to base + 15 * interval
  std::vector<std::pair<uint64_t, double>> rt;
  for (int i = 10; i <= 15; ++i)
    rt.emplace_back(base + i * interval, 100.0 + i);
  write_rrd(rrd_path, rt);
  const time_t last_update = lib::last_update(rrd_path);
  ASSERT_EQ(last_update, static_cast<time_t>(base + 15 * interval));

  // retention data from base to base + 4 * interval
  retention_file f(_tmp.path, make_metric(12, base, 0.0));
  for (int i = 1; i < 5; ++i)
    f.add_metric(make_metric(12, base + i * interval, i));

  lib l("", 0);
  f.merge(rrd_path, l);

  EXPECT_EQ(lib::last_update(rrd_path), last_update);
  auto values = fetch(rrd_path, base, base + 15 * interval);
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(values.count(base + i * interval)) << "retention " << i;
    EXPECT_DOUBLE_EQ(values[base + i * interval], i);
  }
  // the first real-time value is unknown since the creation of the file
  for (int i = 11; i <= 15; ++i) {
    ASSERT_TRUE(values.count(base + i * interval)) << "real-time " << i;
    EXPECT_DOUBLE_EQ(values[base + i * interval], 100.0 + i);
  }
  // nothing between
  for (int i = 5; i < 10; ++i)
    EXPECT_FALSE(values.count(base + i * interval)) << i;
}

/**
 * When the rrd file and the retention have a value for the same row, the one
 * of the rrd file (real-time data) is kept.
 */
TEST_F(RetentionFile, MergeRrdDataHasPriority) {
  const uint64_t base = base_time();
  const fs::path rrd_path = _tmp.path / "12.rrd";

  std::vector<std::pair<uint64_t, double>> rt;
  for (int i = 0; i <= 5; ++i)
    rt.emplace_back(base + i * interval, 100.0 + i);
  write_rrd(rrd_path, rt);

  retention_file f(_tmp.path, make_metric(12, base + 3 * interval, 3.0));

  lib l("", 0);
  f.merge(rrd_path, l);

  auto values = fetch(rrd_path, base, base + 5 * interval);
  ASSERT_TRUE(values.count(base + 3 * interval));
  EXPECT_DOUBLE_EQ(values[base + 3 * interval], 103.0);
}

TEST_F(RetentionFile, MergeStatus) {
  const uint64_t base = base_time();
  const fs::path rrd_path = _tmp.path / "7.rrd";

  retention_file f(_tmp.path, make_status(7, base, 0));
  f.add_status(make_status(7, base + 2 * interval, 2));
  f.add_status(make_status(7, base + interval, 1));
  f.add_status(make_status(7, base + 3 * interval, 3));  // unknown

  lib l("", 0);
  f.merge(rrd_path, l);

  auto values = fetch(rrd_path, base, base + 3 * interval);
  EXPECT_EQ(values.size(), 3u);
  EXPECT_DOUBLE_EQ(values[base], 100.0);
  EXPECT_DOUBLE_EQ(values[base + interval], 75.0);
  EXPECT_DOUBLE_EQ(values[base + 2 * interval], 0.0);
  EXPECT_FALSE(values.count(base + 3 * interval));
}

/**
 * librrd only accepts integers for a DERIVE, decimal values must not make the
 * merge fail.
 */
TEST_F(RetentionFile, MergeDeriveWithDecimalValues) {
  const uint64_t base = base_time();
  const fs::path rrd_path = _tmp.path / "12.rrd";

  // counter increased by 1 per second
  retention_file f(_tmp.path,
                   make_metric(12, base, 0.5, Metric_ValueType_DERIVE));
  for (int i = 1; i <= 4; ++i)
    f.add_metric(make_metric(12, base + i * interval, i * interval + 0.5,
                             Metric_ValueType_DERIVE));

  lib l("", 0);
  f.merge(rrd_path, l);

  auto values = fetch(rrd_path, base, base + 4 * interval);
  ASSERT_EQ(values.size(), 4u);
  for (int i = 1; i <= 4; ++i)
    EXPECT_DOUBLE_EQ(values[base + i * interval], 1.0) << i;
}

TEST_F(RetentionFile, AbortMergeLeavesRrdUntouched) {
  const uint64_t base = base_time();
  const fs::path rrd_path = _tmp.path / "12.rrd";

  write_rrd(rrd_path, {{base + 10 * interval, 1.0}, {base + 11 * interval, 2.0}});
  const std::string before = file_content(rrd_path);

  retention_file f(_tmp.path, make_metric(12, base, 3.0));
  f.abort_merge();
  lib l("", 0);
  f.merge(rrd_path, l);

  EXPECT_EQ(file_content(rrd_path), before);
  EXPECT_FALSE(fs::exists(_tmp.path / "12.rrd.to_merge"));
  EXPECT_FALSE(fs::exists(_tmp.path / "12.rrd.tmp"));
}
