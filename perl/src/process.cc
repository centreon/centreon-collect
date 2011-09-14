/*
** Copyright 2011 Merethis
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

#include <errno.h>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "com/centreon/connector/perl/process.hh"

using namespace com::centreon::connector::perl;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] p Object to copy.
 */
void process::_internal_copy(process const& p) {
  _cmd = p._cmd;
  _fd_err = dup(p._fd_err);
  if (_fd_err < 0) {
    char const* msg(strerror(errno));
    std::cerr << "could not copy process file descriptor: "
              << msg << std::endl;
  }
  _fd_out = dup(p._fd_out);
  if (_fd_out < 0) {
    char const* msg(strerror(errno));
    std::cerr << "could not copy process file descriptor: "
              << msg << std::endl;
  }
  _signal = p._signal;
  _stderr = p._stderr;
  _stdout = p._stdout;
  _timeout = p._timeout;
  return ;
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] cmd_id Command ID.
 *  @param[in] fd_out stdout of process.
 *  @param[in] fd_err stderr of process.
 */
process::process(unsigned long long cmd_id, int fd_out, int fd_err)
  : _cmd(cmd_id),
    _fd_err(fd_err),
    _fd_out(fd_out),
    _signal(SIGTERM),
    _timeout(0) {}

/**
 *  Copy constructor.
 *
 *  @param[in] p Object to copy.
 */
process::process(process const& p) {
  _internal_copy(p);
}

/**
 *  Destructor.
 */
process::~process() {
  if (_fd_err >= 0)
    ::close(_fd_err);
  if (_fd_out >= 0)
    ::close(_fd_out);
}

/**
 *  Assignment operator.
 *
 *  @param[in] p Object to copy.
 *
 *  @return This object.
 */
process& process::operator=(process const& p) {
  this->close();
  _internal_copy(p);
  return (*this);
}

/**
 *  Close file descriptors.
 */
void process::close() {
  if (_fd_err >= 0) {
    // Read remaining data.
    pollfd fd;
    memset(&fd, 0, sizeof(fd));
    fd.fd = _fd_err;
    fd.events = POLLIN | POLLPRI;
    char buffer[BUFSIZ];
    while (poll(&fd, 1, 0) > 0) {
      ssize_t rb(read(_fd_err, buffer, sizeof(buffer)));
      if (rb > 0)
        _stderr.append(buffer, rb);
      else
        break ;
    }
    ::close(_fd_err);
  }
  if (_fd_out >= 0) {
    // Read remaining data.
    pollfd fd;
    memset(&fd, 0, sizeof(fd));
    fd.fd = _fd_out;
    fd.events = POLLIN | POLLPRI;
    char buffer[BUFSIZ];
    while (poll(&fd, 1, 0) > 0) {
      ssize_t rb(read(_fd_out, buffer, sizeof(buffer)));
      if (rb > 0)
        _stdout.append(buffer, rb);
      else
        break ;
    }
    ::close(_fd_out);
  }
  _fd_err = -1;
  _fd_out = -1;
  return ;
}

/**
 *  Get the command ID of this process.
 *
 *  @return Command ID of this process.
 */
unsigned long long process::cmd() const {
  return (_cmd);
}

/**
 *  Get the error output of the process.
 *
 *  @return Error output.
 */
std::string const& process::err() const {
  return (_stderr);
}

/**
 *  Get the standard output of the process.
 *
 *  @return Standard output.
 */
std::string const& process::out() const {
  return (_stdout);
}

/**
 *  Read error data.
 */
void process::read_err() {
  char buffer[BUFSIZ];
  ssize_t rb(read(_fd_err, buffer, sizeof(buffer)));
  if (rb < 0) {
    char const* msg(strerror(errno));
    std::cerr << "error while reading process' stderr: "
              << msg << std::endl;
  }
  else if (!rb)
    this->close();
  else
    _stderr.append(buffer, rb);
  return ;
}

/**
 *  Get the error FD to monitor for read readiness.
 *
 *  @return Error FD to monitor for read readiness.
 */
int process::read_err_fd() const {
  return (_fd_err);
}

/**
 *  Read output data.
 */
void process::read_out() {
  char buffer[BUFSIZ];
  ssize_t rb(read(_fd_out, buffer, sizeof(buffer)));
  if (rb < 0) {
    char const* msg(strerror(errno));
    std::cerr << "error while reading process' stdout: "
              << msg << std::endl;
  }
  else if (!rb)
    this->close();
  else
    _stdout.append(buffer, rb);
  return ;
}

/**
 *  Get the output FD to monitor for read readiness.
 *
 *  @return Output FD to monitor for read readiness.
 */
int process::read_out_fd() const {
  return (_fd_out);
}

/**
 *  Get the signal to send to the process in case it timeouts.
 *
 *  @return Signal to send to process.
 */
int process::signal() const {
  return (_signal);
}

/**
 *  Set the signal to send to the process in case it timeouts.
 *
 *  @param[in] signum Signal to send to process.
 */
void process::signal(int signum) {
  _signal = signum;
  return ;
}

/**
 *  Get the process timeout.
 *
 *  @return Time at which the process will timeout.
 */
time_t process::timeout() const {
  return (_timeout);
}

/**
 *  Set the process timeout.
 *
 *  @param[in] t Timeout.
 */
void process::timeout(time_t t) {
  _timeout = t;
  return ;
}
