/**
* Copyright 2016 Centreon
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

#include "com/centreon/engine/downtimes/downtime_finder.hh"

#include "com/centreon/engine/downtimes/host_downtime.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;

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
    std::multimap<time_t, std::shared_ptr<downtime> > const& map)
    : _map(&map) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
// downtime_finder::downtime_finder(downtime_finder const& other)
//  : _map(other._map) {}

/**
 *  Destructor.
 */
downtime_finder::~downtime_finder() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 */
downtime_finder& downtime_finder::operator=(downtime_finder const& other) {
  if (this != &other)
    _map = other._map;
  return *this;
}

/**
 *  Find downtimes that match all the criterias.
 *
 *  @param[in] criterias  Search criterias.
 */
downtime_finder::result_set downtime_finder::find_matching_all(
    downtime_finder::criteria_set const& criterias) {
  result_set result;
  // Process all downtimes.
  for (auto dt = _map->begin(); dt != _map->end(); ++dt) {
    // Process all criterias.
    bool matched_all{true};
    for (criteria_set::const_iterator it(criterias.begin()),
         end(criterias.end());
         it != end; ++it) {
      switch (dt->second->get_type()) {
        case downtime::host_downtime:
          if (!_match_criteria(
                  *std::static_pointer_cast<host_downtime>(dt->second), *it))
            matched_all = false;
          break;
        case downtime::service_downtime:
          if (!_match_criteria(
                  *std::static_pointer_cast<service_downtime>(dt->second), *it))
            matched_all = false;
          break;
        case downtime::any_downtime:
          /* This case does not need to be handled here. A downtime concerns a
           * host or a service. */
          break;
      }
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
bool downtime_finder::_match_criteria(const host_downtime& dt,
                                      downtime_finder::criteria const& crit) {
  bool retval = false;
  std::string hostname = engine::get_host_name(dt.host_id());

  if (crit.first == "host")
    retval = (crit.second == hostname);
  else if (crit.first == "start") {
    time_t expected(strtoll(crit.second.c_str(), nullptr, 0));
    retval = (expected == dt.get_start_time());
  } else if (crit.first == "end") {
    time_t expected(strtoll(crit.second.c_str(), nullptr, 0));
    retval = (expected == dt.get_end_time());
  } else if (crit.first == "fixed") {
    bool expected(strtol(crit.second.c_str(), nullptr, 0));
    retval = (expected == static_cast<bool>(dt.is_fixed()));
  } else if (crit.first == "triggered_by") {
    unsigned long expected(strtoul(crit.second.c_str(), nullptr, 0));
    retval = (expected == dt.get_triggered_by());
  } else if (crit.first == "duration") {
    int32_t expected{std::stoi(crit.second)};
    retval = (expected == dt.get_duration());
  } else if (crit.first == "author")
    retval = (crit.second == dt.get_author());
  else if (crit.first == "comment")
    retval = (crit.second == dt.get_comment());
  else
    retval = false;
  return retval;
}

/**
 *  Check that a downtime match a specific criteria.
 *
 *  @param[in] dt    Downtime.
 *  @param[in] crit  Search criteria.
 *
 *  @return True if downtime matches the criteria.
 */
bool downtime_finder::_match_criteria(service_downtime const& dt,
                                      downtime_finder::criteria const& crit) {
  bool retval{false};
  auto p = get_host_and_service_names(dt.host_id(), dt.service_id());

  if (crit.first == "host") {
    retval = (crit.second == p.first);
  } else if (crit.first == "service") {
    retval = (crit.second == p.second);
  } else if (crit.first == "start") {
    time_t expected(std::stoull(crit.second, nullptr, 0));
    retval = (expected == dt.get_start_time());
  } else if (crit.first == "end") {
    time_t expected(strtoll(crit.second.c_str(), nullptr, 0));
    retval = (expected == dt.get_end_time());
  } else if (crit.first == "fixed") {
    bool expected(strtol(crit.second.c_str(), nullptr, 0));
    retval = (expected == static_cast<bool>(dt.is_fixed()));
  } else if (crit.first == "triggered_by") {
    unsigned long expected(strtoul(crit.second.c_str(), nullptr, 0));
    retval = (expected == dt.get_triggered_by());
  } else if (crit.first == "duration") {
    int32_t expected(std::stoul(crit.second));
    retval = (expected == dt.get_duration());
  } else if (crit.first == "author")
    retval = (crit.second == dt.get_author());
  else if (crit.first == "comment")
    retval = (crit.second == dt.get_comment());
  else
    retval = false;
  return retval;
}
