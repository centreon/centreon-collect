/*
** Copyright 2011-2019 Centreon
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

#include "com/centreon/connector/ssh/checks/check.hh"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include "com/centreon/connector/ssh/checks/timeout.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon::connector::ssh::checks;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 *
 *  @param[in] skip_stdout Ignore all or first n output lines.
 *  @param[in] skip_stderr Ignore all or first n error lines.
 */
check::check(int skip_stdout, int skip_stderr)
    : _channel(nullptr),
      _cmd_id(0),
      _listnr(nullptr),
      _session(nullptr),
      _skip_stderr(skip_stderr),
      _skip_stdout(skip_stdout),
      _step(chan_open),
      _timeout(0) {}

/**
 *  Destructor.
 */
check::~check() throw() {
  try {
    // Send result if we haven't already done so.
    sessions::session* sess(_session);
    result r;
    r.set_command_id(_cmd_id);
    _send_result_and_unregister(r);

    if (_channel) {
      // Close channel (or at least try to). Here libssh2 sucks. When
      // the close request is received on the remote end, the SSH server
      // closes the pipes it opened with the target process. It then
      // waits for it to exits (more or less forced by SIGPIPE if
      // process writes). However if process does not write we will hang
      // until it exits (which could be like forever).
      int ret(LIBSSH2_ERROR_EAGAIN);
      for (unsigned int i = 0; (i < 32) && (ret == LIBSSH2_ERROR_EAGAIN); ++i) {
        ret = libssh2_channel_close(_channel);
        if ((ret == LIBSSH2_ERROR_SOCKET_SEND) && sess)
          sess->error();
      }

      // Free channel.
      libssh2_channel_free(_channel);
    }
  } catch (...) {
  }
}

/**
 *  Start executing a check.
 *
 *  @param[in] sess    Session on which a channel will be opened.
 *  @param[in] cmd_id  Command ID.
 *  @param[in] cmds    Commands to execute.
 *  @param[in] tmt     Command timeout.
 */
void check::execute(sessions::session& sess,
                    unsigned long long cmd_id,
                    std::list<std::string> const& cmds,
                    const timestamp& tmt) {
  // Log message.
  log_debug(logging::low) << "check " << this << " has ID " << cmd_id;

  // Store command information.
  _cmds = cmds;
  _cmd_id = cmd_id;
  _session = &sess;

  // Register timeout.
  _timeout = multiplexer::instance().com::centreon::task_manager::add(
      new timeout(this), tmt, false, true);

  // Session-related actions.
  sess.listen(this);
  if (sess.is_connected())
    on_connected(sess);
}

/**
 *  Listen the check.
 *
 *  @param[in] listnr Listener.
 */
void check::listen(checks::listener* listnr) {
  log_debug(logging::medium)
      << "check " << this << " is listened by " << listnr;
  _listnr = listnr;
}

/**
 *  Can perform action on channel.
 *
 *  @param[in] sess Unused.
 */
void check::on_available(sessions::session& sess) {
  try {
    switch (_step) {
      case chan_open:
        log_info(logging::high)
            << "attempting to open channel for check " << _cmd_id;
        if (!_open()) {
          log_info(logging::high)
              << "check " << _cmd_id << " channel was successfully opened";
          _step = chan_exec;
          on_available(sess);
        }
        break;
      case chan_exec:
        log_info(logging::high) << "attempting to execute check " << _cmd_id;
        if (!_exec()) {
          log_info(logging::high)
              << "check " << _cmd_id << " was successfully executed";
          _step = chan_read;
          on_available(sess);
        }
        break;
      case chan_read:
        log_info(logging::high)
            << "reading check " << _cmd_id << " result from channel";
        if (!_read()) {
          log_info(logging::high)
              << "result of check " << _cmd_id << " was successfully fetched";
          _step = chan_close;
          on_available(sess);
        }
        break;
      case chan_close: {
        unsigned long long cmd_id(_cmd_id);
        log_info(logging::high)
            << "attempting to close check " << cmd_id << " channel";
        if (!_close()) {
          log_info(logging::medium)
              << "channel of check " << cmd_id << " successfully closed";
        }
      } break;
      default:
        throw(basic_error() << "channel requested to run at invalid step");
    }
  } catch (std::exception const& e) {
    log_error(logging::low)
        << "error occured while executing check " << _cmd_id << " on session "
        << sess.get_credentials().get_user() << "@"
        << sess.get_credentials().get_host() << ": " << e.what();
    result r;
    r.set_command_id(_cmd_id);
    _send_result_and_unregister(r);
  } catch (...) {
    log_error(logging::low)
        << "unknown error occured while executing check " << _cmd_id
        << " on session " << sess.get_credentials().get_user() << "@"
        << sess.get_credentials().get_host();
    result r;
    r.set_command_id(_cmd_id);
    _send_result_and_unregister(r);
  }
}

/**
 *  On session close.
 *
 *  @param[in] sess Closing session.
 */
void check::on_close(sessions::session& sess) {
  (void)sess;
  log_error(logging::medium) << "session closed before check could execute";
  result r;
  r.set_command_id(_cmd_id);
  _send_result_and_unregister(r);
}

/**
 *  Called when session is connected.
 *
 *  @param[in] sess Connected session.
 */
void check::on_connected(sessions::session& sess) {
  log_debug(logging::high) << "manually starting check " << _cmd_id;
  on_available(sess);
}

/**
 *  Called when check timeout occurs.
 */
void check::on_timeout() {
  // Log message.
  log_error(logging::low) << "check " << _cmd_id << " reached timeout";

  // Reset timeout task ID.
  _timeout = 0;

  // Send check result.
  result r;
  r.set_command_id(_cmd_id);
  _send_result_and_unregister(r);
}

