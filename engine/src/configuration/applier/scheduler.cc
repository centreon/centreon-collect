/**
 * Copyright 2011-2024 Centreon
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

#include "com/centreon/engine/configuration/applier/scheduler.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/deleter/listmember.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/timezone_locker.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::logging;
using namespace com::centreon::logging;

/**
 *  Apply new configuration.
 *
 *  @param[in] config        The new configuration.
 *  @param[in] diff          The diff to apply.
 */
void applier::scheduler::apply(configuration::State& config,
                               const configuration::DiffState& diff) {
  // Internal pointer will be used in private methods.
  _pb_config = &config;

  // Remove and create misc event.
  _apply_misc_event();

  // Objects set.
  std::vector<uint64_t> hst_to_unschedule;
  for (auto& d : diff.hosts().removed())
    hst_to_unschedule.emplace_back(d);

  std::vector<std::pair<uint64_t, uint64_t> > svc_to_unschedule;
  for (auto& d : diff.services().removed())
    svc_to_unschedule.emplace_back(d.host_id(), d.service_id());

  std::vector<std::pair<uint64_t, uint64_t> > ad_to_unschedule;
  for (auto& d : diff.anomalydetections().removed())
    ad_to_unschedule.emplace_back(d.host_id(), d.service_id());

  std::vector<uint64_t> hst_to_schedule;
  for (auto& a : diff.hosts().added())
    hst_to_schedule.emplace_back(a.host_id());

  std::vector<std::pair<uint64_t, uint64_t> > svc_to_schedule;
  for (auto& a : diff.services().added())
    svc_to_schedule.emplace_back(a.host_id(), a.service_id());

  std::vector<std::pair<uint64_t, uint64_t> > ad_to_schedule;
  for (auto& a : diff.anomalydetections().added())
    ad_to_schedule.emplace_back(a.host_id(), a.service_id());

  for (auto& m : diff.hosts().modified()) {
    auto it_hst = engine::host::hosts.find(m.host_name());
    if (it_hst != engine::host::hosts.end()) {
      bool has_event(events::loop::instance().find_event(
                         events::loop::low, timed_event::EVENT_HOST_CHECK,
                         it_hst->second.get()) !=
                     events::loop::instance().list_end(events::loop::low));
      bool should_schedule(m.checks_active() && m.check_interval() > 0);
      if (has_event && should_schedule) {
        hst_to_unschedule.emplace_back(m.host_id());
        hst_to_schedule.emplace_back(m.host_id());
      } else if (!has_event && should_schedule)
        hst_to_schedule.emplace_back(m.host_id());
      else if (has_event && !should_schedule)
        hst_to_unschedule.emplace_back(m.host_id());
      // Else it has no event and should not be scheduled, so do nothing.
    }
  }

  for (auto& m : diff.services().modified()) {
    auto it_svc =
        engine::service::services_by_id.find({m.host_id(), m.service_id()});
    if (it_svc != engine::service::services_by_id.end()) {
      bool has_event(events::loop::instance().find_event(
                         events::loop::low, timed_event::EVENT_SERVICE_CHECK,
                         it_svc->second.get()) !=
                     events::loop::instance().list_end(events::loop::low));
      bool should_schedule(m.checks_active() && (m.check_interval() > 0));
      if (has_event && should_schedule) {
        svc_to_unschedule.emplace_back(m.host_id(), m.service_id());
        svc_to_schedule.emplace_back(m.host_id(), m.service_id());
      } else if (!has_event && should_schedule)
        svc_to_schedule.emplace_back(m.host_id(), m.service_id());
      else if (has_event && !should_schedule)
        svc_to_unschedule.emplace_back(m.host_id(), m.service_id());
      // Else it has no event and should not be scheduled, so do nothing.
    }
  }

  for (auto& m : diff.anomalydetections().modified()) {
    auto it_svc =
        engine::service::services_by_id.find({m.host_id(), m.service_id()});
    if (it_svc != engine::service::services_by_id.end()) {
      bool has_event(events::loop::instance().find_event(
                         events::loop::low, timed_event::EVENT_SERVICE_CHECK,
                         it_svc->second.get()) !=
                     events::loop::instance().list_end(events::loop::low));
      bool should_schedule = m.checks_active() && m.check_interval() > 0;
      if (has_event && should_schedule) {
        ad_to_unschedule.emplace_back(m.host_id(), m.service_id());
        ad_to_schedule.emplace_back(m.host_id(), m.service_id());
      } else if (!has_event && should_schedule)
        ad_to_schedule.emplace_back(m.host_id(), m.service_id());
      else if (has_event && !should_schedule)
        ad_to_unschedule.emplace_back(m.host_id(), m.service_id());
      // Else it has no event and should not be scheduled, so do nothing.
    }
  }

  // Remove deleted host check from the scheduler.
  {
    std::vector<com::centreon::engine::host*> old_hosts =
        _get_hosts(hst_to_unschedule, false);
    _unschedule_host_events(old_hosts);
  }

  // Remove deleted service check from the scheduler.
  {
    std::vector<engine::service*> old_services =
        _get_services(svc_to_unschedule, false);
    _unschedule_service_events(old_services);
  }

  // Remove deleted anomalydetection check from the scheduler.
  {
    std::vector<engine::service*> old_anomalydetections =
        _get_anomalydetections(ad_to_unschedule, false);
    _unschedule_service_events(old_anomalydetections);
  }
  // Check if we need to add or modify objects into the scheduler.
  if (!hst_to_schedule.empty() || !svc_to_schedule.empty() ||
      !ad_to_schedule.empty()) {
    memset(&scheduling_info, 0, sizeof(scheduling_info));

    if (config.service_interleave_factor_method().type() ==
        configuration::InterleaveFactor::ilf_user)
      scheduling_info.service_interleave_factor =
          config.service_interleave_factor_method().user_value();
    if (config.service_inter_check_delay_method().type() ==
        configuration::InterCheckDelay::user)
      scheduling_info.service_inter_check_delay =
          config.service_inter_check_delay_method().user_value();
    if (config.host_inter_check_delay_method().type() ==
        configuration::InterCheckDelay::user)
      scheduling_info.host_inter_check_delay =
          config.host_inter_check_delay_method().user_value();

    // Calculate scheduling parameters.
    _calculate_host_scheduling_params();
    _calculate_service_scheduling_params();

    // Get and schedule new hosts.
    {
      std::vector<com::centreon::engine::host*> new_hosts =
          _get_hosts(hst_to_schedule, true);
      _schedule_host_events(new_hosts);
    }

    // Get and schedule new services and anomalydetections.
    {
      std::vector<engine::service*> new_services =
          _get_services(svc_to_schedule, true);
      std::vector<engine::service*> new_anomalydetections =
          _get_anomalydetections(ad_to_schedule, true);
      new_services.insert(
          new_services.end(),
          std::make_move_iterator(new_anomalydetections.begin()),
          std::make_move_iterator(new_anomalydetections.end()));
      _schedule_service_events(new_services);
    }
  }
}

