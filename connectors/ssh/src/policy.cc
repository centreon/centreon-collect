/**
 * Copyright 2011-2014 Centreon
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

#include "com/centreon/connector/ssh/policy.hh"

#include <atomic>
#include <cstdio>

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/result.hh"
#include "com/centreon/connector/ssh/checks/check.hh"
#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/connector/ssh/sessions/session.hh"

using namespace com::centreon::connector::ssh;
using namespace com::centreon::connector;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
policy::policy(const shared_io_context& io_context)
    : _reporter(reporter::create(io_context)), _io_context(io_context) {}

policy::pointer policy::create(const shared_io_context& io_context,
                               const std::string& test_cmd_file) {
  pointer ret(new policy(io_context));
  ret->start(test_cmd_file);
  return ret;
}

void policy::start(const std::string& test_cmd_file) {
  orders::parser::create(_io_context, shared_from_this(), test_cmd_file);
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
    _error = true;
    on_quit();
  }
}

/**
 *  Execution command received.
 *
 *  @param[in] cmd_id      Command ID.
 *  @param[in] timeout     Time the command has to execute.
 *  @param[in] opt         cmd options.
 */
void policy::on_execute(
    uint64_t cmd_id,
    const time_point& timeout,
    const com::centreon::connector::orders::options::pointer& opt) {
  log::core()->info(
      "got request to execute check {0} on session {1}@{2} (key_file: \"{5}\" "
      "timeout {3}, "
      "first command \"{4}\")",
      cmd_id, opt->get_user(), opt->get_host(), opt->get_timeout(),
      opt->get_commands().front(), opt->get_identity_file());

  // Credentials.
  sessions::credentials creds;
  creds.set_host(opt->get_host());
  creds.set_user(opt->get_user());
  creds.set_password(opt->get_authentication());
  creds.set_port(opt->get_port());
  creds.set_key(opt->get_identity_file());

  // Find session.
  auto it = _sessions.find(creds);
  // still alive session?
  if (it != _sessions.end() &&
      it->second->get_state() ==
          sessions::session::e_step::session_error) {  // no => erase it
    _sessions.erase(it);
    it = _sessions.end();
  }

  if (it == _sessions.end()) {
    auto connecting_yet = _connect_waiting_session.find(creds);
    if (connecting_yet !=
        _connect_waiting_session
            .end()) {  // the session is yet connecting => push the check
      connecting_yet->second.waiting_check.push({cmd_id, timeout, opt});
      return;
    }

    log::core()->info("creating session for {}", creds);
    std::shared_ptr<sessions::session> sess(
        std::make_shared<sessions::session>(creds, _io_context));

    connect_waiting_session& connecting = _connect_waiting_session[creds];
    connecting._connecting = sess;
    connecting.waiting_check.push({cmd_id, timeout, opt});

    sess->connect(
        [me = shared_from_this(), sess,
         creds](const boost::system::error_code& err) {
          me->on_connect(err, creds);
        },
        timeout);
  } else {
    on_execute(it->second, cmd_id, timeout, opt);
  }
}

void policy::on_connect(const boost::system::error_code& err,
                        const sessions::credentials& creds) {
  connect_waiting_session& waiting_connect = _connect_waiting_session[creds];
  if (err) {
    std::ostringstream err_detail;
    err_detail << " fail to connect to " << creds;
    while (!waiting_connect.waiting_check.empty()) {
      connect_waiting_session::cmd_info& cmd =
          waiting_connect.waiting_check.front();
      on_error(cmd.cmd_id, err_detail.str());
      waiting_connect.waiting_check.pop();
    }
    _connect_waiting_session.erase(creds);
    return;
  }
  _sessions.emplace(creds, waiting_connect._connecting);
  while (!waiting_connect.waiting_check.empty()) {
    connect_waiting_session::cmd_info& cmd =
        waiting_connect.waiting_check.front();
    on_execute(waiting_connect._connecting, cmd.cmd_id, cmd.timeout, cmd.opt);
    waiting_connect.waiting_check.pop();
  }
  _connect_waiting_session.erase(creds);
}

void policy::on_execute(
    const std::shared_ptr<sessions::session>& session,
    uint64_t cmd_id,
    const time_point& timeout,
    const com::centreon::connector::orders::options::pointer& opt) {
  // Create check object.
  checks::check::pointer chk_ptr(std::make_shared<checks::check>(
      session, cmd_id, opt->get_commands(), timeout, opt->skip_stdout(),
      opt->skip_stderr()));

  chk_ptr->execute([me = shared_from_this()](const result& res) {
    me->_reporter->send_result(res);
  });
}

/**
 *  Quit order was received.
 */
void policy::on_quit() {
  // Exiting.
  log::core()->info("quit request received");
  for (const auto& to_close : _sessions) {
    to_close.second->close();
  }
  _sessions.clear();
  _io_context->stop();
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
