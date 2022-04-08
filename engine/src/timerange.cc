// /*
// ** Copyright 2011-2013 Merethis
// **
// ** This file is part of Centreon Engine.
// **
// ** Centreon Engine is free software: you can redistribute it and/or
// ** modify it under the terms of the GNU General Public License version 2
// ** as published by the Free Software Foundation.
// **
// ** Centreon Engine is distributed in the hope that it will be useful,
// ** but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// ** General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License
// ** along with Centreon Engine. If not, see
// ** <http://www.gnu.org/licenses/>.
// */

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

CCE_BEGIN()

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

CCE_END()