/**
 *  Get the singleton instance of scheduler applier.
 *
 *  @return Singleton instance.
 */
applier::scheduler& applier::scheduler::instance() {
  static applier::scheduler instance;
  return instance;
}

void applier::scheduler::clear() {
  _pb_config = nullptr;
  _evt_check_reaper = nullptr;
  _evt_command_check = nullptr;
  _evt_hfreshness_check = nullptr;
  _evt_orphan_check = nullptr;
  _evt_retention_save = nullptr;
  _evt_sfreshness_check = nullptr;
  _evt_status_save = nullptr;
  _old_check_reaper_interval = 0;
  _old_command_check_interval = 0;
  _old_host_freshness_check_interval = 0;
  _old_retention_update_interval = 0;
  _old_service_freshness_check_interval = 0;
  _old_status_update_interval = 0;

  memset(&scheduling_info, 0, sizeof(scheduling_info));
}

/**
 *  Remove some host from scheduling.
 *
 *  @param[in] h  Host configuration.
 */
void applier::scheduler::remove_host(uint64_t host_id) {
  host_id_map const& hosts(engine::host::hosts_by_id);
  host_id_map::const_iterator hst(hosts.find(host_id));
  if (hst != hosts.end()) {
    std::vector<com::centreon::engine::host*> hvec;
    hvec.push_back(hst->second.get());
    _unschedule_host_events(hvec);
  }
}

/**
 *  Remove some service from scheduling.
 *
 *  @param[in] s  Service configuration.
 */
void applier::scheduler::remove_service(uint64_t host_id, uint64_t service_id) {
  service_id_map const& services(engine::service::services_by_id);
  service_id_map::const_iterator svc(services.find({host_id, service_id}));
  if (svc != services.end()) {
    std::vector<engine::service*> svec;
    svec.push_back(svc->second.get());
    _unschedule_service_events(svec);
  }
}

/**
 *  Default constructor.
 */
