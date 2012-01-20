/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <errno.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/concurrency/mutex.hh"
#include "com/centreon/connector/perl/checks/check.hh"
#include "com/centreon/connector/perl/multiplexer.hh"
#include "com/centreon/connector/perl/policy.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

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
  // Remove from multiplexer.
  try {
    multiplexer::instance().handle_manager::remove(&_sin);
    multiplexer::instance().handle_manager::remove(&_sout);
  }
  catch (...) {}

  // Close checks.
  for (std::map<pid_t, checks::check*>::iterator
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
  _checks.clear();
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
 *  @param[in] cmd_id  Command ID.
 *  @param[in] timeout Time the command has to execute.
 *  @param[in] cmd     Command to execute.
 */
void policy::on_execute(
               unsigned long long cmd_id,
               time_t timeout,
               std::string const& cmd) {
  std::auto_ptr<checks::check> chk(new checks::check);
  chk->listen(this);
  try {
    pid_t child(chk->execute(cmd_id, cmd, timeout));
    _checks[child] = chk.get();
    chk.release();
  }
  catch (std::exception const& e) {
    logging::info(logging::low) << "execution of check "
      << cmd_id << "failed: " << e.what();
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
 *  Check result callback.
 *
 *  @param[in] r Check result callback.
 */
void policy::on_result(checks::result const& r) {
  // Lock mutex.
  static concurrency::mutex processing_mutex;
  concurrency::locker lock(&processing_mutex);

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

  while (!should_exit || !_checks.empty()) {
    // Run multiplexer.
    multiplexer::instance().multiplex();

    // Is there some terminated child ?
    int status(0);
    pid_t child(waitpid(0, &status, WNOHANG));
    while ((child != 0) && (child != (pid_t)-1)) {
      // Check for error.
      if ((child == (pid_t)-1) && (errno != ECHILD)) {
        char const* msg(strerror(errno));
        throw (basic_error() << "waitpid failed: " << msg);
      }

      // Handle process termination.
      std::map<pid_t, checks::check*>::iterator it;
      it = _checks.find(child);
      if (it != _checks.end()) {
        std::auto_ptr<checks::check> chk(it->second);
        _checks.erase(it);
        chk->terminated(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
      }

      // Is there any other terminated child ?
      child = waitpid(0, &status, WNOHANG);
    }
  }

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
