/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/downtimes/recurring_downtime.hh"
#include "common/engine_conf/recurringdowntime_helper.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using com::centreon::engine::downtimes::compute_occurrences;
using com::centreon::engine::downtimes::occurrence;

namespace {
const std::string paris(":Europe/Paris");

// Fabricated instants, Europe/Paris local time.
constexpr time_t mon_2030_01_07_2150 = 1894049400;
constexpr time_t mon_2030_01_07_2155 = 1894049700;
constexpr time_t mon_2030_01_07_2200 = 1894050000;
constexpr time_t mon_2030_01_07_2330 = 1894055400;

Recurringdowntime rule(std::initializer_list<const char*> periods) {
  Recurringdowntime rd;
  rd.set_downtime_id(3);
  rd.add_host_ids(1);
  for (const char* p : periods) {
    if (!recurringdowntime_helper::parse_period(p, rd.add_periods()))
      throw std::invalid_argument(p);
  }
  return rd;
}
}  // namespace

TEST(recurring_downtime_occurrences_test, weekly_occurrence_within_ten_minutes) {
  auto occs = compute_occurrences(rule({"weekly:1 22:00-23:30 fixed"}), paris,
                                  mon_2030_01_07_2155);
  ASSERT_EQ(occs.size(), 1u);
  EXPECT_EQ(occs[0].start, mon_2030_01_07_2200);
  EXPECT_EQ(occs[0].end, mon_2030_01_07_2330);
  EXPECT_TRUE(occs[0].fixed);
}

TEST(recurring_downtime_occurrences_test, look_ahead_window_bounds) {
  auto rd = rule({"weekly:1 22:00-23:30 fixed"});
  // Start exactly 600 s ahead: included.
  EXPECT_EQ(compute_occurrences(rd, paris, mon_2030_01_07_2150).size(), 1u);
  // 601 s ahead: not yet.
  EXPECT_TRUE(compute_occurrences(rd, paris, mon_2030_01_07_2150 - 1).empty());
  // Running: included until its end, excluded.
  EXPECT_EQ(compute_occurrences(rd, paris, mon_2030_01_07_2330 - 1).size(), 1u);
  EXPECT_TRUE(compute_occurrences(rd, paris, mon_2030_01_07_2330).empty());
  // A wider look-ahead.
  EXPECT_EQ(
      compute_occurrences(rd, paris, mon_2030_01_07_2150 - 3600, 3600 + 600)
          .size(),
      1u);
}

TEST(recurring_downtime_occurrences_test, weekly_other_day_ignored) {
  EXPECT_TRUE(compute_occurrences(rule({"weekly:2,3,4,5,6,7 22:00-23:30 fixed"}),
                                  paris, mon_2030_01_07_2155)
                  .empty());
}

TEST(recurring_downtime_occurrences_test, timezone_of_the_rule_applied) {
  auto rd = rule({"weekly:1 22:00-23:30 fixed"});
  // 22:00 UTC is 23:00 in Paris in January.
  EXPECT_TRUE(compute_occurrences(rd, "UTC", mon_2030_01_07_2155).empty());
  auto occs = compute_occurrences(rd, "UTC", mon_2030_01_07_2155 + 3600);
  ASSERT_EQ(occs.size(), 1u);
  EXPECT_EQ(occs[0].start, mon_2030_01_07_2200 + 3600);
}

TEST(recurring_downtime_occurrences_test, process_timezone_restored) {
  const char* before = getenv("TZ");
  std::string saved = before ? before : "";
  compute_occurrences(rule({"weekly:1 22:00-23:30 fixed"}), "UTC",
                      mon_2030_01_07_2155);
  const char* after = getenv("TZ");
  EXPECT_EQ(saved, after ? after : "");
}

TEST(recurring_downtime_occurrences_test, monthly_day_missing_in_month_skipped) {
  // 2030-04-30 23:55: tomorrow is May 1st, April has no 31st.
  constexpr time_t apr_30_2355 = 1903816500;
  constexpr time_t may_01_0000 = 1903816800;
  EXPECT_TRUE(compute_occurrences(rule({"monthly:31 00:00-01:00 fixed"}), paris,
                                  apr_30_2355)
                  .empty());
  auto occs = compute_occurrences(rule({"monthly:1 00:00-01:00 fixed"}), paris,
                                  apr_30_2355);
  ASSERT_EQ(occs.size(), 1u);
  EXPECT_EQ(occs[0].start, may_01_0000);

  // January has a 31st.
  constexpr time_t jan_30_2355 = 1896044100;
  constexpr time_t jan_31_0000 = 1896044400;
  occs = compute_occurrences(rule({"monthly:31 00:00-01:00 flexible:600"}),
                             paris, jan_30_2355);
  ASSERT_EQ(occs.size(), 1u);
  EXPECT_EQ(occs[0].start, jan_31_0000);
  EXPECT_FALSE(occs[0].fixed);
  EXPECT_EQ(occs[0].duration, 600u);
}

