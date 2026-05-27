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

#ifndef CCC_DOWNTIMES_DOWNTIME_FINDER_HH
#define CCC_DOWNTIMES_DOWNTIME_FINDER_HH

namespace com::centreon::common::downtimes {

class downtime;
class host_downtime;
class service_downtime;

/**
 *  @class downtime_finder downtime_finder.hh
 * "com/centreon/engine/downtime_finder.hh"
 *  @brier Find active downtimes.
 *
 *  This class can find active downtimes according to some criterias.
 */
class downtime_finder {
 public:
  using criteria = std::pair<std::string, std::string>;
  using criteria_set = std::vector<criteria>;
  using result_set = std::vector<unsigned long>;

  downtime_finder(std::multimap<time_t, std::shared_ptr<downtime>> const& map);
  downtime_finder(downtime_finder const& other) = default;
  downtime_finder(downtime_finder&& other) = default;
  downtime_finder& operator=(const downtime_finder&) = delete;
  ~downtime_finder() noexcept = default;
  result_set find_matching_all(criteria_set const& criterias);

 private:
  bool _match_criteria(host_downtime const& dt, criteria const& crit);
  bool _match_criteria(service_downtime const& dt, criteria const& crit);

  const std::multimap<time_t, std::shared_ptr<downtime>>& _map;
};
}  // namespace com::centreon::common::downtimes

#endif  // !CCC_DOWNTIMES_DOWNTIME_FINDER_HH
