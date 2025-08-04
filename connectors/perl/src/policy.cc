/**
 * Copyright 2022 Centreon
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

#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/connector/log.hh"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::perl;

/**
 *  Default constructor.
 */
policy::policy(const shared_io_context& io_context)
    : _reporter(reporter::create(io_context)),
      _io_context(io_context),
      _second_timer(*io_context),
      _end_timer(*io_context) {}

void policy::create(const shared_io_context& io_context,
                    const std::string& test_cmd_file) {
  std::shared_ptr<policy> ret(new policy(io_context));
  ret->start(test_cmd_file);
}

void policy::start(const std::string& test_cmd_file) {
  orders::parser::create(_io_context, shared_from_this(), test_cmd_file);
  checks::shared_signal_set signal(
      std::make_shared<asio::signal_set>(*_io_context, SIGCHLD));
  signal->async_wait([me = shared_from_this(), this](
                         const boost::system::error_code& err, int) {
    if (!err) {
      wait_pid();
    }
  });
  start_second_timer();
}

/**
 * @brief sometimes asio::signal_set miss signal so this timer checks forgotten
 * childs
 *
 */
void policy::start_second_timer() {
  _second_timer.expires_after(std::chrono::seconds(1));
  _second_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        if (!err) {
          me->wait_pid();
          me->start_second_timer();
        }
      });
}

/**
 * @brief get child exit status
 *
 */
void policy::wait_pid() {
  siginfo_t child_info;
  child_info.si_pid = 0;
  while (!waitid(P_ALL, 0, &child_info, WNOHANG | WEXITED)) {
    if (!child_info.si_pid) {  // no exited child
      break;
    }
    pid_to_check_map::iterator ended = _checks.find(child_info.si_pid);
    if (ended == _checks.end()) {
      log::core()->error("pid {} inconnu", child_info.si_pid);
      child_info.si_pid = 0;
      continue;
    }
    ended->second->set_exit_code(child_info.si_status);
    _checks.erase(ended);
    child_info.si_pid = 0;
  }
}

/**
 *  Called if stdin is closed.
 */
void policy::on_eof() {
  log::core()->info("stdin is closed");
  on_quit();
}

/**
 *  Called if an error occured on stdin.
 *
 *  @param[in] cmd_id Command ID.
 *  @param[in] msg    Associated message.
 */
void policy::on_error(uint64_t cmd_id, const std::string& msg) {
  if (cmd_id) {
    result r;
    r.set_command_id(cmd_id);
    r.set_executed(false);
    r.set_error(msg);
    _reporter->send_result(r);
  } else {
    log::core()->info("error occurred while parsing stdin");
    on_quit();
  }
}

/**
 *  Execution command received.
 *
 *  @param[in] cmd_id  Command ID.
 *  @param[in] timeout Time the command has to execute.
 *  @param[in] cmd     Command to execute.
 */
void policy::on_execute(
    uint64_t cmd_id,
    const time_point& timeout,
    const std::shared_ptr<com::centreon::connector::orders::options>& opt) {
  checks::check::pointer check = std::make_shared<checks::check>(
      cmd_id, *opt, timeout, _reporter, _io_context);

  try {
    pid_t child = check->execute();
    if (child > 0) {
      _checks[child] = check;
    }
  } catch (const std::exception& e) {
    _reporter->send_result({cmd_id, -1, e.what()});
  }
}

/**
 *  Quit order was received.
 */
void policy::on_quit() {
  // Exiting.
  log::core()->info("quit request received");
  start_end_timer(false);
}

/**
 * @brief before stop io_context, we wait after all checks end
 *
 */
void policy::start_end_timer(bool final) {
  _end_timer.expires_after(std::chrono::milliseconds(10));
  _end_timer.async_wait([me = shared_from_this(),
                         final](const boost::system::error_code& err) {
    if (!err) {
      log::core()->trace("{} checks remaining", checks::check::get_nb_check());
      if (checks::check::get_nb_check()) {
        me->start_end_timer(false);
      } else {
        if (final) {
          me->_io_context->stop();
        } else {  // a last delay to allow reporter to write everything on
                  // stdout
          me->start_end_timer(true);
        }
      }
    }
  });
}

/**
 *  Version request was received.
 */
void policy::on_version() {
  // Report version 1.0.
  log::core()->info(
      "monitoring engine requested protocol version, sending 1.0");
  _reporter->send_version(1, 0);
}
