/**
 * Copyright 2011-2013,2015,2019,2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include "com/centreon/engine/commands/connector.hh"

#include "com/centreon/common/hex_dump.hh"
#include "com/centreon/engine/commands/result.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/version.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::engine::commands;

connector_map connector::connectors;

constexpr std::string_view _query_ending("\0\0\0\0", 4);

/**
 *  Constructor.
 *
 *  @param[in] connector_name  The connector name.
 *  @param[in] connector_line  The connector command line.
 */
connector::connector(const std::string& connector_name,
                     const std::string& connector_line,
                     const std::shared_ptr<asio::io_context>& io_context,
                     const std::shared_ptr<command_listener>& listener)
    : command(connector_name, connector_line, listener, e_type::connector),
      _state(e_state::not_started),
      _logger(commands_logger),
      _io_context(io_context),
      _timeout_timer(*io_context) {
  bool enable_environment_macros = pb_config.enable_environment_macros();
  if (enable_environment_macros) {
    SPDLOG_LOGGER_WARN(runtime_logger,
                       "Warning: Connector does not enable environment macros");
  }
  SPDLOG_LOGGER_DEBUG(_logger, "create connector {} with cmdline: {}",
                      connector_name, connector_line);
}

connector::~connector() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete connector {}", get_name());
}

/**
 * @brief constructor is protected and it is mandatory to use this function to
 * create a connector
 *
 *  @param[in] connector_name  The connector name.
 *  @param[in] connector_line  The connector command line.
 * @return std::shared_ptr<connector>
 */
std::shared_ptr<connector> connector::load(
    std::string const& connector_name,
    std::string const& connector_line,
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<command_listener>& listener) {
  std::shared_ptr<connector> ret(
      new connector(connector_name, connector_line, io_context, listener));
  ret->_timeout_timer_start();
  return ret;
}

/**
 *  Run a command.
 *
 *  @param[in] args    The command arguments.
 *  @param[in] macros  The macros data struct.
 *  @param[in] timeout The command timeout.
 *
 *  @return The command id.
 */
uint64_t connector::run(const std::string& processed_cmd,
                        nagios_macros&,
                        uint32_t timeout,
                        const check_result::pointer& to_push_to_checker,
                        const notifier* caller) {
  SPDLOG_LOGGER_TRACE(_logger,
                      "connector::run: connector='{}', cmd='{}', timeout={}",
                      _name, processed_cmd, timeout);

  // Set query informations.
  uint64_t command_id(get_uniq_id());

  if (!gest_call_interval(command_id, to_push_to_checker, caller)) {
    return command_id;
  }

  timestamp start_time = timestamp::now();

  SPDLOG_LOGGER_TRACE(_logger, "connector::run: id={}", command_id);
  try {
    {
      std::lock_guard l(_lock);

      _queries.emplace(command_id, processed_cmd, start_time, timeout, false);
      if (_state == e_state::not_started) {
        _connector_start_nolock();  // check will be sent once connector started
      } else if (_state == e_state::running) {
        // Send check to the connector.
        _send_query_execute_nolock(processed_cmd, command_id, start_time,
                                   timeout);
      }
    }

    SPDLOG_LOGGER_TRACE(_logger, "connector::run: start command success: id={}",
                        command_id);
  } catch (...) {
    SPDLOG_LOGGER_TRACE(_logger, "connector::run: start command failed: id={}",
                        command_id);
    throw;
  }
  return command_id;
}

/**
 *  Run a command and wait the result.
 *
 *  @param[in]  args    The command arguments.
 *  @param[in]  macros  The macros data struct.
 *  @param[in]  timeout The command timeout.
 *  @param[out] res     The result of the command.
 */