applier::scheduler::scheduler()
    : _pb_config(nullptr),
      _evt_check_reaper(nullptr),
      _evt_command_check(nullptr),
      _evt_hfreshness_check(nullptr),
      _evt_orphan_check(nullptr),
      _evt_retention_save(nullptr),
      _evt_sfreshness_check(nullptr),
      _evt_status_save(nullptr),
      _old_check_reaper_interval(0),
      _old_command_check_interval(0),
      _old_host_freshness_check_interval(0),
      _old_retention_update_interval(0),
      _old_service_freshness_check_interval(0),
      _old_status_update_interval(0) {}

/**
 *  Default destructor.
 */
applier::scheduler::~scheduler() noexcept {}

/**
 *  Remove and create misc event if necessary.
 */
void applier::scheduler::_apply_misc_event() {
  // Get current time.
  time_t const now = time(nullptr);

  // Remove and add check result reaper event.
  if (!_evt_check_reaper ||
      _old_check_reaper_interval != _pb_config->check_reaper_interval()) {
    _remove_misc_event(_evt_check_reaper);
    _evt_check_reaper =
        _create_misc_event(timed_event::EVENT_CHECK_REAPER,
                           now + _pb_config->check_reaper_interval(),
                           _pb_config->check_reaper_interval());
    _old_check_reaper_interval = _pb_config->check_reaper_interval();
  }

  // Remove and add an external command check event.
  if ((!_evt_command_check && _pb_config->check_external_commands()) ||
      (_evt_command_check && !_pb_config->check_external_commands()) ||
      (_old_command_check_interval != _pb_config->command_check_interval())) {
    _remove_misc_event(_evt_command_check);
    if (_pb_config->check_external_commands()) {
      unsigned long interval(5);
      if (_pb_config->command_check_interval() != -1)
        interval = (unsigned long)_pb_config->command_check_interval();
      _evt_command_check = _create_misc_event(timed_event::EVENT_COMMAND_CHECK,
                                              now + interval, interval);
    }
    _old_command_check_interval = _pb_config->command_check_interval();
  }

  // Remove and add a host result "freshness" check event.
  if ((!_evt_hfreshness_check && _pb_config->check_host_freshness()) ||
      (_evt_hfreshness_check && !_pb_config->check_host_freshness()) ||
      (_old_host_freshness_check_interval !=
       _pb_config->host_freshness_check_interval())) {
    _remove_misc_event(_evt_hfreshness_check);
    if (_pb_config->check_host_freshness())
      _evt_hfreshness_check =
          _create_misc_event(timed_event::EVENT_HFRESHNESS_CHECK,
                             now + _pb_config->host_freshness_check_interval(),
                             _pb_config->host_freshness_check_interval());
    _old_host_freshness_check_interval =
        _pb_config->host_freshness_check_interval();
  }

  // Remove and add an orphaned check event.
  if ((!_evt_orphan_check && _pb_config->check_orphaned_services()) ||
      (!_evt_orphan_check && _pb_config->check_orphaned_hosts()) ||
      (_evt_orphan_check && !_pb_config->check_orphaned_services() &&
       !_pb_config->check_orphaned_hosts())) {
    _remove_misc_event(_evt_orphan_check);
    if (_pb_config->check_orphaned_services() ||
        _pb_config->check_orphaned_hosts())
      _evt_orphan_check = _create_misc_event(
          timed_event::EVENT_ORPHAN_CHECK, now + DEFAULT_ORPHAN_CHECK_INTERVAL,
          DEFAULT_ORPHAN_CHECK_INTERVAL);
  }

  // Remove and add a retention data save event if needed.
  if ((!_evt_retention_save && _pb_config->retain_state_information()) ||
      (_evt_retention_save && !_pb_config->retain_state_information()) ||
      (_old_retention_update_interval !=
       _pb_config->retention_update_interval())) {
    _remove_misc_event(_evt_retention_save);
    if (_pb_config->retain_state_information() &&
        _pb_config->retention_update_interval() > 0) {
      unsigned long interval(_pb_config->retention_update_interval() * 60);
      _evt_retention_save = _create_misc_event(
          timed_event::EVENT_RETENTION_SAVE, now + interval, interval);
    }
    _old_retention_update_interval = _pb_config->retention_update_interval();
  }

  // Remove add a service result "freshness" check event.
  if ((!_evt_sfreshness_check && _pb_config->check_service_freshness()) ||
      (!_evt_sfreshness_check && !_pb_config->check_service_freshness()) ||
      _old_service_freshness_check_interval !=
          _pb_config->service_freshness_check_interval()) {
    _remove_misc_event(_evt_sfreshness_check);
    if (_pb_config->check_service_freshness())
      _evt_sfreshness_check = _create_misc_event(
          timed_event::EVENT_SFRESHNESS_CHECK,
          now + _pb_config->service_freshness_check_interval(),
          _pb_config->service_freshness_check_interval());
    _old_service_freshness_check_interval =
        _pb_config->service_freshness_check_interval();
  }

  // Remove and add a status save event.
  if (!_evt_status_save ||
      (_old_status_update_interval != _pb_config->status_update_interval())) {
    _remove_misc_event(_evt_status_save);
    _evt_status_save =
        _create_misc_event(timed_event::EVENT_STATUS_SAVE,
                           now + _pb_config->status_update_interval(),
                           _pb_config->status_update_interval());
    _old_status_update_interval = _pb_config->status_update_interval();
  }
}

