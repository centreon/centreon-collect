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

#include "check.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::agent;

/**
 * @brief calc a duration from a string like 3w or 2d1h5m30s
 * allowed units are s, m, h , d, w no case sensitive
 * @param duration_str  string to parse
 * @param default_unit  when a number is given without unit we use default_unit
 * @param erase_sign if true, number signs are not taken into account
 */
duration com::centreon::agent::duration_from_string(
    const std::string_view& duration_str,
    char default_unit,
    bool erase_sign) {
  static re2::RE2 duration_regex("(-?\\d+[sSmMhHdDwW]?)");

  duration ret{0};
  std::string_view captured;
  std::string_view copy_str = duration_str;
  while (RE2::Consume(&copy_str, duration_regex, &captured)) {
    char unit = default_unit;
    if (*captured.rbegin() > '9') {
      unit = *captured.rbegin();
      captured = captured.substr(0, captured.size() - 1);
    }

    int value = 0;
    if (!absl::SimpleAtoi(captured, &value)) {
      throw exceptions::msg_fmt("fail to parse this duration:{}", duration_str);
    }
    if (erase_sign && value < 0) {
      value = -value;
    }
    switch (unit) {
      case 's':
      case 'S':
        ret += std::chrono::seconds(value);
        break;
      case 'm':
      case 'M':
        ret += std::chrono::minutes(value);
        break;
      case 'h':
      case 'H':
        ret += std::chrono::hours(value);
        break;
      case 'd':
      case 'D':
        ret += std::chrono::days(value);
        break;
      case 'w':
      case 'W':
        ret += std::chrono::weeks(value);
        break;
    }
  }
  return ret;
}

/**
 * @brief update check interval of a check
 *
 * @param cmd_name name of command (entered by user in centreon UI)
 * @param last_check_interval
 */
void checks_statistics::add_interval_stat(const std::string& cmd_name,
                                          const duration& last_check_interval) {
  auto it = _stats.find(cmd_name);
  if (it == _stats.end()) {
    _stats.insert({cmd_name, last_check_interval, {}});
  } else {
    _stats.get<0>().modify(it, [last_check_interval](check_stat& it) {
      it.last_check_interval = last_check_interval;
    });
  }
}

/**
 * @brief update check duration of a check
 *
 * @param cmd_name name of command (entered by user in centreon UI)
 * @param last_check_duration
 */
void checks_statistics::add_duration_stat(const std::string& cmd_name,
                                          const duration& last_check_duration) {
  auto it = _stats.find(cmd_name);
  if (it == _stats.end()) {
    _stats.insert({cmd_name, {}, last_check_duration});
  } else {
    _stats.get<0>().modify(it, [last_check_duration](check_stat& it) {
      it.last_check_duration = last_check_duration;
    });
  }
}

const std::array<std::string_view, 4> check::status_label = {
    "OK", "WARNING", "CRITICAL", "UNKNOWN"};

/**
 * @brief Construct a new check::check object
 *
 * @param io_context
 * @param logger
 * @param first_start_expected start expected
 * @param check_interval check interval between two checks (not only this but
 * also others)
 * @param serv
 * @param command_name
 * @param cmd_line
 * @param cnf
 * @param handler
 */
check::check(const std::shared_ptr<asio::io_context>& io_context,
             const std::shared_ptr<spdlog::logger>& logger,
             time_point first_start_expected,
             duration check_interval,
             const std::string& serv,
             const std::string& command_name,
             const std::string& cmd_line,
             const engine_to_agent_request_ptr& cnf,
             completion_handler&& handler,
             const checks_statistics::pointer& stat)
    : _start_expected(first_start_expected, check_interval),
      _service(serv),
      _command_name(command_name),
      _command_line(cmd_line),
      _conf(cnf),
      _time_out_timer(*io_context),
      _completion_handler(handler),
      _stat(stat),
      _io_context(io_context),
      _logger(logger) {}

/**
 * @brief start timeout timer and init some flags used by timeout and completion
 * must be called first by daughter check class
 * @code {.c++}
 * void my_check::start_check(const duration & timeout) {
 *    if (!_start_check(timeout))
 *       return;
 *    ....do job....
 * }
 * @endcode
 *
 *
 * @param timeout
 * @return true if check can be done, false otherwise
 */
