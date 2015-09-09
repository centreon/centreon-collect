/*
** Copyright 2011-2013 Centreon
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
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
  log_info(logging::low) << "stdin is closed";
  on_quit();
  return ;
}

/**
 *  Called if an error occured on stdin.
 */
void policy::on_error() {
  log_info(logging::low)
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
    log_info(logging::low) << "execution of check "
      << cmd_id << " failed: " << e.what();
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
  log_info(logging::low)
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
  log_info(logging::medium)
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
      log_info(logging::medium) << "process " << child
        << " exited with status " << status;
      std::map<pid_t, checks::check*>::iterator it;
      it = _checks.find(child);
      if (it != _checks.end()) {
        std::auto_ptr<checks::check> chk(it->second);
        _checks.erase(it);
        chk->terminated(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
      }
      log_debug(logging::medium)
        << _checks.size() << " checks still running";

      // Is there any other terminated child ?
      child = waitpid(0, &status, WNOHANG);
    }
  }

  // Run as long as some data remains.
  log_info(logging::low)
    << "reporting last data to monitoring engine";
  while (_reporter.can_report() && _reporter.want_write(_sout))
    multiplexer::instance().multiplex();

  return (!_error);
}
