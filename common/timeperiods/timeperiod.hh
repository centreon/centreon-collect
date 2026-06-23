/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2026 Centreon
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
#ifndef CCE_OBJECTS_TIMEPERIOD_HH
#define CCE_OBJECTS_TIMEPERIOD_HH

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "common/engine_conf/timeperiod_helper.hh"
#include "common/timeperiods/daterange.hh"

/* Forward declaration. */
namespace com::centreon::common::timeperiods {
class timeperiod;
}

typedef absl::flat_hash_map<
    std::string,
    std::shared_ptr<com::centreon::common::timeperiods::timeperiod>>
    timeperiod_map;
typedef std::unordered_multimap<std::string,
                                com::centreon::common::timeperiods::timeperiod*>
    timeperiodexclusion;

namespace com::centreon::common::timeperiods {

class timeperiod {
  std::string _name;
  std::string _alias;
  timeperiodexclusion _exclusions;

 public:
  timeperiod(const com::centreon::engine::configuration::Timeperiod& obj);
  void set_exclusions(
      const com::centreon::engine::configuration::StringSet& exclusions);
  void set_exceptions(
      const com::centreon::engine::configuration::ExceptionArray& array);
  void set_days(const com::centreon::engine::configuration::DaysArray& array);

  std::string const& get_name() const { return _name; };
  void set_name(const std::string& name);
  const std::string& get_alias() const { return _alias; };
  void set_alias(const std::string& alias);
  const timeperiodexclusion& get_exclusions() const { return _exclusions; };
  timeperiodexclusion& get_exclusions() { return _exclusions; };
  void get_next_valid_time_per_timeperiod(
      time_t preferred_time,
      time_t* invalid_time,
      bool notif_timeperiod,
      const absl::TimeZone& tz = absl::LocalTimeZone());
  void get_next_invalid_time_per_timeperiod(
      time_t preferred_time,
      time_t* invalid_time,
      bool notif_timeperiod,
      const absl::TimeZone& tz = absl::LocalTimeZone());

  void resolve(const timeperiod_map& all, uint32_t& w, uint32_t& e);

  bool operator==(timeperiod const& obj) noexcept;
  bool operator!=(timeperiod const& obj) noexcept;

  days_array days;
  exception_array exceptions;
};

bool check_time_against_period(time_t test_time,
                               timeperiod* tperiod,
                               const absl::TimeZone& tz = absl::LocalTimeZone());
bool check_time_against_period_for_notif(
    time_t test_time,
    timeperiod* tperiod,
    const absl::TimeZone& tz = absl::LocalTimeZone());
void get_next_valid_time(time_t pref_time,
                         time_t* valid_time,
                         timeperiod* tperiod,
                         const absl::TimeZone& tz = absl::LocalTimeZone());

}  // namespace com::centreon::common::timeperiods

#endif  // !CCE_OBJECTS_TIMEPERIOD_HH
