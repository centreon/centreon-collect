/**
 * Copyright 1999-2009 Ethan Galstad
 * Copyright 2009-2010 Nagios Core Development Team and Community Contributors
 * Copyright 2011-2013 Merethis
 * Copyright 2013-2025 Centreon
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

#include "com/centreon/engine/events/loop.hh"
#include <future>
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/extended_conf.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/statusdata.hh"
#include "common/engine_conf/parser.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::events;
using namespace com::centreon::engine::logging;

/**
 *  Get instance of the events loop singleton.
 *
 *  @return The singleton.
 */
loop& loop::instance() {
  static loop instance;
  return instance;
}

void loop::clear() {
  _event_list_low.clear();
  _event_list_high.clear();

  _need_reload = false;
  _reload_running = false;
}

/**
 *  Start the events loop thread.
 */
void loop::run() {
  // Debug message.
  engine_logger(dbg_functions, basic) << "events::loop::run()";
  functions_logger->trace("events::loop::run()");

  engine_logger(log_info_message, basic)
      << "Configuration loaded, main loop starting.";
  process_logger->info("Configuration loaded, main loop starting.");
  // Initialize some time members.
  time(&_last_time);
  _last_status_update = 0L;

  // Initialize fake "sleep" event.
  _sleep_event.event_type = timed_event::EVENT_SLEEP;
  _sleep_event.run_time = _last_time;
  _sleep_event.recurring = false;
  _sleep_event.event_interval = 0L;
  _sleep_event.compensate_for_time_change = false;
  _sleep_event.timing_func = nullptr;
  _sleep_event.event_data = nullptr;
  _sleep_event.event_args = nullptr;
  _sleep_event.event_options = 0;

  _dispatching();
  clear();
}

/**
 *  Default constructor.
 */
loop::loop() : _need_reload(false), _reload_running(false) {}

/**
 * @brief Reload the configuration and apply its difference with the current
 * one.
 *
 * @param reloading A boolean to know if the configuration is currently
 * reloading.
 */
static void apply_conf(std::atomic<bool>* reloading) {
  configuration::error_cnt err;
  process_logger->info("Starting to reload configuration.");
  try {
    configuration::State config;
    configuration::state_helper config_hlp(&config);
    {
      configuration::parser p;
      std::string path(::pb_indexed_config.state().cfg_main());
      p.parse(path, &config, err);
    }
    configuration::extended_conf::update_state(&config);
    configuration::applier::state::instance().apply(config, err);
    process_logger->info("Configuration reloaded, main loop continuing.");
  } catch (std::exception const& e) {
    config_logger->error("Error: {}", e.what());
  }
  *reloading = false;
  process_logger->info("Reload configuration finished.");
}

static void apply_diff(std::unique_ptr<configuration::DiffState> diff_conf,
                       std::atomic<bool>* reloading) {
  configuration::error_cnt err;
  process_logger->info("Starting to reload configuration.");
  try {
    process_logger->info("Configuration reloaded, main loop continuing.");
    configuration::applier::state::instance().apply_diff(*diff_conf, err);
  } catch (const std::exception& e) {
    config_logger->error("Error: {}", e.what());
  }
  *reloading = false;
  process_logger->info("Reload configuration finished.");
}

/**
 *  Slot to dispatch Centreon Engine events.
 */
