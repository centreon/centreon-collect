/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "scheduler.hh"
#include "check.hh"
#include "check_cpu.hh"
#include "check_health.hh"
#include "config.hh"
#ifdef _WIN32
#include "check_counter.hh"
#include "check_event_log.hh"
#include "check_files.hh"
#include "check_memory.hh"
#include "check_process.hh"
#include "check_sched.hh"
#include "check_service.hh"
#include "check_uptime.hh"
#endif
#include "check_exec.hh"
#include "com/centreon/common/check_timeperiod.hh"
#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/common/utf8.hh"
#include "drive_size.hh"

#include "common/crypto/aes256.hh"

using namespace com::centreon::agent;

namespace {
// Format a UTC offset given in seconds east of UTC as a signed "+HH:MM" /
// "-HH:MM" string for human-readable diagnostics.
std::string format_utc_offset(int32_t off) {
  const char sign = off < 0 ? '-' : '+';
  const int abs_s = off < 0 ? -off : off;
  auto two = [](int v) {
    const std::string s = std::to_string(v);
    return s.size() < 2 ? "0" + s : s;
  };
  return std::string(1, sign) + two(abs_s / 3600) + ":" +
         two((abs_s % 3600) / 60);
}
}  // namespace

void scheduler::check_host_timezone(
    const std::shared_ptr<spdlog::logger>& logger,
    int32_t cfg_offset,
    bool cfg_dst,
    const std::string& cfg_tz_name) {
  // --- Read this host's current local UTC offset + DST (platform-specific).
  int32_t host_off = 0;
  bool host_dst = false;
#ifdef _WIN32
  TIME_ZONE_INFORMATION tzi;
  const DWORD id = GetTimeZoneInformation(&tzi);
  if (id == TIME_ZONE_ID_INVALID) {
    SPDLOG_LOGGER_ERROR(
        logger, "cannot retreive the timezone information form api windows");
    return;
  }
  const long active_bias =
      (id == TIME_ZONE_ID_DAYLIGHT) ? tzi.DaylightBias : tzi.StandardBias;
  host_off = -static_cast<int32_t>((tzi.Bias + active_bias) * 60);
  host_dst = (id == TIME_ZONE_ID_DAYLIGHT);
#else
  // POSIX: tm_gmtoff already folds in any active DST.
  const time_t now = time(nullptr);
  struct tm tmv = {};
  if (!localtime_r(&now, &tmv)) {
    SPDLOG_LOGGER_ERROR(logger, "cannot retreive the timezone information");
    return;
  }
  host_off = static_cast<int32_t>(tmv.tm_gmtoff);
  host_dst = tmv.tm_isdst > 0;
#endif

  // Strip a leading POSIX ':' (":Europe/Paris") for a cleaner message.
  std::string_view tz = cfg_tz_name;
  if (!tz.empty() && tz.front() == ':')
    tz.remove_prefix(1);

  // not the same offset
  if (cfg_offset != host_off) {
    SPDLOG_LOGGER_ERROR(
        logger,
        "host timezone mismatch - configured zone '{}' is at UTC{} (DST {}) "
        "but the agent host is at UTC{} (DST {}); timeperiod checks may be "
        "evaluated at the wrong local time",
        tz, format_utc_offset(cfg_offset), cfg_dst ? "on" : "off",
        format_utc_offset(host_off), host_dst ? "on" : "off");
    return;
  }
  // not same dst (daylight save time)
  if (cfg_dst != host_dst) {
    SPDLOG_LOGGER_ERROR(
        logger,
        "configured zone '{}' and the agent host currently agree at UTC{} but "
        "handle DST differently (configured DST {}, host DST {}); timeperiod "
        "checks may diverge at the next DST transition",
        tz, format_utc_offset(host_off), cfg_dst ? "on" : "off",
        host_dst ? "on" : "off");
    return;
  }
  SPDLOG_LOGGER_DEBUG(
      logger,
      "host timezone match - configured zone '{}' and the agent host are both "
      "at UTC{} (DST {})",
      tz, format_utc_offset(host_off), host_dst ? "on" : "off");
  return;
}

/**
 * @brief destructor
 *
 */
scheduler::~scheduler() {
  SPDLOG_LOGGER_DEBUG(_logger, "scheduler delete {:p}",
                      static_cast<const void*>(this));
}

