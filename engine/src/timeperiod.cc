/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
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

#include "com/centreon/engine/timeperiod.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/daterange.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/timerange.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::string;

timeperiod_map timeperiod::timeperiods;

/**
 * @brief Constructor of a timeperiod from its configuration protobuf object.
 *
 * @param obj The configuration protobuf object.
 */
timeperiod::timeperiod(const configuration::Timeperiod& obj)
    : _name{obj.timeperiod_name()}, _alias{obj.alias()}, _config{obj} {
  if (_name.empty() || _alias.empty()) {
    engine_logger(log_config_error, basic)
        << "Error: Name or alias for timeperiod is NULL";
    config_logger->error("Error: Name or alias for timeperiod is NULL");
    throw engine_error() << "Could not register time period '" << _name << "'";
  }

  // Check if the timeperiod already exist.
  timeperiod_map::const_iterator it{timeperiod::timeperiods.find(_name)};
  if (it != timeperiod::timeperiods.end()) {
    config_logger->error("Error: Timeperiod '{}' has already been defined",
                         _name);
    throw engine_error() << "Could not register time period '" << _name << "'";
  }

  // Fill time period structure.
  for (auto& r : obj.timeranges().sunday())
    days[0].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().monday())
    days[1].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().tuesday())
    days[2].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().wednesday())
    days[3].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().thursday())
    days[4].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().friday())
    days[5].emplace_back(r.range_start(), r.range_end());
  for (auto& r : obj.timeranges().saturday())
    days[6].emplace_back(r.range_start(), r.range_end());

  auto fill_exceptions = [this](const auto& obj_daterange, int idx) {
    for (auto& r : obj_daterange) {
      exceptions[idx].emplace_back(static_cast<daterange::type_range>(r.type()),
                                   r.syear(), r.smon(), r.smday(), r.swday(),
                                   r.swday_offset(), r.eyear(), r.emon(),
                                   r.emday(), r.ewday(), r.ewday_offset(),
                                   r.skip_interval(), r.timerange());
    }
  };

  fill_exceptions(obj.exceptions().calendar_date(), 0);
  fill_exceptions(obj.exceptions().month_date(), 1);
  fill_exceptions(obj.exceptions().month_day(), 2);
  fill_exceptions(obj.exceptions().month_week_day(), 3);
  fill_exceptions(obj.exceptions().week_day(), 4);

  set_exclusions(obj.exclude());
}

void timeperiod::set_exclusions(const configuration::StringSet& exclusions) {
  _exclusions.clear();
  for (auto& s : exclusions.data())
    _exclusions.emplace(s, nullptr);
}

void timeperiod::set_exceptions(const configuration::ExceptionArray& array) {
  for (auto& e : exceptions)
    e.clear();

  auto fill_exceptions = [this](const auto& obj_daterange, int idx) {
    for (auto& r : obj_daterange) {
      //      std::list<timerange> tr;
      //      for (auto& t : r.timerange())
      //        tr.emplace_back(t.range_start(), t.range_end());
      exceptions[idx].emplace_back(static_cast<daterange::type_range>(r.type()),
                                   r.syear(), r.smon(), r.smday(), r.swday(),
                                   r.swday_offset(), r.eyear(), r.emon(),
                                   r.emday(), r.ewday(), r.ewday_offset(),
                                   r.skip_interval(), r.timerange());
    }
  };

  fill_exceptions(array.calendar_date(), 0);
  fill_exceptions(array.month_date(), 1);
  fill_exceptions(array.month_day(), 2);
  fill_exceptions(array.month_week_day(), 3);
  fill_exceptions(array.week_day(), 4);

  // Keep the protobuf config in sync so calculation helpers can use it.
  *_config.mutable_exceptions() = array;
}

void timeperiod::set_name(std::string const& name) {
  _name = name;
}

void timeperiod::set_alias(std::string const& alias) {
  _alias = alias;
}

/**
 *  Equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool timeperiod::operator==(timeperiod const& obj) noexcept {
  if (_name == obj._name && _alias == obj._alias &&
      (_exclusions.size() == obj._exclusions.size() &&
       std::equal(_exclusions.begin(), _exclusions.end(),
                  obj._exclusions.begin(), obj._exclusions.end()))) {
    for (uint32_t i{0}; i < exceptions.size(); ++i)
      if (exceptions[i] != obj.exceptions[i])
        return false;
    for (uint32_t i{0}; i < days.size(); ++i)
      if (days[i] != obj.days[i])
        return false;
    return true;
  }
  return false;
}

/**
 *  Not equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool timeperiod::operator!=(timeperiod const& obj) noexcept {
  return !(*this == obj);
}

/**
 *  See if the specified time falls into a valid time range in the given
 *  time period.
 *
 *  @param[in] test_time  Time to test.
 *  @param[in] tperiod    Target time period.
 *
 *  @return true on success, false on failure.
 */
bool check_time_against_period(time_t test_time, timeperiod* tperiod) {
  SPDLOG_LOGGER_TRACE(functions_logger, "check_time_against_period()");
  // If no period was specified, assume the time is good.
  if (!tperiod)
    return true;
  return com::centreon::common::check_time_against_period(
      test_time, tperiod->get_config(),
      [tperiod](const std::string& name) -> const configuration::Timeperiod* {
        auto it = tperiod->get_exclusions().find(name);
        return (it != tperiod->get_exclusions().end() && it->second)
                   ? &it->second->get_config()
                   : nullptr;
      });
}

