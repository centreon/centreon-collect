/**
 * Copyright 2011-2013,2017-2024 Centreon
 * Copyright 2017 - 2024 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/deleter/listmember.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/timeperiod.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

#ifdef LEGACY_CONF
/**
 *  Add new time period.
 *
 *  @param[in] obj  The new time period to add in the monitoring engine.
 */
void applier::timeperiod::add_object(configuration::timeperiod const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Creating new time period '" << obj.timeperiod_name() << "'.";
  config_logger->debug("Creating new time period '{}'.", obj.timeperiod_name());

  // Add time period to the global configuration set.
  config->timeperiods().insert(obj);

  // Create time period.
  auto tp =
      std::make_shared<engine::timeperiod>(obj.timeperiod_name(), obj.alias());

  engine::timeperiod::timeperiods.insert({obj.timeperiod_name(), tp});

  // Notify event broker.
  timeval tv(get_broker_timestamp(nullptr));
  broker_adaptive_timeperiod_data(NEBTYPE_TIMEPERIOD_ADD, NEBFLAG_NONE,
                                  NEBATTR_NONE, tp.get(), CMD_NONE, &tv);

  // Fill time period structure.
  for (uint32_t i = 0; i < obj.timeranges().size(); i++) {
    for (auto& tr : obj.timeranges()[i])
      tp->days[i].push_back({tr.range_start(), tr.range_end()});
  }
  for (uint32_t i = 0; i < obj.exceptions().size(); i++) {
    for (auto& dr : obj.exceptions()[i]) {
      tp->exceptions[i].push_back(
          {static_cast<com::centreon::engine::daterange::type_range>(dr.type()),
           dr.get_syear(), dr.get_smon(), dr.get_smday(), dr.get_swday(),
           dr.get_swday_offset(), dr.get_eyear(), dr.get_emon(), dr.get_emday(),
           dr.get_ewday(), dr.get_ewday_offset(), dr.get_skip_interval(),
           dr.get_timerange()});
    }
  }
  _add_exclusions(obj.exclude(), tp.get());
}
#else
/**
 * @brief Add new time period.
 *
 *  @param[in] obj  The new time period to add in the monitoring engine.
 */
void applier::timeperiod::add_object(const configuration::Timeperiod& obj) {
  // Logging.
  config_logger->debug("Creating new time period '{}'.", obj.timeperiod_name());

  if (obj.timeperiod_name().empty() || obj.alias().empty()) {
    throw engine_error() << fmt::format(
        "Could not register time period '{}' (alias '{}'): timeperiod name and "
        "alias must not be empty",
        obj.timeperiod_name(), obj.alias());
  }

  // Add time period to the global configuration set.
  configuration::Timeperiod* c_tp = pb_config.add_timeperiods();
  c_tp->CopyFrom(obj);

  // Create time period.
  auto tp = std::make_shared<engine::timeperiod>(obj);
  engine::timeperiod::timeperiods.insert({obj.timeperiod_name(), tp});
}
#endif

#ifdef LEGACY_CONF
/**
 *  @brief Expand time period.
 *
 *  Time period objects do not need expansion. Therefore this method
 *  does nothing.
 *
 *  @param[in] s  Unused.
 */
void applier::timeperiod::expand_objects(configuration::state& s) {
  (void)s;
}
#else
/**
 *  @brief Expand time period.
 *
 *  Time period objects do not need expansion. Therefore this method
 *  does nothing.
 *
 *  @param[in] s  Unused.
 */
void applier::timeperiod::expand_objects(configuration::State& s
                                         [[maybe_unused]]) {}
#endif

#ifdef LEGACY_CONF
/**
 *  Modify time period.
 *
 *  @param[in] obj  The time period to modify in the monitoring engine.
 */
