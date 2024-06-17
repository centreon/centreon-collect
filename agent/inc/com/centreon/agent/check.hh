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

#ifndef CENTREON_AGENT_CHECK_HH
#define CENTREON_AGENT_CHECK_HH

#include "agent.pb.h"
#include "com/centreon/common/perfdata.hh"

namespace com::centreon::agent {

using engine_to_agent_request_ptr =
    std::shared_ptr<com::centreon::agent::EngineToAgent>;

using time_point = std::chrono::system_clock::time_point;
using duration = std::chrono::system_clock::duration;

/**
 * @brief base class for check
 * start_expected is set by scheduler and increased by check_period on each
 * check
 *
 */
class check : public std::enable_shared_from_this<check> {
 public:
  using completion_handler = std::function<void(
      const std::shared_ptr<check>& caller,
      int status,
      const std::list<com::centreon::common::perfdata>& perfdata,
      const std::list<std::string>& outputs)>;

 private:
  time_point _start_expected;
  const std::string& _host;
  const std::string& _service;
  const std::string& _command_name;
  const std::string& _command_line;
  // by owning a reference to the original request, we can get only reference to
  // host, service and command_line
  engine_to_agent_request_ptr _conf;

  asio::system_timer _time_out_timer;

  void _start_timeout_timer(const duration& timeout);

  bool _running_check = false;
  unsigned _running_check_index = 0;
  completion_handler _completion_handler;

 protected:
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  unsigned _get_running_check_index() const { return _running_check_index; }
  const completion_handler& _get_completion_handler() const {
    return _completion_handler;
  }

  virtual void _timeout_timer_handler(const boost::system::error_code& err,
                                      unsigned start_check_index);

 public:
  using pointer = std::shared_ptr<check>;

  template <typename handler_type>
  check(const std::shared_ptr<asio::io_context>& io_context,
        const std::shared_ptr<spdlog::logger>& logger,
        time_point exp,
        const std::string& hst,
        const std::string& serv,
        const std::string& command_name,
        const std::string& cmd_line,
        const engine_to_agent_request_ptr& cnf,
        handler_type&& handler)
      : _start_expected(exp),
        _host(hst),
        _service(serv),
        _command_name(command_name),
        _command_line(cmd_line),
        _conf(cnf),
        _io_context(io_context),
        _logger(logger),
        _time_out_timer(*io_context),
        _completion_handler(handler) {}

  virtual ~check() = default;

  struct pointer_compare {
    bool operator()(const check::pointer& left,
                    const check::pointer& right) const {
      return left->_start_expected < right->_start_expected;
    }
  };

  void add_duration_to_start_expected(const duration& to_add);

  time_point get_start_expected() const { return _start_expected; }

  const std::string& get_host() const { return _host; }

  const std::string& get_service() const { return _service; }

  const std::string& get_command_name() const { return _command_name; }

  const std::string& get_command_line() const { return _command_line; }

  const engine_to_agent_request_ptr& get_conf() const { return _conf; }

  void on_completion(unsigned start_check_index,
                     unsigned status,
                     const std::list<com::centreon::common::perfdata>& perfdata,
                     const std::list<std::string>& outputs);

  virtual void start_check(const duration& timeout);
};

}  // namespace com::centreon::agent

#endif