void loop::_dispatching() {
  std::atomic<bool> reloading{false};
  for (;;) {
    // See if we should exit or restart (a signal was encountered).
    if (sigshutdown)
      break;

    // If we don't have any events to handle, exit.
    if (_event_list_high.empty() && _event_list_low.empty()) {
      engine_logger(log_runtime_error, basic)
          << "There aren't any events that need to be handled! "
          << "Exiting...";
      runtime_logger->error(
          "There aren't any events that need to be handled! Exiting...");
      break;
    }

    if (sighup) {
      com::centreon::logging::engine::instance().reopen();
      _need_reload = true;
      sighup = false;
    }

    std::unique_ptr<configuration::DiffState> diff_conf = cbm->diff_state();
    // Start reload configuration.
    if (_need_reload || diff_conf) {
      if (!reloading) {
        reloading = true;
        if (_need_reload) {
          process_logger->info("Reloading...");
          auto future [[maybe_unused]] =
              std::async(std::launch::async, apply_conf, &reloading);
        } else {
          process_logger->info("Reloading from Broker...");
          auto future [[maybe_unused]] = std::async(
              std::launch::async, apply_diff, std::move(diff_conf), &reloading);
        }
      } else {
        engine_logger(log_info_message, most) << "Already reloading...";
        process_logger->info("Already reloading...");
      }
      _need_reload = false;
    }

    // Get the current time.
    time_t current_time;
    time(&current_time);

    configuration::applier::state::instance().lock();

    time_t time_change_threshold =
        pb_indexed_config.state().time_change_threshold();
    uint32_t max_parallel_service_checks =
        pb_indexed_config.state().max_parallel_service_checks();
    bool execute_service_checks =
        pb_indexed_config.state().execute_service_checks();
    bool execute_host_checks = pb_indexed_config.state().execute_host_checks();
    uint32_t interval_length = pb_indexed_config.state().interval_length();
    double sleep_time = pb_indexed_config.state().sleep_time();
    int32_t command_check_interval =
        pb_indexed_config.state().command_check_interval();

    // Hey, wait a second...  we traveled back in time!
    if (current_time < _last_time)
      compensate_for_system_time_change(
          static_cast<unsigned long>(_last_time),
          static_cast<unsigned long>(current_time));
    // Else if the time advanced over the specified threshold,
    // try and compensate...
    else if (current_time - _last_time >=
             static_cast<time_t>(time_change_threshold))
      compensate_for_system_time_change(
          static_cast<unsigned long>(_last_time),
          static_cast<unsigned long>(current_time));

    // Keep track of the last time.
    _last_time = current_time;

    // Log messages about event lists.
    engine_logger(dbg_events, more) << "** Event Check Loop";
    events_logger->debug("** Event Check Loop");
    if (!_event_list_high.empty()) {
      engine_logger(dbg_events, more)
          << "Next High Priority Event Time: "
          << my_ctime(&(*_event_list_high.begin())->run_time);
      events_logger->debug("Next High Priority Event Time: {}",
                           my_ctime(&(*_event_list_high.begin())->run_time));
    } else {
      engine_logger(dbg_events, more)
          << "No high priority events are scheduled...";
      events_logger->debug("No high priority events are scheduled...");
    }
    if (!_event_list_low.empty()) {
      engine_logger(dbg_events, more)
          << "Next Low Priority Event Time:  "
          << my_ctime(&(*_event_list_low.begin())->run_time);
      events_logger->debug("Next Low Priority Event Time:  {}",
                           my_ctime(&(*_event_list_low.begin())->run_time));
    } else {
      engine_logger(dbg_events, more)
          << "No low priority events are scheduled...";
      events_logger->debug("No low priority events are scheduled...");
    }
    engine_logger(dbg_events, more)
        << "Current/Max Service Checks: " << currently_running_service_checks
        << '/' << max_parallel_service_checks;
    events_logger->debug("Current/Max Service Checks: {}/{}",
                         currently_running_service_checks,
                         max_parallel_service_checks);

    // Update status information occassionally - NagVis watches the
    // NDOUtils DB to see if Engine is alive.
    if ((unsigned long)(current_time - _last_status_update) > 5) {
      _last_status_update = current_time;
      update_program_status(false);
    }

    // Handle high priority events.
    bool run_event(true);
    if (!_event_list_high.empty() &&
        current_time >= _event_list_high.front()->run_time) {
      // Remove the first event from the timing loop.
      auto temp_event = std::move(_event_list_high.front());

      _event_list_high.pop_front();
      // We may have just removed the only item from the list.

      // Handle the event.
      temp_event->handle_timed_event();

      // Reschedule the event if necessary.
      if (temp_event->recurring)
        reschedule_event(std::move(temp_event), events::loop::high);
    }
    // Handle low priority events.
    else if (!_event_list_low.empty() &&
             current_time >= _event_list_low.front()->run_time) {
      // Default action is to execute the event.
      run_event = true;

      // Run a few checks before executing a service check...
      if (_event_list_low.front()->event_type ==
          timed_event::EVENT_SERVICE_CHECK) {
        int nudge_seconds(0);
        service* temp_service(
            static_cast<service*>(_event_list_low.front()->event_data));

        // Don't run a service check if we're already maxed out on the
        // number of parallel service checks...
        if (max_parallel_service_checks != 0 &&
            currently_running_service_checks >= max_parallel_service_checks) {
          // Move it at least 5 seconds (to overcome the current peak),
          // with a random 10 seconds (to spread the load).
          nudge_seconds = 5 + (rand() % 10);
          engine_logger(dbg_events | dbg_checks, basic)
              << "**WARNING** Max concurrent service checks ("
              << currently_running_service_checks << "/"
              << max_parallel_service_checks << ") has been reached!  Nudging "
              << temp_service->get_hostname() << ":"
              << temp_service->description() << " by " << nudge_seconds
              << " seconds...";
          events_logger->trace(
              "**WARNING** Max concurrent service checks ({}/{}) has been "
              "reached!  Nudging {}:{} by {} seconds...",
              currently_running_service_checks, max_parallel_service_checks,
              temp_service->get_hostname(), temp_service->description(),
              nudge_seconds);

          engine_logger(log_runtime_warning, basic)
              << "\tMax concurrent service checks ("
              << currently_running_service_checks << "/"
              << max_parallel_service_checks << ") has been reached.  Nudging "
              << temp_service->get_hostname() << ":"
              << temp_service->description() << " by " << nudge_seconds
              << " seconds...";
          runtime_logger->warn(
              "\tMax concurrent service checks ({}/{}) has been reached.  "
              "Nudging {}:{} by {} seconds...",
              currently_running_service_checks, max_parallel_service_checks,
              temp_service->get_hostname(), temp_service->description(),
              nudge_seconds);
          run_event = false;
        }

        // Don't run a service check if active checks are disabled.
        if (!execute_service_checks) {
          engine_logger(dbg_events | dbg_checks, more)
              << "We're not executing service checks right now, "
              << "so we'll skip this event.";
          events_logger->debug(
              "We're not executing service checks right now, so we'll skip "
              "this event.");
          run_event = false;
        }

        // Forced checks override normal check logic.
        if (temp_service->get_check_options() & CHECK_OPTION_FORCE_EXECUTION)
          run_event = true;

        // Reschedule the check if we can't run it now.
        if (!run_event) {
          // Remove the service check from the event queue and
          // reschedule it for a later time. Since event was not
          // executed, it needs to be remove()'ed to maintain sync with
          // event broker modules.
          auto temp_event = std::move(_event_list_low.front());
          _event_list_low.pop_front();

          // We nudge the next check time when it is
          // due to too many concurrent service checks.
          if (nudge_seconds)
            temp_service->set_next_check(
                (time_t)(temp_service->get_next_check() + nudge_seconds));
          // Otherwise reschedule (TODO: This should be smarter as it
          // doesn't consider its timeperiod).
          else {
            if (notifier::soft == temp_service->get_state_type() &&
                temp_service->get_current_state() != service::state_ok)
              temp_service->set_next_check(
                  (time_t)(temp_service->get_next_check() +
                           temp_service->retry_interval() * interval_length));
            else
              temp_service->set_next_check(
                  (time_t)(temp_service->get_next_check() +
                           temp_service->check_interval() * interval_length));
          }
          temp_event->run_time = temp_service->get_next_check();
          reschedule_event(std::move(temp_event), events::loop::low);
          temp_service->update_status();
          run_event = false;
        }
      }
      // Run a few checks before executing a host check...
      else if (timed_event::EVENT_HOST_CHECK ==
               _event_list_low.front()->event_type) {
        // Default action is to execute the event.
        run_event = true;
        host* temp_host(
            static_cast<host*>(_event_list_low.front()->event_data));

        // Don't run a host check if active checks are disabled.
        if (!execute_host_checks) {
          engine_logger(dbg_events | dbg_checks, more)
              << "We're not executing host checks right now, "
              << "so we'll skip this event.";
          events_logger->debug(
              "We're not executing host checks right now, so we'll skip this "
              "event.");
          run_event = false;
        }

        // Forced checks override normal check logic.
        if (temp_host->get_check_options() & CHECK_OPTION_FORCE_EXECUTION)
          run_event = true;

        // Reschedule the host check if we can't run it right now.
        if (!run_event) {
          // Remove the host check from the event queue and reschedule
          // it for a later time. Since event was not executed, it needs
          // to be remove()'ed to maintain sync with event broker
          // modules.
          auto temp_event = std::move(_event_list_low.front());
          _event_list_low.pop_front();

          // Reschedule.
          if ((notifier::soft == temp_host->get_state_type()) &&
              (temp_host->get_current_state() != host::state_up))
            temp_host->set_next_check(
                (time_t)(temp_host->get_next_check() +
                         temp_host->retry_interval() * interval_length));
          else
            temp_host->set_next_check(
                (time_t)(temp_host->get_next_check() +
                         temp_host->check_interval() * interval_length));
          temp_event->run_time = temp_host->get_next_check();
          reschedule_event(std::move(temp_event), events::loop::low);
          temp_host->update_status();
          run_event = false;
        }
      }

      // Run the event.
      if (run_event) {
        // Remove the first event from the timing loop.
        auto temp_event = std::move(_event_list_low.front());
        _event_list_low.pop_front();
        // We may have just removed the only item from the list.

        // Handle the event.
        engine_logger(dbg_events, more) << "Running event...";
        events_logger->debug("Running event...");
        temp_event->handle_timed_event();

        // Reschedule the event if necessary.
        if (temp_event->recurring)
          reschedule_event(std::move(temp_event), events::loop::low);
      }
      // Wait a while so we don't hog the CPU...
      else {
        engine_logger(dbg_events, most)
            << "Did not execute scheduled event. Idling for a bit...";
        events_logger->debug(
            "Did not execute scheduled event. Idling for a bit...");
        uint64_t d = static_cast<uint64_t>(sleep_time * 1000000000);
        std::this_thread::sleep_for(std::chrono::nanoseconds(d));
      }
    }
    // We don't have anything to do at this moment in time...
    else if ((_event_list_high.empty() ||
              current_time < (*_event_list_high.begin())->run_time) &&
             (_event_list_low.empty() ||
              current_time < (*_event_list_low.begin())->run_time)) {
      engine_logger(dbg_events, most)
          << "No events to execute at the moment. Idling for a bit...";
      events_logger->debug(
          "No events to execute at the moment. Idling for a bit...");

      // Check for external commands if we're supposed to check as
      // often as possible.
      if (command_check_interval == -1) {
        // Send data to event broker.
        broker_external_command(NEBTYPE_EXTERNALCOMMAND_CHECK, CMD_NONE,
                                nullptr);
      }

      auto t1 = std::chrono::system_clock::now();
      auto delay = std::chrono::nanoseconds(
          static_cast<uint64_t>(1000000000 * sleep_time));
      command_manager::instance().execute();

      // Set time to sleep so we don't hog the CPU...
      timespec stime;
      stime.tv_sec = (time_t)sleep_time;
      stime.tv_nsec =
          (long)((sleep_time - (double)stime.tv_sec) * 1000000000ull);

      // Populate fake "sleep" event.
      _sleep_event.run_time = current_time;
      _sleep_event.event_data = (void*)&stime;

      auto t2 = std::chrono::system_clock::now();
      auto laps = t2 - t1;
      if (laps < delay) {
        delay -= laps;
        std::this_thread::sleep_for(delay);
      }
    }
    configuration::applier::state::instance().unlock();
  }
}

