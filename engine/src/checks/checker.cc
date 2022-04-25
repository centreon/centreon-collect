/*
** Copyright 1999-2010 Ethan Galstad
** Copyright 2011-2019 Centreon
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

#include "com/centreon/engine/checks/checker.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/neberrors.hh"
#include "com/centreon/engine/objects.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/exceptions/interruption.hh"

using namespace com::centreon;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::checks;

checker* checker::_instance = nullptr;
static constexpr time_t max_check_reaper_time = 30;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Get instance of the checker singleton.
 *
 *  @return This singleton.
 */
checker& checker::instance() {
  /* This singleton does not follow the C++ good practices, because we
   * need to control when to destroy it. */
  assert(_instance);
  return *_instance;
}

void checker::init() {
  if (!_instance)
    _instance = new checker();
}

void checker::deinit() {
  if (_instance) {
    delete _instance;
    _instance = nullptr;
  }
}

void checker::clear() noexcept {
  try {
    std::lock_guard<std::mutex> lock(_mut_reap);
    while (!_to_reap_partial.empty()) {
      check_result* result = _to_reap_partial.front();
      _to_reap_partial.pop_front();
      delete result;
    }
    while (!_to_reap.empty()) {
      check_result* result = _to_reap.front();
      _to_reap.pop_front();
      delete result;
    }
    auto it = _waiting_check_result.begin();
    while (it != _waiting_check_result.end()) {
      delete it->second;
      it = _waiting_check_result.erase(it);
    }
    _to_forget.clear();
  } catch (...) {
  }
}

/**
 *  Reap and process all results received by execution process.
 */
void checker::reap() {
  engine_logger(dbg_functions, basic) << "checker::reap";
  log_v2::functions()->trace("checker::reap()");

  engine_logger(dbg_checks, basic) << "Starting to reap check results.";
  log_v2::checks()->trace("Starting to reap check results.");

  // Time to start reaping.
  time_t reaper_start_time;
  time(&reaper_start_time);

  // Reap check results.
  unsigned int reaped_checks(0);
  {  // Scope to release mutex in all termination cases.
    {
      std::lock_guard<std::mutex> lock(_mut_reap);
      if (!_to_forget.empty()) {
        for (notifier* n : _to_forget) {
          for (auto it = _waiting_check_result.begin();
               it != _waiting_check_result.end();) {
            if (it->second->get_notifier() == n) {
              delete it->second;
              it = _waiting_check_result.erase(it);
            } else
              ++it;
          }
          for (auto it = _to_reap_partial.begin();
               it != _to_reap_partial.end();) {
            if ((*it)->get_notifier() == n) {
              delete *it;
              it = _to_reap_partial.erase(it);
            } else
              ++it;
          }
        }
        _to_forget.clear();
      }
      std::swap(_to_reap, _to_reap_partial);
    }

    // Process check results.
    while (!_to_reap.empty()) {
      // Get result host or service check.
      ++reaped_checks;
      engine_logger(dbg_checks, basic)
          << "Found a check result (#" << reaped_checks << ") to handle...";
      log_v2::checks()->trace("Found a check result (#{}) to handle...",
                              reaped_checks);
      check_result* result = _to_reap.front();
      _to_reap.pop_front();

      // Service check result->
      if (service_check == result->get_object_check_type()) {
        service* svc = static_cast<service*>(result->get_notifier());
        try {
          // Check if the service exists.
          engine_logger(dbg_checks, more)
              << "Handling check result for service " << svc->get_host_id()
              << "/" << svc->get_service_id() << "...";
          log_v2::checks()->debug("Handling check result for service {}/{}...",
                                  svc->get_host_id(), svc->get_service_id());
          svc->handle_async_check_result(result);
        } catch (std::exception const& e) {
          engine_logger(log_runtime_warning, basic)
              << "Check result queue errors for service " << svc->get_host_id()
              << "/" << svc->get_service_id() << " : " << e.what();
          log_v2::runtime()->warn(
              "Check result queue errors for service {}/{} : {}",
              svc->get_host_id(), svc->get_service_id(), e.what());
        }
      }
      // Host check result->
      else {
        host* hst = static_cast<host*>(result->get_notifier());
        try {
          // Process the check result->
          engine_logger(dbg_checks, more) << "Handling check result for host "
                                          << hst->get_host_id() << "...";
          log_v2::checks()->debug("Handling check result for host {}...",
                                  hst->get_host_id());
          hst->handle_async_check_result_3x(result);
        } catch (std::exception const& e) {
          engine_logger(log_runtime_error, basic)
              << "Check result queue errors for "
              << "host " << hst->get_host_id() << " : " << e.what();
          log_v2::runtime()->error("Check result queue errors for host {} : {}",
                                   hst->get_host_id(), e.what());
        }
      }

      delete result;

      // Check if reaping has timed out.
      time_t current_time;
      time(&current_time);
      // Maximum Check Result Reaper Time is set to 30
      if (current_time - reaper_start_time > max_check_reaper_time) {
        engine_logger(dbg_checks, basic)
            << "Breaking out of check result reaper: "
            << "max reaper time exceeded";
        log_v2::checks()->trace(
            "Breaking out of check result reaper: max reaper time exceeded");
        break;
      }

      // Caught signal, need to break.
      if (sigshutdown) {
        engine_logger(dbg_checks, basic)
            << "Breaking out of check result reaper: signal encountered";
        log_v2::checks()->trace(
            "Breaking out of check result reaper: signal encountered");
        break;
      }
    }
  }

  // Reaping finished.
  engine_logger(dbg_checks, basic)
      << "Finished reaping " << reaped_checks << " check results";
  log_v2::checks()->trace("Finished reaping {} check results", reaped_checks);
}