void connector::run(const std::string& processed_cmd,
                    nagios_macros& macros,
                    uint32_t timeout,
                    result& res) {
  (void)macros;

  SPDLOG_LOGGER_TRACE(_logger,
                      "connector::run: connector='{}', cmd='{}', timeout={}",
                      _name, processed_cmd, timeout);

  // Set query informations.
  uint64_t command_id(get_uniq_id());
  timestamp start_time = timestamp::now();

  SPDLOG_LOGGER_TRACE(_logger, "connector::run: id={}", command_id);

  try {
    {
      std::lock_guard l(_lock);

      _queries.emplace(command_id, processed_cmd, start_time, timeout, false);
      if (_state == e_state::not_started) {
        _connector_start_nolock();  // check will be sent once connector started
      } else if (_state == e_state::running) {
        // Send check to the connector.
        _send_query_execute_nolock(processed_cmd, command_id, start_time,
                                   timeout);
      }
    }

    SPDLOG_LOGGER_TRACE(_logger, "connector::run: start command success: id={}",
                        command_id);
  } catch (...) {
    SPDLOG_LOGGER_TRACE(_logger, "connector::run: start command failed: id={}",
                        command_id);
    throw;
  }

  struct {
    const std::shared_ptr<connector> me;
    const uint64_t cmd_id;
    result& to_fill;

    bool operator()() const {
      auto result = me->_results.find(cmd_id);
      if (result != me->_results.end()) {
        to_fill = result->second;
        me->_results.erase(result);
        return true;
      } else {
        return false;
      }
    }

  } result_waiter = {shared_from_this(), command_id, res};

  absl::MutexLock lck(&_results_m);
  bool have_result = _results_m.AwaitWithTimeout(
      absl::Condition(&result_waiter), absl::Seconds(timeout));

  if (!have_result) {
    SPDLOG_LOGGER_ERROR(_logger, "time out to execute {}", processed_cmd);
    res.command_id = command_id;
    res.exit_code = service::state_unknown;
    res.exit_status = common::e_exit_status::timeout;
    res.start_time = start_time;
    res.end_time = timestamp::now();
    res.output = "(Process Timeout)";
  }
}

/**
 *  Set connector command line.
 *
 *  @param[in] command_line The new command line.
 */
void connector::set_command_line(const std::string& command_line) {
  // Close connector properly.
  _connector_close();

  // Change command line.
  {
    std::lock_guard l(_lock);
    command::set_command_line(command_line);
  }
}

/**
 * @brief Handles data received from the standard output stream of the
 * associated process.
 *
 * This function is called whenever new data is available on the stdout of the
 * managed process. It checks if the data originates from the expected process,
 * and process response(s).
 *
 * @param caller Shared pointer to the process instance emitting the stdout
 * data.
 * @param data   The data received from the process's stdout stream.
 */
void connector::_on_stdout_recv(
    const std::shared_ptr<common::process<true>>& caller,
    const std::string_view& data) {
  typedef void (connector::*recv_query)(const std::string_view&);
  static const std::array<recv_query, 8> tab_recv_query{
      nullptr,
      &connector::_recv_query_version,
      nullptr,
      &connector::_recv_query_execute,
      nullptr,
      &connector::_recv_query_quit,
      &connector::_recv_query_error,
      nullptr};

  try {
    SPDLOG_LOGGER_TRACE(
        _logger, "{} received: {}", get_name(),
        common::hex_dump((const unsigned char*)data.data(), data.size(), 16));

    // Split output into queries responses.
    std::list<std::string> responses;
    {
      std::lock_guard l(_lock);
      if (caller != _process) {
        return;
      }
      _data_available.append(data);
      size_t end_search = _data_available.find(_query_ending);
      while (end_search != std::string::npos) {
        responses.emplace_back(_data_available.substr(0, end_search));
        _data_available.erase(0, end_search + _query_ending.size());
        end_search = _data_available.find(_query_ending);
      }
    }
    SPDLOG_LOGGER_TRACE(_logger,
                        "connector::data_is_available: responses.size={}",
                        responses.size());

    // Parse queries responses.
    for (std::string_view str : responses) {
      auto first_sep = str.find('\0');
      if (first_sep == std::string::npos) {
        SPDLOG_LOGGER_ERROR(_logger, "bad response received from connector {}",
                            get_name());
        continue;
      }
      uint32_t id;
      if (!absl::SimpleAtoi(str.substr(0, first_sep), &id)) {
        SPDLOG_LOGGER_ERROR(
            _logger, "bad response id received from connector {}", get_name());
        continue;
      }
      SPDLOG_LOGGER_TRACE(_logger,
                          "connector::data_is_available: request id={}", id);

      if (id >= tab_recv_query.size() || !tab_recv_query[id]) {
        SPDLOG_LOGGER_ERROR(_logger, "unknown query type: {}", id);
        continue;
      }
      if (first_sep >= str.size()) {
        SPDLOG_LOGGER_ERROR(_logger,
                            "no response data received from connector {} for "
                            "response of type {}",
                            get_name(), id);
        continue;
      }
      (this->*tab_recv_query[id])(str.substr(first_sep + 1));
    }
  } catch (std::exception const& e) {
    runtime_logger->warn("Warning: Connector '{}' error: {}", _name, e.what());
  }
}

/**
 * @brief Handles data received from the standard error stream of the associated
 * process.
 *
 * This function is called whenever new data is available on the standard error
 * (stderr) output of the managed process. It checks if the data originates from
 * the expected process, and logs the error message using the configured logger.
 *
 * @param caller Shared pointer to the process instance emitting the stderr
 * data.
 * @param data   The data received from the process's standard error stream.
 */