/**
 * @brief to call after creation
 * it create a default configuration with no check and start send timer
 */
void scheduler::_start() {
  SPDLOG_LOGGER_DEBUG(_logger, "scheduler start {:p}",
                      static_cast<const void*>(this));
  _init_export_request();
  _next_send_time_point = std::chrono::system_clock::now();
  _check_time_step =
      time_step(_next_send_time_point, std::chrono::milliseconds(100));
  update(_conf);
  _start_custom_checks_watcher();
}

/**
 * @brief if a custom checks file is configured, watch it in order to refresh
 * commands without agent restart
 *
 */
void scheduler::_start_custom_checks_watcher() {
  const config* conf = config::instance_ptr();
  if (!conf || conf->get_path_to_custom_checks().empty()) {
    return;
  }
  _custom_checks_watcher = common::file_watcher::load(
      _io_context, _logger, conf->get_path_to_custom_checks(),
      [me = std::weak_ptr<scheduler>(shared_from_this())]() {
        std::shared_ptr<scheduler> to_notify = me.lock();
        if (to_notify) {
          to_notify->_on_custom_checks_file_change();
        }
      });
}

/**
 * @brief called (from the io_context thread) when the custom checks file has
 * been created, modified or deleted
 * It only refreshes the custom check commands of the global configuration:
 * they will be used the next time check objects are built (new engine
 * configuration)
 * If the file can't be read or is malformed, the reload fails and the
 * previous commands are kept
 *
 */
void scheduler::_on_custom_checks_file_change() {
  if (!_alive) {
    return;
  }
  if (config::reload_custom_checks()) {
    SPDLOG_LOGGER_INFO(
        _logger, "custom checks file updated => refresh custom check commands");
  }
}

/**
 * @brief start periodic metric sent to engine
 *
 */
void scheduler::_start_send_timer() {
  _next_send_time_point +=
      std::chrono::seconds(_conf->config().export_period());
  _send_timer.expires_at(_next_send_time_point);
  _send_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_send_timer_handler(err);
      });
}

/**
 * @brief send all check results to engine
 *
 * @param err
 */
void scheduler::_send_timer_handler(const boost::system::error_code& err) {
  if (err || !_alive) {
    return;
  }
  if (_current_request->mutable_otel_request()->resource_metrics_size() > 0) {
    _metric_sender(_current_request);
    _init_export_request();
  }
  _start_send_timer();
}

/**
 * @brief create export request and fill some attributes
 *
 */
void scheduler::_init_export_request() {
  _current_request = std::make_shared<MessageFromAgent>();
  _serv_to_scope_metrics.clear();
}

/**
 * @brief create a default empty configuration to scheduler
 *
 */
std::shared_ptr<com::centreon::agent::MessageToAgent>
scheduler::default_config() {
  std::shared_ptr<com::centreon::agent::MessageToAgent> ret =
      std::make_shared<com::centreon::agent::MessageToAgent>();
  ret->mutable_config()->set_export_period(1);
  ret->mutable_config()->set_max_concurrent_checks(10);
  return ret;
}

/**
 * @brief start check timer.
 * When it will expire, we will call every check whose start_expected is lower
 * than the actual time point
 * if no check available, we start timer for 100ms
 *
 */
void scheduler::_start_check_timer() {
  _check_time_step.increment_to_after_now();
  _check_timer.expires_at(_check_time_step.value());
  _check_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_check_timer_handler(err);
      });
}

/**
 * @brief check timer handler
 *
 * @param err
 */
void scheduler::_check_timer_handler(const boost::system::error_code& err) {
  if (err || !_alive) {
    return;
  }
  _start_waiting_check();
  _start_check_timer();
}

/**
 * @brief start all waiting checks, no more concurrent checks than
 * max_concurrent_checks
 * check started are removed from queue and will be inserted once completed
 */
