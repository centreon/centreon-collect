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

#ifndef CENTREON_AGENT_CHECK_UPTIME_HH
#define CENTREON_AGENT_CHECK_UPTIME_HH

#include "check.hh"

namespace com::centreon::agent {

/**
 * @brief check uptime
 *
 */
class check_uptime : public check {
  unsigned _second_warning_threshold;
  unsigned _second_critical_threshold;

 public:
  check_uptime(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               time_point first_start_expected,
               duration inter_check_delay,
               duration check_interval,
               const std::string& serv,
               const std::string& cmd_name,
               const std::string& cmd_line,
               const rapidjson::Value& args,
               const engine_to_agent_request_ptr& cnf,
               check::completion_handler&& handler,
               const checks_statistics::pointer& stat);

  static void help(std::ostream& help_stream);

  void start_check(const duration& timeout) override;

  e_status compute(uint64_t ms_uptime,
                   std::string* output,
                   common::perfdata* perfs);
};
}  // namespace com::centreon::agent
#endif