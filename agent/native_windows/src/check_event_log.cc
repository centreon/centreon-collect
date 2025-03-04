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

#include "check_event_log.hh"
#include <__msvc_chrono.hpp>
#include "check.hh"
#include "event_log/data.hh"

#include "com/centreon/common/rapidjson_helper.hh"
#include "event_log/uniq.hh"

using namespace com::centreon::agent;

check_event_log::check_event_log(
    const std::shared_ptr<asio::io_context>& io_context,
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
            stat) {
  com::centreon::common::rapidjson_helper arg(args);
  try {
    if (args.IsObject()) {
      duration scan_range =
          duration_from_string(arg.get_string("scan-range", ""), 's', true);

      if (scan_range.count() == 0) {  // default: 24h
        scan_range = std::chrono::days(1);
      }

      _data = std::make_unique<event_log::event_container>(
          arg.get_string("file"),
          arg.get_string("unique-index", "${provider}${id}"),
          arg.get_string(
              "filter-event",
              "written > -60m and level in ('error', 'warning', 'critical')"),
          arg.get_string("warning-status", "level = 'warning'"),
          arg.get_string("critical-status", "level in ('error', 'critical')"),
          scan_range, logger);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_event_log, fail to parse arguments: {}",
                        e.what());
    throw;
  }
}

/**
 * @brief get uptime with GetTickCount64
 *
 * @param timeout unused
 */
void check_event_log::start_check([[maybe_unused]] const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }
  std::string output;
  common::perfdata perf;
  e_status status = compute(GetTickCount64(), &output, &perf);

  _io_context->post([me = shared_from_this(), this, out = std::move(output),
                     status, performance = std::move(perf)]() {
    on_completion(_get_running_check_index(), status, {performance}, {out});
  });
}

e_status check_event_log::compute(uint64_t ms_uptime,
                                  std::string* output,
                                  com::centreon::common::perfdata* perf) {
  return e_status::ok;
}