void scheduler::_start_waiting_check() {
  std::vector<std::pair<const check::pointer, uint64_t>> save_defer_checks;
  time_point now = std::chrono::system_clock::now();
  const time_t now_t = std::chrono::system_clock::to_time_t(now);
  if (!_waiting_check_queue.empty()) {
    for (check_queue::iterator to_check = _waiting_check_queue.begin();
         !_waiting_check_queue.empty() &&
         to_check != _waiting_check_queue.end() &&
         to_check->second->get_start_expected() <= now &&
         _active_check < _conf->config().max_concurrent_checks();) {
      const std::string& period_name =
          to_check->second->get_check_period_name();
      const bool in_period = com::centreon::common::is_time_in_period_by_name(
          now_t, period_name, _timeperiods);
      SPDLOG_LOGGER_DEBUG(
          _logger,
          "timeperiod check: service='{}' period='{}' now={} in_period={}",
          to_check->second->get_service(),
          period_name.empty() ? "(none)" : period_name, now_t, in_period);
      if (!in_period) {
        // next_valid_time_in_period_by_name returns now_t (= orig) when no
        // valid slot is found in the next 366 days (get_next_valid_time uses
        // notif=false, which falls back to orig on failure).  Comparing
        // next_t > now_t therefore distinguishes a real future opening from
        // the "period has no active windows" sentinel.
        const time_t next_t =
            com::centreon::common::next_valid_time_in_period_by_name(
                now_t, period_name, _timeperiods);
        const check::pointer deferred = to_check->second;
        to_check = _waiting_check_queue.erase(to_check);
        time_step slot(_check_time_step);
        if (next_t > now_t) {
          SPDLOG_LOGGER_DEBUG(
              _logger,
              "service '{}': outside period '{}', next open at {} ({}s from "
              "now)",
              deferred->get_service(), period_name, next_t, next_t - now_t);
          slot.increment_to_after_min(
              std::chrono::system_clock::from_time_t(next_t));
        } else {
          // next_t == now_t: no valid window in the next 366 days.
          // Defer by the same horizon so we do not spin wastefully.  Any
          // engine config update triggers update() which rebuilds the queue,
          // so a corrected period will be picked up regardless.
          static constexpr auto one_year =
              std::chrono::seconds(366 * 24 * 60 * 60);
          SPDLOG_LOGGER_DEBUG(
              _logger,
              "service '{}': period '{}' has no active time in the next "
              "year, deferring by one year",
              deferred->get_service(), period_name);
          slot.increment_to_after_min(now + one_year);
        }
        save_defer_checks.push_back(std::pair(deferred, slot.get_step_index()));
        continue;
      }
      SPDLOG_LOGGER_DEBUG(_logger,
                          "service '{}': inside period '{}', starting check",
                          to_check->second->get_service(), period_name);
      _start_check(to_check->second);
      to_check = _waiting_check_queue.erase(to_check);
    }

    for (const auto& defer_check : save_defer_checks) {
      uint64_t steps = defer_check.second;
      while (!_waiting_check_queue.emplace(steps, defer_check.first).second) {
        ++steps;
      }
    }
  }
}

/**
 * @brief called on a message sent by engine
 *
 * @param request
 */
void scheduler::on_engine_request(const engine_to_agent_request_ptr& request) {
  if (request->has_config()) {
    update(request);
  } else if (request->has_force_check()) {
    force_check(request);
  }
}

/**
 * @brief called when we receive a new configuration
 * It initialize check queue and restart all checks schedule
 * running checks stay alive but their completion will not be handled
 * We compute start_expected of checks in order to spread checks over
 * check_interval
 * @param conf
 */
