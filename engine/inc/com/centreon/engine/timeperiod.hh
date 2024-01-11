/**
 * Copyright 2011-2013 Merethis
 * Copyright 2023      Centreon
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

#ifndef CCE_OBJECTS_TIMEPERIOD_HH
#define CCE_OBJECTS_TIMEPERIOD_HH

#include <absl/container/flat_hash_map.h>

#include "com/centreon/engine/daterange.hh"
#include "common/configuration/state-generated.pb.h"

/* Forward declaration. */
namespace com::centreon::engine {
class timeperiod;
}

using timeperiod_map =
    std::unordered_map<std::string,
                       std::shared_ptr<com::centreon::engine::timeperiod>>;
using timeperiodexclusion =
    std::unordered_multimap<std::string, com::centreon::engine::timeperiod*>;

namespace com::centreon::engine {

class timeperiod {
 public:
  days_array days;

  timeperiod(std::string const& name, std::string const& alias);
#if !LEGACY_CONF
  timeperiod(const configuration::Timeperiod& obj);
#endif
  void set_days(const configuration::DaysArray& array);
  void set_exceptions(const configuration::ExceptionArray& array);
  void set_exclusions(const configuration::StringSet& exclusions);

  std::string const& get_name() const {
    return _name;
  };
  void set_name(std::string const& name);
  std::string const get_alias() const {
    return _alias;
  };
  void set_alias(std::string const& alias);
  const timeperiodexclusion& exclusions() const {
    return _exclusions;
  }
  timeperiodexclusion& mut_exclusions() {
    return _exclusions;
  }
  const std::array<daterange_list, DATERANGE_TYPES>& exceptions() const {
    return _exceptions;
  }

  std::array<daterange_list, DATERANGE_TYPES>& mut_exceptions() {
    return _exceptions;
  }

  void get_next_valid_time_per_timeperiod(time_t preferred_time,
                                          time_t* invalid_time,
                                          bool notif_timeperiod);
  void get_next_invalid_time_per_timeperiod(time_t preferred_time,
                                            time_t* invalid_time,
                                            bool notif_timeperiod);

  void resolve(int& w, int& e);

  bool operator==(const timeperiod& obj) noexcept;
  bool operator!=(const timeperiod& obj) noexcept;

  static timeperiod_map timeperiods;

 private:
  std::string _name;
  std::string _alias;
  timeperiodexclusion _exclusions;
  std::array<daterange_list, DATERANGE_TYPES> _exceptions;
};

}  // namespace com::centreon::engine

bool check_time_against_period(time_t test_time,
                               com::centreon::engine::timeperiod* tperiod);
bool check_time_against_period_for_notif(
    time_t test_time,
    com::centreon::engine::timeperiod* tperiod);
void get_next_valid_time(time_t pref_time,
                         time_t* valid_time,
                         com::centreon::engine::timeperiod* tperiod);

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::timeperiod const& obj);
std::ostream& operator<<(std::ostream& os, timeperiodexclusion const& obj);

#endif  // !CCE_OBJECTS_TIMEPERIOD_HH
