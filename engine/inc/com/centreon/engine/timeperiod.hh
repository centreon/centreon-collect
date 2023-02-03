/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_OBJECTS_TIMEPERIOD_HH
#define CCE_OBJECTS_TIMEPERIOD_HH

#include "com/centreon/engine/daterange.hh"
#include "configuration/state-generated.pb.h"

/* Forward declaration. */
CCE_BEGIN()
class timeperiod;
CCE_END()

using timeperiod_map =
    std::unordered_map<std::string,
                       std::shared_ptr<com::centreon::engine::timeperiod>>;
using timeperiodexclusion =
    std::unordered_multimap<std::string, com::centreon::engine::timeperiod*>;

CCE_BEGIN()

class timeperiod {
 public:
  days_array days;

  timeperiod(std::string const& name, std::string const& alias);
  timeperiod(const configuration::Timeperiod& obj);
  void set_days(const configuration::DaysArray& array);
  void set_exceptions(const configuration::ExceptionArray& array);
  void set_exclusions(const configuration::StringSet& exclusions);

  std::string const& get_name() const { return _name; };
  void set_name(std::string const& name);
  std::string const get_alias() const { return _alias; };
  void set_alias(std::string const& alias);
  timeperiodexclusion const& get_exclusions() const { return _exclusions; };
  timeperiodexclusion& get_exclusions() { return _exclusions; };
  void get_next_valid_time_per_timeperiod(time_t preferred_time,
                                          time_t* invalid_time,
                                          bool notif_timeperiod);
  void get_next_invalid_time_per_timeperiod(time_t preferred_time,
                                            time_t* invalid_time,
                                            bool notif_timeperiod);

  void resolve(int& w, int& e);

  bool operator==(timeperiod const& obj) throw();
  bool operator!=(timeperiod const& obj) throw();

  exception_array exceptions;

  static timeperiod_map timeperiods;

 private:
  std::string _name;
  std::string _alias;
  timeperiodexclusion _exclusions;
};

CCE_END()

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