void scheduler::update(const engine_to_agent_request_ptr& conf) {
  _waiting_check_queue.clear();
  _active_check = 0;
  size_t nb_check = conf->config().services().size();

  if (nb_check > 0) {
    // raz stats in order to not keep statistics of deleted checks
    checks_statistics::pointer statistics =
        std::make_shared<checks_statistics>();

    if (!conf->config().key().empty() && !conf->config().salt().empty()) {
      try {
        _credentials_decrypt = std::make_shared<common::crypto::aes256>(
            conf->config().key(), conf->config().salt());
        SPDLOG_LOGGER_INFO(_logger,
                           "Agent is ready to receive encrypted credentials");
      } catch (const std::exception& e) {
        _credentials_decrypt.reset();
        SPDLOG_LOGGER_ERROR(_logger,
                            "Invalid credentials received from engine, agent "
                            "will be unable to decrypt credentials");
      }
    } else {
      _credentials_decrypt.reset();
      SPDLOG_LOGGER_INFO(_logger, "Agent will need no encrypted credentials");
    }

    // first we group checks by check_interval
    std::map<uint32_t, std::vector<Service*>> group_serv;
    for (auto& serv : *conf->mutable_config()->mutable_services()) {
      if (serv.check_interval() == 0) {
        serv.set_check_interval(300);  // five minutes by default
      }
      if (serv.retry_interval() == 0) {
        serv.set_retry_interval(60);  // one minute by default
      }
      if (serv.max_attempts() == 0) {
        serv.set_max_attempts(3);  // three attempt by default
      }
      // An empty service_description is the host check
      // note: older engines that don't send the field.
      if (serv.service_description().empty() && serv.has_utc_offset()) {
        check_host_timezone(_logger, serv.utc_offset(), serv.dst_active(),
                            serv.timezone());
      }
      uint32_t check_interval = serv.check_interval();
      uint32_t retry_interval = serv.retry_interval();
      auto min_interval = std::min(check_interval, retry_interval);
      group_serv[min_interval].push_back(&serv);
    }

    srand(time(nullptr));
    std::chrono::milliseconds first_inter_check_delay(
        (group_serv.begin()->first * 1000) / nb_check);
    // in order to avoid collision when we will use a time_step equal to
    // first_inter_check_delay / 2 with a little random
    // first_inter_check_delay <= 1 ms the count/10 is 0
    if (first_inter_check_delay.count() / 10 == 0) {
      first_inter_check_delay = std::chrono::milliseconds(10);
    }

    duration time_unit = first_inter_check_delay / 2 +
                         std::chrono::milliseconds(
                             rand() % (first_inter_check_delay.count() / 10));

    std::chrono::seconds accuracy = std::chrono::seconds(5);
    // we need to respect check_interval accuracy
    while (1) {
      bool need_to_continue = false;
      for (const auto& [interval, _] : group_serv) {
        if (std::chrono::seconds(interval) % time_unit > accuracy) {
          time_unit -= time_unit / 10;
          need_to_continue = true;
          break;
        }
      }
      if (!need_to_continue) {
        break;
      }
    }

    SPDLOG_LOGGER_DEBUG(_logger, "all checks will use a time step of {}",
                        time_unit);

    auto group_iter = group_serv.begin();

    /**
     * When we receive conf, old checks are yet running, so without the delay
     * of 1 second above, we could have this scenario: at 12:00:00.100 an old
     * check executes at 12:00:00.200 we receive a new configuration at
     * 12:00:00.200 we executes the first check so if checks are fast, engine
     * can receives two checks for the same service with the same time
     * (rounded to 1 second)
     */
    time_point next =
        std::chrono::system_clock::now() + std::chrono::seconds(1);

    _check_time_step = time_step(next, time_unit);

    auto last_inserted_iter = _waiting_check_queue.end();
    unsigned step_index = 0;
    while (true) {
      const auto& serv = **group_iter->second.rbegin();

      if (_logger->level() == spdlog::level::trace) {
        SPDLOG_LOGGER_TRACE(
            _logger, "check expected to start at {} for service {} command {}",
            next, serv.service_description(), serv.command_line());
      } else {
        SPDLOG_LOGGER_TRACE(_logger,
                            "check expected to start at {} for service {}",
                            next, serv.service_description());
      }
      try {
        auto check_to_schedule = _check_builder(
            _io_context, _logger, next, serv, conf,
            [me = shared_from_this()](
                const std::shared_ptr<check>& check, unsigned status,
                const std::list<com::centreon::common::perfdata>& perfdata,
                const std::list<std::string>& outputs) {
              me->_check_handler(check, status, perfdata, outputs);
            },
            statistics, _credentials_decrypt);
        last_inserted_iter = _waiting_check_queue.emplace_hint(
            last_inserted_iter, step_index, check_to_schedule);
        next += first_inter_check_delay;
        ++step_index;
      } catch (const std::exception& e) {
        SPDLOG_LOGGER_ERROR(
            _logger, "service: {}  command:{} won't be scheduled cause: {}",
            serv.service_description(), serv.command_name(), e.what());
      }
      group_iter->second.pop_back();
      if (group_iter->second.empty()) {
        group_iter = group_serv.erase(group_iter);
      } else {
        ++group_iter;
      }
      if (group_serv.empty()) {
        break;
      }
      if (group_iter == group_serv.end()) {
        group_iter = group_serv.begin();
      }
    }
  }

  _conf = conf;

  _timeperiods.clear();
  for (const auto& tp : conf->config().timeperiods()) {
    if (tp.has_timeperiod_name()) {
      _timeperiods.emplace(tp.timeperiod_name(), &tp);
      SPDLOG_LOGGER_DEBUG(_logger, "registered timeperiod '{}'",
                          tp.timeperiod_name());
    }
  }

  // first start check in waiting queue
  _start_waiting_check();
  // start send timer and check timer
  // safe because the expire_at cancel the previous timer
  _start_check_timer();
  _start_send_timer();
}

