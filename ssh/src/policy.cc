/*
** Copyright 2011-2012 Merethis
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
#include <stdio.h>
#include <stdlib.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/concurrency/mutex.hh"
#include "com/centreon/connector/ssh/checks/check.hh"
#include "com/centreon/connector/ssh/checks/result.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/connector/ssh/policy.hh"
#include "com/centreon/connector/ssh/sessions/session.hh"
#include "com/centreon/delayed_delete.hh"
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
policy::~policy() throw () {
  try {
    // Remove from multiplexer.
    multiplexer::instance().handle_manager::remove(&_sin);
    multiplexer::instance().handle_manager::remove(&_sout);
  }
  catch (...) {}

  // Close checks.
  for (std::map<
         unsigned long long,
         std::pair<checks::check*, sessions::session*> >::iterator
         it = _checks.begin(),
         end = _checks.end();
       it != end;
       ++it) {
    try {
      it->second.first->unlisten(this);
    }
    catch (...) {}
    delete it->second.first;
  }
  _checks.clear();

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
  logging::info(logging::low) << "stdin is closed";
  on_quit();
  return ;
}

/**
 *  Called if an error occured on stdin.
 */
void policy::on_error() {
  logging::info(logging::low)
    << "error occurred while parsing stdin";
  _error = true;
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
    _checks[cmd_id] = std::make_pair(chk.get(), it->second);
    chk.release();
  }
  catch (std::exception const& e) {
    logging::error(logging::low) << "could not launch check ID "
      << cmd_id << " on host " << host << " because an error occurred: "
      << e.what();
    checks::result r;
    r.set_command_id(cmd_id);
    on_result(r);
  }
  catch (...) {
    logging::error(logging::low) << "could not launch check ID "
      << cmd_id << " on host " << host << " because an error occurred";
    checks::result r;
    r.set_command_id(cmd_id);
    on_result(r);
  }

  return ;
}

/**
 *  Quit order was received.
 */
void policy::on_quit() {
  // Exiting.
  logging::info(logging::low)
    << "quit request received";
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
  static concurrency::mutex processing_mutex;

  // Lock mutex.
  concurrency::locker lock(&processing_mutex);

  // Remove check from list.
  std::map<unsigned long long, std::pair<checks::check*, sessions::session*> >::iterator chk;
  chk = _checks.find(r.get_command_id());
  if (chk != _checks.end()) {
    try {
      chk->second.first->unlisten(this);
    }
    catch (...) {}
    delete chk->second.first;
    sessions::session* sess(chk->second.second);
    _checks.erase(chk);

    // Check session.
    if (!sess->is_connected()) {
      logging::debug(logging::medium) << "session " << sess << " is not"
           " connected, checking if any check working with it remains";
      bool found(false);
      for (std::map<unsigned long long, std::pair<checks::check*, sessions::session*> >::iterator
             it = _checks.begin(),
             end = _checks.end();
           it != end;
           ++it)
        if (it->second.second == sess)
          found = true;
      if (!found) {
        std::map<sessions::credentials, sessions::session*>::iterator
          it, end;
        for (it = _sessions.begin(), end = _sessions.end();
             it != end;
             ++it) {
          if (it->second == sess)
            break ;
        }
        if (it == end)
          logging::error(logging::high) << "session " << sess
            << " was not found in policy list, deleting anyway";
        else {
          logging::info(logging::high) << "session "
           << it->first.get_user() << "@" << it->first.get_host()
           << " that is not connected and has "
              "no check running will be deleted";
          _sessions.erase(it);
        }
        std::auto_ptr<delayed_delete<sessions::session> >
          dd(new delayed_delete<sessions::session>(sess));
        multiplexer::instance().task_manager::add(
          dd.get(),
          0,
          true,
          true);
        dd.release();
      }
    }
  }
  lock.unlock();

  // Send check result back to monitoring engine.
  _reporter.send_result(r);

  return ;
}

/**
 *  Version request was received.
 */
void policy::on_version() {
  // Report version 1.0.
  logging::info(logging::medium)
    << "monitoring engine requested protocol version, sending 1.0";
  _reporter.send_version(1, 0);
  return ;
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
  while (!should_exit)
    multiplexer::instance().multiplex();

  // Run as long as a check remains.
  logging::info(logging::low) << "waiting for checks to terminate";
  while (!_checks.empty())
    multiplexer::instance().multiplex();

  // Run as long as some data remains.
  logging::info(logging::low)
    << "reporting last data to monitoring engine";
  while (_reporter.can_report() && _reporter.want_write(_sout))
    multiplexer::instance().multiplex();

  return (!_error);
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
  _internal_copy(p);
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
  _internal_copy(p);
  return (*this);
}

/**
 *  Calls abort().
 *
 *  @param[in] p Unused.
 */
void policy::_internal_copy(policy const& p) {
  (void)p;
  assert(!"policy is not copyable");
  abort();
  return ;
}