void connector::_on_stderr_recv(
    const std::shared_ptr<common::process<true>>& caller,
    const std::string_view& data) {
  std::lock_guard l(_lock);
  if (caller == _process) {
    SPDLOG_LOGGER_ERROR(_logger, "connector error:{}", data);
  }
}

/**
 *  Called at the end of the process execution. It checks if the event
 *  originates from the expected process.
 *
 *  @param[in] caller Shared pointer to the ended process instance
 */
void connector::_on_process_end(const common::process<true>& caller) {
  try {
    std::lock_guard l(_lock);
    SPDLOG_LOGGER_TRACE(_logger, "end of connector process {} process: {}",
                        get_name(), caller.get_pid());
    if (&caller != _process.get()) {
      return;
    }

    SPDLOG_LOGGER_TRACE(_logger, "end of connector process {}", get_name());
    _state = e_state::not_started;
    _process.reset();
    _connector_start_nolock();
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "Error: Connector '{}' termination routine failed: {}",
                        get_name(), e.what());
  }
}

/**
 * @brief Stop connector.
 */
void connector::stop_connector() {
  try {
    _connector_close();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "Error: could not stop connector properly: {}",
                        e.what());
  }
}

/**
 *  Close connection with the process.
 */
void connector::_connector_close() {
  std::lock_guard l(_lock);

  _timeout_timer.cancel();
  // Exit if connector is not running.
  if (_state == e_state::not_started)
    return;

  SPDLOG_LOGGER_TRACE(_logger, "_connector_close {}: process={}", get_name(),
                      _process->get_pid());

  // Ask connector to quit properly.
  _send_query_quit_nolock();
  _process->kill();
  _process.reset();
  _state = e_state::not_started;
}

/**
 *  Start connection with the process.
 */
void connector::_connector_start_nolock() {
  if (_state != e_state::not_started)
    return;

  _state = e_state::starting;

  common::process<true>::shared_env no_env;
  try {
    _process = std::make_shared<common::process<true>>(
        _io_context, _logger, get_command_line(), true, true, no_env);

    _state = e_state::starting;
    _process->start_process(
        [me = shared_from_this()](const common::process<true>& proc,
                                  int /*exit_code*/, int, /*exit status*/
                                  const std::string& /*stdout*/,
                                  const std::string& /*stderr*/
        ) { me->_on_process_end(proc); },
        [me = shared_from_this(), proc = _process->weak_from_this()](
            const boost::system::error_code& err,
            const std::string_view& received) {
          if (!err) {
            auto sub_process = proc.lock();
            if (sub_process) {
              me->_on_stdout_recv(
                  std::static_pointer_cast<common::process<true>>(sub_process),
                  received);
            }
          }
        },
        [me = shared_from_this(), proc = _process->weak_from_this()](
            const boost::system::error_code& err,
            const std::string_view& received) {
          if (!err) {
            auto sub_process = proc.lock();
            if (sub_process) {
              me->_on_stderr_recv(
                  std::static_pointer_cast<common::process<true>>(sub_process),
                  received);
            }
          }
        },
        {

        });

    SPDLOG_LOGGER_TRACE(_logger, "connector {} process {} started", get_name(),
                        _process->get_pid());
    _send_query_version_nolock();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to start connector {} : {}", get_name(),
                        e.what());
    _process.reset();
    _state = e_state::not_started;
    throw;
  }

  runtime_logger->info("Connector '{}' has started", _name);
}

/**
 *  Receive an error from the connector.
 *
 *  @param[in] data  The query to parse.
 */
void connector::_recv_query_error(const std::string_view& data) {
  try {
    SPDLOG_LOGGER_TRACE(_logger, "connector::_recv_query_error");
    auto code_mess_sep = data.find('\0');
    if (code_mess_sep == std::string_view::npos) {
      throw exceptions::msg_fmt("invalid error message format");
    }
    unsigned code;
    if (!absl::SimpleAtoi(data.substr(0, code_mess_sep), &code) || code > 2) {
      throw exceptions::msg_fmt("invalid code value from error message: {}",
                                data);
    }

    switch (code) {
        // Information message.
      case 0:
        SPDLOG_LOGGER_INFO(_logger, "Info: Connector '{}': {}", _name,
                           data.substr(code_mess_sep + 1));
        break;
        // Warning message.
      case 1:
        SPDLOG_LOGGER_WARN(_logger, "Warning: Connector '{}': {}", _name,
                           data.substr(code_mess_sep + 1));
        break;
        // Error message.
      case 2:
        SPDLOG_LOGGER_ERROR(_logger, "Error: Connector '{}': {}", _name,
                            data.substr(code_mess_sep + 1));
        break;
    }
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(_logger, "Warning: Connector '{}': {}", _name,
                        e.what());
  }
}

