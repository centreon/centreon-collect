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

#include <windows.h>

#include "check_uptime.hh"

#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::agent;

static const absl::flat_hash_map<std::string_view, unsigned> _unit_multiplier =
    {{"m", 60},    {"minute", 60}, {"h", 3600},   {"hour", 3600},
     {"d", 86400}, {"day", 86400}, {"w", 604800}, {"week", 604800}};

/**
 * @brief Construct a new check uptime::check uptime object
 *
 * @param io_context
 * @param logger
 * @param first_start_expected
 * @param serv
 * @param args
 * @param cnf
 * @param handler
 */
check_uptime::check_uptime(const std::shared_ptr<asio::io_context>& io_context,
                           const std::shared_ptr<spdlog::logger>& logger,
                           time_point first_start_expected,
                           const Service& serv,
                           const rapidjson::Value& args,
                           const engine_to_agent_request_ptr& cnf,
                           check::completion_handler&& handler,
                           const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            serv,
            cnf,
            std::move(handler),
            stat) {
  com::centreon::common::rapidjson_helper arg(args);
  try {
    if (args.IsObject()) {
      // the default value "", disable the threshold
      _warning_threshold.extract_range(
          arg.get_string_or_int_as_string("warning-uptime", ""));
      _critical_threshold.extract_range(
          arg.get_string_or_int_as_string("critical-uptime", ""));

      if (!_warning_threshold.is_valid() || !_critical_threshold.is_valid()) {
        throw std::runtime_error(
            "check uptime, invalid warning-uptime or critical-uptime "
            "range");
      }

      // the number of warning/critical will always be positive or zero
      // if the low threshold is not set, we take the default value
      _warning_threshold.set_default_low(0);
      _critical_threshold.set_default_low(0);

      std::string unit = arg.get_string("unit", "s");
      boost::to_lower(unit);
      auto multiplier = _unit_multiplier.find(unit);

      if (multiplier != _unit_multiplier.end()) {
        // apply unit multiplier to thresholds , if the threshold is disabled,
        // no effect
        _warning_threshold.unit_multiplier(multiplier->second);
        _critical_threshold.unit_multiplier(multiplier->second);
      }
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_uptime, fail to parse arguments: {}",
                        e.what());
    throw;
  }
}

/**
 * @brief get uptime with GetTickCount64
 *
 * @param timeout unused
 */
void check_uptime::start_check([[maybe_unused]] const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }
  std::string output;
  common::perfdata perf;
  e_status status = compute(GetTickCount64(), &output, &perf);

  asio::post(
      *_io_context, [me = shared_from_this(), this, out = std::move(output),
                     status, performance = std::move(perf)]() {
        on_completion(_get_running_check_index(), status, {performance}, {out});
      });
}

/**
 * @brief calculate status, output and perfdata from uptime
 *
 * @param ms_uptime
 * @param output
 * @param perfs
 * @return e_status
 */
e_status check_uptime::compute(uint64_t ms_uptime,
                               std::string* output,
                               common::perfdata* perf) {
  uint64_t uptime = ms_uptime / 1000;
  uint64_t uptime_bis = uptime;

  std::string sz_uptime;
  if (uptime > 86400) {
    sz_uptime = fmt::format("{}d ", uptime / 86400);
    uptime %= 86400;
  }
  if (uptime > 3600 || !sz_uptime.empty()) {
    absl::StrAppend(&sz_uptime, uptime / 3600, "h ");
    uptime %= 3600;
  }
  if (uptime > 60 || !sz_uptime.empty()) {
    absl::StrAppend(&sz_uptime, uptime / 60, "m ");
    uptime %= 60;
  }
  absl::StrAppend(&sz_uptime, uptime, "s");

  using namespace std::literals;
  e_status status = e_status::ok;
  if (_critical_threshold.is_triggered(uptime_bis)) {
    *output = "CRITICAL: System uptime is: " + sz_uptime;
    status = e_status::critical;
  } else if (_warning_threshold.is_triggered(uptime_bis)) {
    *output = "WARNING: System uptime is: " + sz_uptime;
    status = e_status::warning;
  } else {
    *output = "OK: System uptime is: " + sz_uptime;
  }

  perf->name("uptime"sv);
  perf->unit("s");
  perf->value(uptime_bis);
  perf->min(0);
  _critical_threshold.set_pref_details_c(*perf);
  _warning_threshold.set_pref_details_w(*perf);

  return status;
}

void check_uptime::help(std::ostream& help_stream) {
  help_stream <<
      R"(
- uptime  params:" 
    unit (defaults s): can be s, second, m, minute, h, hour, d, day, w, week
    warning-uptime: warning threshold
    critical-uptime: critical threshold

 An example of configuration:
  {
    "check": "uptime",
    "args": {
      "unit": "day",
      "warning-uptime": "1:",
      "critical-uptime": "2:"
    }
  }
  Examples of output:
    OK: System uptime is: 5d 1h 1m 1s
    CRITICAL: System uptime is: 1d 4h 0m 0s
  Metrics:
    uptime
)";
}
