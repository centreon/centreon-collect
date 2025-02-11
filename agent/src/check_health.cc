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

#include "check_health.hh"
#include <iterator>
#include "com/centreon/common/rapidjson_helper.hh"
#include "config.hh"
#include "version.hh"

using namespace com::centreon::agent;

/**
 * @brief Construct a new check_health object
 *
 * @param io_context
 * @param logger
 * @param first_start_expected
 * @param check_interval
 * @param serv
 * @param cmd_name
 * @param cmd_line
 * @param args
 * @param cnf
 * @param handler
 */
check_health::check_health(const std::shared_ptr<asio::io_context>& io_context,
                           const std::shared_ptr<spdlog::logger>& logger,
                           time_point first_start_expected,
                           duration check_interval,
                           const std::string& serv,
                           const std::string& cmd_name,
                           const std::string& cmd_line,
                           const rapidjson::Value& args,
                           const engine_to_agent_request_ptr& cnf,
                           check::completion_handler&& handler,
                           const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler),
            stat),
      _warning_check_interval(0),
      _critical_check_interval(0),
      _warning_check_duration(0),
      _critical_check_duration(0),
      _measure_timer(*io_context) {
  com::centreon::common::rapidjson_helper arg(args);
  try {
    if (args.IsObject()) {
      _warning_check_interval = arg.get_unsigned("warning-interval", 0);
      _critical_check_interval = arg.get_unsigned("critical-interval", 0);
      _warning_check_duration = arg.get_unsigned("warning-runtime", 0);
      _critical_check_duration = arg.get_unsigned("critical-runtime", 0);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_health, fail to parse arguments: {}",
                        e.what());
    throw;
  }

  if (config::instance().use_reverse_connection()) {
    _info_output = "Version: " CENTREON_AGENT_VERSION
                   " - Connection mode: Poller initiated - Current "
                   "configuration: {} checks - Average runtime: {}s";
  } else {
    _info_output = "Version: " CENTREON_AGENT_VERSION
                   " - Connection mode: Agent initiated - Current "
                   "configuration: {} checks - Average runtime: {}s";
  }
}

/**
 * @brief start a timer to do the job
 *
 * @param timeout unused
 */
void check_health::start_check([[maybe_unused]] const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }

  // we wait a little in order to have statistics check_interval/2
  _measure_timer.expires_from_now(get_raw_start_expected().get_step() / 2);
  _measure_timer.async_wait(
      [me = shared_from_this(), start_check_index = _get_running_check_index()](
          const boost::system::error_code& err) mutable {
        std::static_pointer_cast<check_health>(me)->_measure_timer_handler(
            err, start_check_index);
      });
}

/**
 * @brief timer handler that do the job
 *
 * @param err  set if canceled
 * @param start_check_index used by on_completion
 */
void check_health::_measure_timer_handler(const boost::system::error_code& err,
                                          unsigned start_check_index) {
  if (err) {
    return;
  }
  std::string output;
  std::list<common::perfdata> perf;
  e_status status = compute(&output, &perf);

  on_completion(start_check_index, status, perf, {output});
}

/**
 * @brief calculate status, output and perfdata from statistics
 *
 * @param ms_uptime
 * @param output
 * @param perfs
 * @return e_status
 */