/**
 *  Receive response to the query execute.
 *
 *  @param[in] data  The query to parse.
 */
void connector::_recv_query_execute(const std::string_view& data) {
  try {
    SPDLOG_LOGGER_TRACE(_logger, "connector::_recv_query_execute");
    // Get query informations.
    auto fields = absl::StrSplit(data, '\0');
    if (std::distance(fields.begin(), fields.end()) < 5) {
      throw exceptions::msg_fmt("no enough fields received: {}", data);
    }
    auto field_iter = fields.begin();
    uint64_t command_id;
    if (!absl::SimpleAtoi(*field_iter, &command_id)) {
      throw exceptions::msg_fmt("invalid command_id: {}", *field_iter);
    }
    ++field_iter;
    bool is_executed = (*field_iter == "1");
    ++field_iter;
    int exit_code;
    if (!absl::SimpleAtoi(*field_iter, &exit_code)) {
      throw exceptions::msg_fmt("invalid exit_code: {}", *field_iter);
    }
    ++field_iter;
    auto std_err = *field_iter;
    ++field_iter;
    auto std_out = *field_iter;
    SPDLOG_LOGGER_TRACE(_logger, "connector::_recv_query_execute: id={}",
                        command_id);
    query_info info;
    {
      std::lock_guard l(_lock);

      // Get query information with the command_id.
      auto& command_id_index = _queries.get<0>();
      auto it = command_id_index.find(command_id);
      if (it == command_id_index.end()) {
        SPDLOG_LOGGER_ERROR(
            _logger, "recv query failed: command_id({}) not found into queries",
            command_id);
        return;
      }
      // Get data.
      info = *it;
      // Remove query from queries.
      _queries.erase(it);
    }

    // Initialize result.
    result res;
    res.command_id = command_id;
    res.end_time = timestamp::now();
    res.exit_code = service::state_unknown;
    res.exit_status = common::e_exit_status::normal;
    res.start_time = info.start_time;

    uint32_t execution_time((res.end_time - res.start_time).to_mseconds());

    // Check if the check timeout.
    if (info.timeout > 0 && execution_time > info.timeout * 1000) {
      res.exit_status = common::e_exit_status::timeout;
      res.output = "(Process Timeout)";
    }
    // The check result was properly returned.
    else {
      if (exit_code < 0 || exit_code > 3)
        res.exit_code = service::state_unknown;
      else
        res.exit_code = exit_code;
      res.output = (is_executed ? std_out : std_err);
    }

    if (res.exit_status == common::e_exit_status::normal) {
      SPDLOG_LOGGER_TRACE(_logger,
                          "connector::_recv_query_execute: "
                          "id={}, {}",
                          command_id, res);
    }

    if (res.exit_status == common::e_exit_status::timeout) {
      SPDLOG_LOGGER_ERROR(_logger,
                          "connector::_recv_query_execute timeout of: "
                          "id={}, {}",
                          command_id, res);
    }

    update_result_cache(command_id, res);

    if (!info.waiting_result) {
      // Forward result to the listener.
      if (_listener)
        (_listener->finished)(res);
    } else {
      absl::MutexLock l(&_results_m);
      // Push result into list of results.
      _results[command_id] = res;
    }
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(_logger, "Connector '{}': {}", _name, e.what());
  }
}

/**
 *  Receive response to the query quit.
 *
 */
void connector::_recv_query_quit(const std::string_view&) {
  SPDLOG_LOGGER_TRACE(_logger, "connector::_recv_query_quit");
  std::lock_guard l(_lock);
  _process.reset();
  _state = e_state::not_started;
}

/**
 *  Receive response to the query version.
 *
 *  @param[in] data  Has version of engine to use with the connector.
 */