/**
 *  Run an host check and wait check result.
 *
 *  @param[in]  hst                     Host to check.
 *  @param[out] check_result_code       The return value of the execution.
 *  @param[in]  check_options           Event options.
 *  @param[in]  use_cached_result       Used the last result.
 *  @param[in]  check_timestamp_horizon XXX
 */
void checker::run_sync(host* hst,
                       host::host_state* check_result_code,
                       int check_options,
                       int use_cached_result,
                       unsigned long check_timestamp_horizon) {
  engine_logger(dbg_functions, basic)
      << "checker::run: hst=" << hst << ", check_options=" << check_options
      << ", use_cached_result=" << use_cached_result
      << ", check_timestamp_horizon=" << check_timestamp_horizon;
  log_v2::functions()->trace(
      "checker::run: hst={:p}, check_options={}"
      ", use_cached_result={}"
      ", check_timestamp_horizon={}",
      (void*)hst, check_options, use_cached_result, check_timestamp_horizon);

  // Preamble.
  if (!hst)
    throw engine_error() << "Attempt to run synchronous check on invalid host";
  if (!hst->get_check_command_ptr())
    throw engine_error() << "Attempt to run synchronous active check on host '"
                         << hst->get_name() << "' with no check command";

  engine_logger(dbg_checks, basic)
      << "** Run sync check of host '" << hst->get_name() << "'...";
  log_v2::checks()->trace("** Run sync check of host '{}'...", hst->get_name());

  // Check if the host is viable now.
  if (!hst->verify_check_viability(check_options, nullptr, nullptr)) {
    if (check_result_code)
      *check_result_code = hst->get_current_state();
    engine_logger(dbg_checks, basic) << "Host check is not viable at this time";
    log_v2::checks()->trace("Host check is not viable at this time");
    return;
  }

  // Time to start command.
  timeval start_time;
  gettimeofday(&start_time, nullptr);

  // Can we use the last cached host state?
  if (use_cached_result && !(check_options & CHECK_OPTION_FORCE_EXECUTION)) {
    // We can used the cached result, so return it and get out of here.
    if (hst->has_been_checked() &&
        (static_cast<unsigned long>(start_time.tv_sec -
                                    hst->get_last_check()) <=
         check_timestamp_horizon)) {
      if (check_result_code)
        *check_result_code = hst->get_current_state();
      engine_logger(dbg_checks, more)
          << "* Using cached host state: " << hst->get_current_state();
      log_v2::checks()->debug("* Using cached host state: {}",
                              hst->get_current_state());

      // Update statistics.
      update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, start_time.tv_sec);
      update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, start_time.tv_sec);
      return;
    }
  }

  // Checking starts.
  engine_logger(dbg_checks, more)
      << "* Running actual host check: old state=" << hst->get_current_state();
  log_v2::checks()->debug("* Running actual host check: old state={}",
                          hst->get_current_state());

  // Update statistics.
  update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, start_time.tv_sec);
  update_check_stats(SERIAL_HOST_CHECK_STATS, start_time.tv_sec);

  // Reset host check latency, since on-demand checks have none.
  hst->set_latency(0.0);

  // Adjust check attempts.
  hst->adjust_check_attempt(true);

  // Update host state.
  hst->set_last_state(hst->get_current_state());
  if (notifier::hard == hst->get_state_type())
    hst->set_last_hard_state(hst->get_current_state());

  // Save old plugin output for state stalking.
  std::string old_plugin_output{hst->get_plugin_output()};

  // Set the checked flag.
  hst->set_has_been_checked(true);

  // Clear the freshness flag.
  hst->set_is_being_freshened(false);

  // Clear check options - we don't want old check options retained.
  hst->set_check_options(CHECK_OPTION_NONE);

  // Set the check type.
  hst->set_check_type(checkable::check_active);

  // Send broker event.
  timeval end_time;
  memset(&end_time, 0, sizeof(end_time));
  broker_host_check(NEBTYPE_HOSTCHECK_INITIATE, NEBFLAG_NONE, NEBATTR_NONE, hst,
                    checkable::check_active, hst->get_current_state(),
                    hst->get_state_type(), start_time, end_time,
                    hst->check_command().c_str(), hst->get_latency(), 0.0,
                    config->host_check_timeout(), false, 0, nullptr, nullptr,
                    nullptr, nullptr, nullptr);

  // Execute command synchronously.
  host::host_state host_result(_execute_sync(hst));

  // Process result.
  hst->process_check_result_3x(host_result, old_plugin_output, check_options,
                               false, use_cached_result,
                               check_timestamp_horizon);
  if (check_result_code)
    *check_result_code = hst->get_current_state();

  // Synchronous check is done.
  engine_logger(dbg_checks, more)
      << "* Sync host check done: new state=" << hst->get_current_state();
  log_v2::checks()->debug("* Sync host check done: new state={}",
                          hst->get_current_state());

  // Get the end time of command.
  gettimeofday(&end_time, nullptr);

  // Send event broker.
  broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED, NEBFLAG_NONE, NEBATTR_NONE,
                    hst, checkable::check_active, hst->get_current_state(),
                    hst->get_state_type(), start_time, end_time,
                    hst->check_command().c_str(), hst->get_latency(),
                    hst->get_execution_time(), config->host_check_timeout(),
                    false, hst->get_current_state(), nullptr,
                    const_cast<char*>(hst->get_plugin_output().c_str()),
                    const_cast<char*>(hst->get_long_plugin_output().c_str()),
                    const_cast<char*>(hst->get_perf_data().c_str()), nullptr);
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
checker::checker() : commands::command_listener() {}

