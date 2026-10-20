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

Metric make_metric(uint64_t metric_id, uint64_t time, double value) {
  Metric m;
  m.set_metric_id(metric_id);
  m.set_time(time);
  m.set_value(value);
  m.set_value_type(Metric_ValueType_DERIVE);
  m.set_rrd_len(15552000);
  m.set_interval(300);
  return m;
}

Status make_status(uint64_t index_id, uint64_t time, uint32_t state) {
  Status s;
  s.set_index_id(index_id);
  s.set_time(time);
  s.set_state(state);
  s.set_rrd_len(86400);
  s.set_interval(60);
  return s;
}

/// Copy the slots of a file in a vector.
std::vector<retention_file::slot> read_slots(const retention_file& f) {
  const retention_file::mapped_slot slots = f.get_slots();
  return std::vector<retention_file::slot>(slots.begin(), slots.end());
}

}  // namespace

class RetentionFile : public ::testing::Test {
 protected:
  tmp_dir _tmp;
};

TEST_F(RetentionFile, CreateMetricFile) {
  retention_file f(_tmp.path, make_metric(12, 1000, 3.5));

  EXPECT_TRUE(fs::exists(_tmp.path / "12_metric.ret"));
  EXPECT_EQ(f.get_id(), 12);
  EXPECT_FALSE(f.is_status());
  EXPECT_EQ(f.rrd_len(), 15552000u);
  EXPECT_EQ(f.interval(), 300u);
  EXPECT_EQ(f.first_ts(), 1000u);
  EXPECT_EQ(f.last_ts(), 1000u);

  auto slots = read_slots(f);
  ASSERT_EQ(slots.size(), 1u);
  EXPECT_EQ(slots[0].ts, 1000u);
  EXPECT_DOUBLE_EQ(slots[0].val.d_value, 3.5);
}

TEST_F(RetentionFile, CreateStatusFile) {
  retention_file f(_tmp.path, make_status(7, 2000, 2));

  EXPECT_TRUE(fs::exists(_tmp.path / "7_status.ret"));
  EXPECT_EQ(f.get_id(), 7);
  EXPECT_TRUE(f.is_status());
  EXPECT_EQ(f.rrd_len(), 86400u);
  EXPECT_EQ(f.interval(), 60u);
  EXPECT_EQ(f.first_ts(), 2000u);
  EXPECT_EQ(f.last_ts(), 2000u);

  auto slots = read_slots(f);
  ASSERT_EQ(slots.size(), 1u);
  EXPECT_EQ(slots[0].ts, 2000u);
  EXPECT_EQ(slots[0].val.i_value, 2);
}

TEST_F(RetentionFile, CreateExistingFileThrows) {
  retention_file f(_tmp.path, make_metric(12, 1000, 1.0));
  EXPECT_THROW(retention_file(_tmp.path, make_metric(12, 2000, 2.0)),
               std::exception);
}

TEST_F(RetentionFile, AddUpdatesFirstAndLastTs) {
  retention_file f(_tmp.path, make_metric(12, 1000, 1.0));

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

  // Slots are kept in their arrival order.
  auto slots = read_slots(f);
  ASSERT_EQ(slots.size(), 4u);
  EXPECT_EQ(slots[0].ts, 1000u);
  EXPECT_EQ(slots[1].ts, 1300u);
  EXPECT_EQ(slots[2].ts, 700u);
  EXPECT_EQ(slots[3].ts, 1100u);
}

TEST_F(RetentionFile, AddWrongTypeThrows) {
  retention_file metric_file(_tmp.path, make_metric(12, 1000, 1.0));
  EXPECT_THROW(metric_file.add_status(make_status(12, 1100, 0)),
               std::exception);

  retention_file status_file(_tmp.path, make_status(7, 1000, 0));
  EXPECT_THROW(status_file.add_metric(make_metric(7, 1100, 1.0)),
               std::exception);

  // Nothing has been written.
  EXPECT_EQ(read_slots(metric_file).size(), 1u);
  EXPECT_EQ(read_slots(status_file).size(), 1u);
}

TEST_F(RetentionFile, ReopenReadsHeader) {
  {
    retention_file f(_tmp.path, make_metric(12, 1000, 1.0));
    f.add_metric(make_metric(12, 1300, 2.0));
    f.add_metric(make_metric(12, 400, 3.0));
  }

  retention_file f(_tmp.path / "12_metric.ret");
  EXPECT_EQ(f.get_id(), 12);
  EXPECT_FALSE(f.is_status());
  EXPECT_EQ(f.rrd_len(), 15552000u);
  EXPECT_EQ(f.interval(), 300u);
  EXPECT_EQ(f.first_ts(), 400u);
  EXPECT_EQ(f.last_ts(), 1300u);
  EXPECT_EQ(read_slots(f).size(), 3u);

  // The reopened file can still be appended.
  f.add_metric(make_metric(12, 1600, 4.0));
  EXPECT_EQ(f.last_ts(), 1600u);
  EXPECT_EQ(read_slots(f).size(), 4u);
}

TEST_F(RetentionFile, OpenMissingFileThrows) {
  EXPECT_THROW(retention_file(_tmp.path / "404_metric.ret"), std::exception);
  EXPECT_FALSE(fs::exists(_tmp.path / "404_metric.ret"));
}

TEST_F(RetentionFile, OpenTruncatedFileThrowsAndRemovesIt) {
  fs::path p = _tmp.path / "13_metric.ret";
  {
    std::ofstream out(p, std::ios::binary);
    out << "too short";
  }
  EXPECT_THROW(retention_file{p}, std::exception);
  EXPECT_FALSE(fs::exists(p));
}

TEST_F(RetentionFile, SortSlots) {
  retention_file f(_tmp.path, make_metric(12, 1500, 15.0));
  for (uint64_t ts : {300u, 1200u, 600u, 1800u, 900u})
    f.add_metric(make_metric(12, ts, ts / 100.0));

  f.sort_slots();

  auto slots = read_slots(f);
  ASSERT_EQ(slots.size(), 6u);
  for (size_t i = 0; i < slots.size(); ++i) {
    // values follow their timestamp
    EXPECT_EQ(slots[i].ts, 300u * (i + 1));
    EXPECT_DOUBLE_EQ(slots[i].val.d_value, slots[i].ts / 100.0);
  }

  // The header is not modified by the sort.
  EXPECT_EQ(f.first_ts(), 300u);
  EXPECT_EQ(f.last_ts(), 1800u);

  // The sort is written in the file.
  retention_file reopened(_tmp.path / "12_metric.ret");
  auto reopened_slots = read_slots(reopened);
  ASSERT_EQ(reopened_slots.size(), 6u);
  EXPECT_TRUE(std::is_sorted(
      reopened_slots.begin(), reopened_slots.end(),
      [](const auto& l, const auto& r) { return l.ts < r.ts; }));
}

TEST_F(RetentionFile, SortStatusSlots) {
  retention_file f(_tmp.path, make_status(7, 180, 2));
  f.add_status(make_status(7, 60, 0));
  f.add_status(make_status(7, 120, 1));

  f.sort_slots();

  auto slots = read_slots(f);
  ASSERT_EQ(slots.size(), 3u);
  EXPECT_EQ(slots[0].ts, 60u);
  EXPECT_EQ(slots[0].val.i_value, 0);
  EXPECT_EQ(slots[1].ts, 120u);
  EXPECT_EQ(slots[1].val.i_value, 1);
  EXPECT_EQ(slots[2].ts, 180u);
  EXPECT_EQ(slots[2].val.i_value, 2);
}
