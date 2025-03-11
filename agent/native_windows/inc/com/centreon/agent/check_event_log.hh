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

#ifndef CENTREON_AGENT_CHECK_EVENT_LOG_HH
#define CENTREON_AGENT_CHECK_EVENT_LOG_HH

#include "check.hh"
#include "event_log/container.hh"
#include "event_log/data.hh"
#include "event_log/uniq.hh"

namespace com::centreon::agent {

/**
 * @brief Check for event log.
 *
 * This class is responsible for checking the event log.
 * events are stored in container class. The main job of this class is to format
 * output
 */
class check_event_log : public check {
  std::unique_ptr<event_log::event_container> _data;
  std::unique_ptr<event_log::event_comparator> _event_compare;

  unsigned _warning_threshold;
  unsigned _critical_threshold;
  std::string _empty_output;
  std::string _ok_syntax;
  std::string _event_detail_syntax;
  bool _need_message_content;
  bool _verbose;
  std::string _output_syntax;

  void _calc_event_detail_syntax(const std::string_view& param);
  void _calc_output_format();

  template <bool need_written_str>
  std::string print_event_detail(const std::string& file,
                                 e_status status,
                                 const event_log::event& evt) const;

 public:
  check_event_log(const std::shared_ptr<asio::io_context>& io_context,
                  const std::shared_ptr<spdlog::logger>& logger,
                  time_point first_start_expected,
                  duration check_interval,
                  const std::string& serv,
                  const std::string& cmd_name,
                  const std::string& cmd_line,
                  const rapidjson::Value& args,
                  const engine_to_agent_request_ptr& cnf,
                  check::completion_handler&& handler,
                  const checks_statistics::pointer& stat);

  static std::shared_ptr<check_event_log> load(
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
      const checks_statistics::pointer& stat);

  static void help(std::ostream& help_stream);

  void start_check(const duration& timeout) override;

  e_status compute(event_log::event_container& data,
                   std::string* output,
                   std::list<common::perfdata>* perfs);
};

}  // namespace com::centreon::agent

#endif