/**
 *  Attempts to compensate for a change in the system time.
 *
 *  @param[in] last_time    The last time.
 *  @param[in] current_time The current time.
 */
void loop::compensate_for_system_time_change(unsigned long last_time,
                                             unsigned long current_time) {
  int days{0};
  int hours{0};
  int minutes{0};
  int seconds{0};
  long time_difference = current_time - last_time;

  engine_logger(dbg_functions, basic) << "compensate_for_system_time_change()";
  functions_logger->trace("compensate_for_system_time_change()");

  // we moved back in time...
  if (time_difference < 0) {
    get_time_breakdown(-time_difference, &days, &hours, &minutes, &seconds);
    engine_logger(dbg_events, basic)
        << "Detected a backwards time change of " << days << "d " << hours
        << "h " << minutes << "m " << seconds << "s.";
    events_logger->trace("Detected a backwards time change of {}d {}h {}m {}s.",
                         days, hours, minutes, seconds);
  }
  // we moved into the future...
  else {
    get_time_breakdown(time_difference, &days, &hours, &minutes, &seconds);
    engine_logger(dbg_events, basic)
        << "Detected a forwards time change of " << days << "d " << hours
        << "h " << minutes << "m " << seconds << "s.";
    events_logger->trace("Detected a forwards time change of {}d {}h {}m {}s.",
                         days, hours, minutes, seconds);
  }

  // log the time change.
  engine_logger(log_process_info | log_runtime_warning, basic)
      << "Warning: A system time change of " << days << "d " << hours << "h "
      << minutes << "m " << seconds << "s ("
      << (time_difference < 0 ? "backwards" : "forwards")
      << " in time) has been detected.  Compensating...";
  runtime_logger->warn(
      "Warning: A system time change of {}d {}h {}m {}s ({} in time) has been "
      "detected.  Compensating...",
      days, hours, minutes, seconds,
      time_difference < 0 ? "backwards" : "forwards");

  // adjust the next run time for all high priority timed events.
  for (auto it = _event_list_high.begin(), end = _event_list_high.end();
       it != end; ++it) {
    // skip special events that occur at specific times...
    if (!(*it)->compensate_for_time_change)
      continue;

    // use custom timing function.
    if ((*it)->timing_func) {
      union {
        time_t (*func)(void);
        void* data;
      } timing;
      timing.data = (*it)->timing_func;
      (*it)->run_time = (*timing.func)();
    }

    // else use standard adjustment.
    else
      (*it)->run_time =
          adjust_timestamp_for_time_change(time_difference, (*it)->run_time);
  }

  // resort event list (some events may be out of order at this point).
  resort_event_list(events::loop::high);

  // adjust the next run time for all low priority timed events.
  for (auto it = _event_list_low.begin(), end = _event_list_low.end();
       it != end; ++it) {
    // skip special events that occur at specific times...
    if (!(*it)->compensate_for_time_change)
      continue;

    // use custom timing function.
    if ((*it)->timing_func) {
      union {
        time_t (*func)(void);
        void* data;
      } timing;
      timing.data = (*it)->timing_func;
      (*it)->run_time = (*timing.func)();
    }

    // else use standard adjustment.
    else
      (*it)->run_time =
          adjust_timestamp_for_time_change(time_difference, (*it)->run_time);
  }

  // resort event list (some events may be out of order at this point).
  resort_event_list(events::loop::low);

  // adjust service timestamps.
  for (service_map::iterator it(service::services.begin()),
       end(service::services.end());
       it != end; ++it) {
    it->second->set_last_notification(adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_notification()));
    it->second->set_last_check(adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_check()));
    it->second->set_next_check(adjust_timestamp_for_time_change(
        time_difference, it->second->get_next_check()));
    it->second->set_last_state_change(adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_state_change()));
    it->second->set_last_hard_state_change(adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_hard_state_change()));

    it->second->set_initial_notif_time(adjust_timestamp_for_time_change(
        time_difference, it->second->get_initial_notif_time()));
    it->second->set_last_acknowledgement(adjust_timestamp_for_time_change(
        time_difference, it->second->last_acknowledgement()));

    // recalculate next re-notification time.
    it->second->set_next_notification(it->second->get_next_notification_time(
        it->second->get_last_notification()));

    // update the status data.
    it->second->update_status();
  }

  // adjust host timestamps.
  for (host_map::iterator it(com::centreon::engine::host::hosts.begin()),
       end(com::centreon::engine::host::hosts.end());
       it != end; ++it) {
    time_t last_host_notif{adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_notification())};
    time_t last_check{adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_check())};
    time_t next_check{adjust_timestamp_for_time_change(
        time_difference, it->second->get_next_check())};
    time_t last_state_change{adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_state_change())};
    time_t last_hard_state_change{adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_hard_state_change())};
    time_t last_state_history_update{adjust_timestamp_for_time_change(
        time_difference, it->second->get_last_state_history_update())};

    it->second->set_last_notification(last_host_notif);
    it->second->set_last_check(last_check);
    it->second->set_next_check(next_check);
    it->second->set_last_state_change(last_state_change);
    it->second->set_last_hard_state_change(last_hard_state_change);
    it->second->set_last_state_history_update(last_state_history_update);
    // recalculate next re-notification time.
    it->second->set_next_notification(it->second->get_next_notification_time(
        it->second->get_last_notification()));

    // update the status data.
    it->second->update_status();
  }

  // adjust program timestamps.
  program_start =
      adjust_timestamp_for_time_change(time_difference, program_start);
  event_start = adjust_timestamp_for_time_change(time_difference, event_start);
  last_command_check =
      adjust_timestamp_for_time_change(time_difference, last_command_check);

  // update the status data.
  update_program_status(false);
}