/**
 *  How should we determine the host inter-check delay to use.
 *
 *  @param[in] method The method to use to calculate inter check delay.
 */
void applier::scheduler::_calculate_host_inter_check_delay(
    const configuration::InterCheckDelay& method) {
  switch (method.type()) {
    case configuration::InterCheckDelay::none:
      scheduling_info.host_inter_check_delay = 0.0;
      break;

    case configuration::InterCheckDelay::dumb:
      scheduling_info.host_inter_check_delay = 1.0;
      break;

    case configuration::InterCheckDelay::user:
      // the user specified a delay, so don't try to calculate one.
      break;

    case configuration::InterCheckDelay::smart:
    default:
      // be smart and calculate the best delay to use
      // to minimize local load...
      if (scheduling_info.total_scheduled_hosts > 0 &&
          scheduling_info.host_check_interval_total > 0) {
        // calculate the average check interval for hosts.
        scheduling_info.average_host_check_interval =
            scheduling_info.host_check_interval_total /
            (double)scheduling_info.total_scheduled_hosts;

        // calculate the average inter check delay (in seconds)
        // needed to evenly space the host checks out.
        scheduling_info.average_host_inter_check_delay =
            scheduling_info.average_host_check_interval /
            (double)scheduling_info.total_scheduled_hosts;

        // set the global inter check delay value.
        scheduling_info.host_inter_check_delay =
            scheduling_info.average_host_inter_check_delay;

        // calculate max inter check delay and see if we should use that
        // instead.
        double const max_inter_check_delay(
            (scheduling_info.max_host_check_spread * 60) /
            (double)scheduling_info.total_scheduled_hosts);
        if (scheduling_info.host_inter_check_delay > max_inter_check_delay)
          scheduling_info.host_inter_check_delay = max_inter_check_delay;
      } else
        scheduling_info.host_inter_check_delay = 0.0;

      events_logger->debug("Total scheduled host checks:  {}",
                           scheduling_info.total_scheduled_hosts);
      events_logger->debug("Host check interval total:    {}",
                           scheduling_info.host_check_interval_total);
      events_logger->debug("Average host check interval:  {:.2f} sec",
                           scheduling_info.average_host_check_interval);
      events_logger->debug("Host inter-check delay:       {:.2f} sec",
                           scheduling_info.host_inter_check_delay);
  }
}

/**
 *  Calculate host scheduling params.
 */
void applier::scheduler::_calculate_host_scheduling_params() {
  engine_logger(dbg_events, most)
      << "Determining host scheduling parameters...";
  events_logger->debug("Determining host scheduling parameters...");

  // get current time.
  time_t const now(time(nullptr));

  // get total hosts and total scheduled hosts.
  for (host_map::const_iterator it(engine::host::hosts.begin()),
       end(engine::host::hosts.end());
       it != end; ++it) {
    com::centreon::engine::host& hst(*it->second);

    bool schedule_check(true);
    if (!hst.check_interval() || !hst.active_checks_enabled())
      schedule_check = false;
    else {
      timezone_locker lock(hst.get_timezone());
      if (!check_time_against_period(now, hst.check_period_ptr)) {
        time_t next_valid_time(0);
        get_next_valid_time(now, &next_valid_time, hst.check_period_ptr);
        if (now == next_valid_time)
          schedule_check = false;
      }
    }

    if (schedule_check) {
      hst.set_should_be_scheduled(true);
      ++scheduling_info.total_scheduled_hosts;
      scheduling_info.host_check_interval_total +=
          static_cast<unsigned long>(hst.check_interval());
    } else {
      hst.set_should_be_scheduled(false);
      engine_logger(dbg_events, more)
          << "Host " << hst.name() << " should not be scheduled.";
      events_logger->debug("Host {} should not be scheduled.", hst.name());
    }

    ++scheduling_info.total_hosts;
  }

  // Default max host check spread (in minutes).
  scheduling_info.max_host_check_spread = _pb_config->max_host_check_spread();

  // Adjust the check interval total to correspond to
  // the interval length.
  scheduling_info.host_check_interval_total =
      scheduling_info.host_check_interval_total * _pb_config->interval_length();

  _calculate_host_inter_check_delay(
      _pb_config->host_inter_check_delay_method());
}

