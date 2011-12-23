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
#include <stdio.h>
#include <stdlib.h>
#include "com/centreon/connector/ssh/checks/check.hh"
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
 */
check::check()
  : _channel(NULL),
    _cmd_id(0),
    _listnr(NULL),
    _session(NULL),
    _step(chan_open) {}

/**
 *  Destructor.
 */
check::~check() throw () {
  if (_channel) {
    // Close channel.
    while (libssh2_channel_close(_channel) == LIBSSH2_ERROR_EAGAIN)
      ;

    // Free channel.
    libssh2_channel_free(_channel);
  }
}

/**
 *  Session socket was closed.
 *
 *  @param[in] h Session socket handle.
 */
void check::close(handle& h) {
  (void)h;
  logging::error(logging::low)
    << "session socket was closed, check is aborted";
  result r;
  r.set_command_id(_cmd_id);
  _send_result_and_unregister(r);
  return ;
}

/**
 *  Error on session socket.
 *
 *  @param[in] h Session socket handle.
 */
void check::error(handle& h) {
  (void)h;
  logging::error(logging::low)
    << "session socket has error, check is aborted";
  result r;
  r.set_command_id(_cmd_id);
  _send_result_and_unregister(r);
  return ;
}

/**
 *  Start executing a check.
 *
 *  @param[in] sess    Session on which a channel will be opened.
 *  @param[in] cmd_id  Command ID.
 *  @param[in] cmd     Command to execute.
 *  @param[in] timeout Command timeout.
 */
void check::execute(
              sessions::session& sess,
              unsigned long long cmd_id,
              std::string const& cmd,
              time_t timeout) {
  _cmd = cmd;
  _cmd_id = cmd_id;
  _session = &sess;
  _timeout = timeout;
  if (sess.is_connected())
    on_connected(sess);
  else
    sess.listen(this);
  return ;
}

/**
 *  Listen the check.
 *
 *  @param[in] listnr Listener.
 */
void check::listen(checks::listener* listnr) {
  _listnr = listnr;
  return ;
}

/**
 *  On session close.
 *
 *  @param[in] sess Closing session.
 */
void check::on_close(sessions::session& sess) {
  (void)sess;
  logging::error(logging::medium)
    << "session closed before check could execute";
  result r;
  r.set_command_id(_cmd_id);
  _send_result_and_unregister(r);
  return ;
}

/**
 *  Called when session is connected.
 *
 *  @param[in] sess Connected session.
 */
void check::on_connected(sessions::session& sess) {
  multiplexer::instance().handle_manager::add(
    sess.get_socket_handle(),
    this);
  return ;
}

/**
 *  Can perform action on channel.
 *
 *  @param[in] h Unused.
 */
void check::read(handle& h) {
  try {
    switch (_step) {
    case chan_open:
      if (!_open()) {
        _step = chan_exec;
        read(h);
      }
      break ;
    case chan_exec:
      if (!_exec()) {
        _step = chan_read;
        read(h);
      }
      break ;
    case chan_read:
      if (!_read()) {
        _step = chan_close;
        read(h);
      }
      break ;
    case chan_close:
      _close();
      break ;
    default:
      throw (basic_error() << "channel requested to run at invalid step");
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high)
      << "error occured while executing a check: " << e.what();
    result r;
    r.set_command_id(_cmd_id);
    _send_result_and_unregister(r);
  }
  catch (...) {
    logging::error(logging::high)
      << "unknown error occured while executing a check";
    result r;
    r.set_command_id(_cmd_id);
    _send_result_and_unregister(r);
  }
  return ;
}

/**
 *  Stop listening to the check.
 *
 *  @param[in] listnr Listener.
 */
void check::unlisten(checks::listener* listnr) {
  (void)listnr;
  _listnr = NULL;
  return ;
}

/**
 *  Do we want to read ?
 *
 *  @param[in] h Handle.
 *
 *  @return true if we want to read.
 */
