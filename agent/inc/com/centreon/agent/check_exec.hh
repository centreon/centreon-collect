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

#ifndef CENTREON_AGENT_CHECK_EXEC_HH
#define CENTREON_AGENT_CHECK_EXEC_HH

#include "check.hh"
#include "com/centreon/common/process/process.hh"

namespace com::centreon::agent {

class check_exec;

namespace detail {

/**
 * @brief This class is used by check_exec class to execute plugins
 * It calls check_exec::on_completion once process is ended AND we have received
 * an eof on stdout pipe
 * stderr pipe is not read as plugins should not use it
 * As we are in asynchronous world, running index is carried until completion to
 * ensure that completion is called for the right process and not for the
 * previous one
 */
class process : public common::process<false> {
  bool _process_ended;
  bool _stdout_eof;
  std::string _stdout;
  unsigned _running_index;
  std::weak_ptr<check_exec> _parent;

  void _on_completion();

 public:
  process(const std::shared_ptr<asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const std::string& cmd_line,
          const std::shared_ptr<check_exec>& parent);

  void start(unsigned running_index);

  void kill() { common::process<false>::kill(); }

  int get_exit_status() const {
    return common::process<false>::get_exit_status();
  }

  const std::string& get_stdout() const { return _stdout; }

 protected:
  void on_stdout_read(const boost::system::error_code& err,
                      size_t nb_read) override;
  void on_stderr_read(const boost::system::error_code& err,
                      size_t nb_read) override;

  void on_process_end(const boost::system::error_code& err,
                      int raw_exit_status) override;
};

}  // namespace detail

/**
 * @brief check that executes a process (plugins)
 *
 */
class check_exec : public check {
  std::shared_ptr<detail::process> _process;

 protected:
  using check::completion_handler;

  void _on_timeout() override;

  void _init();

 public:
  check_exec(const std::shared_ptr<asio::io_context>& io_context,
             const std::shared_ptr<spdlog::logger>& logger,
             time_point first_start_expected,
             duration inter_check_delay,
             duration check_interval,
             const std::string& serv,
             const std::string& cmd_name,
             const std::string& cmd_line,
             const engine_to_agent_request_ptr& cnf,
             check::completion_handler&& handler,
             const checks_statistics::pointer& stat);

  static std::shared_ptr<check_exec> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      time_point first_start_expected,
      duration inter_check_delay,
      duration check_interval,
      const std::string& serv,
      const std::string& cmd_name,
      const std::string& cmd_line,
      const engine_to_agent_request_ptr& cnf,
      check::completion_handler&& handler,
      const checks_statistics::pointer& stat);

  void start_check(const duration& timeout) override;

  int get_pid() const;

  void on_completion(unsigned running_index);
};

/**
 * @class check_dummy
 * @brief A class derived from check that is used to send an error message.
 *
 * This class represents a dummy check that sends a predefined error message.
 * It inherits from the check class and overrides the start_check method.
 *
 * @param io_context Shared pointer to the ASIO io_context.
 * @param logger Shared pointer to the spdlog logger.
 * @param first_start_expected The expected start time of the first check.
 * @param check_interval The interval between checks.
 * @param serv The service name.
 * @param cmd_name The command name.
 * @param cmd_line The command line.
 * @param output The predefined error message to be sent.
 * @param cnf Configuration for the engine to agent request.
 * @param handler Completion handler for the check.
 * @param stat Pointer to the checks statistics.
 */
class check_dummy : public check {
  std::string _output;

 protected:
  using check::completion_handler;

 public:
  check_dummy(const std::shared_ptr<asio::io_context>& io_context,
              const std::shared_ptr<spdlog::logger>& logger,
              time_point first_start_expected,
              duration inter_check_delay,
              duration check_interval,
              const std::string& serv,
              const std::string& cmd_name,
              const std::string& cmd_line,
              const std::string& output,
              const engine_to_agent_request_ptr& cnf,
              check::completion_handler&& handler,
              const checks_statistics::pointer& stat);

  static std::shared_ptr<check_dummy> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger,
      time_point first_start_expected,
      duration inter_check_delay,
      duration check_interval,
      const std::string& serv,
      const std::string& cmd_name,
      const std::string& cmd_line,
      const std::string& output,
      const engine_to_agent_request_ptr& cnf,
      check::completion_handler&& handler,
      const checks_statistics::pointer& stat);

  void start_check(const duration& timeout) override;

  std::string get_output() { return _output; }
};
}  // namespace com::centreon::agent

#endif
