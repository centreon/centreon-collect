/**
 * Copyright 2022-2024 Centreon
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

#include "common/timeperiods/timerange.hh"
#include <fmt/ostream.h>
#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::common::timeperiods {

timerange::timerange(uint64_t start, uint64_t end) {
  // Make sure we have the data we need. The exception carries the offending
  // bound; the caller building the timeperiod surfaces it (no logging here, so
  // timerange does not depend on the timeperiod_manager).
  if (start > 86400)
    throw msg_fmt(
        "Could not create timerange: start time {} is not valid (must be "
        "<= 86400)",
        start);
  if (end > 86400)
    throw msg_fmt(
        "Could not create timerange: end time {} is not valid (must be "
        "<= 86400)",
        end);

  _range_start = start;
  _range_end = end;
}

/**
 *  Dump timerange content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The timerange to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, timerange const& obj) {
  uint32_t start_hours = obj.get_range_start() / 3600;
  uint32_t start_minutes = (obj.get_range_start() % 3600) / 60;
  uint32_t end_hours = obj.get_range_end() / 3600;
  uint32_t end_minutes = (obj.get_range_end() % 3600) / 60;
  fmt::print(os, "{:02}:{:02}-{:02}:{:02}", start_hours, start_minutes,
             end_hours, end_minutes);
  return os;
}

/**
 *  Dump timerange_list content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The timerange_list to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, timerange_list const& obj) {
  bool first = true;
  for (const auto& tr : obj) {
    if (!first)
      os << ", ";
    first = false;
    os << tr;
  }
  return os;
}

}  // namespace com::centreon::common::timeperiods