/**
 *  See if the specified time falls into a valid time range in the given
 *  time period for the notification.
 *
 *  @param[in] test_time  Time to test.
 *  @param[in] tperiod    Target time period.
 *
 *  @return true on success, false on failure.
 */
bool check_time_against_period_for_notif(time_t test_time,
                                         timeperiod* tperiod) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "check_time_against_period_for_notif()");
  // If no period was specified, assume the time is good.
  if (!tperiod)
    return true;
  return com::centreon::common::check_time_against_period_for_notif(
      test_time, tperiod->get_config(),
      [tperiod](const std::string& name) -> const configuration::Timeperiod* {
        auto it = tperiod->get_exclusions().find(name);
        return (it != tperiod->get_exclusions().end() && it->second)
                   ? &it->second->get_config()
                   : nullptr;
      });
}

/**
 *  Get the next invalid time within a time period (used to compute
 *  exclusions).
 *
 *  @param[in]  preferred_time  The preferred time to check.
 *  @param[out] invalid_time    Variable to fill.
 *  @param[in]  notif_timeperiod    if called for the notification .
 */
void timeperiod::get_next_invalid_time_per_timeperiod(time_t preferred_time,
                                                      time_t* invalid_time,
                                                      bool notif_timeperiod) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "get_next_invalid_time_per_timeperiod()");
  com::centreon::common::detail::get_next_invalid_time_per_timeperiod(
      _config, preferred_time, invalid_time, notif_timeperiod,
      [this](const std::string& name) -> const configuration::Timeperiod* {
        auto it = _exclusions.find(name);
        return (it != _exclusions.end() && it->second) ? &it->second->_config
                                                       : nullptr;
      });
}

/**
 *  Get the next valid time within a time period.
 *
 *  @param[in]  preferred_time      The preferred time to check.
 *  @param[out] valid_time          Variable to fill.
 *  @param[in]  notif_timeperiod    if called for the notification .
 */
void timeperiod::get_next_valid_time_per_timeperiod(time_t preferred_time,
                                                    time_t* valid_time,
                                                    bool notif_timeperiod) {
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "{} get_next_valid_time_per_timeperiod()", _name);
  com::centreon::common::get_next_valid_time_per_timeperiod(
      _config, preferred_time, valid_time, notif_timeperiod,
      [this](const std::string& name) -> const configuration::Timeperiod* {
        auto it = _exclusions.find(name);
        return (it != _exclusions.end() && it->second) ? &it->second->_config
                                                       : nullptr;
      });
}

/**
 *  Given a preferred time, get the next valid time within a time
 *  period.
 *
 *  @param[in]  preferred_time  The preferred time to check.
 *  @param[out] valid_time      Variable to fill.
 *  @param[in]  tperiod         The time period to use.
 */
void get_next_valid_time(time_t pref_time,
                         time_t* valid_time,
                         timeperiod* tperiod) {
  SPDLOG_LOGGER_TRACE(functions_logger, "get_next_valid_time()");
  if (!tperiod) {
    *valid_time = pref_time;
    return;
  }
  com::centreon::common::get_next_valid_time(
      pref_time, valid_time, &tperiod->get_config(),
      [tperiod](const std::string& name) -> const configuration::Timeperiod* {
        if (!tperiod)
          return nullptr;
        auto it = tperiod->get_exclusions().find(name);
        return (it != tperiod->get_exclusions().end() && it->second)
                   ? &it->second->get_config()
                   : nullptr;
      });
}

/**
 * Resolve the timeperiod object by checking all its contained pointers and
 * assigning them.
 *
 * @param w[out] Number of warnings produced during this resolution.
 * @param e[out] Number of errors produced during this resolution.
 *
 */
void timeperiod::resolve(uint32_t& w __attribute__((unused)), uint32_t& e) {
  uint32_t errors = 0;

  // Check for illegal characters in timeperiod name.
  if (contains_illegal_object_chars(_name.c_str())) {
    engine_logger(log_verification_error, basic)
        << "Error: The name of time period '" << _name
        << "' contains one or more illegal characters.";
    config_logger->error(
        "Error: The name of time period '{}' contains one or more illegal "
        "characters.",
        _name);
    errors++;
  }

  // Check for valid timeperiod names in exclusion list.
  for (timeperiodexclusion::iterator it{_exclusions.begin()},
       end{_exclusions.end()};
       it != end; ++it) {
    timeperiod_map::const_iterator found{
        timeperiod::timeperiods.find(it->first)};

    if (found == timeperiod::timeperiods.end()) {
      engine_logger(log_verification_error, basic)
          << "Error: Excluded time period '" << it->first
          << "' specified in timeperiod '" << _name
          << "' is not defined anywhere!";
      config_logger->error(
          "Error: Excluded time period '{}' specified in timeperiod '{}' is "
          "not defined anywhere!",
          it->first, _name);
      errors++;
    } else {
      // Save the timeperiod pointer for later.
      it->second = found->second.get();
    }
  }

  // Add errors.
  if (errors) {
    e += errors;
    throw engine_error() << "Cannot resolve time period '" << _name << "'";
  }
}

void timeperiod::set_days(const configuration::DaysArray& array) {
  for (auto& d : days)
    d.clear();

  for (auto& r : array.sunday())
    days[0].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.monday())
    days[1].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.tuesday())
    days[2].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.wednesday())
    days[3].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.thursday())
    days[4].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.friday())
    days[5].emplace_back(r.range_start(), r.range_end());
  for (auto& r : array.saturday())
    days[6].emplace_back(r.range_start(), r.range_end());

  // Keep the protobuf config in sync so calculation helpers can use it.
  *_config.mutable_timeranges() = array;
}
