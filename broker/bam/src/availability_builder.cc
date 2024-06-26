/**
 * Copyright 2014-2015, 2021-2024 Centreon
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

#include "com/centreon/broker/bam/availability_builder.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Constructor.
 *
 *  @param[in] ending_point    The point from when the builder should stop to
 * build the availability.
 *  @param[in] starting_point  The point from when the builder should build the
 * availability.
 */
availability_builder::availability_builder(time_t ending_point,
                                           time_t starting_point /* = 0 */)
    : _start(starting_point),
      _end(ending_point),
      _available{0},
      _unavailable(0),
      _degraded(0),
      _unknown(0),
      _downtime(0),
      _alert_unavailable_opened(0),
      _alert_degraded_opened(0),
      _alert_unknown_opened(0),
      _nb_downtime(0),
      _timeperiods_is_default(false) {}

/**
 *  Destructor
 */
availability_builder::~availability_builder() {}

/**
 *  Add an event to the builder.
 *
 *  @param[in] status           The status of the event.
 *  @param[in] start            The start time of the event.
 *  @param[in] end              The end of the event.
 *  @param[in] was_in_downtime  Was the event in downtime?
 *  @param[in] tp               The timeperiod of the event.
 *  @param[in] logger           The logger to use.
 */
void availability_builder::add_event(
    short status,
    time_t start,
    time_t end,
    bool was_in_downtime,
    time::timeperiod::ptr const& tp,
    const std::shared_ptr<spdlog::logger>& logger) {
  logger->trace(
      "availability_builder::add_event (status: {}, start: {}, end: {}, was in "
      "downtime: {}",
      status, start, end, was_in_downtime);
  // Check that the event was closed.
  if (end == 0)
    end = _end;
  // Check that the end of the event is not before the starting point of the
  // computing.
  if (end < _start)
    return;
  // Check if event was opened "today".
  bool opened_today(start >= _start && start < _end);
  // Check that the event times are within the computed day.
  if (start < _start)
    start = _start;
  if (_end < end)
    end = _end;

  // Compute the sla_duration on the period.
  uint32_t sla_duration = tp->duration_intersect(start, end);
  if (sla_duration == (uint32_t)-1)
    return;

  // Update the data.
  if (was_in_downtime) {
    _downtime += sla_duration;
    if (opened_today)
      ++_nb_downtime;
  } else {
    switch (status) {
      case 0:
        _available += sla_duration;
        break;
      case 1:
        _degraded += sla_duration;
        if (opened_today)
          ++_alert_degraded_opened;
        break;
      case 2:
        _unavailable += sla_duration;
        if (opened_today)
          ++_alert_unavailable_opened;
        break;
      default:
        _unknown += sla_duration;
        if (opened_today)
          ++_alert_unknown_opened;
        break;
    }
  }
}

/**
 *  Get the duration in second when the BA was ok.
 *
 *  @return  The duration in second when the BA was ok.
 */
int availability_builder::get_available() const {
  return _available;
}

/**
 *  Get the duration in second when the BA was critical.
 *
 *  @return  The duration in second when the BA was critical.
 */
int availability_builder::get_unavailable() const {
  return _unavailable;
}

/**
 *  Get the duration in second when the BA was in a warning state.
 *
 *  @return  The duration in second when the BA was in a warning state.
 */
int availability_builder::get_degraded() const {
  return _degraded;
}

/**
 * Get the duration in second when the BA was in an unknown state.
 *
 * @return The duration in second when the BA was in an unknown state.
 */
int availability_builder::get_unknown() const {
  return _unknown;
}

/**
 *  Get the duration in second when the BA was in downtime.
 *
 *  @return  The duration in second when the BA was in downtime.
 */
int availability_builder::get_downtime() const {
  return _downtime;
}

/**
 *  Get the number of unavailable event opened.
 *
 *  @return  The number of unavailable event opened.
 */
int availability_builder::get_unavailable_opened() const {
  return _alert_unavailable_opened;
}

/**
 *  Get the number of degraded event opened.
 *
 *  @return  The number of degraded event opened.
 */
int availability_builder::get_degraded_opened() const {
  return _alert_degraded_opened;
}

/**
 *  Get the number of unknown event opened.
 *
 *  @return  The number of unknown event opened.
 */
int availability_builder::get_unknown_opened() const {
  return _alert_unknown_opened;
}

/**
 *  Get the number of downtime event opened.
 *
 *  @return  The number of downtime event opened.
 */
int availability_builder::get_downtime_opened() const {
  return _nb_downtime;
}

/**
 *  Set if the timeperiod is default.
 *
 *  @param[in] val  True if the timeperiod is default.
 */
void availability_builder::set_timeperiod_is_default(bool val) {
  _timeperiods_is_default = val;
}

/**
 *  Get if the timeperiod is default.
 *
 *  @param[in] val  True if the timeperiod is default.
 */
bool availability_builder::get_timeperiod_is_default() const {
  return _timeperiods_is_default;
}
