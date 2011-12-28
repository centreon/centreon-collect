/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <memory>
#include <stdlib.h>
#include "com/centreon/connector/ssh/checks/check.hh"
#include "com/centreon/connector/ssh/checks/result.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/connector/ssh/policy.hh"
#include "com/centreon/connector/ssh/sessions/session.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon::connector::ssh;

// Exit flag.
extern volatile bool should_exit;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
policy::policy() {
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
policy::~policy() throw () {
  // Remove from multiplexer.
  multiplexer::instance().handle_manager::remove(&_sin);
  multiplexer::instance().handle_manager::remove(&_sout);

  // Close checks.
  for (std::map<unsigned long long, checks::check*>::iterator
         it = _checks.begin(),
         end = _checks.end();
       it != end;
       ++it) {
    try {
      it->second->unlisten(this);
    }
    catch (...) {}
    delete it->second;
  }

  // Close sessions.
  for (std::map<sessions::credentials, sessions::session*>::iterator
         it = _sessions.begin(),
         end = _sessions.end();
       it != end;
       ++it) {
    try {
      it->second->close();
    }
    catch (...) {}
    delete it->second;
  }
}

/**
 *  Called if stdin is closed.
 */
void policy::on_eof() {
  logging::info(logging::high) << "stdin is closed";
  on_quit();
  return ;
}

/**
 *  Called if an error occured on stdin.
 */
void policy::on_error() {
  logging::info(logging::high)
    << "error occurred while parsing stdin";
  on_quit();
  return ;
}

/**
 *  Execution command received.
 *
 *  @param[in] cmd_id   Command ID.
 *  @param[in] timeout  Time the command has to execute.
 *  @param[in] host     Target host.
 *  @param[in] user     User.
 *  @param[in] password Password.
 *  @param[in] cmd      Command to execute.
 */
void policy::on_execute(
               unsigned long long cmd_id,
               time_t timeout,
               std::string const& host,
               std::string const& user,
               std::string const& password,
               std::string const& cmd) {
  try {
    // Credentials.
    sessions::credentials creds;
    creds.set_host(host);
    creds.set_user(user);
    creds.set_password(password);

    // Find session.
    std::map<sessions::credentials, sessions::session*>::iterator it;
    it = _sessions.find(creds);
    if (it == _sessions.end()) {
      logging::info(logging::low) << "creating session for "
        << user << "@" << host;
      std::auto_ptr<sessions::session> sess(new sessions::session(creds));
      sess->connect();
      _sessions[creds] = sess.get();
      sess.release();
      it = _sessions.find(creds);
    }

    // Launch check.
    std::auto_ptr<checks::check> chk(new checks::check);
    chk->listen(this);
    chk->execute(*it->second, cmd_id, cmd, timeout);
    _checks[cmd_id] = chk.get();
    chk.release();
  }
  catch (std::exception const& e) {
    logging::error(logging::high) << "could not launch check ID "
      << cmd_id << " on host " << host << " because an error occurred: "
      << e.what();
  }
  catch (...) {
    logging::error(logging::high) << "could not launch check ID "
      << cmd_id << " on host " << host << " because an error occurred";
  }

  return ;
}

/**
 *  Quit order was received.
 */
void policy::on_quit() {
  // Exiting.
  logging::info(logging::high)
    << "quit request received from monitoring engine";
  should_exit = true;
  multiplexer::instance().handle_manager::remove(&_sin);
  return ;
}

/**
 *  Check result has arrived.
 *
 *  @param[in] r Check result.
 */
void policy::on_result(checks::result const& r) {
  // Remove check from list.
  std::map<unsigned long long, checks::check*>::iterator it;
  it = _checks.find(r.get_command_id());
  if (it != _checks.end()) {
    try {
      it->second->unlisten(this);
    }
    catch (...) {}
    delete it->second;
    _checks.erase(it);
  }

  // Send check result back to monitoring engine.
  _reporter.send_result(r);

  return ;
}

/**
 *  Version request was received.
 */
void policy::on_version() {
  // Report version 1.0.
  logging::info(logging::low)
    << "monitoring engine requested protocol version, sending 1.0";
  _reporter.send_version(1, 0);
  return ;
}

/**
 *  Run the program.
 */
void policy::run() {
  // Run multiplexer.
  while (!should_exit)
    multiplexer::instance().multiplex();

  // Run as long as a check remains.
  logging::info(logging::high) << "waiting for checks to terminate";
  while (!_checks.empty())
    multiplexer::instance().multiplex();

  // Run as long as some data remains.
  // XXX

  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] p Unused.
 */
policy::policy(policy const& p)
  : orders::listener(p), checks::listener(p) {
  assert(!"policy is not copyable");
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] p Unused.
 *
 *  @return This object.
 */
policy& policy::operator=(policy const& p) {
  (void)p;
  assert(!"policy is not copyable");
  abort();
  return (*this);
}