void applier::timeperiod::modify_object(configuration::timeperiod const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Modifying time period '" << obj.timeperiod_name() << "'.";
  config_logger->debug("Modifying time period '{}'.", obj.timeperiod_name());

  // Find old configuration.
  set_timeperiod::iterator it_cfg(config->timeperiods_find(obj.key()));
  if (it_cfg == config->timeperiods().end())
    throw(engine_error() << "Could not modify non-existing "
                         << "time period '" << obj.timeperiod_name() << "'");

  // Find time period object.
  timeperiod_map::iterator it_obj(
      engine::timeperiod::timeperiods.find(obj.key()));
  if (it_obj == engine::timeperiod::timeperiods.end() || !it_obj->second)
    throw(engine_error() << "Could not modify non-existing "
                         << "time period object '" << obj.timeperiod_name()
                         << "'");
  engine::timeperiod* tp(it_obj->second.get());

  // Update the global configuration set.
  configuration::timeperiod old_cfg(*it_cfg);
  config->timeperiods().erase(it_cfg);
  config->timeperiods().insert(obj);

  // Modify properties.
  if (obj.alias() != tp->get_alias())
    tp->set_alias(obj.alias().empty() ? obj.timeperiod_name() : obj.alias());

  // Time ranges modified ?
  if (obj.timeranges() != old_cfg.timeranges()) {
    for (uint32_t i = 0; i < tp->days.size(); i++) {
      tp->days[i].clear();
      for (auto& tr : obj.timeranges()[i])
        tp->days[i].push_back({tr.range_start(), tr.range_end()});
    }
  }

  // Exceptions modified ?
  if (obj.exceptions() != old_cfg.exceptions()) {
    for (uint32_t i = 0; i < obj.exceptions().size(); i++) {
      tp->exceptions[i].clear();
      for (auto& dr : obj.exceptions()[i]) {
        tp->exceptions[i].push_back(
            {static_cast<com::centreon::engine::daterange::type_range>(
                 dr.type()),
             dr.get_syear(), dr.get_smon(), dr.get_smday(), dr.get_swday(),
             dr.get_swday_offset(), dr.get_eyear(), dr.get_emon(),
             dr.get_emday(), dr.get_ewday(), dr.get_ewday_offset(),
             dr.get_skip_interval(), dr.get_timerange()});
      }
    }
  }

  // Exclusions modified ?
  if (obj.exclude() != old_cfg.exclude()) {
    // Delete old exclusions.
    tp->get_exclusions().clear();
    // Create new exclusions.
    _add_exclusions(obj.exclude(), tp);
  }

  // Notify event broker.
  timeval tv(get_broker_timestamp(nullptr));
  broker_adaptive_timeperiod_data(NEBTYPE_TIMEPERIOD_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, tp, CMD_NONE, &tv);
}
#else
/**
 *  Modify time period.
 *
 *  @param[in] obj  The time period to modify in the monitoring engine.
 */
void applier::timeperiod::modify_object(
    configuration::Timeperiod* to_modify,
    const configuration::Timeperiod& new_obj) {
  // Logging.
  config_logger->debug("Modifying time period '{}'.",
                       to_modify->timeperiod_name());

  // Find time period object.
  timeperiod_map::iterator it_obj =
      engine::timeperiod::timeperiods.find(to_modify->timeperiod_name());
  if (it_obj == engine::timeperiod::timeperiods.end() || !it_obj->second)
    throw engine_error() << fmt::format(
        "Could not modify non-existing time period object '{}'",
        to_modify->timeperiod_name());

  engine::timeperiod* tp(it_obj->second.get());

  // Modify properties.
  if (to_modify->alias() != new_obj.alias()) {
    tp->set_alias(new_obj.alias().empty() ? new_obj.timeperiod_name()
                                          : new_obj.alias());
    to_modify->set_alias(new_obj.alias());
  }

  // Time ranges modified ?
  if (!MessageDifferencer::Equals(to_modify->timeranges(),
                                  new_obj.timeranges())) {
    tp->set_days(new_obj.timeranges());
    to_modify->mutable_timeranges()->CopyFrom(new_obj.timeranges());
  }

  // Exceptions modified ?
  if (!MessageDifferencer::Equals(to_modify->exceptions(),
                                  new_obj.exceptions())) {
    tp->set_exceptions(new_obj.exceptions());
    to_modify->mutable_exceptions()->CopyFrom(new_obj.exceptions());
  }

  // Exclusions modified ?
  if (!MessageDifferencer::Equals(to_modify->exclude(), new_obj.exclude())) {
    // Delete old exclusions.
    tp->get_exclusions().clear();
    // Create new exclusions.
    tp->set_exclusions(new_obj.exclude());
    to_modify->mutable_exclude()->CopyFrom(new_obj.exclude());
  }
}
#endif