/**
 *  Add an event to list ordered by execution time.
 *
 *  @param[in] event           The new event to add.
 *  @param[in] event_list      The head of the event list.
 *  @param[in] event_list_tail The tail of the event list.
 */
void loop::add_event(std::unique_ptr<timed_event>&& event,
                     loop::priority priority) {
  engine_logger(dbg_functions, basic) << "add_event()";
  functions_logger->trace("add_event()");

  timed_event_list* list;

  if (priority == loop::low) {
    list = &_event_list_low;
  } else {
    list = &_event_list_high;
  }

  // add the event to the head of the list if there are
  // no other events.
  if (list->empty())
    list->push_front(std::move(event));

  // add event to head of the list if it should be executed first.
  else if (event->run_time < (*list->begin())->run_time)
    list->push_front(std::move(event));

  // else place the event according to next execution time.
  else {
    // start from the end of the list, as new events are likely to
    // be executed in the future, rather than now...
    for (timed_event_list::reverse_iterator it = list->rbegin(),
                                            end = list->rend();
         it != end; ++it) {
      if (event->run_time >= (*it)->run_time) {
        list->insert(it.base(), std::move(event));
        break;
      }
    }
  }
}

void loop::remove_downtime(uint64_t downtime_id) {
  engine_logger(dbg_functions, basic) << "loop::remove_downtime()";
  functions_logger->trace("loop::remove_downtime()");

  for (auto it = _event_list_high.begin(), end = _event_list_high.end();
       it != end; ++it) {
    if ((*it)->event_type != timed_event::EVENT_SCHEDULED_DOWNTIME)
      continue;
    if (((uint64_t)(*it)->event_data) == downtime_id) {
      _event_list_high.erase(it);
      break;
    }
  }
}

