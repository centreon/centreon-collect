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

#ifndef CENTREON_AGENT_HEALTH_CHECK_HH
#define CENTREON_AGENT_HEALTH_CHECK_HH

#include "check.hh"

namespace com::centreon::agent {

class check_health : public check {
  unsigned _warning_check_interval;
  unsigned _critical_check_interval;
  unsigned _warning_check_duration;
  unsigned _critical_check_duration;

  std::string _info_output;

  // we use this timer to delay measure in order to have some checks yet done
  // when we will compute the first statistics
  asio::system_timer _measure_timer;

  void _measure_timer_handler(const boost::system::error_code& err,
                              unsigned start_check_index);

 public:
  check_health(const std::shared_ptr<asio::io_context>& io_context,
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

  e_status compute(std::string* output, std::list<common::perfdata>* perfs);
};

}  // namespace com::centreon::agent

#endif  // CENTREON_AGENT_HEALTH_CHECK_HH
