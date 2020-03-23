/*
** Copyright 2011-2014 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/connector/ssh/policy.hh"

#include <atomic>
#include <cstdio>
#include <memory>

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/ssh/checks/check.hh"
#include "com/centreon/connector/ssh/checks/result.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/connector/ssh/sessions/session.hh"
#include "com/centreon/delayed_delete.hh"

using namespace com::centreon::connector::ssh;

// Exit flag.
extern std::atomic<bool> should_exit;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
policy::policy() : _sin(stdin), _sout(stdout) {
  // Send information back.
  multiplexer::instance().handle_manager::add(&_sout, &_reporter);

  // Listen orders.
  _parser.listen(this);

  // Parser listens stdin.
  multiplexer::instance().handle_manager::add(&_sin, &_parser);
}

/**
 *  Destructor.
 */
policy::~policy() noexcept {
  try {
    // Remove from multiplexer.
    multiplexer::instance().handle_manager::remove(&_sin);
    multiplexer::instance().handle_manager::remove(&_sout);
  } catch (...) {
  }

  // Close checks.
  for (auto& c : _checks) {
    try {
      c.second.first->unlisten(this);
    } catch (...) {
    }
    delete c.second.first;
  }
  _checks.clear();

  // Close sessions.
  for (auto& _session : _sessions) {
    try {
      _session.second->close();
    } catch (...) {
    }
    delete _session.second;
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
void policy::on_error(uint64_t cmd_id, char const* msg) {
  if (cmd_id) {
    checks::result r;
    r.set_command_id(cmd_id);
    r.set_executed(false);
    r.set_error(msg);
    on_result(r);
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
 *  @param[in] host        Target host.
 *  @param[in] port        Connection port.
 *  @param[in] user        User.
 *  @param[in] password    Password.
 *  @param[in] key         Identity file.
 *  @param[in] cmds        Commands to execute.
 *  @param[in] skip_stdout Ignore all or first n output lines.
 *  @param[in] skip_stderr Ignore all or first n error lines.
 *  @param[in] use_ipv6    Version of ip protocol to use.
 */
void policy::on_execute(uint64_t cmd_id,
                        const timestamp& timeout,
                        std::string const& host,
                        unsigned short port,
                        std::string const& user,
                        std::string const& password,
                        std::string const& key,
                        std::list<std::string> const& cmds,
                        int skip_stdout,
                        int skip_stderr,
                        bool use_ipv6) {
  try {
    // Log message.
    log::core()->info(
        "got request to execute check {0} on session {1}@{2} (timeout {3}, "
        "first command \"{4}\")",
        cmd_id, user, host, timeout.to_seconds(), cmds.front());

    // Credentials.
    sessions::credentials creds;
    creds.set_host(host);
    creds.set_user(user);
    creds.set_password(password);
    creds.set_port(port);
    creds.set_key(key);

    // Object lock.
    std::unique_lock<std::mutex> lock(_mutex);

    // Find session.
    auto it = _sessions.find(creds);
    if (it == _sessions.end()) {
      log::core()->info("creating session for {0}@{1}:{2}", user, host, port);
      std::unique_ptr<sessions::session> sess{new sessions::session(creds)};
      sess->connect(use_ipv6);
      _sessions[creds] = sess.release();
      it = _sessions.find(creds);
    }

    sessions::session* sess = it->second;

    // Create check object.
    checks::check* chk_ptr = new checks::check(skip_stdout, skip_stderr);
    chk_ptr->listen(this);
    _checks[cmd_id] = std::make_pair(chk_ptr, sess);

    // Release lock and run copied pointer (we might be called in
    // on_result() and mutex must be available).
    lock.unlock();

    chk_ptr->execute(*sess, cmd_id, cmds, timeout);
  } catch (std::exception const& e) {
    log::core()->error(
        "could not launch check ID {0} on host {1} because an error occurred: "
        "{2}",
        cmd_id, host, e.what());
    checks::result r;
    r.set_command_id(cmd_id);
    on_result(r);
  } catch (...) {
    log::core()->error(
        "could not launch check ID {0} on host {1} because an error occurred",
        cmd_id, host);
    checks::result r;
    r.set_command_id(cmd_id);
    on_result(r);
  }
}

/**
 *  Quit order was received.
 */
void policy::on_quit() {
  // Exiting.
  log::core()->info("quit request received");
  should_exit = true;
  multiplexer::instance().handle_manager::remove(&_sin);
}

/**
 *  Check result has arrived.
 *
 *  @param[in] r Check result.
 */
void policy::on_result(checks::result const& r) {
  // Object lock.
  std::lock_guard<std::mutex> lock(_mutex);

  // Remove check from list.
  std::map<uint64_t, std::pair<checks::check*, sessions::session*> >::iterator
      chk;
  chk = _checks.find(r.get_command_id());
  if (chk == _checks.end())
    log::core()->error("got result of check {} which is not registered",
                       r.get_command_id());
  else {
    try {
      chk->second.first->unlisten(this);
      chk->second.second->unlisten(chk->second.first);
    } catch (...) {
    }
    delete chk->second.first;
    sessions::session* sess(chk->second.second);
    _checks.erase(chk);

    // Check session.
    if (!sess->is_connected()) {
      log::core()->debug(
          "session {} is not connected, checking if any check working with it "
          "remains",
          static_cast<void*>(sess));
      bool found(false);
      for (auto& _check : _checks)
        if (_check.second.second == sess) {
          found = true;
          break;
        }
      if (!found) {
        std::map<sessions::credentials, sessions::session*>::iterator it, end;
        for (it = _sessions.begin(), end = _sessions.end(); it != end; ++it) {
          if (it->second == sess)
            break;
        }
        if (it == end)
          log::core()->error(
              "session {} was not found in policy list, deleting anyway",
              static_cast<void*>(sess));
        else {
          log::core()->info(
              "session {0}@{1}:{2} that is not connected and has no check "
              "running will be deleted",
              it->first.get_user(), it->first.get_host(), it->first.get_port());
          _sessions.erase(it);
        }
        delayed_delete<sessions::session>* dd =
            new delayed_delete<sessions::session>(sess);
        multiplexer::instance().task_manager::add(dd, 0, true, true);
      }
    }
  }

  // Send check result back to monitoring engine.
  _reporter.send_result(r);
}

/**
 *  Version request was received.
 */
void policy::on_version() {
  // Report version 1.0.
  log::core()->info(
      "monitoring engine requested protocol version, sending 1.0");
  _reporter.send_version(1, 0);
}

/**
 *  Run the program.
 *
 *  @return false if program terminated prematurely.
 */
bool policy::run() {
  // No error occurred yet.
  _error = false;

  // Run multiplexer.
  while (!should_exit) {
    log::core()->debug("multiplexing");
    multiplexer::instance().multiplex();
  }

  // Run as long as a check remains.
  log::core()->info("waiting for checks to terminate");
  while (!_checks.empty()) {
    log::core()->debug("multiplexing remaining checks ({})", _checks.size());
    multiplexer::instance().multiplex();
  }

  // Run as long as some data remains.
  log::core()->info("reporting last data to monitoring engine");
  while (_reporter.can_report() && _reporter.want_write(_sout)) {
    log::core()->debug("multiplexing remaining data");
    multiplexer::instance().multiplex();
  }

  return !_error;
}