bool check::want_read(handle& h) {
  return (_session->want_read(h));
}

/**
 *  Do we want to write ?
 *
 *  @param[in] h Handle.
 *
 *  @return true if we want to write.
 */
bool check::want_write(handle& h) {
  return (_session->want_write(h));
}

/**
 *  Can perform action on channel.
 *
 *  @param[in] h Unused.
 */
void check::write(handle& h) {
  read(h);
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
 *  @param[in] c Unused.
 */
check::check(check const& c) : sessions::listener(c) {
  (void)c;
  assert(!"check is not copyable");
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] c Unused.
 *
 *  @return This object.
 */
check& check::operator=(check const& c) {
  (void)c;
  assert(!"check is not copyable");
  abort();
  return (*this);
}

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
        libssh2_session_last_error(
          _session->get_libssh2_session(),
          &msg,
          NULL,
          0);
        throw (basic_error() << "could not close channel: " << msg);
      }
      retval = true;
    }
    // Close succeeded.
    else {
      // Get exit status.
      int exitcode(libssh2_channel_get_exit_status(_channel));

      // Free channel.
      libssh2_channel_free(_channel);
      _channel = NULL;

      // Method should not be called again.
      retval = false;

      // Send results to parent process.
      result r;
      r.set_command_id(_cmd_id);
      r.set_error(_stderr);
      r.set_executed(true);
      r.set_exit_code(exitcode);
      r.set_output(_stdout);
      _send_result_and_unregister(r);
    }
  }
  // Attempt to close a closed channel.
  else
    throw (basic_error()
             << "channel requested to close whereas it wasn't opened");

  return (retval);
}

/**
 *  Attempt to execute the command.
 *
 *  @return true while the command was not successfully executed.
 */
bool check::_exec() {
  // Attempt to execute command.
  int ret(libssh2_channel_exec(_channel, _cmd.c_str()));

  // Check that we can try again later.
  if (ret && (ret != LIBSSH2_ERROR_EAGAIN)) {
    char* msg;
    libssh2_session_last_error(
      _session->get_libssh2_session(),
      &msg,
      NULL,
      0);
    throw (basic_error()
             << "could not execute command on SSH channel: "
             << msg << " (error " << ret << ")");
  }

  // Check whether command succeeded or if we can try again later.
  return (ret == LIBSSH2_ERROR_EAGAIN);
}

/**
 *  Attempt to open a channel.
 *
 *  @return true while the channel was not successfully opened.
 */
bool check::_open() {
  // Return value.
  bool retval;

  // Attempt to open channel.
  _channel = libssh2_channel_open_session(
               _session->get_libssh2_session());
  if (_channel)
    retval = false;
  // Channel creation failed, check that we can try again later.
  else {
    char* msg;
    int ret(libssh2_session_last_error(
              _session->get_libssh2_session(),
              &msg,
              NULL,
              0));
    if (ret != LIBSSH2_ERROR_EAGAIN)
      throw (basic_error() << "could not open SSH channel: " << msg);
    else
      retval = true;
  }

  return (retval);
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
      libssh2_session_last_error(
        _session->get_libssh2_session(),
        &msg,
        NULL,
        0);
      throw (basic_error() << "failed to read command output: " << msg);
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
  return ((orb > 0)
          || (LIBSSH2_ERROR_EAGAIN == orb)
          || (erb > 0)
          || (LIBSSH2_ERROR_EAGAIN == erb));
}

/**
 *  Send check result and unregister from session.
 *
 *  @param[in] r Check result.
 */
void check::_send_result_and_unregister(result const& r) {
  // Unregister from multiplexer.
  multiplexer::instance().handle_manager::remove(this);

  // Unregister from session.
  _session->unlisten(this);
  _session = NULL;

  // Check that was haven't already send a check result.
  if (_cmd_id) {
    // Send check result to listeners.
    if (_listnr)
      _listnr->on_result(r);

    // Reset command ID.
    _cmd_id = 0;
  }

  return ;
}