bool check::_start_check(const duration& timeout) {
  if (_running_check) {
    SPDLOG_LOGGER_ERROR(_logger, "check for service {} is already running",
                        _service);
    asio::post(*_io_context,
               [me = shared_from_this(), to_call = _completion_handler]() {
                 to_call(me, e_status::unknown,
                         std::list<com::centreon::common::perfdata>(),
                         {"a check is already running"});
               });
    return false;
  }
  _running_check = true;
  _start_timeout_timer(timeout);
  SPDLOG_LOGGER_TRACE(_logger, "start check for service {}", _service);

  time_point now = std::chrono::system_clock::now();

  if (_last_start.time_since_epoch().count() != 0) {
    _stat->add_interval_stat(_command_name, now - _last_start);
  }

  _last_start = now;

  return true;
}

/**
 * @brief start check timeout timer
 *
 * @param timeout
 */
void check::_start_timeout_timer(const duration& timeout) {
  _time_out_timer.expires_after(timeout);
  _time_out_timer.async_wait(
      [me = shared_from_this(), start_check_index = _running_check_index](
          const boost::system::error_code& err) {
        me->_timeout_timer_handler(err, start_check_index);
      });
}

/**
 * @brief timeout timer handler
 *
 * @param err
 * @param start_check_index
 */
void check::_timeout_timer_handler(const boost::system::error_code& err,
                                   unsigned start_check_index) {
  if (err) {
    return;
  }
  if (start_check_index == _running_check_index) {
    SPDLOG_LOGGER_ERROR(_logger, "check timeout for service {} cmd: {}",
                        _service, _command_name);
    this->_on_timeout();
    on_completion(start_check_index, 3 /*unknown*/,
                  std::list<com::centreon::common::perfdata>(),
                  {"Timeout at execution of " + _command_line});
  }
}

/**
 * @brief called when check is ended
 * _running_check is increased so that next check will be identified by this new
 * id. We also cancel timeout timer
 *
 * @param start_check_index
 * @param status
 * @param perfdata
 * @param outputs
 */
void check::on_completion(
    unsigned start_check_index,
    unsigned status,
    const std::list<com::centreon::common::perfdata>& perfdata,
    const std::list<std::string>& outputs) {
  if (start_check_index == _running_check_index) {
    SPDLOG_LOGGER_TRACE(_logger,
                        "end check for service {} cmd: {} status:{} output: {}",
                        _service, _command_name, status,
                        outputs.empty() ? "" : outputs.front());
    _time_out_timer.cancel();
    _running_check = false;
    ++_running_check_index;
    _stat->add_duration_stat(_command_name,
                             std::chrono::system_clock::now() - _last_start);
    _completion_handler(shared_from_this(), status, perfdata, outputs);
  }
}

/**
 * @brief get a double value from a json number or a string containing a number
 *
 * @param cmd_name used to trace exception
 * @param field_name used to trace exception
 * @param val rapidjson value
 * @param must_be_positive if true, value must be positive
 * @throw exception-object if value is not a number
 * @return std::optional<double> set if value is not an empty string
 */
std::optional<double> check::get_double(const std::string& cmd_name,
                                        const char* field_name,
                                        const rapidjson::Value& val,
                                        bool must_be_positive) {
  double value;
  if (val.IsNumber()) {
    value = val.GetDouble();
  } else if (val.IsString()) {
    const char* to_conv = val.GetString();
    if (!*to_conv) {
      return {};
    }
    if (!absl::SimpleAtod(to_conv, &value)) {
      throw exceptions::msg_fmt("command: {}, parameter {} is not a number",
                                cmd_name, field_name);
    }
  } else {
    throw exceptions::msg_fmt("command: {}, parameter {} is not a number",
                              cmd_name, field_name);
  }
  if (must_be_positive && value < 0) {
    throw exceptions::msg_fmt("command: {}, {} is negative for parameter {}",
                              cmd_name, value, field_name);
  }
  return value;
}

/**
 * @brief get a boolean value from a json object
 * It can be a boolean value or a string containing a boolean
 *
 * @param cmd_name
 * @param field_name
 * @param val
 * @throw exception-object if value is not a boolean
 * @return std::optional<bool>
 */
std::optional<bool> check::get_bool(const std::string& cmd_name,
                                    const char* field_name,
                                    const rapidjson::Value& val) {
  bool value;
  if (val.IsBool()) {
    value = val.GetBool();
  } else if (val.IsString()) {
    const char* to_conv = val.GetString();
    if (!*to_conv) {
      return {};
    }
    if (!absl::SimpleAtob(to_conv, &value)) {
      throw exceptions::msg_fmt("command: {}, parameter {} is not a boolean",
                                cmd_name, field_name);
    }
  } else {
    throw exceptions::msg_fmt("command: {}, parameter {} is not a boolean",
                              cmd_name, field_name);
  }
  return value;
}
