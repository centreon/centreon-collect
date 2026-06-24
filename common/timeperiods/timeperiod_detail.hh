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
#ifndef CCC_TIMEPERIODS_TIMEPERIOD_DETAIL_HH
#define CCC_TIMEPERIODS_TIMEPERIOD_DETAIL_HH

#include <ctime>

#include "absl/time/time.h"

/* Internal calendar helpers of the timeperiods engine. They are not part of the
 * public API; they are declared here only so they can be unit-tested in
 * isolation (and pinned before the planned Abseil rewrite).
 *
 * IMPORTANT — the date arguments follow the struct tm convention, NOT the human
 * calendar:
 *   - `year`  is a tm_year, i.e. the year minus 1900 (2016 -> 116);
 *   - `month` is a tm_mon, i.e. 0-based (January = 0, October = 9);
 *   - `weekday` is a tm_wday, i.e. 0-based from Sunday (Sunday = 0, Monday = 1,
 *     ... Saturday = 6).
 */

namespace com::centreon::common::timeperiods {

/* Midnight of the `monthday`-th day of (year, month) in timezone `tz`.
 * A positive `monthday` is counted from the start of the month (1 = the 1st); a
 * negative `monthday` is counted from the end (-1 = the last day, -2 = the
 * day before the last). Returns (time_t)-1 if the day does not exist in the
 * month (e.g. a positive day that overflows into the next month). */
time_t calculate_time_from_day_of_month(int year,
                                        int month,
                                        int monthday,
                                        const absl::TimeZone& tz);

/* Midnight of the `weekday_offset`-th `weekday` of (year, month) in timezone
 * `tz`. A positive offset is counted from the start of the month (1 = the first
 * such weekday, 3 = the third); a negative offset is counted from the end
 * (-1 = the last such weekday). Returns (time_t)-1 when that occurrence does not
 * exist (e.g. the 5th Tuesday of a month that has only four). */
time_t calculate_time_from_weekday_of_month(int year,
                                            int month,
                                            int weekday,
                                            int weekday_offset,
                                            const absl::TimeZone& tz);

}  // namespace com::centreon::common::timeperiods

#endif  // !CCC_TIMEPERIODS_TIMEPERIOD_DETAIL_HH
