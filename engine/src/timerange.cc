/**
* Copyright 2022 Centreon
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

#include "com/centreon/engine/timerange.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

timerange::timerange(uint64_t start, uint64_t end) {
  // Make sure we have the data we need.
  if (start > 86400) {
    engine_logger(log_config_error, basic)
        << "Error: Start time " << start << " is not valid for timeperiod";
    log_v2::config()->error("Error: Start time {} is not valid for timeperiod",
                            start);
    throw engine_error() << "Could not create timerange "
                         << "start'" << start << "' end '" << end << "'";
  }
  if (end > 86400) {
    engine_logger(log_config_error, basic)
        << "Error: End time " << end << " is not value for timeperiod";
    log_v2::config()->error("Error: End time {} is not value for timeperiod",
                            end);
    throw engine_error() << "Could not create timerange "
                         << "start'" << start << "' end '" << end << "'";
  }

  _range_start = start;
  _range_end = end;
}

namespace com::centreon::engine {

/**
 *  Dump timerange content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The timerange to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, timerange const& obj) {
  unsigned int start_hours(obj.get_range_start() / 3600);
  unsigned int start_minutes((obj.get_range_start() % 3600) / 60);
  unsigned int end_hours(obj.get_range_end() / 3600);
  unsigned int end_minutes((obj.get_range_end() % 3600) / 60);
  os << std::setfill('0') << std::setw(2) << start_hours << ":"
     << std::setfill('0') << std::setw(2) << start_minutes << "-"
     << std::setfill('0') << std::setw(2) << end_hours << ":"
     << std::setfill('0') << std::setw(2) << end_minutes;
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
  for (timerange_list::const_iterator it(obj.begin()), end(obj.end());
       it != end; ++it)
    os << *it << ((next(it) == obj.end()) ? "" : ", ");
  return os;
}

}