/**
 *  How should we determine the service inter-check delay
 *  to use (in seconds).
 *
 *  @param[in] method The method to use to calculate inter check delay.
 */
void applier::scheduler::_calculate_service_inter_check_delay(
    const configuration::InterCheckDelay& method) {
  switch (method.type()) {
    case configuration::InterCheckDelay::none:
      scheduling_info.service_inter_check_delay = 0.0;
      break;

    case configuration::InterCheckDelay::dumb:
      scheduling_info.service_inter_check_delay = 1.0;
      break;

    case configuration::InterCheckDelay::user:
      // the user specified a delay, so don't try to calculate one.
      break;

    case configuration::InterCheckDelay::smart:
    default:
      // be smart and calculate the best delay to use to
      // minimize local load...
      if (scheduling_info.total_scheduled_services > 0 &&
          scheduling_info.service_check_interval_total > 0) {
        // calculate the average inter check delay (in seconds) needed
        // to evenly space the service checks out.
        scheduling_info.average_service_inter_check_delay =
            scheduling_info.average_service_check_interval /
            (double)scheduling_info.total_scheduled_services;

        // set the global inter check delay value.
        scheduling_info.service_inter_check_delay =
            scheduling_info.average_service_inter_check_delay;

        // calculate max inter check delay and see if we should use that
        // instead.
        double const max_inter_check_delay(
            (scheduling_info.max_service_check_spread * 60) /
            (double)scheduling_info.total_scheduled_services);
        if (scheduling_info.service_inter_check_delay > max_inter_check_delay)
          scheduling_info.service_inter_check_delay = max_inter_check_delay;
      } else
        scheduling_info.service_inter_check_delay = 0.0;

      events_logger->debug("Total scheduled service checks:  {}",
                           scheduling_info.total_scheduled_services);
      events_logger->debug("Average service check interval:  {:.2f} sec",
                           scheduling_info.average_service_check_interval);
      events_logger->debug("Service inter-check delay:       {:.2f} sec",
                           scheduling_info.service_inter_check_delay);
  }
}

/**
 *  How should we determine the service interleave factor.
 *
 *  @param[in] method The method to use to calculate interleave factor.
 */
void applier::scheduler::_calculate_service_interleave_factor(
    const configuration::InterleaveFactor& method) {
  switch (method.type()) {
    case configuration::InterleaveFactor::ilf_user:
      // the user supplied a value, so don't do any calculation.
      break;

    case configuration::InterleaveFactor::ilf_smart:
    default:
      scheduling_info.service_interleave_factor =
          (int)(ceil(scheduling_info.average_scheduled_services_per_host));

      events_logger->debug("Total scheduled service checks: {}",
                           scheduling_info.total_scheduled_services);
      events_logger->debug("Total hosts:                    {}",
                           scheduling_info.total_hosts);
      events_logger->debug("Service Interleave factor:      {}",
                           scheduling_info.service_interleave_factor);
  }
}

/**
 *  Calculate service scheduling params.
 */
void applier::scheduler::_calculate_service_scheduling_params() {
  events_logger->debug("Determining service scheduling parameters...");

  // get current time.
  time_t const now(time(nullptr));

  // get total services and total scheduled services.
  for (service_id_map::const_iterator
           it(engine::service::services_by_id.begin()),
       end(engine::service::services_by_id.end());
       it != end; ++it) {
    engine::service& svc(*it->second);

    bool schedule_check(true);
    if (!svc.check_interval() || !svc.active_checks_enabled())
      schedule_check = false;

    {
      timezone_locker lock(svc.get_timezone());
      if (!check_time_against_period(now, svc.check_period_ptr)) {
        time_t next_valid_time(0);
        get_next_valid_time(now, &next_valid_time, svc.check_period_ptr);
        if (now == next_valid_time)
          schedule_check = false;
      }
    }

    if (schedule_check) {
      svc.set_should_be_scheduled(true);
      ++scheduling_info.total_scheduled_services;
      scheduling_info.service_check_interval_total +=
          static_cast<unsigned long>(svc.check_interval());
    } else {
      svc.set_should_be_scheduled(false);
      events_logger->debug("Service {} on host {} should not be scheduled.",
                           svc.description(), svc.get_hostname());
    }
    ++scheduling_info.total_services;
  }

  // default max service check spread (in minutes).
  scheduling_info.max_service_check_spread =
      _pb_config->max_service_check_spread();

  // used later in inter-check delay calculations.
  scheduling_info.service_check_interval_total =
      scheduling_info.service_check_interval_total *
      _pb_config->interval_length();

  if (scheduling_info.total_hosts) {
    scheduling_info.average_services_per_host =
        scheduling_info.total_services / (double)scheduling_info.total_hosts;
    scheduling_info.average_scheduled_services_per_host =
        scheduling_info.total_scheduled_services /
        (double)scheduling_info.total_hosts;
  }

  // calculate rolling average execution time (available
  // from retained state information).
  if (scheduling_info.total_scheduled_services)
    scheduling_info.average_service_check_interval =
        scheduling_info.service_check_interval_total /
        (double)scheduling_info.total_scheduled_services;

  _calculate_service_inter_check_delay(
      _pb_config->service_inter_check_delay_method());
  _calculate_service_interleave_factor(
      _pb_config->service_interleave_factor_method());
}

