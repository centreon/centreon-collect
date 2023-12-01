/**
 * Copyright 2011-2013 Merethis
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/log_v2.hh"
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
 */
commands::command::command(const std::string& name,
                           const std::string& command_line,
                           command_listener* listener)
    : _command_line(command_line), _listener{listener}, _name(name) {
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
      if (group_search->second->launch_time + config->interval_length() >=
              now &&
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
    SPDLOG_LOGGER_TRACE(log_v2::commands(),
                        "command::run: id={} , reuse result", command_id);
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