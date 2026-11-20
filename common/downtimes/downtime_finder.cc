/**
 * Copyright 2016, 2026 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "common/downtimes/downtime_finder.hh"

#include "common/downtimes/downtime_manager.hh"

namespace com::centreon::common::downtimes {
// Helper macro.
#define ARE_STRINGS_MATCHING(stdstring, cstring) \
  ((cstring && (cstring == stdstring)) || (!cstring && stdstring.empty()))

/**
 *  Constructor.
 *
 *  @param[in] list  Active downtime list. The search will be performed
 *                   on this list.
 */
downtime_finder::downtime_finder(
    const std::multimap<time_t, std::shared_ptr<downtime>>& map)
    : _map(map) {}

/**
 *  Find downtimes that match all the criterias.
 *
 *  @param[in] criterias  Search criterias.
 */
downtime_finder::result_set downtime_finder::find_matching_all(
    downtime_finder::criteria_set const& criterias) {
  result_set result;
  // Process all downtimes.
  for (auto dt = _map.begin(); dt != _map.end(); ++dt) {
    // Process all criterias.
    bool matched_all{true};
    for (criteria_set::const_iterator it = criterias.begin(),
                                      end = criterias.end();
         it != end; ++it) {
      if (!_match_criteria(*dt->second, *it))
        matched_all = false;
    }

    // If downtime matched all criterias, add it to the result set.
    if (matched_all)
      result.push_back(dt->second->get_downtime_id());
  }
  return result;
}

/**
 *  Check that a downtime match a specific criteria.
 *
 *  @param[in] dt    Downtime.
 *  @param[in] crit  Search criteria.
 *
 *  @return True if downtime matches the criteria.
 */
bool downtime_finder::_match_criteria(const downtime& dt,
                                      downtime_finder::criteria const& crit) {
  bool retval = false;

  if (crit.first == "host") {
    if (dt.get_type() == downtime::service_downtime) {
      auto p = downtime_manager::instance().callbacks().get_host_and_service_names(
          dt.host_id(), dt.service_id());
      retval = (crit.second == p.first);
    } else {
      std::string hostname =
          downtime_manager::instance().callbacks().get_host_name(dt.host_id());
      retval = (crit.second == hostname);
    }
  } else if (crit.first == "service") {
    if (dt.get_type() == downtime::service_downtime) {
      auto p = downtime_manager::instance().callbacks().get_host_and_service_names(
          dt.host_id(), dt.service_id());
      retval = (crit.second == p.second);
    }
    /* host downtimes never match a "service" criteria */
  } else if (crit.first == "start") {
    int64_t expected;
    if (absl::SimpleAtoi(crit.second, &expected))
      retval = (static_cast<time_t>(expected) == dt.get_start_time());
  } else if (crit.first == "end") {
    int64_t expected;
    if (absl::SimpleAtoi(crit.second, &expected))
      retval = (static_cast<time_t>(expected) == dt.get_end_time());
  } else if (crit.first == "fixed") {
    bool expected;
    if (absl::SimpleAtob(crit.second, &expected))
      retval = (expected == dt.is_fixed());
  } else if (crit.first == "triggered_by") {
    uint64_t expected;
    if (absl::SimpleAtoi(crit.second, &expected))
      retval = (expected == dt.get_triggered_by());
  } else if (crit.first == "duration") {
    uint32_t expected;
    if (absl::SimpleAtoi(crit.second, &expected))
      retval = (expected == dt.get_duration());
  } else if (crit.first == "author") {
    retval = (crit.second == dt.get_author());
  } else if (crit.first == "comment") {
    retval = (crit.second == dt.get_comment());
  }
  return retval;
}

}  // namespace com::centreon::common::downtimes