#ifdef LEGACY_CONF
/**
 *  Remove old time period.
 *
 *  @param[in] obj  The time period to remove from the monitoring engine.
 */
void applier::timeperiod::remove_object(configuration::timeperiod const& obj) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Removing time period '" << obj.timeperiod_name() << "'.";
  config_logger->debug("Removing time period '{}'.", obj.timeperiod_name());

  // Find time period.
  timeperiod_map::iterator it(engine::timeperiod::timeperiods.find(obj.key()));
  if (it != engine::timeperiod::timeperiods.end() && it->second) {
    // Notify event broker.
    timeval tv(get_broker_timestamp(nullptr));
    broker_adaptive_timeperiod_data(NEBTYPE_TIMEPERIOD_DELETE, NEBFLAG_NONE,
                                    NEBATTR_NONE, it->second.get(), CMD_NONE,
                                    &tv);

    // Erase time period (will effectively delete the object).
    engine::timeperiod::timeperiods.erase(it);
  }

  // Remove time period from the global configuration set.
  config->timeperiods().erase(obj);
}
#else
void applier::timeperiod::remove_object(ssize_t idx) {
  /* obj is the object to remove */
  auto& obj = pb_config.timeperiods()[idx];
  config_logger->debug("Removing time period '{}'.", obj.timeperiod_name());

  // Find time period.
  timeperiod_map::iterator it =
      engine::timeperiod::timeperiods.find(obj.timeperiod_name());
  if (it != engine::timeperiod::timeperiods.end() && it->second) {
    // Erase time period (will effectively delete the object).
    engine::timeperiod::timeperiods.erase(it);
  }

  // Remove time period from the global configuration set.
  pb_config.mutable_timeperiods()->DeleteSubrange(idx, 1);
}
#endif

#ifdef LEGACY_CONF
/**
 *  @brief Resolve a time period object.
 *
 *  This method does nothing because a time period object does not rely
 *  on any external object.
 *
 *  @param[in] obj Unused.
 */
void applier::timeperiod::resolve_object(configuration::timeperiod const& obj,
                                         error_cnt& err) {
  // Logging.
  engine_logger(logging::dbg_config, logging::more)
      << "Resolving time period '" << obj.timeperiod_name() << "'.";
  config_logger->debug("Resolving time period '{}'.", obj.timeperiod_name());

  // Find time period.
  timeperiod_map::iterator it{engine::timeperiod::timeperiods.find(obj.key())};
  if (engine::timeperiod::timeperiods.end() == it || !it->second)
    throw engine_error() << "Cannot resolve non-existing "
                         << "time period '" << obj.timeperiod_name() << "'";

  // Resolve time period.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#else
/**
 *  @brief Resolve a time period object.
 *
 *  This method does nothing because a time period object does not rely
 *  on any external object.
 *
 *  @param[in] obj Unused.
 */
void applier::timeperiod::resolve_object(const configuration::Timeperiod& obj,
                                         error_cnt& err) {
  // Logging.
  config_logger->debug("Resolving time period '{}'.", obj.timeperiod_name());

  // Find time period.
  timeperiod_map::iterator it =
      engine::timeperiod::timeperiods.find(obj.timeperiod_name());
  if (engine::timeperiod::timeperiods.end() == it || !it->second)
    throw engine_error() << "Cannot resolve non-existing "
                         << "time period '" << obj.timeperiod_name() << "'";

  // Resolve time period.
  it->second->resolve(err.config_warnings, err.config_errors);
}
#endif

#ifdef LEGACY_CONF
/**
 *  Add exclusions to a time period.
 *
 *  @param[in]  exclusions Exclusions.
 *  @param[out] tp         Time period object.
 */
void applier::timeperiod::_add_exclusions(
    std::set<std::string> const& exclusions,
    engine::timeperiod* tp) {
  for (set_string::const_iterator it(exclusions.begin()), end(exclusions.end());
       it != end; ++it)
    tp->get_exclusions().insert({*it, nullptr});
}
#endif