/**
 * @brief do a force check by moving service (if waiting in queue) to the top
 * of the queue
 *
 * @param request
 */
void scheduler::force_check(const engine_to_agent_request_ptr& request) {
  auto force = request->force_check();
  if (!_waiting_check_queue.empty()) {
    for (check_queue::iterator to_check = _waiting_check_queue.begin();
         !_waiting_check_queue.empty() &&
         to_check != _waiting_check_queue.end();
         ++to_check) {
      if (to_check->second->get_service_id() == force.serv_id() &&
          to_check->second->get_host_id() == force.host_id()) {
        SPDLOG_LOGGER_INFO(_logger, "force check of service {} {}@{}",
                           to_check->second->get_service(),
                           to_check->second->get_service_id(),
                           to_check->second->get_host_id());
        _start_check(to_check->second);
        _waiting_check_queue.erase(to_check);
        return;
      }
    }
  }
  SPDLOG_LOGGER_INFO(_logger,
                     "service {}@{} not in queue (perhaps yet running) => it "
                     "won't be check forced",
                     force.serv_id(), force.host_id());
}

/**
 * @brief start a check
 *
 * @param check
 */
void scheduler::_start_check(const check::pointer& check) {
  ++_active_check;
  if (_logger->level() <= spdlog::level::trace) {
    SPDLOG_LOGGER_TRACE(_logger, "start check for service {} command {}",
                        check->get_service(), check->get_command_line());
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "start check for service {}",
                        check->get_service());
  }
  check->start_check(std::chrono::seconds(_conf->config().check_timeout()));
}

/**
 * @brief completion check handler
 * if conf has been updated during check, it does nothing
 *
 * @param check
 * @param status
 * @param perfdata
 * @param outputs
 */
void scheduler::_check_handler(
    const check::pointer& check,
    unsigned status,
    const std::list<com::centreon::common::perfdata>& perfdata,
    const std::list<std::string>& outputs) {
  // conf has changed => no repush for next check
  if (check->get_conf() != _conf) {
    return;
  }

  SPDLOG_LOGGER_DEBUG(_logger,
                      "end check for service {} command {}, status {} {} "
                      "CA:{}/{} , outputs: {}",
                      check->get_service(), check->get_command_line(), status,
                      check->get_status_confirmed() ? "HARD" : "SOFT",
                      check->get_current_attempt(), check->get_max_attempts(),
                      outputs.front());

  if (_conf->config().use_exemplar()) {
    _store_result_in_metrics_and_exemplars(check, status, perfdata, outputs);
  } else {
    _store_result_in_metrics(check, status, perfdata, outputs);
  }

  --_active_check;

  if (_alive) {
    time_point now = std::chrono::system_clock::now();

    // repush for next check and search a free start slot in queue
    check->increment_start_expected_to_after_min_timepoint(now);

    time_step slot_search(_check_time_step);
    slot_search.increment_to_after_min(check->get_start_expected());
    uint64_t steps = slot_search.get_step_index();
    while (!_waiting_check_queue.emplace(steps, check).second) {
      // slot yet reserved => try next
      ++steps;
    }
    SPDLOG_LOGGER_DEBUG(_logger,
                        "next check expected at {} for {}, slot time "
                        "{} [index : {}] , check insert at index {}",
                        check->get_start_expected(), check->get_service(),
                        slot_search.value(), slot_search.get_step_index(),
                        steps);
  }
}

/**
 * @brief to call on process termination or accepted connection error
 *
 */
void scheduler::stop() {
  if (_alive) {
    SPDLOG_LOGGER_DEBUG(_logger, "scheduler stop {:p}",
                        static_cast<const void*>(this));
    _alive = false;
    _send_timer.cancel();
    _check_timer.cancel();
    if (_custom_checks_watcher) {
      _custom_checks_watcher->stop();
      _custom_checks_watcher.reset();
    }
  }
}