/**
 *  Remove an event given by its iterator from the queue.
 *
 *  @param[in]     it              The iterator to the event to remove.
 *  @param[in]     priority        This to now which list to work with.
 */
void loop::remove_event(timed_event_list::iterator& it,
                        loop::priority priority) {
  engine_logger(dbg_functions, basic) << "loop::remove_event()";
  functions_logger->trace("loop::remove_event()");

  if (priority == loop::low)
    _event_list_low.erase(it);
  else
    _event_list_high.erase(it);
}

/**
 *  Remove an event given from the queue.
 *
 *  @param[in]     evt             Pointer to timed_event.
 *  @param[in]     priority        This is to know which list to work with.
 */
void loop::remove_event(timed_event* evt, loop::priority priority) {
  engine_logger(dbg_functions, basic) << "loop::remove_event()";
  functions_logger->trace("loop::remove_event()");
  timed_event_list* list;
  if (priority == loop::low)
    list = &_event_list_low;
  else
    list = &_event_list_high;

  list->erase(std::remove_if(list->begin(), list->end(),
                             [evt](const std::unique_ptr<timed_event>& e) {
                               return evt == e.get();
                             }),
              list->end());
}

void loop::remove_events(loop::priority priority,
                         uint32_t event_type,
                         void* data) noexcept {
  timed_event_list* list;
  if (priority == loop::low)
    list = &_event_list_low;
  else
    list = &_event_list_high;

  list->erase(std::remove_if(
                  list->begin(), list->end(),
                  [event_type, data](const std::unique_ptr<timed_event>& e) {
                    return e->event_type == event_type && e->event_data == data;
                  }),
              list->end());
}