/**
 *  Create and register new misc event.
 *
 *  @param[in] type     The event type.
 *  @param[in] start    The date time to start event.
 *  @param[in] interval The rescheduling interval.
 *  @param[in] data     The timed event data.
 *
 *  @return The new event.
 */
timed_event* applier::scheduler::_create_misc_event(int type,
                                                    time_t start,
                                                    unsigned long interval,
                                                    void* data) {
  auto evt{std::make_unique<timed_event>(type, start, true, interval, nullptr,
                                         true, data, nullptr, 0)};
  timed_event* retval = evt.get();
  events::loop::instance().schedule(std::move(evt), true);
  return retval;
}

/**
 *  Get engine hosts struct with configuration hosts objects.
 *
 *  @param[in]  hst_ids             The list of host IDs to get.
 *  @param[in]  throw_if_not_found  Flag to throw if an host is not
 *                                  found.
 */
std::vector<com::centreon::engine::host*> applier::scheduler::_get_hosts(
    const std::vector<uint64_t>& hst_ids,
    bool throw_if_not_found) {
  std::vector<engine::host*> retval;
  for (auto host_id : hst_ids) {
    auto it_hst = engine::host::hosts_by_id.find(host_id);
    if (it_hst == engine::host::hosts_by_id.end()) {
      if (throw_if_not_found)
        throw engine_error()
            << "Could not schedule non-existing host with ID " << host_id;
    } else
      retval.push_back(it_hst->second.get());
  }
  return retval;
}

/**
 *  Get engine services struct with configuration services objects.
 *
 *  @param[in]  svc_ids             The list of configuration service IDs
 * objects.
 *  @param[in]  throw_if_not_found  Flag to throw if an host is not
 *                                  found.
 *  @return a vector of services.
 */
std::vector<com::centreon::engine::service*> applier::scheduler::_get_services(
    const std::vector<std::pair<uint64_t, uint64_t> >& svc_ids,
    bool throw_if_not_found) {
  std::vector<com::centreon::engine::service*> retval;
  for (auto& p : svc_ids) {
    service_id_map::const_iterator it_svc =
        engine::service::services_by_id.find({p.first, p.second});
    if (it_svc == engine::service::services_by_id.end()) {
      if (throw_if_not_found)
        throw engine_error() << fmt::format(
            "Cannot schedule non-existing service ({},{})", p.first, p.second);
    } else
      retval.push_back(it_svc->second.get());
  }
  return retval;
}

/**
 *  Get engine services struct with configuration services objects.
 *
 *  @param[in]  svc_cfg             The list of configuration services objects.
 *  @param[in]  throw_if_not_found  Flag to throw if an host is not
 *                                  found.
 *  @return a vector of services.
 */
std::vector<com::centreon::engine::service*>
applier::scheduler::_get_anomalydetections(
    const std::vector<std::pair<uint64_t, uint64_t> >& ad_ids,
    bool throw_if_not_found) {
  std::vector<engine::service*> retval;
  for (auto& p : ad_ids) {
    service_id_map::const_iterator it_svc =
        engine::service::services_by_id.find({p.first, p.second});
    if (it_svc == engine::service::services_by_id.end()) {
      if (throw_if_not_found)
        throw engine_error() << fmt::format(
            "Cannot schedule non-existing anomalydetection ({},{})", p.first,
            p.second);
    } else
      retval.push_back(it_svc->second.get());
  }
  return retval;
}

/**
 *  Remove misc event.
 *
 *  @param[int,out] evt The event to remove.
 */