/**
 * @brief stores results in telegraf manner
 *
 * @param check
 * @param status
 * @param perfdata
 * @param outputs
 */
void scheduler::_store_result_in_metrics(
    [[maybe_unused]] const check::pointer& check,
    [[maybe_unused]] unsigned status,
    [[maybe_unused]] const std::list<com::centreon::common::perfdata>& perfdata,
    [[maybe_unused]] const std::list<std::string>& outputs) {
  // auto scope_metrics =
  //     get_scope_metrics(check->get_host(), check->get_service());
  // unsigned now = std::chrono::duration_cast<std::chrono::nanoseconds>(
  //                    std::chrono::system_clock::now().time_since_epoch())
  //                    .count();

  // auto state_metrics = scope_metrics->add_metrics();
  // state_metrics->set_name(check->get_command_name() + "_state");
  // if (!outputs.empty()) {
  //   const std::string& first_line = *outputs.begin();
  //   size_t pipe_pos = first_line.find('|');
  //   state_metrics->set_description(pipe_pos != std::string::npos
  //                                      ? first_line.substr(0, pipe_pos)
  //                                      : first_line);
  // }
  // auto data_point = state_metrics->mutable_gauge()->add_data_points();
  // data_point->set_time_unix_nano(now);
  // data_point->set_as_int(status);

  // we aggregate perfdata results by type (min, max, )
}

/**
 * @brief store results with centreon sauce
 *
 * @param check
 * @param status
 * @param perfdata
 * @param outputs
 */
void scheduler::_store_result_in_metrics_and_exemplars(
    const check::pointer& check,
    unsigned status,
    const std::list<com::centreon::common::perfdata>& perfdata,
    const std::list<std::string>& outputs) {
  // we don't want to erase existing previous metrics, so we send right now
  auto exist = _serv_to_scope_metrics.find(check->get_service());
  if (exist != _serv_to_scope_metrics.end()) {
    _metric_sender(_current_request);
    _init_export_request();
  }

  auto& scope_metrics = _get_scope_metrics(check->get_service());
  uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  uint64_t check_start = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             check->get_last_start().time_since_epoch())
                             .count();

  auto state_metrics = _get_metric(scope_metrics, "status");
  if (!outputs.empty()) {
    const std::string& first_line = *outputs.begin();
    size_t pipe_pos = first_line.find('|');
    state_metrics->set_description(common::check_string_utf8(
        pipe_pos != std::string::npos ? first_line.substr(0, pipe_pos)
                                      : first_line));
  }
  auto data_point = state_metrics->mutable_gauge()->add_data_points();
  data_point->set_time_unix_nano(now);
  data_point->set_start_time_unix_nano(check_start);
  data_point->set_as_int(status);

  // add exemplar for status_confirmed
  _add_exemplar("status_confirmed", check->get_status_confirmed(), *data_point);
  _add_exemplar("current_attempt", check->get_current_attempt(), *data_point);

  for (const com::centreon::common::perfdata& perf : perfdata) {
    _add_metric_to_scope(check_start, now, perf, scope_metrics);
  }
  if (!_average_metric_length &&
      _current_request->otel_request().resource_metrics_size() > 10) {
    _average_metric_length =
        _current_request->ByteSizeLong() /
        _current_request->otel_request().resource_metrics_size();
  }
  if (_current_request->otel_request().resource_metrics_size() *
          _average_metric_length >
      2 * 1024 * 1024) {
    _metric_sender(_current_request);
    _init_export_request();
  }
}

/**
 * @brief metrics are grouped by host service
 * (one resource_metrics by host serv pair)
 * no resource_metrics for this service must exist before calling this
 * function
 * @param service
 * @return a new scheduler::scope_metric_request&
 */
scheduler::scope_metric_request& scheduler::_get_scope_metrics(
    const std::string& service) {
  ::opentelemetry::proto::metrics::v1::ResourceMetrics* new_res =
      _current_request->mutable_otel_request()->add_resource_metrics();

  auto* host_attrib = new_res->mutable_resource()->add_attributes();
  host_attrib->set_key("host.name");
  host_attrib->mutable_value()->set_string_value(_supervised_host);
  auto* serv_attrib = new_res->mutable_resource()->add_attributes();
  serv_attrib->set_key("service.name");
  serv_attrib->mutable_value()->set_string_value(service);

  ::opentelemetry::proto::metrics::v1::ScopeMetrics* new_scope =
      new_res->add_scope_metrics();

  scope_metric_request to_insert;
  to_insert.scope_metric = new_scope;

  return _serv_to_scope_metrics.emplace(service, to_insert).first->second;
}