/**
 *  Default destructor.
 */
checker::~checker() noexcept {
  clear();
}

/**
 *  Slot to catch the result of the execution and add to the reap queue.
 *
 *  @param[in] res The result of the execution.
 */
void checker::finished(commands::result const& res) noexcept {
  // Debug message.
  engine_logger(dbg_functions, basic) << "checker::finished: res=" << &res;
  log_v2::functions()->trace("checker::finished: res={:p}", (void*)&res);

  std::unique_lock<std::mutex> lock(_mut_reap);
  auto it_id = _waiting_check_result.find(res.command_id);
  if (it_id == _waiting_check_result.end()) {
    engine_logger(log_runtime_warning, basic)
        << "command ID '" << res.command_id << "' not found";
    log_v2::runtime()->warn("command ID '{}' not found", res.command_id);
    return;
  }

  // Find check result.
  check_result* result = it_id->second;
  _waiting_check_result.erase(it_id);
  lock.unlock();

  // Update check result.
  struct timeval tv = {.tv_sec = res.end_time.to_seconds(),
                       .tv_usec = res.end_time.to_useconds() % 1000000ll};

  result->set_finish_time(tv);
  result->set_early_timeout(res.exit_status == process::timeout);
  result->set_return_code(res.exit_code);
  result->set_exited_ok(res.exit_status == process::normal ||
                        res.exit_status == process::timeout);
  result->set_output(res.output);

  // Queue check result.
  lock.lock();
  _to_reap_partial.push_back(result);
}

/**
 *  Run an host check with waiting check result.
 *
 *  @param[in] hst The host to check.
 *
 *  @result Return if the host is up ( host::state_up) or host down (
 * host::state_down).
 */