void applier::scheduler::_remove_misc_event(timed_event*& evt) {
  if (evt) {
    events::loop::instance().remove_event(evt, events::loop::high);
    evt = nullptr;
  }
}

/**
 *  Schedule host events (checks notably).
 *
 *  @param[in] hosts  The list of hosts to schedule.
 */
void applier::scheduler::_schedule_host_events(
    std::vector<com::centreon::engine::host*> const& hosts) {
  engine_logger(dbg_events, most) << "Scheduling host checks...";
  events_logger->debug("Scheduling host checks...");

  // get current time.
  time_t const now(time(nullptr));

  unsigned int const end(hosts.size());

  // determine check times for host checks.
  int mult_factor(0);
  for (unsigned int i(0); i < end; ++i) {
    com::centreon::engine::host& hst(*hosts[i]);

    engine_logger(dbg_events, most) << "Host '" << hst.name() << "'";
    events_logger->debug("Host '{}'", hst.name());

    // skip hosts that shouldn't be scheduled.
    if (!hst.get_should_be_scheduled()) {
      engine_logger(dbg_events, most) << "Host check should not be scheduled.";
      events_logger->debug("Host check should not be scheduled.");
      continue;
    }

    // calculate preferred host check time.
    hst.set_next_check(
        (time_t)(now + (mult_factor * scheduling_info.host_inter_check_delay)));

    time_t time = hst.get_next_check();
    engine_logger(dbg_events, most)
        << "Preferred Check Time: " << hst.get_next_check() << " --> "
        << my_ctime(&time);
    events_logger->debug("Preferred Check Time: {} --> {}",
                         hst.get_next_check(), my_ctime(&time));

    // Make sure the host can actually be scheduled at this time.
    {
      timezone_locker lock(hst.get_timezone());
      if (!check_time_against_period(hst.get_next_check(),
                                     hst.check_period_ptr)) {
        time_t next_valid_time(0);
        get_next_valid_time(hst.get_next_check(), &next_valid_time,
                            hst.check_period_ptr);
        hst.set_next_check(next_valid_time);
      }
    }

    time = hst.get_next_check();
    engine_logger(dbg_events, most)
        << "Actual Check Time: " << hst.get_next_check() << " --> "
        << my_ctime(&time);
    events_logger->debug("Actual Check Time: {} --> {}", hst.get_next_check(),
                         my_ctime(&time));

    if (!scheduling_info.first_host_check ||
        (hst.get_next_check() < scheduling_info.first_host_check))
      scheduling_info.first_host_check = hst.get_next_check();
    if (hst.get_next_check() > scheduling_info.last_host_check)
      scheduling_info.last_host_check = hst.get_next_check();

    ++mult_factor;
  }

  // Need to optimize add_event insert.
  std::multimap<time_t, com::centreon::engine::host*> hosts_to_schedule;

  // add scheduled host checks to event queue.
  for (engine::host* h : hosts) {
    // update status of all hosts (scheduled or not).
    // FIXME DBO: Is this really needed?
    // h->update_status();

    // skip most hosts that shouldn't be scheduled.
    if (!h->get_should_be_scheduled()) {
      // passive checks are an exception if a forced check was
      // scheduled before Centreon Engine was restarted.
      if (!(!h->active_checks_enabled() && h->get_next_check() &&
            (h->get_check_options() & CHECK_OPTION_FORCE_EXECUTION)))
        continue;
    }
    hosts_to_schedule.insert(std::make_pair(h->get_next_check(), h));
  }

  // Schedule events list.
  for (std::multimap<time_t, com::centreon::engine::host*>::const_iterator
           it(hosts_to_schedule.begin()),
       end(hosts_to_schedule.end());
       it != end; ++it) {
    com::centreon::engine::host& hst(*it->second);

    // Schedule a new host check event.
    events::loop::instance().schedule(
        std::make_unique<timed_event>(
            timed_event::EVENT_HOST_CHECK, hst.get_next_check(), false, 0,
            nullptr, true, (void*)&hst, nullptr, hst.get_check_options()),
        false);
  }

  // Schedule acknowledgement expirations.
  engine_logger(dbg_events, most)
      << "Scheduling host acknowledgement expirations...";
  events_logger->debug("Scheduling host acknowledgement expirations...");
  for (int i(0), end(hosts.size()); i < end; ++i)
    if (hosts[i]->problem_has_been_acknowledged())
      hosts[i]->schedule_acknowledgement_expiration();
}

/**
 *  Schedule service events (checks notably).
 *
 *  @param[in] services  The list of services to schedule.
 */