e_status check_health::compute(std::string* output,
                               std::list<common::perfdata>* perf) {
  e_status ret = e_status::ok;

  const checks_statistics& stats = get_stats();

  if (stats.size() == 0) {
    *output = "UNKNOWN: No check yet performed";
    return e_status::unknown;
  }

  absl::flat_hash_set<std::string_view> written_to_output;

  unsigned average_runtime = 0;
  for (const auto& stat : stats.get_ordered_by_duration()) {
    average_runtime += std::chrono::duration_cast<std::chrono::seconds>(
                           stat.last_check_duration)
                           .count();
  }

  auto append_state_to_output = [&](e_status status, std::string* temp_output,
                                    const auto& iter) {
    if (written_to_output.insert(iter->cmd_name).second) {
      if (temp_output->empty()) {
        *temp_output = status_label[status];
      } else {
        temp_output->push_back(',');
        temp_output->push_back(' ');
      }
      if (status > ret) {
        ret = status;
      }
      absl::StrAppend(temp_output, iter->cmd_name, " runtime:",
                      std::chrono::duration_cast<std::chrono::seconds>(
                          iter->last_check_duration)
                          .count(),
                      "s interval:",
                      std::chrono::duration_cast<std::chrono::seconds>(
                          iter->last_check_interval)
                          .count(),
                      "s");
    }
  };

  std::string critical_output;
  if (_critical_check_duration > 0) {
    auto critical_duration = std::chrono::seconds(_critical_check_duration);
    for (auto iter = stats.get_ordered_by_duration().rbegin();
         iter != stats.get_ordered_by_duration().rend() &&
         iter->last_check_duration > critical_duration;
         ++iter) {
      append_state_to_output(e_status::critical, &critical_output, iter);
    }
  }

  if (_critical_check_interval > 0) {
    auto critical_interval = std::chrono::seconds(_critical_check_interval);
    for (auto iter = stats.get_ordered_by_interval().rbegin();
         iter != stats.get_ordered_by_interval().rend() &&
         iter->last_check_interval > critical_interval;
         ++iter) {
      append_state_to_output(e_status::critical, &critical_output, iter);
    }
  }

  std::string warning_output;
  if (_warning_check_duration) {
    auto warning_duration = std::chrono::seconds(_warning_check_duration);
    for (auto iter = stats.get_ordered_by_duration().rbegin();
         iter != stats.get_ordered_by_duration().rend() &&
         iter->last_check_duration > warning_duration;
         ++iter) {
      append_state_to_output(e_status::warning, &warning_output, iter);
    }
  }

  if (_warning_check_interval) {
    auto warning_interval = std::chrono::seconds(_warning_check_interval);
    for (auto iter = stats.get_ordered_by_interval().rbegin();
         iter != stats.get_ordered_by_interval().rend() &&
         iter->last_check_interval > warning_interval;
         ++iter) {
      append_state_to_output(e_status::warning, &warning_output, iter);
    }
  }

  unsigned max_check_interval =
      std::chrono::duration_cast<std::chrono::seconds>(
          stats.get_ordered_by_interval().rbegin()->last_check_interval)
          .count();
  unsigned max_check_duration =
      std::chrono::duration_cast<std::chrono::seconds>(
          stats.get_ordered_by_duration().rbegin()->last_check_duration)
          .count();

  auto& interval_perf = perf->emplace_back();
  interval_perf.name("interval");
  interval_perf.unit("s");
  interval_perf.value(max_check_interval);
  if (_warning_check_interval > 0) {
    interval_perf.warning_low(0);
    interval_perf.warning(_warning_check_interval);
  }
  if (_critical_check_interval > 0) {
    interval_perf.critical_low(0);
    interval_perf.critical(_critical_check_interval);
  }

  auto& duration_perf = perf->emplace_back();
  duration_perf.name("runtime");
  duration_perf.unit("s");
  duration_perf.value(max_check_duration);
  if (_warning_check_duration > 0) {
    duration_perf.warning_low(0);
    duration_perf.warning(_warning_check_duration);
  }
  if (_critical_check_duration > 0) {
    duration_perf.critical_low(0);
    duration_perf.critical(_critical_check_duration);
  }

  if (ret != e_status::ok) {
    if (!critical_output.empty()) {
      output->append(critical_output);
      if (!warning_output.empty()) {
        *output += " - ";
        output->append(warning_output);
      }
    } else if (!warning_output.empty()) {
      output->append(warning_output);
    }
    *output += " - ";
  } else {
    *output = "OK: ";
  }
  fmt::vformat_to(std::back_inserter(*output), _info_output, get_stats().size(),
                  average_runtime / get_stats().size());

  return ret;
}

void check_health::help(std::ostream& help_stream) {
  help_stream << R"(
- health params:
  - warning-interval (s): warning if a check interval is greater than this value
  - critical-interval (s): critical if a check interval is greater than this value
  - warning-runtime (s): warning if a check duration is greater than this value
  - critical-runtime (s): critical if a check duration is greater than this value
  An example of configuration:
  {
    "check": "health",
    "args": {
      "warning-runtime": 30,
      "critical-runtime": 50,
      "warning-interval": 60,
      "critical-interval": "90"
    }
  }
  Examples of output:
    CRITICAL: command2 runtime:25s interval:15s - WARNING: command1 runtime:20s interval:10s - Version: 24.11.0 - Connection mode: Poller initiated - Current configuration: 2 checks - Average runtime: 22s
  Metrics:
    runtime
    interval

)";
}