TEST(recurring_downtime_occurrences_test, nth_weekday_of_month) {
  // 2030-01-07 is the first Monday of January.
  EXPECT_EQ(compute_occurrences(rule({"nth:first:1 22:00-23:30 fixed"}), paris,
                                mon_2030_01_07_2155)
                .size(),
            1u);
  EXPECT_TRUE(compute_occurrences(rule({"nth:second:1 22:00-23:30 fixed"}),
                                  paris, mon_2030_01_07_2155)
                  .empty());
  EXPECT_TRUE(compute_occurrences(rule({"nth:last:1 22:00-23:30 fixed"}), paris,
                                  mon_2030_01_07_2155)
                  .empty());
}

TEST(recurring_downtime_occurrences_test, last_weekday_until_end_of_day) {
  // 2030-01-27 is the last Sunday of January; 01:00-24:00.
  constexpr time_t sun_jan_27_0055 = 1895702100;
  constexpr time_t sun_jan_27_0100 = 1895702400;
  constexpr time_t mon_jan_28_0000 = 1895785200;
  auto rd = rule({"nth:last:7 01:00-24:00 fixed"});
  auto occs = compute_occurrences(rd, paris, sun_jan_27_0055);
  ASSERT_EQ(occs.size(), 1u);
  EXPECT_EQ(occs[0].start, sun_jan_27_0100);
  EXPECT_EQ(occs[0].end, mon_jan_28_0000);
  // Still running one minute before midnight.
  EXPECT_EQ(compute_occurrences(rd, paris, mon_jan_28_0000 - 60).size(), 1u);
  // The Sunday before is not the last one.
  EXPECT_TRUE(
      compute_occurrences(rd, paris, sun_jan_27_0055 - 7 * 86400).empty());
}

TEST(recurring_downtime_occurrences_test, window_ending_at_midnight) {
  // 22:00-24:00 is still running at 23:59 local.
  auto rd = rule({"weekly:1 22:00-24:00 fixed"});
  constexpr time_t tue_jan_08_0000 = 1894057200;
  auto occs = compute_occurrences(rd, paris, tue_jan_08_0000 - 60);
  ASSERT_EQ(occs.size(), 1u);
  EXPECT_EQ(occs[0].end, tue_jan_08_0000);
}

TEST(recurring_downtime_occurrences_test, dst_spring_forward_hour_skipped) {
  // 2030-03-31 in Paris: 02:00 jumps to 03:00.
  constexpr time_t sun_mar_31_0155 = 1901148900;
  constexpr time_t sun_mar_31_0300_cest = 1901149200;
  EXPECT_TRUE(compute_occurrences(rule({"weekly:7 02:00-03:00 fixed"}), paris,
                                  sun_mar_31_0155)
                  .empty());
  // A window that only starts in the missing hour begins at 03:00.
  auto occs = compute_occurrences(rule({"weekly:7 02:00-04:00 fixed"}), paris,
                                  sun_mar_31_0155);
  ASSERT_EQ(occs.size(), 1u);
  EXPECT_EQ(occs[0].start, sun_mar_31_0300_cest);
  EXPECT_EQ(occs[0].end, sun_mar_31_0300_cest + 3600);
}

TEST(recurring_downtime_occurrences_test, several_periods_sorted_and_unique) {
  auto occs = compute_occurrences(
      rule({"weekly:1 22:05-23:00 fixed", "weekly:1 22:00-23:30 fixed",
            "weekly:1 22:00-23:30 fixed"}),
      paris, mon_2030_01_07_2155);
  ASSERT_EQ(occs.size(), 2u);
  EXPECT_EQ(occs[0].start, mon_2030_01_07_2200);
  EXPECT_EQ(occs[1].start, mon_2030_01_07_2200 + 300);
}