void applier::scheduler::_schedule_service_events(
    std::vector<engine::service*> const& services) {
  engine_logger(dbg_events, most) << "Scheduling service checks...";
  events_logger->debug("Scheduling service checks...");

  // get current time.
  time_t const now(time(nullptr));

  int total_interleave_blocks(scheduling_info.total_scheduled_services);
  // calculate number of service interleave blocks.
  if (scheduling_info.service_interleave_factor)
    total_interleave_blocks =
        (int)ceil(scheduling_info.total_scheduled_services /
                  (double)scheduling_info.service_interleave_factor);

  // determine check times for service checks (with
  // interleaving to minimize remote load).

  int current_interleave_block(0);

  if (scheduling_info.service_interleave_factor > 0) {
    int interleave_block_index(0);
    for (engine::service* s : services) {
      if (interleave_block_index >= scheduling_info.service_interleave_factor) {
        ++current_interleave_block;
        interleave_block_index = 0;
      }

      // skip this service if it shouldn't be scheduled.
      if (!s->get_should_be_scheduled())
        continue;

      int const mult_factor(current_interleave_block +
                            ++interleave_block_index * total_interleave_blocks);

      // set the preferred next check time for the service.
      s->set_next_check(
          (time_t)(now +
                   mult_factor * scheduling_info.service_inter_check_delay));

      // Make sure the service can actually be scheduled when we want.
      {
        timezone_locker lock(s->get_timezone());
        if (!check_time_against_period(s->get_next_check(),
                                       s->check_period_ptr)) {
          time_t next_valid_time(0);
          get_next_valid_time(s->get_next_check(), &next_valid_time,
                              s->check_period_ptr);
          s->set_next_check(next_valid_time);
        }
      }

      if (!scheduling_info.first_service_check ||
          s->get_next_check() < scheduling_info.first_service_check)
        scheduling_info.first_service_check = s->get_next_check();
      if (s->get_next_check() > scheduling_info.last_service_check)
        scheduling_info.last_service_check = s->get_next_check();
    }
  }

  // Need to optimize add_event insert.
  std::multimap<time_t, engine::service*> services_to_schedule;

  // add scheduled service checks to event queue.
  for (engine::service* s : services) {
    // update status of all services (scheduled or not).
    // FIXME DBO: Is this really needed?
    // s->update_status();

    // skip most services that shouldn't be scheduled.
    if (!s->get_should_be_scheduled()) {
      // passive checks are an exception if a forced check was
      // scheduled before Centreon Engine was restarted.
      if (!(!s->active_checks_enabled() && s->get_next_check() &&
            (s->get_check_options() & CHECK_OPTION_FORCE_EXECUTION)))
        continue;
    }
    services_to_schedule.insert(std::make_pair(s->get_next_check(), s));
  }

  // Schedule events list.
  for (std::multimap<time_t, engine::service*>::const_iterator
           it(services_to_schedule.begin()),
       end(services_to_schedule.end());
       it != end; ++it) {
    engine::service* s(it->second);
    // Create a new service check event.
    events::loop::instance().schedule(
        std::make_unique<timed_event>(
            timed_event::EVENT_SERVICE_CHECK, s->get_next_check(), false, 0,
            nullptr, true, (void*)s, nullptr, s->get_check_options()),
        false);
  }

  // Schedule acknowledgement expirations.
  engine_logger(dbg_events, most)
      << "Scheduling service acknowledgement expirations...";
  events_logger->debug("Scheduling service acknowledgement expirations...");
  for (engine::service* s : services)
    if (s->problem_has_been_acknowledged())
      s->schedule_acknowledgement_expiration();
}

/**
 *  Unschedule host events.
 *
 *  @param[in] hosts  The list of hosts to unschedule.
 */
void applier::scheduler::_unschedule_host_events(
    std::vector<com::centreon::engine::host*> const& hosts) {
  for (auto& h : hosts) {
    events::loop::instance().remove_events(events::loop::low,
                                           timed_event::EVENT_HOST_CHECK, h);
    events::loop::instance().remove_events(
        events::loop::low, timed_event::EVENT_EXPIRE_HOST_ACK, h);
  }
}

/**
 *  Unschedule service events.
 *
 *  @param[in] services  The list of services to unschedule.
 */
void applier::scheduler::_unschedule_service_events(
    std::vector<engine::service*> const& services) {
  for (auto& s : services) {
    events::loop::instance().remove_events(events::loop::low,
                                           timed_event::EVENT_SERVICE_CHECK, s);
    events::loop::instance().remove_events(
        events::loop::low, timed_event::EVENT_EXPIRE_SERVICE_ACK, s);
  }
}
