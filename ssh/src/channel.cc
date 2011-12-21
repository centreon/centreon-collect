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
#include "com/centreon/connector/ssh/channel.hh"
#include "com/centreon/connector/ssh/commander.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] sess    Session object.
 *  @param[in] cmd     Command line to execute.
 *  @param[in] cmd_id  Command ID.
 */
channel::channel(
           LIBSSH2_SESSION* sess,
           std::string const& cmd,
           unsigned long long cmd_id)
  : _channel(NULL),
    _cmd(cmd),
    _cmd_id(cmd_id),
    _session(sess),
    _step(chan_open) {}

/**
 *  Destructor.
 */
channel::~channel() {
  if (_channel) {
    // Close channel.
    while (libssh2_channel_close(_channel) == LIBSSH2_ERROR_EAGAIN)
      ;

    // Free channel.
    libssh2_channel_free(_channel);
  }
}

/**
 *  Get command ID associated with channel.
 *
 *  @return Command ID associated with channel.
 */
unsigned long long channel::get_command_id() const {
  return (_cmd_id);
}

/**
 *  Attempt to run command.
 *
 *  @param[out] cr Command result.
 *
 *  @return false when the check result is available.
 */
bool channel::run(check_result& cr) {
  bool retval(true);
  switch (_step) {
   case chan_open:
    if (!_open()) {
      _step = chan_exec;
      retval = run(cr);
    }
    break ;
   case chan_exec:
    if (!_exec()) {
      _step = chan_read;
      retval = run(cr);
    }
    break ;
   case chan_read:
    if (!_read()) {
      _step = chan_close;
      retval = run(cr);
    }
    break ;
   case chan_close:
     retval = _close(cr);
    break ;
   default:
    throw (basic_error() << "channel requested to run at invalid step");
  }
  return (retval);
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
 *  @param[in] c Object to copy.
 */
channel::channel(channel const& c) {
  (void)c;
  assert(!"channel is not copyable");
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] c Object to copy.
 *
 *  @return This object.
 */
channel& channel::operator=(channel const& c) {
  (void)c;
  assert(!"channel is not copyable");
  abort();
  return (*this);
}

/**
 *  Attempt to close channel.
 *
 *  @return true while channel was not closed properly.
 */
bool channel::_close(check_result& cr) {
  // Close failed.
  bool retval;

  // Check that channel was opened.
  if (_channel) {
    // Attempt to close channel.
    int ret(libssh2_channel_close(_channel));
    if (ret) {
      if (ret != LIBSSH2_ERROR_EAGAIN) {
        char* msg;
        libssh2_session_last_error(_session, &msg, NULL, 0);
        throw (basic_error() << "could not close channel: " << msg);
      }
      retval = true;
    }
    // Close succeeded.
    else {
      // Get exit status.
      int exitcode(libssh2_channel_get_exit_status(_channel));

      // Send results to parent process.
      cr.set_command_id(_cmd_id);
      cr.set_error(_stderr);
      cr.set_executed(true);
      cr.set_exit_code(exitcode);
      cr.set_output(_stdout);

      // Free channel.
      libssh2_channel_free(_channel);
      _channel = NULL;

      // Method should not be called again.
      retval = false;
    }
  }
  // No channel = successful close.
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
bool channel::_exec() {
  // Attempt to execute command.
  int ret(libssh2_channel_exec(_channel, _cmd.c_str()));

  // Check that we can try again later.
  if (ret && (ret != LIBSSH2_ERROR_EAGAIN)) {
    char* msg;
    libssh2_session_last_error(_session, &msg, NULL, 0);
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
bool channel::_open() {
  // Return value.
  bool retval;

  // Attempt to open channel.
  _channel = libssh2_channel_open_session(_session);
  if (_channel)
    retval = false;
  // Channel creation failed, check that we can try again later.
  else {
    char* msg;
    int ret(libssh2_session_last_error(_session, &msg, NULL, 0));
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
bool channel::_read() {
  // Read command's stdout.
  char buffer[BUFSIZ];
  int orb(libssh2_channel_read_ex(_channel, 0, buffer, sizeof(buffer)));

  // Error occured.
  if (orb < 0) {
    // Only throw is error is fatal.
    if (orb != LIBSSH2_ERROR_EAGAIN) {
      char* msg;
      libssh2_session_last_error(_session, &msg, NULL, 0);
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