timed_event_list::iterator loop::find_event(loop::priority priority,
                                            uint32_t event_type,
                                            void* data) {
  timed_event_list* list;

  engine_logger(dbg_functions, basic) << "resort_event_list()";
  functions_logger->trace("resort_event_list()");

  // move current event list to temp list.
  if (priority == loop::low)
    list = &_event_list_low;
  else
    list = &_event_list_high;

  for (auto it = list->begin(), end = list->end(); it != end; ++it)
    if ((*it)->event_type == event_type && (*it)->event_data == data)
      return it;

  return list->end();
}

/**
 *  Reschedule an event in order of execution time.
 *
 *  @param[in]     event           The event to reschedule.
 *  @param[in,out] event_list      The head of the event list.
 *  @param[in,out] event_list_tail The tail of the event list.
 */
void loop::reschedule_event(std::unique_ptr<timed_event>&& event,
                            loop::priority priority) {
  functions_logger->trace("reschedule_event()");

  // reschedule recurring events...
  if (event->recurring) {
    // use custom timing function.
    if (event->timing_func) {
      union {
        time_t (*func)(void);
        void* data;
      } timing;
      timing.data = event->timing_func;
      event->run_time = (*timing.func)();
    }

    // normal recurring events.
    else {
      event->run_time = event->run_time + event->event_interval;
      time_t current_time = time(nullptr);
      if (event->run_time < current_time)
        event->run_time = current_time;
    }
  }

  // add the event to the event list.
  add_event(std::move(event), priority);
}

/**
 *  Resorts an event list by event execution time - needed when
 *  compensating for system time changes.
 *
 *  @param[in,out] event_list      The head of the event list.
 *  @param[in,out] event_list_tail The tail of the event list.
 */
void loop::resort_event_list(loop::priority priority) {
  timed_event_list* list;

  engine_logger(dbg_functions, basic) << "resort_event_list()";
  functions_logger->trace("resort_event_list()");

  // move current event list to temp list.
  if (priority == loop::low)
    list = &_event_list_low;
  else
    list = &_event_list_high;

  std::sort(list->begin(), list->end(),
            [](const std::unique_ptr<timed_event>& first,
               const std::unique_ptr<timed_event>& second) {
              return first->run_time < second->run_time;
            });
}

/**
 *  Schedule a timed event.
 *
 * @param high_priority Priority list.
 */
void loop::schedule(std::unique_ptr<timed_event>&& evt, bool high_priority) {
  // add the event to the event list.
  if (high_priority)
    add_event(std::move(evt), loop::high);
  else
    add_event(std::move(evt), loop::low);
}