/**
 * @brief one metric by metric name (can contains several datapoints in case
 * of multiple checks during send period )
 *
 * @param scope_metric
 * @param metric_name
 * @return ::opentelemetry::proto::metrics::v1::Metric*
 */
::opentelemetry::proto::metrics::v1::Metric* scheduler::_get_metric(
    scope_metric_request& scope_metric,
    const std::string& metric_name) {
  auto exist = scope_metric.metrics.find(metric_name);
  if (exist != scope_metric.metrics.end()) {
    return exist->second;
  }

  ::opentelemetry::proto::metrics::v1::Metric* new_metric =
      scope_metric.scope_metric->add_metrics();
  new_metric->set_name(metric_name);

  scope_metric.metrics.emplace(metric_name, new_metric);

  return new_metric;
}

/**
 * @brief add a perfdata to metric
 *
 * @param now
 * @param perf
 * @param scope_metric
 */
void scheduler::_add_metric_to_scope(
    uint64_t check_start,
    uint64_t now,
    const com::centreon::common::perfdata& perf,
    scope_metric_request& scope_metric) {
  auto metric = _get_metric(scope_metric, perf.name());
  metric->set_unit(perf.unit());
  auto data_point = metric->mutable_gauge()->add_data_points();
  data_point->set_as_double(perf.value());
  data_point->set_time_unix_nano(now);
  data_point->set_start_time_unix_nano(check_start);
  switch (perf.value_type()) {
    case com::centreon::common::perfdata::counter: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("counter");
      break;
    }
    case com::centreon::common::perfdata::derive: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("derive");
      break;
    }
    case com::centreon::common::perfdata::absolute: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("absolute");
      break;
    }
    case com::centreon::common::perfdata::automatic: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("auto");
      break;
    }
    case com::centreon::common::perfdata::gauge: {
      auto attrib_type = data_point->add_attributes();
      attrib_type->set_key("gauge");
      break;
    }
  }
  if (perf.critical() <= std::numeric_limits<double>::max()) {
    _add_exemplar(perf.critical_mode() ? "crit_ge" : "crit_gt", perf.critical(),
                  *data_point);
  }
  if (perf.critical_low() <= std::numeric_limits<double>::max()) {
    _add_exemplar(perf.critical_mode() ? "crit_le" : "crit_lt",
                  perf.critical_low(), *data_point);
  }
  if (perf.warning() <= std::numeric_limits<double>::max()) {
    _add_exemplar(perf.warning_mode() ? "warn_ge" : "warn_gt", perf.warning(),
                  *data_point);
  }
  if (perf.warning_low() <= std::numeric_limits<double>::max()) {
    _add_exemplar(perf.critical_mode() ? "warn_le" : "warn_lt",
                  perf.warning_low(), *data_point);
  }
  if (perf.min() <= std::numeric_limits<double>::max()) {
    _add_exemplar("min", perf.min(), *data_point);
  }
  if (perf.max() <= std::numeric_limits<double>::max()) {
    _add_exemplar("max", perf.max(), *data_point);
  }
}

/**
 * @brief add an exemplar to metric such as crit_le, min, max..
 *
 * @param label
 * @param value
 * @param data_point
 */
void scheduler::_add_exemplar(
    const char* label,
    double value,
    ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_point) {
  auto exemplar = data_point.add_exemplars();
  auto attrib = exemplar->add_filtered_attributes();
  attrib->set_key(label);
  exemplar->set_as_double(value);
}

/**
 * @brief add an exemplar to metric such as crit_le, min, max..
 *
 * @param label
 * @param value
 * @param data_point
 */
void scheduler::_add_exemplar(
    const char* label,
    bool value,
    ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_point) {
  auto exemplar = data_point.add_exemplars();
  auto attrib = exemplar->add_filtered_attributes();
  attrib->set_key(label);
  exemplar->set_as_int(value);
}

/**
 * @brief add an exemplar to metric
 *
 * @param label
 * @param value
 * @param data_point
 */