com::centreon::engine::host::host_state checker::_execute_sync(host* hst) {
  engine_logger(dbg_functions, basic) << "checker::_execute_sync: hst=" << hst;
  log_v2::functions()->trace("checker::_execute_sync: hst={:p}", (void*)hst);

  // Preamble.
  if (!hst)
    throw engine_error() << "Attempt to run synchronous check on invalid host";
  if (!hst->get_check_command_ptr())
    throw engine_error() << "Attempt to run synchronous active check on host '"
                         << hst->get_name() << "' with no check command";

  engine_logger(dbg_checks, basic)
      << "** Executing sync check of host '" << hst->get_name() << "'...";
  log_v2::checks()->trace("** Executing sync check of host '{}'...",
                          hst->get_name());

  // Send broker event.
  timeval start_time;
  timeval end_time;
  memset(&start_time, 0, sizeof(start_time));
  memset(&end_time, 0, sizeof(end_time));
  int ret(broker_host_check(
      NEBTYPE_HOSTCHECK_SYNC_PRECHECK, NEBFLAG_NONE, NEBATTR_NONE, hst,
      checkable::check_active, hst->get_current_state(), hst->get_state_type(),
      start_time, end_time, hst->check_command().c_str(), hst->get_latency(),
      0.0, config->host_check_timeout(), false, 0, nullptr, nullptr, nullptr,
      nullptr, nullptr));

  // Host sync check was cancelled or overriden by NEB module.
  if ((NEBERROR_CALLBACKCANCEL == ret) || (NEBERROR_CALLBACKOVERRIDE == ret))
    return hst->get_current_state();

  // Get current host macros.
  nagios_macros* macros(get_global_macros());
  grab_host_macros_r(macros, hst);
  std::string tmp;
  get_raw_command_line_r(macros, hst->get_check_command_ptr(),
                         hst->check_command().c_str(), tmp, 0);

  // Time to start command.
  gettimeofday(&start_time, nullptr);

  // Update last host check.
  hst->set_last_check(start_time.tv_sec);

  // Get command object.
  commands::command* cmd = hst->get_check_command_ptr();
  std::string processed_cmd(cmd->process_cmd(macros));
  const char* tmp_processed_cmd = processed_cmd.c_str();

  // Send broker event.
  broker_host_check(NEBTYPE_HOSTCHECK_RAW_START, NEBFLAG_NONE, NEBATTR_NONE,
                    hst, checkable::check_active, host::state_up,
                    hst->get_state_type(), start_time, end_time,
                    hst->check_command().c_str(), 0.0, 0.0,
                    config->host_check_timeout(), false, service::state_ok,
                    processed_cmd.c_str(),
                    const_cast<char*>(hst->get_plugin_output().c_str()),
                    const_cast<char*>(hst->get_long_plugin_output().c_str()),
                    const_cast<char*>(hst->get_perf_data().c_str()), nullptr);

  // Debug messages.
  engine_logger(dbg_commands, more)
      << "Raw host check command: "
      << hst->get_check_command_ptr()->get_command_line();
  log_v2::commands()->trace("Raw host check command: {}",
                            hst->get_check_command_ptr()->get_command_line());

  engine_logger(dbg_commands, more)
      << "Processed host check ommand: " << processed_cmd;
  log_v2::commands()->trace("Processed host check ommand: {}", processed_cmd);

  // Cleanup.
  hst->set_plugin_output("");
  hst->set_long_plugin_output("");
  hst->set_perf_data("");

  // Send broker event.
  timeval start_cmd;
  timeval end_cmd;
  gettimeofday(&start_cmd, nullptr);
  memset(&end_cmd, 0, sizeof(end_cmd));
  broker_system_command(NEBTYPE_SYSTEM_COMMAND_START, NEBFLAG_NONE,
                        NEBATTR_NONE, start_cmd, end_cmd, 0,
                        config->host_check_timeout(), false, 0,
                        tmp_processed_cmd, nullptr, nullptr);

  // Run command.
  commands::result res;
  try {
    cmd->run(processed_cmd, *macros, config->host_check_timeout(), res);
  } catch (std::exception const& e) {
    // Update check result.
    res.command_id = 0;
    res.end_time = timestamp::now();
    res.exit_code = service::state_unknown;
    res.exit_status = process::normal;
    res.output = "(Execute command failed)";
    res.start_time = res.end_time;

    engine_logger(log_runtime_warning, basic)
        << "Error: Synchronous host check command execution failed: "
        << e.what();
    log_v2::runtime()->warn(
        "Error: Synchronous host check command execution failed: {}", e.what());
  }

  // Get output.
  char* output(string::dup(res.output));

  unsigned int execution_time(0);
  if (res.end_time >= res.start_time)
    execution_time = res.end_time.to_seconds() - res.start_time.to_seconds();

  // Send broker event.
  memset(&start_cmd, 0, sizeof(start_time));
  start_cmd.tv_sec = res.start_time.to_seconds();
  start_cmd.tv_usec =
      res.start_time.to_useconds() - start_cmd.tv_sec * 1000000ull;
  memset(&end_cmd, 0, sizeof(end_time));
  end_cmd.tv_sec = res.end_time.to_seconds();
  end_cmd.tv_usec = res.end_time.to_useconds() - end_cmd.tv_sec * 1000000ull;
  broker_system_command(NEBTYPE_SYSTEM_COMMAND_END, NEBFLAG_NONE, NEBATTR_NONE,
                        start_cmd, end_cmd, execution_time,
                        config->host_check_timeout(),
                        res.exit_status == process::timeout, res.exit_code,
                        tmp_processed_cmd, output, nullptr);

  // Cleanup.
  delete[] output;
  clear_volatile_macros_r(macros);

  // If the command timed out.
  if (res.exit_status == process::timeout) {
    std::ostringstream oss;
    oss << "Host check timed out after " << config->host_check_timeout()
        << "  seconds";
    res.output = oss.str();
    engine_logger(log_runtime_warning, basic)
        << "Warning: Host check command '" << processed_cmd << "' for host '"
        << hst->get_name() << "' timed out after "
        << config->host_check_timeout() << " seconds";
    log_v2::runtime()->warn(
        "Warning: Host check command '{}' for host '{}' timed out after {} "
        "seconds",
        processed_cmd, hst->get_name(), config->host_check_timeout());
  }

  // Update values.
  hst->set_execution_time(execution_time);
  hst->set_check_type(checkable::check_active);

  // Get plugin output.
  std::string pl_output;
  std::string lpl_output;
  std::string perfdata_output;

  // Parse the output: short and long output, and perf data.
  parse_check_output(res.output, pl_output, lpl_output, perfdata_output, true,
                     false);

  hst->set_plugin_output(pl_output);
  hst->set_long_plugin_output(lpl_output);
  hst->set_perf_data(perfdata_output);

  // A nullptr host check command means we should assume the host is UP.
  if (hst->check_command().empty()) {
    hst->set_plugin_output("(Host assumed to be UP)");
    res.exit_code = service::state_ok;
  }

  // Make sure we have some data.
  if (hst->get_plugin_output().empty())
    hst->set_plugin_output("(No output returned from host check)");

  std::string ploutput(hst->get_plugin_output());
  std::replace(ploutput.begin(), ploutput.end(), ';', ':');
  hst->set_plugin_output(ploutput);

  // let WARNING states indicate the host is up (fake the result to be UP = 0).
  if (res.exit_code == service::state_warning)
    res.exit_code = service::state_ok;

  // Get host state from plugin exit code.
  host::host_state return_result(
      (res.exit_code == service::state_ok) ? host::state_up : host::state_down);

  // Get the end time of command.
  gettimeofday(&end_time, nullptr);

  // Send broker event.
  broker_host_check(
      NEBTYPE_HOSTCHECK_RAW_END, NEBFLAG_NONE, NEBATTR_NONE, hst,
      checkable::check_active, return_result, hst->get_state_type(), start_time,
      end_time, hst->check_command().c_str(), 0.0, execution_time,
      config->host_check_timeout(), res.exit_status == process::timeout,
      res.exit_code, tmp_processed_cmd,
      const_cast<char*>(hst->get_plugin_output().c_str()),
      const_cast<char*>(hst->get_long_plugin_output().c_str()),
      const_cast<char*>(hst->get_perf_data().c_str()), nullptr);

  // Termination.
  engine_logger(dbg_checks, basic)
      << "** Sync host check done: state=" << return_result;
  log_v2::checks()->trace("** Sync host check done: state={}", return_result);
  return return_result;
}

/**
 * @brief This method stores a command id and the check_result at the origin of
 *this command execution. The command is executed asynchronously. When the
 *execution is finished, thanks to this command we will be able to find the
 *check_result.
 *
 * @param id A command id
 * @param check_result A check_result coming from a service or a host.
 */
void checker::add_check_result(uint64_t id,
                               check_result* check_result) noexcept {
  std::lock_guard<std::mutex> lock(_mut_reap);
  _waiting_check_result[id] = check_result;
}

/**
 * @brief This method stores a check_result already finished in the _to_reap
 *list. The goal of this list is to update services and hosts with check_result.
 *
 * @param check_result The check_result already finished.
 */
void checker::add_check_result_to_reap(check_result* check_result) noexcept {
  std::lock_guard<std::mutex> lock(_mut_reap);
  _to_reap_partial.push_back(check_result);
}

/**
 * @brief Notifiers added here will be removed from current checks. This task
 * is necessary because the user could remove a service or a host while a check
 * is made on it.
 *
 * @param n The notifier to forget.
 */
void checker::forget(notifier* n) noexcept {
  if (_instance) {
    std::lock_guard<std::mutex> lock(_instance->_mut_reap);
    _instance->_to_forget.push_back(n);
  }
}
