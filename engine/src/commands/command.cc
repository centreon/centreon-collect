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

#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros/grab.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

static std::atomic<uint64_t> _id{0};

command_map commands::command::commands;

/**
 *  Default constructor
 *
 *  @param[in] name         The command name.
 *  @param[in] command_line The command line.
 *  @param[in] listener     The command listener to catch events.
 *  @param[in] cmd_type     object type (exec, forward, raw, connector, otel)
 */
commands::command::command(const std::string& name,
                           const std::string& command_line,
                           command_listener* listener,
                           e_type cmd_type)
    : _type(cmd_type),
      _command_line(command_line),
      _listener{listener},
      _name(name) {
  if (_name.empty())
    throw engine_error() << "Could not create a command with an empty name";
  if (_listener) {
    std::function<void()> f = [this] { _listener = nullptr; };
    _listener->reg(this, f);
  }
}

/**
 *  Destructor.
 */
commands::command::~command() noexcept {
  if (_listener) {
    _listener->unreg(this);
  }
}

/**
 *  Compare two result.
 *
 *  @param[in] right The object to compare.
 *
 *  @return True if object have the same value.
 */
bool commands::command::operator==(command const& right) const noexcept {
  return _name == right._name && _command_line == right._command_line;
}

/**
 *  Compare two result.
 *
 *  @param[in] right The object to compare.
 *
 *  @return True if object have the different value.
 */
bool commands::command::operator!=(command const& right) const noexcept {
  return !operator==(right);
}

/**
 *  Get the command line.
 *
 *  @return The command line.
 */
const std::string& commands::command::get_command_line() const noexcept {
  return _command_line;
}

/**
 *  Get the command name.
 *
 *  @return The command name.
 */
const std::string& commands::command::get_name() const noexcept {
  return _name;
}

/**
 *  Set the command line.
 *
 *  @param[in] command_line The command line.
 */
void commands::command::set_command_line(const std::string& command_line) {
  _command_line = command_line;
}

/**
 *  Set the command listener.
 *
 *  @param[in] listener  The listener who catch events.
 */
void commands::command::set_listener(
    commands::command_listener* listener) noexcept {
  if (_listener)
    _listener->unreg(this);
  _listener = listener;
  if (_listener) {
    std::function<void()> f([this] { _listener = nullptr; });
    _listener->reg(this, f);
  }
}

/**
 *  Get the processed command line.
 *
 *  @param[in] macros The macros list.
 *
 *  @return The processed command line.
 */
std::string commands::command::process_cmd(nagios_macros* macros) const {
  std::string command_line;
  process_macros_r(macros, this->get_command_line(), command_line, 0);
  return command_line;
}

/**
 *  Get the unique command id.
 *
 *  @return The unique command id.
 */
uint64_t commands::command::get_uniq_id() {
  return ++_id;
}

void commands::command::remove_caller(void* caller) {
  std::unique_lock<std::mutex> l(_lock);
  _result_cache.erase(caller);
}

/**
 * @brief ensure that checks isn't used to often
 *
 * @param command_id
 * @param to_push_to_checker check_result to push to checks::checker
 * @param caller pointer of the caller object as a service or anomalydetection
 * @return true check can ben done
 * @return false check musn't be done, the previous result is pushed to
 * checks::checker
 */
bool commands::command::gest_call_interval(
    uint64_t command_id,
    const check_result::pointer& to_push_to_checker,
    const void* caller) {
  std::shared_ptr<result> result_to_reuse;

  {
    std::lock_guard<std::mutex> lock(_lock);
    // are we allowed to execute command
    caller_to_last_call_map::iterator group_search = _result_cache.find(caller);
    if (group_search != _result_cache.end()) {
      time_t now = time(nullptr);
      uint32_t interval_length = pb_indexed_config.state().interval_length();
      if (group_search->second->launch_time + interval_length >= now &&
          group_search->second->res) {  // old check is too recent
        result_to_reuse = std::make_shared<result>(*group_search->second->res);
        result_to_reuse->command_id = command_id;
        result_to_reuse->start_time = timestamp::now();
        result_to_reuse->end_time = timestamp::now();
      } else {
        // old check is old enough => we do the check
        group_search->second->launch_time = now;
        _current[command_id] = group_search->second;
      }
    }
  }

  checks::checker::instance().add_check_result(command_id, to_push_to_checker);
  if (_listener && result_to_reuse) {
    _listener->finished(*result_to_reuse);
    SPDLOG_LOGGER_TRACE(commands_logger, "command::run: id={} , reuse result",
                        command_id);
    return false;
  }
  return true;
}

void commands::command::update_result_cache(uint64_t command_id,
                                            const result& res) {
  std::lock_guard<std::mutex> lock(_lock);
  cmdid_to_last_call_map::iterator to_update = _current.find(command_id);
  if (to_update != _current.end()) {
    to_update->second->res = std::make_shared<result>(res);
    _current.erase(to_update);
  }
}

namespace com::centreon::engine::commands {

std::ostream& operator<<(std::ostream& s, const commands::command& cmd) {
  s << "cmd_name:" << cmd.get_name() << " cmd_line:" << cmd.get_command_line();
  return s;
}

}  // namespace com::centreon::engine::commands