void scheduler::_add_exemplar(
    const char* label,
    int value,
    ::opentelemetry::proto::metrics::v1::NumberDataPoint& data_point) {
  auto exemplar = data_point.add_exemplars();
  auto attrib = exemplar->add_filtered_attributes();
  attrib->set_key(label);
  exemplar->set_as_int(value);
}

/**
 * @brief build a check object from command lline
 *
 * @param io_context
 * @param logger logger that will be used by the check
 * @param start_expected timepoint of first check
 * @param service
 * @param cmd_name
 * @param cmd_line
 * @param conf conf given by engine
 * @param handler handler that will be called on check completion
 * @return std::shared_ptr<check>
 */
std::shared_ptr<check> scheduler::default_check_builder(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<spdlog::logger>& logger,
    time_point first_start_expected,
    const Service& service,
    const engine_to_agent_request_ptr& conf,
    check::completion_handler&& handler,
    const checks_statistics::pointer& stat,
    const std::shared_ptr<common::crypto::aes256>& credentials_decrypt) {
  std::string command_line;
  // has to decrypt cmd_line
  if (credentials_decrypt &&
      !service.command_line().compare(0, 9, "encrypt::")) {
    try {
      command_line = credentials_decrypt->decrypt(
          std::string_view(service.command_line()).substr(9));
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger,
                          "Fail to decrypt command line for service {} : {}",
                          service.service_description(), e.what());
      return check_dummy::load(
          io_context, logger, first_start_expected, service,
          fmt::format("Unable to decrypt command line {}", e.what()), conf,
          std::move(handler), stat);
    }
  } else {
    command_line = service.command_line();
  }
  using namespace std::literals;
  // test native checks where cmd_line is a json
  try {
    rapidjson::Document native_check_info =
        common::rapidjson_helper::read_from_string(command_line);
    common::rapidjson_helper native_params(native_check_info);
    try {
      std::string_view check_type = native_params.get_string("check");
      const rapidjson::Value* args;
      if (native_params.has_member("args")) {
        args = &native_params.get_member("args");
      } else {
        static const rapidjson::Value no_arg;
        args = &no_arg;
      }

      std::optional<duration> custom_timeout;
      if (args->IsObject() && args->HasMember("timeout")) {
        auto timeout_sec = check::get_double(service.command_name(), "timeout",
                                             (*args)["timeout"], true);
        if (timeout_sec.has_value() && timeout_sec.value() > 0) {
          custom_timeout =
              std::chrono::seconds(static_cast<unsigned>(timeout_sec.value()));
        }
      }

      std::shared_ptr<check> result;
      if (check_type == "cpu_percentage"sv) {
        result = std::make_shared<check_cpu>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "health"sv) {
        result = std::make_shared<check_health>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "custom"sv) {
        result = std::make_shared<check_custom>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat, credentials_decrypt);
#ifdef _WIN32
      } else if (check_type == "uptime"sv) {
        result = std::make_shared<check_uptime>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "storage"sv) {
        result = std::make_shared<check_drive_size>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "memory"sv) {
        result = std::make_shared<check_memory>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "service"sv) {
        result = std::make_shared<check_service>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "counter"sv) {
        result = std::make_shared<check_counter>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "tasksched"sv) {
        result = std::make_shared<check_sched>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "files"sv) {
        result = std::make_shared<check_files>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
      } else if (check_type == "eventlog_nscp"sv) {
        result = check_event_log::load(io_context, logger, first_start_expected,
                                       service, *args, conf, std::move(handler),
                                       stat);
      } else if (check_type == "process_nscp"sv) {
        result = std::make_shared<check_process>(
            io_context, logger, first_start_expected, service, *args, conf,
            std::move(handler), stat);
#endif
      } else {
        throw exceptions::msg_fmt("command {}, unknown native check:{}",
                                  service.command_name(), command_line);
      }

      if (custom_timeout.has_value()) {
        result->set_custom_timeout(custom_timeout.value());
      }
      return result;
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "unexpected error: {}", e.what());
      return check_dummy::load(io_context, logger, first_start_expected,
                               service, std::string(e.what()), conf,
                               std::move(handler), stat);
    }
  } catch (const std::exception&) {
    return check_exec::load(io_context, logger, first_start_expected, service,
                            command_line, conf, std::move(handler), stat,
                            credentials_decrypt);
  }
}