void connector::_recv_query_version(const std::string_view& data) {
  SPDLOG_LOGGER_TRACE(_logger, "connector::_recv_query_version");
  try {
    // Parse query version response to get major and minor
    // engine version supported by the connector.
    unsigned major, minor;
    auto major_minor_sep = data.find('\0');
    if (major_minor_sep == std::string_view::npos) {
      throw exceptions::msg_fmt(
          "bad version format receive from connector => kill connector");
    }

    if (!absl::SimpleAtoi(data.substr(0, major_minor_sep), &major)) {
      throw exceptions::msg_fmt("bad major version received from connector: {}",
                                data);
    }

    if (!absl::SimpleAtoi(data.substr(major_minor_sep + 1), &minor)) {
      throw exceptions::msg_fmt("bad minor version received from connector: {}",
                                data);
    }

    SPDLOG_LOGGER_TRACE(_logger,
                        "connector::_recv_query_version: "
                        "major={}, minor={}",
                        major, minor);
    // Check the version.
    if (major < CENTREON_ENGINE_VERSION_MAJOR ||
        (major == CENTREON_ENGINE_VERSION_MAJOR &&
         minor <= CENTREON_ENGINE_VERSION_MINOR)) {
      std::lock_guard l(_lock);
      _state = e_state::running;
      SPDLOG_LOGGER_TRACE(
          _logger, "connector::_connector_start: send queries: queries.size={}",
          _queries.size());
      // Resend commands.
      for (const auto& query : _queries) {
        _send_query_execute_nolock(query.processed_cmd, query.command_id,
                                   query.start_time, query.timeout);
      }
    } else {
      SPDLOG_LOGGER_ERROR(_logger,
                          "incompatible version of connector {}: {}:{}",
                          get_name(), major, minor);
      std::lock_guard l(_lock);
      _process.reset();
      _state = e_state::not_started;
    }
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(_logger, "Connector '{}': {}", _name, e.what());
    std::lock_guard l(_lock);
    _process.reset();
    _state = e_state::not_started;
  }
}

/**
 *  Send query execute. To ask connector to execute.
 *
 *  @param[in]  cmdline     The command to execute.
 *  @param[in]  command_id  The command id.
 *  @param[in]  start       The start time.
 *  @param[in]  timeout     The timeout.
 */
void connector::_send_query_execute_nolock(const std::string& cmdline,
                                           uint64_t command_id,
                                           timestamp const& start,
                                           uint32_t timeout) {
  SPDLOG_LOGGER_TRACE(_logger,
                      "connector::_send_query_execute: "
                      "id={}, "
                      "cmd='{}', "
                      "start={}, "
                      "timeout={}",
                      command_id, cmdline, start.to_seconds(), timeout);
  std::ostringstream oss;
  oss << "2" << '\0' << command_id << '\0' << timeout << '\0'
      << start.to_seconds() << '\0' << cmdline << _query_ending;

  _process->write_to_child_stdin(oss.str());
}

/**
 *  Send query quit. To ask connector to quit properly.
 */
void connector::_send_query_quit_nolock() {
  SPDLOG_LOGGER_TRACE(_logger, "connector::_send_query_quit");
  std::string query("4\0", 2);
  _process->write_to_child_stdin(query);
}

/**
 *  Send query verion. To ask connector version.
 */
void connector::_send_query_version_nolock() {
  SPDLOG_LOGGER_TRACE(_logger, "connector::_send_query_version");
  std::string query("0");
  query.append(_query_ending);
  _process->write_to_child_stdin(query);
}

/**
 * @brief start timeout timer that will expire in one second
 *
 */
void connector::_timeout_timer_start() {
  std::lock_guard l(_lock);
  _timeout_timer.expires_after(std::chrono::seconds(1));
  _timeout_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        if (!err) {
          me->_timeout_timer_handler();
          me->_timeout_timer_start();
        }
      });
}

/**
 * @brief this handler called every second will check expired commands and then
 * generate a timeout result
 * expired commands are also removed from _queries queue so their results will
 * be ignored
 *
 */
void connector::_timeout_timer_handler() {
  // check time out for commands
  timestamp now = timestamp::now();
  std::vector<result> results;
  {
    std::lock_guard l(_lock);
    if (_queries.empty()) {
      return;
    }
    auto& timeout_index = _queries.get<1>();
    for (auto query_iter = timeout_index.begin();
         !_queries.empty() && query_iter != timeout_index.end() &&
         query_iter->abs_timeout <= now;) {
      if (query_iter->waiting_result) {
        ++query_iter;
      } else {
        results.emplace_back(query_iter->command_id, now,
                             service::state_unknown,
                             common::e_exit_status::timeout,
                             query_iter->start_time, "(Process Timeout)");
        query_iter = timeout_index.erase(query_iter);
      }
    }
  }
  for (const auto& res : results) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "time out of "
                        "id={}, {}",
                        res.command_id, res);

    update_result_cache(res.command_id, res);

    // Forward result to the listener.
    if (_listener)
      (_listener->finished)(res);
  }
}

/**
 *  Dump connector content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The connector to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, connector const& obj) {
  os << "connector {\n"
        "  name:         "
     << obj.get_name()
     << "\n"
        "  command_line: "
     << obj.get_command_line()
     << "\n"
        "}\n";
  return os;
}