/**
 *  Stop listening to the check.
 *
 *  @param[in] listnr Listener.
 */
void check::unlisten(checks::listener* listnr) {
  log_debug(logging::medium)
      << "listener " << listnr << " stops listening check " << this;
  _listnr = nullptr;
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Attempt to close channel.
 *
 *  @return true while channel was not closed properly.
 */
bool check::_close() {
  bool retval;

  // Check that channel was opened.
  if (_channel) {
    // Attempt to close channel.
    int ret(libssh2_channel_close(_channel));
    if (ret) {
      if (ret != LIBSSH2_ERROR_EAGAIN) {
        char* msg;
        libssh2_session_last_error(_session->get_libssh2_session(), &msg,
                                   nullptr, 0);
        if (ret == LIBSSH2_ERROR_SOCKET_SEND)
          _session->error();
        throw(basic_error() << "could not close channel: " << msg);
      }
      retval = true;
    }
    // Close succeeded.
    else {
      // Get exit status.
      int exitcode(libssh2_channel_get_exit_status(_channel));

      // Free channel.
      libssh2_channel_free(_channel);
      _channel = nullptr;

      // Method should not be called again.
      retval = false;

      if (_skip_stdout != -1)
        _skip_data(_stdout, _skip_stdout);
      if (_skip_stderr != -1)
        _skip_data(_stderr, _skip_stderr);

      // Send results to parent process.
      if (_cmds.empty()) {
        result r;
        r.set_command_id(_cmd_id);
        r.set_error(_stderr);
        r.set_executed(true);
        r.set_exit_code(exitcode);
        r.set_output(_stdout);
        _send_result_and_unregister(r);
      } else {
        _step = chan_open;
        on_available(*_session);
      }
    }
  }
  // Attempt to close a closed channel.
  else
    throw(
        basic_error() << "channel requested to close whereas it wasn't opened");

  return retval;
}

/**
 *  Attempt to execute the command.
 *
 *  @return true while the command was not successfully executed.
 */
bool check::_exec() {
  // Attempt to execute command.
  int ret(libssh2_channel_exec(_channel, _cmds.front().c_str()));

  // Check that we can try again later.
  if (ret && (ret != LIBSSH2_ERROR_EAGAIN)) {
    char* msg;
    libssh2_session_last_error(_session->get_libssh2_session(), &msg, nullptr,
                               0);
    if (ret == LIBSSH2_ERROR_SOCKET_SEND)
      _session->error();
    throw basic_error() << "could not execute command on SSH channel: " << msg
                        << " (error " << ret << ")";
  }

  // Check whether command succeeded or if we can try again later.
  bool retval;
  if (ret == LIBSSH2_ERROR_EAGAIN)
    retval = true;
  else {
    retval = false;
    _cmds.pop_front();
  }
  return retval;
}

/**
 *  Attempt to open a channel.
 *
 *  @return true while the channel was not successfully opened.
 */
bool check::_open() {
  _channel = _session->new_channel();
  return !_channel;
}

/**
 *  Attempt to read command output.
 *
 *  @return true while command output can be read again.
 */
bool check::_read() {
  // Read command's stdout.
  char buffer[BUFSIZ];
  int orb(libssh2_channel_read_ex(_channel, 0, buffer, sizeof(buffer)));

  // Error occured.
  if (orb < 0) {
    // Only throw is error is fatal.
    if (orb != LIBSSH2_ERROR_EAGAIN) {
      char* msg;
      libssh2_session_last_error(_session->get_libssh2_session(), &msg, nullptr,
                                 0);
      if (orb == LIBSSH2_ERROR_SOCKET_SEND)
        _session->error();
      throw(basic_error() << "failed to read command output: " << msg);
    }
  }
  // Append data.
  else
    _stdout.append(buffer, orb);

  // Read command's stderr.
  int erb(libssh2_channel_read_ex(_channel, 1, buffer, sizeof(buffer)));
  if (erb > 0)
    _stderr.append(buffer, erb);

  // Should we read again ?
  return (((orb > 0) || (LIBSSH2_ERROR_EAGAIN == orb) || (erb > 0) ||
           (LIBSSH2_ERROR_EAGAIN == erb)) &&
          !libssh2_channel_eof(_channel));
}

/**
 *  Send check result and unregister from session.
 *
 *  @param[in] r Check result.
 */
void check::_send_result_and_unregister(result const& r) {
  // Remove timeout task.
  if (_timeout) {
    try {
      multiplexer::instance().com::centreon::task_manager::remove(_timeout);
    } catch (...) {
    }
    _timeout = 0;
  }

  // Check that session is valid.
  if (_session) {
    // Unregister from session.
    log_debug(logging::high)
        << "check " << this << " is unregistering from session " << _session;

    // Unregister from session.
    _session->unlisten(this);
    _session = nullptr;
  }

  // Check that we haven't already send a check result.
  if (_cmd_id) {
    // Reset command ID.
    _cmd_id = 0;

    // Send check result to listener.
    if (_listnr)
      _listnr->on_result(r);
  }
}

/**
 *  Skip n lines.
 *
 *  @param[in] data    The string to truncate.
 *  @param[in] nb_line The number of lines to keep.
 *
 *  @return The first argument.
 */
std::string& check::_skip_data(std::string& data, int nb_line) {
  if (nb_line < 0)
    return data;
  if (!nb_line)
    data.clear();
  else {
    size_t pos(0);
    for (int i(0); i < nb_line && pos != std::string::npos; ++i)
      pos = data.find("\n", pos + 1);
    if (pos != std::string::npos)
      data.resize(pos);
  }
  return data;
}
