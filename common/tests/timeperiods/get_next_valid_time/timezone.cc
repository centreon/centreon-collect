/**
 * Copyright 2026 Centreon
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

#include <atomic>
#include <cstdlib>
#include <ctime>
#include <thread>

#include "common/tests/timeperiods/utils.hh"
#include "common/timeperiods/timeperiod.hh"

using namespace com::centreon::common::timeperiods;

namespace {

// RAII guard: set the process TZ for the duration of a scope, then restore the
// previous value (the test suite pins TZ=Europe/Paris globally, see
// common/tests/test_main.cc). It mutates process-global state through
// setenv/tzset, which is exactly the brick the timeperiods rework wants to get
// rid of: these golden-master tests freeze the current behaviour so the
// upcoming switch to a per-evaluation absl::TimeZone can be proven equivalent.
class scoped_tz {
  std::string _saved;
  bool _had_saved;

 public:
  explicit scoped_tz(const char* tz) {
    const char* cur = ::getenv("TZ");
    _had_saved = cur != nullptr;
    if (_had_saved)
      _saved = cur;
    ::setenv("TZ", tz, 1);
    ::tzset();
  }
  ~scoped_tz() {
    if (_had_saved)
      ::setenv("TZ", _saved.c_str(), 1);
    else
      ::unsetenv("TZ");
    ::tzset();
  }
};

// Build a timezone-independent absolute instant from UTC calendar fields, so a
// single "now" can be reused across timezones without being reinterpreted.
time_t utc_instant(int year, int month, int day, int hour, int min, int sec) {
  struct tm t;
  std::memset(&t, 0, sizeof(t));
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = min;
  t.tm_sec = sec;
  return timegm(&t);
}

// Compute "next Monday 09:00" for a "Monday 09:00-17:00" timeperiod, starting
// from the same absolute Sunday instant, evaluated under timezone @p tz.
time_t next_monday_0900(const char* tz) {
  scoped_tz guard(tz);
  timeperiod_creator creator;
  creator.new_timeperiod();
  // Monday (day index 1) 09:00-17:00.
  creator.new_timerange(9, 0, 17, 0, 1);

  // Sunday 2024-06-23 00:00:00 UTC, an absolute instant earlier than the next
  // Monday 09:00 in every timezone tested here.
  time_t now = utc_instant(2024, 6, 23, 0, 0, 0);
  set_time(now);

  time_t computed = static_cast<time_t>(-1);
  get_next_valid_time(now, &computed, creator.get_timeperiods());
  return computed;
}

}  // namespace

// Given a "Monday 09:00-17:00" timeperiod and a fixed absolute instant
// When get_next_valid_time() is evaluated under different timezones
// Then each result is that timezone's local Monday 09:00 (different absolute
//      instants), proving the computation is interpreted in the active zone.
// These hard-coded epochs are the golden master (verified with `date`):
//   Europe/Paris    09:00 CEST = 07:00 UTC
//   UTC             09:00      = 09:00 UTC
//   America/New_York 09:00 EDT = 13:00 UTC
TEST(GetNextValidTimeTimezone, WorkHoursInterpretedInLocalTimezone) {
  EXPECT_EQ(next_monday_0900("Europe/Paris"), static_cast<time_t>(1719212400));
  EXPECT_EQ(next_monday_0900("UTC"), static_cast<time_t>(1719219600));
  EXPECT_EQ(next_monday_0900("America/New_York"),
            static_cast<time_t>(1719234000));
}

// The same timeperiod definitely yields *different* absolute instants depending
// on the timezone — the property that makes per-evaluation timezone support
// matter at all. Westwards zones reach their local 09:00 later in absolute time.
TEST(GetNextValidTimeTimezone, WorkHoursDifferAcrossTimezones) {
  time_t paris = next_monday_0900("Europe/Paris");
  time_t utc = next_monday_0900("UTC");
  time_t new_york = next_monday_0900("America/New_York");
  EXPECT_LT(paris, utc);
  EXPECT_LT(utc, new_york);
  EXPECT_EQ(utc - paris, 2 * 3600);     // Paris is UTC+2 in June.
  EXPECT_EQ(new_york - utc, 4 * 3600);  // New_York is UTC-4 in June.
}

// A timezone with a half-hour offset (Lord Howe Island, UTC+10:30 in June)
// catches any arithmetic that assumes whole-hour offsets.
//   Monday 2024-06-24 09:00 +10:30 = Sunday 2024-06-23 22:30 UTC = 1719181800
TEST(GetNextValidTimeTimezone, HalfHourOffsetTimezone) {
  EXPECT_EQ(next_monday_0900("Australia/Lord_Howe"),
            static_cast<time_t>(1719181800));
}

// DISABLED until the rework lands: today the timezone is process-global state
// (setenv/tzset via engine's timezone_manager), so two threads evaluating the
// same timeperiod under different timezones race on that global. Once
// get_next_valid_time() takes the timezone as a per-call parameter (an
// immutable absl::TimeZone), this test must pass and proves the computation is
// thread-safe and zone-independent — the core deliverable of the rework.
TEST(GetNextValidTimeTimezone, DISABLED_ConcurrentEvaluationsAreTimezoneIndependent) {
  std::atomic<time_t> paris{static_cast<time_t>(-1)};
  std::atomic<time_t> new_york{static_cast<time_t>(-1)};

  std::thread t1([&] { paris = next_monday_0900("Europe/Paris"); });
  std::thread t2([&] { new_york = next_monday_0900("America/New_York"); });
  t1.join();
  t2.join();

  EXPECT_EQ(paris.load(), static_cast<time_t>(1719212400));
  EXPECT_EQ(new_york.load(), static_cast<time_t>(1719234000));
}
