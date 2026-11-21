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
#ifndef CCC_ENGINE_CONF_TIMEPERIOD_LEGACY_HH
#define CCC_ENGINE_CONF_TIMEPERIOD_LEGACY_HH

#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::configuration {

/* Legacy text → protobuf conversion for timeperiods.
 *
 * The timeperiod data carried by BAM (the `mod_bam_reporting_timeperiods*`
 * tables) and by the historical Nagios configuration is expressed as text:
 * day time ranges ("09:00-17:00,19:00-21:00") and exception lines ("monday 4
 * january 01:00-11:00", "2009-08-11 / 2", "day 3 09:00-17:00", …). The
 * `common::timeperiods` library, however, is built from a structured
 * `configuration::Timeperiod` protobuf.
 *
 * These helpers fill that protobuf from the legacy text form. They are the
 * single place where the legacy grammar is parsed: BAM uses them to feed the
 * shared library, the unit tests use them to replay the historical fixtures,
 * and PHP can lean on them as the reference grammar the day it emits the
 * protobuf directly. */

/**
 * @brief Parse legacy time-range text into a repeated Timerange.
 *
 * @param[in]  text  Comma-separated ranges, e.g. "09:00-17:00,19:00-21:00".
 *                   An empty string is valid and yields no range.
 * @param[out] out   Repeated field filled with {range_start, range_end} in
 *                   seconds since midnight.
 *
 * @return true on success, false on a malformed range.
 */
bool legacy_build_timeranges(
    const std::string& text,
    ::google::protobuf::RepeatedPtrField<Timerange>* out);

/**
 * @brief Set a weekday's time ranges of @p tp from legacy text.
 *
 * @param[in,out] tp    Timeperiod to fill.
 * @param[in]     day   Day index, 0 = Sunday … 6 = Saturday.
 * @param[in]     text  Legacy time-range text (see legacy_build_timeranges).
 *
 * @return true on success, false on a malformed range or an out-of-range day.
 */
bool legacy_set_weekday(Timeperiod& tp, int day, const std::string& text);

/**
 * @brief Parse a legacy exception and append it to @p tp.
 *
 * Mirrors the BAM `add_exception(daterange, timerange)` (two DB columns) and
 * the Nagios "speday" syntax: the two parts are concatenated and parsed as a
 * single date-range + time-range line. The resulting Daterange is appended to
 * the matching list of `tp.exceptions()` (calendar_date / month_date /
 * month_day / month_week_day / week_day).
 *
 * @param[in,out] tp             Timeperiod to fill.
 * @param[in]     daterange_text Date-range part, e.g. "monday 4 january".
 * @param[in]     timerange_text Time-range part, e.g. "01:00-11:00".
 *
 * @return true on success, false if the line could not be parsed.
 */
bool legacy_add_exception(Timeperiod& tp,
                          const std::string& daterange_text,
                          const std::string& timerange_text);

}  // namespace com::centreon::engine::configuration

#endif  // !CCC_ENGINE_CONF_TIMEPERIOD_LEGACY_HH
