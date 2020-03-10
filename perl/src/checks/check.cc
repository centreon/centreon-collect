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

#include "com/centreon/connector/perl/checks/check.hh"
#include <csignal>
#include <cstdlib>
#include <memory>
#include "com/centreon/connector/perl/checks/listener.hh"
#include "com/centreon/connector/perl/checks/result.hh"
#include "com/centreon/connector/perl/checks/timeout.hh"
#include "com/centreon/connector/perl/embedded_perl.hh"
#include "com/centreon/connector/perl/multiplexer.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl::checks;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
check::check() : _child((pid_t)-1), _cmd_id(0), _listnr(NULL), _timeout(0) {}

/**
 *  Destructor.
 */
check::~check() throw() {
  try {
    // Send result if we haven't already done so.
    result r;
    r.set_command_id(_cmd_id);
    _send_result_and_unregister(r);
  } catch (...) {
  }
}

/**
 *  Error occurred on one pipe.
 *
 *  @param[in] h Pipe.
 */
void check::error(handle& h) {
  (void)h;
  result r;
  r.set_command_id(_cmd_id);
  _send_result_and_unregister(r);
}

/**
 *  Execute a Perl script.
 *
 *  @param[in] cmd_id Command ID.
 *  @param[in] cmd    Command line.
 *  @param[in] tmt    Timeout.
 *
 *  @return Process ID.
 */
pid_t check::execute(unsigned long long cmd_id,
                     std::string const& cmd,
                     const timestamp& tmt) {
  // Run process.
  int fds[3];
  _child = embedded_perl::instance().run(cmd, fds);
  ::close(fds[0]);
  _out.set_fd(fds[1]);
  _err.set_fd(fds[2]);

  // Store command ID.
  log_debug(logging::low) << "check " << this << " has ID " << cmd_id;
  _cmd_id = cmd_id;

  // Register with multiplexer.
  multiplexer::instance().handle_manager::add(&_err, this);
  multiplexer::instance().handle_manager::add(&_out, this);

  // Register timeout.
  std::unique_ptr<timeout> t(new timeout(this, false));
  _timeout = multiplexer::instance().com::centreon::task_manager::add(
      t.get(), tmt, false, true);
  t.release();

  return _child;
}

/**
 *  Listen the check.
 *
 *  @param[in] listnr New listener.
 */
void check::listen(listener* listnr) {
  log_debug(logging::medium)
      << "check " << this << " is listened by " << listnr;
  _listnr = listnr;
}

/**
 *  Called when check timeout occurs.
 *
 *  @param[in] final Did we set the final timeout ?
 */
void check::on_timeout(bool final) {
  // Log message.
  log_error(logging::low) << "check " << _cmd_id << " (pid=" << _child
                          << ") reached timeout";

  // Reset timeout task ID.
  _timeout = 0;

  if (_child <= 0)
    return;

  if (final) {
    // Send SIGKILL (not catchable, not ignorable).
    kill(_child, SIGKILL);
    _child = (pid_t)-1;
  } else {
    // Try graceful shutdown.
    kill(_child, SIGTERM);

    // Schedule a final timeout.
    std::unique_ptr<timeout> t(new timeout(this, true));
    _timeout = multiplexer::instance().com::centreon::task_manager::add(
        t.get(), time(NULL) + 1, false, true);
    t.release();
  }
}

/**
 *  Read data from handle.
 *
 *  @param[in] h Handle.
 */
void check::read(handle& h) {
  char buffer[1024];
  unsigned long rb(h.read(buffer, sizeof(buffer)));
  if (&h == &_err) {
    log_debug(logging::high)
        << "reading from process " << _child << "'s stdout";
    _stderr.append(buffer, rb);
  } else {
    log_debug(logging::high) << "reading from process " << _child << "' stderr";
    _stdout.append(buffer, rb);
  }
}

/**
 *  Process termination callback.
 *
 *  @param[in] exit_code Process exit code.
 */
void check::terminated(int exit_code) {
  // Read possibly remaining data.
  log_debug(logging::medium)
      << "reading remaining data from process " << _child;
  try {
    char buffer[1024];
    unsigned long rb(_out.read(buffer, sizeof(buffer)));
    while (rb != 0) {
      _stdout.append(buffer, rb);
      rb = _out.read(buffer, rb);
    }
  } catch (...) {
  }
  try {
    char buffer[1024];
    unsigned long rb(_err.read(buffer, sizeof(buffer)));
    while (rb != 0) {
      _stderr.append(buffer, rb);
      rb = _err.read(buffer, sizeof(buffer));
    }
  } catch (...) {
  }

  // Reset PID.
  _child = (pid_t)-1;

  // Send check result.
  result r;
  r.set_command_id(_cmd_id);
  r.set_executed(true);
  r.set_exit_code(exit_code);
  r.set_error(_stderr);
  r.set_output(_stdout);
  _send_result_and_unregister(r);
}

/**
 *  Unlisten the check.
 *
 *  @param[in] listnr Old listener.
 */
void check::unlisten(listener* listnr) {
  log_debug(logging::medium)
      << "listener " << listnr << " stops listening check " << this;
  _listnr = NULL;
}

/**
 *  Check want to read.
 *
 *  @return true.
 */
bool check::want_read(handle& h) {
  (void)h;
  return true;
}

/**
 *  Write callback.
 *
 *  @param[in] h Unused.
 */
void check::write(handle& h) {
  // This is an error, we shouldn't have been called.
  (void)h;
  result r;
  r.set_command_id(_cmd_id);
  _send_result_and_unregister(r);
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Send check result and unregister.
 *
 *  @param[in] r Check result.
 */
void check::_send_result_and_unregister(result const& r) {
  // Kill subprocess.
  if (_child > 0) {
    kill(_child, SIGKILL);
    _child = (pid_t)-1;
  }

  // Remove timeout task.
  if (_timeout) {
    try {
      multiplexer::instance().com::centreon::task_manager::remove(_timeout);
    } catch (...) {
    }
    _timeout = 0;
  }

  // Check that we haven't already send a check result.
  if (_cmd_id) {
    // Unregister from multiplexer.
    multiplexer::instance().handle_manager::remove(this);

    // Reset command ID.
    _cmd_id = 0;

    // Send check result to listener.
    if (_listnr)
      _listnr->on_result(r);
  }
}
